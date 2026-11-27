#include <ul/menu/ui/ui_ArtworkClient.hpp>
#include <switch.h>
#include <curl/curl.h>
#include <json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace ul::menu::art {
    namespace {
        using Json=nlohmann::json;
        constexpr const char *CA="sdmc:/glassnx/cacert.pem";
        constexpr const char *Key="sdmc:/glassnx/steamgriddb.key";
        constexpr const char *Api="https://www.steamgriddb.com/api/v2";
        struct Response {std::string bytes;size_t limit;};
        size_t Write(char *ptr,size_t size,size_t count,void *opaque){
            auto &r=*static_cast<Response*>(opaque);
            if(size && count>SIZE_MAX/size)return 0;
            const size_t n=size*count;
            if(n>r.limit-r.bytes.size())return 0;
            r.bytes.append(ptr,n);return n;
        }
        bool ReadKey(std::string &key){
            auto fp=fopen(Key,"rb");if(!fp)return false;
            char buf[130]={};const auto n=fread(buf,1,sizeof(buf)-1,fp);fclose(fp);
            key.assign(buf,n);
            while(!key.empty()&&(key.back()=='\r'||key.back()=='\n'||key.back()==' '))key.pop_back();
            return key.size()>=16&&key.size()<=128&&std::all_of(key.begin(),key.end(),[](unsigned char c){return std::isalnum(c)!=0||c=='-'||c=='_';});
        }
        bool Fetch(const std::string &url,bool auth,size_t limit,std::string &out,std::string &error){
            auto ca=fopen(CA,"rb");
            if(!ca){error="Missing /glassnx/cacert.pem (trusted CA bundle).";return false;}
            fclose(ca);
            std::string key;
            if(auth&&!ReadKey(key)){error="Missing or invalid /glassnx/steamgriddb.key.";return false;}
            if(auth&&url.rfind(std::string(Api)+"/",0)!=0){error="Invalid API URL.";return false;}
            if(!auth&&url.rfind("https://cdn2.steamgriddb.com/",0)!=0&&url.rfind("https://cdn.steamgriddb.com/",0)!=0){error="Unsupported artwork host.";return false;}
            if(R_FAILED(socketInitializeDefault())){error="Network unavailable.";return false;}
            auto curl=curl_easy_init();
            if(!curl){socketExit();error="Could not initialize HTTPS.";return false;}
            curl_slist *headers=nullptr;
            if(auth)headers=curl_slist_append(nullptr,("Authorization: Bearer "+key).c_str());
            if(auth&&!headers){curl_easy_cleanup(curl);socketExit();error="Could not allocate request headers.";return false;}
            Response response{{},limit};
            curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
            curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
            curl_easy_setopt(curl,CURLOPT_USERAGENT,"GlassNX/0.14");
            curl_easy_setopt(curl,CURLOPT_CAINFO,CA);
            curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
            curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
            curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
            curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
            curl_easy_setopt(curl,CURLOPT_TIMEOUT,10L);
            curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,Write);
            curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
            const auto rc=curl_easy_perform(curl);long http=0;
            curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
            curl_slist_free_all(headers);curl_easy_cleanup(curl);socketExit();
            if(rc!=CURLE_OK||http!=200){error="HTTPS request failed ("+std::to_string(int(rc))+", HTTP "+std::to_string(http)+"). Check Wi-Fi, CA and key.";return false;}
            out=std::move(response.bytes);return true;
        }
        std::string Encode(const std::string &s){
            const char *hex="0123456789ABCDEF";std::string out;
            for(unsigned char c:s){
                if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);
                else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
            }
            return out;
        }
        bool Data(const std::string &path,Json &data,std::string &error){
            std::string bytes;if(!Fetch(std::string(Api)+path,true,512*1024,bytes,error))return false;
            const auto j=Json::parse(bytes,nullptr,false);
            if(!j.is_object()||!j.contains("success")||j["success"]!=true||!j.contains("data")||!j["data"].is_array()){error="Invalid SteamGridDB response.";return false;}
            data=j["data"];return true;
        }
        bool JpegSize(const std::string &b,int &w,int &h){
            const auto u=[&](size_t p){return static_cast<unsigned char>(b[p]);};
            if(b.size()<4||u(0)!=255||u(1)!=216)return false;
            size_t p=2;
            while(p+4<=b.size()){
                if(u(p++)!=255)return false;
                while(p<b.size()&&u(p)==255)++p;
                if(p>=b.size())return false;
                const int marker=u(p++);
                if(marker==217||marker==218)return false;
                if(p+2>b.size())return false;
                const size_t n=(u(p)<<8)|u(p+1);
                if(n<2||n>b.size()-p)return false;
                if(marker==192||marker==193||marker==194){
                    if(n<8)return false;
                    h=(u(p+3)<<8)|u(p+4);w=(u(p+5)<<8)|u(p+6);
                    return w>0&&h>0;
                }
                p+=n;
            }
            return false;
        }
    }
    bool FindArtworkGames(const std::string &name,std::vector<ArtworkGame> &out,std::string &error){
        Json data;if(!Data("/search/autocomplete/"+Encode(name),data,error))return false;
        for(const auto &v:data)if(out.size()<8&&v.is_object()&&v.contains("id")&&v["id"].is_number_integer()&&v.contains("name")&&v["name"].is_string()){
            const auto id=v["id"].get<int>();const auto title=v["name"].get<std::string>();
            if(id>0&&!title.empty())out.push_back({id,title.substr(0,100)});
        }
        if(out.empty()){error="No matching games.";return false;}return true;
    }
    bool FindPictures(int id,bool hero,std::vector<ArtworkHero> &out,std::string &error){
        Json data;if(!Data(std::string(hero?"/heroes/game/":"/grids/game/")+std::to_string(id)+(hero?"?types=static&nsfw=false&humor=false":"?types=static&nsfw=false&humor=false&dimensions=512x512,1024x1024,600x900,460x215"),data,error))return false;
        for(const auto &v:data)if(out.size()<6&&v.is_object()&&v.contains("url")&&v["url"].is_string()&&v.contains("width")&&v["width"].is_number_integer()&&v.contains("height")&&v["height"].is_number_integer()){
            if(v.contains("nsfw")&&v["nsfw"]!=false)continue;
            if(v.contains("humor")&&v["humor"]!=false)continue;
            const auto url=v["url"].get<std::string>();const int w=v["width"].get<int>(),h=v["height"].get<int>();
            if(w<=0||h<=0||w>(hero?1920:1024)||h>(hero?1080:1024))continue;
            if(url.size()<4||url.size()>512)continue;
            if(url.substr(url.size()-4)!=".jpg"&&url.substr(url.size()-4)!=".png"&&(url.size()<5||url.substr(url.size()-5)!=".jpeg"))continue;
            out.push_back({url,w,h});
        }
        if(out.empty()){error="No compatible static JPG or PNG artwork found.";return false;}return true;
    }
    bool DownloadPicture(const ArtworkHero &item,bool hero,std::string &data,std::string &error){
        if(!Fetch(item.url,false,3*1024*1024,data,error))return false;
        int w=0,h=0;
        if(data.size()>=24&&memcmp(data.data(),"\x89PNG\r\n\x1a\n",8)==0&&memcmp(data.data()+12,"IHDR",4)==0){
            auto be=[&](size_t i){return (uint32_t(uint8_t(data[i]))<<24)|(uint32_t(uint8_t(data[i+1]))<<16)|(uint32_t(uint8_t(data[i+2]))<<8)|uint32_t(uint8_t(data[i+3]));};
            const auto pw=be(16),ph=be(20);
            if(pw<=1920&&ph<=1080){w=int(pw);h=int(ph);}
        } else JpegSize(data,w,h);
        if(w<=0||h<=0||w>(hero?1920:1024)||h>(hero?1080:1024)){error="Unsupported image dimensions.";data.clear();return false;}
        return true;
    }
    bool SavePicture(const std::string &path,const std::string &data,std::string &error){
        mkdir("sdmc:/glassnx/art",0777);
        const auto temp=path+".tmp";
        auto fp=fopen(temp.c_str(),"wb");if(!fp){error="Unable to save artwork.";return false;}
        const bool wrote=fwrite(data.data(),1,data.size(),fp)==data.size();
        const bool closed=fclose(fp)==0;
        if(!wrote||!closed||rename(temp.c_str(),path.c_str())!=0){remove(temp.c_str());error="Artwork could not be saved.";return false;}
        return true;
    }
}
