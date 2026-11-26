#include <ul/man/man_Manager.hpp>
#include <ul/ul_Include.hpp>
#include <ul/fs/fs_Stdio.hpp>

namespace ul::man {

    namespace {
        const std::string BaseFile=std::string(BaseSystemPath)+"/exefs.nsp";
        const std::string ActiveFile=std::string(ActiveSystemPath)+"/exefs.nsp";
        bool SameFile(const std::string &a,const std::string &b) {
            auto fa=fopen(a.c_str(),"rb");if(!fa)return false;
            auto fb=fopen(b.c_str(),"rb");if(!fb){fclose(fa);return false;}
            u8 aa[4096],bb[4096];bool same=true;
            for(;;) {
                const auto na=fread(aa,1,sizeof(aa),fa),nb=fread(bb,1,sizeof(bb),fb);
                if(na!=nb || memcmp(aa,bb,na)!=0 || ferror(fa)||ferror(fb)){same=false;break;}
                if(na==0)break;
            }
            fclose(fa);fclose(fb);return same;
        }
    }
    bool IsBasePresent() {
        return fs::ExistsFile(BaseFile) &&
            fs::ExistsFile("sdmc:/glassnx/bin/uMenu/main") &&
            fs::ExistsFile("sdmc:/glassnx/bin/uMenu/main.npdm") &&
            fs::ExistsFile("sdmc:/glassnx/bin/uMenu/romfs.bin") &&
            fs::ExistsFile("sdmc:/glassnx/bin/uLoader/applet/main") &&
            fs::ExistsFile("sdmc:/glassnx/bin/uLoader/applet/main.npdm") &&
            fs::ExistsFile("sdmc:/glassnx/bin/uLoader/application/main") &&
            fs::ExistsFile("sdmc:/glassnx/bin/uLoader/application/main.npdm");
    }

    bool IsSystemActive() {
        return IsBasePresent() && SameFile(BaseFile,ActiveFile);
    }

    bool ActivateSystem() {
        if(!IsBasePresent())return false;
        if(IsSystemActive())return true;
        // Never silently overwrite a different HOME replacement.
        if(fs::ExistsFile(ActiveFile))return false;
        fs::CreateDirectory(ActiveSystemPath);
        const auto tmp=ActiveFile+".glassnx-tmp";
        fs::CopyFile(BaseFile,tmp);
        if(!SameFile(BaseFile,tmp)){remove(tmp.c_str());return false;}
        if(rename(tmp.c_str(),ActiveFile.c_str())!=0){remove(tmp.c_str());return false;}
        return IsSystemActive();
    }

    bool DeactivateSystem() {
        // Leave NXThemes romfs, configuration and unrelated overrides intact.
        if(!IsSystemActive())return false;
        return remove(ActiveFile.c_str())==0;
    }

    std::string Version::AsString() const {
        auto as_str = std::to_string(this->major) + "." + std::to_string(this->minor);
        if(this->micro > 0) {
            as_str += "." + std::to_string(this->micro);
        }
        return as_str;
    }

    Version Version::FromString(const std::string &ver_str) {
        auto ver_str_cpy = ver_str;
        Version v = {};
        size_t pos = 0;
        std::string token;
        u32 c = 0;
        std::string delimiter = ".";
        while((pos = ver_str_cpy.find(delimiter)) != std::string::npos) {
            token = ver_str_cpy.substr(0, pos);
            if(c == 0) {
                v.major = std::stoi(token);
            }
            else if(c == 1) {
                v.minor = std::stoi(token);
            }
            else if(c == 2) {
                v.micro = std::stoi(token);
            }
            ver_str_cpy.erase(0, pos + delimiter.length());
            c++;
        }

        if(c == 0) {
            v.major = std::stoi(ver_str_cpy);
        }
        else if(c == 1) {
            v.minor = std::stoi(ver_str_cpy);
        }
        else if(c == 2) {
            v.micro = std::stoi(ver_str_cpy);
        }
        return v;
    }

}
