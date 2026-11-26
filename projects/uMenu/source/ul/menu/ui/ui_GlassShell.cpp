#include <ul/menu/ui/ui_GlassShell.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <SDL2/SDL_image.h>
#include <switch/runtime/devices/fs_dev.h>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {
    namespace {
        constexpr const char *Themes[] = {"Pastel Dream","Midnight Glass","Frosted White","Aurora"};
        constexpr pu::ui::Color Tints[] = {{113,111,190,0},{18,28,62,148},{198,205,237,70},{36,123,122,100}};
        constexpr pu::ui::Color GlassColors[] = {{177,186,242,255},{85,108,175,255},{193,208,241,255},{105,206,197,255}};
        std::string EntryName(Entry &e) {
            if(e.Is<EntryType::Folder>()) return e.folder_info.name;
            if(e.Is<EntryType::Application>() || e.Is<EntryType::Homebrew>()) {
                if(e.control.name.empty()) e.TryLoadNacp();
                return e.control.name.empty() ? (e.Is<EntryType::Homebrew>() ? "Homebrew" : "Game") : e.control.name;
            }
            switch(e.type) {
                case EntryType::SpecialEntryMiiEdit:return "Mii Editor";
                case EntryType::SpecialEntryWebBrowser:return "Web Browser";
                case EntryType::SpecialEntryUserPage:return "Profile";
                case EntryType::SpecialEntrySettings:return "System Settings";
                case EntryType::SpecialEntryThemes:return "Themes";
                case EntryType::SpecialEntryControllers:return "Controllers";
                case EntryType::SpecialEntryAlbum:return "Album";
                case EntryType::SpecialEntryAmiibo:return "amiibo";
                default:return "System shortcut";
            }
        }
        std::string Battery() {return std::to_string(os::GetBatteryLevel())+"%";}
        std::string Clock() {
            const auto t=os::GetCurrentTime(); char b[16]={};
            snprintf(b,sizeof(b),"%02d:%02d",int(t.h),int(t.min)); return b;
        }
    }
    GlassShell::GlassShell(EntryMenu::Ref e,std::function<void(u32)> l,std::function<void()> old)
        :entries(e),launch(l),legacy(old) {LoadPreferences();LoadBackdrop();ReadRecent();}

    namespace {
        const char *PrefsPath() {
            return g_GlobalSettings.ams_is_emummc?"sdmc:/ulaunch/glassnx-emu.prefs":"sdmc:/ulaunch/glassnx-sys.prefs";
        }
        std::string Fold(std::string v) {
            for(auto &c:v)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return v;
        }
    }
    void GlassShell::ReadRecent() {
        const std::string path=std::string(PrefsPath())+".recent";
        auto fp=fopen(path.c_str(),"rb");if(!fp)return;
        unsigned long long id=0;while(recent.size()<64 && fscanf(fp,"%llx",&id)==1)if(id)recent.push_back(static_cast<u64>(id));
        fclose(fp);
    }
    void GlassShell::SaveRecent() {
        const std::string path=std::string(PrefsPath())+".recent",temp=path+".tmp";
        auto fp=fopen(temp.c_str(),"wb");if(!fp)return;
        bool ok=true;for(auto id:recent)if(fprintf(fp,"%016llx\n",static_cast<unsigned long long>(id))<0)ok=false;
        if(fclose(fp)!=0)ok=false;
        if(!ok || rename(temp.c_str(),path.c_str())!=0)remove(temp.c_str());
    }
    void GlassShell::RecordLaunch(u64 id) {
        if(!id)return;
        recent.erase(std::remove(recent.begin(),recent.end(),id),recent.end());
        recent.insert(recent.begin(),id);if(recent.size()>64)recent.resize(64);SaveRecent();
    }
    void GlassShell::LoadPreferences() {
        auto fp=fopen(PrefsPath(),"rb");if(!fp)return;
        int version=0,t=0,g=65,b=65,m=1,sort=0;unsigned count=0;
        if(fscanf(fp,"GlassNX %d %d %d %d %d %d %u",&version,&t,&g,&b,&m,&sort,&count)==7 && version==1 && count<=512) {
            theme=std::clamp(t,0,3);glass=std::clamp(g,35,85);blur=std::clamp(b,0,100);motion=m!=0;sort_name=sort!=0;
            for(unsigned i=0;i<count;i++){unsigned long long id=0;if(fscanf(fp,"%llx",&id)!=1)break;if(id)favorites.push_back(static_cast<u64>(id));}
        }
        fclose(fp);
    }
    void GlassShell::SavePreferences() {
        const std::string temp=std::string(PrefsPath())+".tmp";
        auto fp=fopen(temp.c_str(),"wb");if(!fp){g_MenuApplication->ShowNotification("Could not save appearance settings");return;}
        bool ok=fprintf(fp,"GlassNX 1 %d %d %d %d %d %u\n",theme,glass,blur,int(motion),int(sort_name),static_cast<unsigned>(favorites.size()))>0;
        for(auto id:favorites)if(fprintf(fp,"%016llx\n",static_cast<unsigned long long>(id))<0)ok=false;
        if(fclose(fp)!=0)ok=false;
        if(ok && rename(temp.c_str(),PrefsPath())==0)return;
        remove(temp.c_str());g_MenuApplication->ShowNotification("Could not save appearance settings");
    }
    void GlassShell::SetTheme(int index) {
        theme=std::clamp(index,0,3);materials.clear();SavePreferences();
        transition=std::chrono::steady_clock::now();
    }
    bool GlassShell::Favorite(u64 id) const {return std::find(favorites.begin(),favorites.end(),id)!=favorites.end();}
    void GlassShell::ToggleFavorite() {
        auto list=List();if(focus<0||focus>=static_cast<int>(list.size()))return;
        const auto &e=entries->GetEntries()[list[focus]];if(!e.Is<EntryType::Application>())return;
        auto it=std::find(favorites.begin(),favorites.end(),e.app_info.app_id);
        if(it!=favorites.end()){favorites.erase(it);g_MenuApplication->ShowNotification("Removed from favorites");}
        else if(favorites.size()<512){favorites.push_back(e.app_info.app_id);g_MenuApplication->ShowNotification("Added to favorites");}
        SavePreferences();
    }
    void GlassShell::Search() {
        SwkbdConfig cfg;if(R_FAILED(swkbdCreate(&cfg,0)))return;
        swkbdConfigMakePresetDefault(&cfg);
        swkbdConfigSetType(&cfg,SwkbdType_All);
        swkbdConfigSetGuideText(&cfg,"Search your library");
        swkbdConfigSetInitialText(&cfg,search);
        char result[128]={};auto rc=ShowSwkbd(&cfg,result,sizeof(result));swkbdClose(&cfg);
        if(R_SUCCEEDED(rc)){search=result;focus=offset=0;}
    }
    void GlassShell::LoadBackdrop() {
        const auto path=TryFindImage("ui/Background");if(path.empty())return;
        auto src=IMG_Load(path.c_str());if(!src)return;
        auto small=SDL_CreateRGBSurfaceWithFormat(0,240,135,32,SDL_PIXELFORMAT_RGBA32);
        if(small && SDL_BlitScaled(src,nullptr,small,nullptr)==0 && SDL_LockSurface(small)==0) {
            backdrop.resize(240*135*4);
            for(int y=0;y<135;y++)std::copy_n(static_cast<const u8*>(small->pixels)+y*small->pitch,240*4,backdrop.data()+y*240*4);
            SDL_UnlockSurface(small);
        }
        if(small)SDL_FreeSurface(small);
        SDL_FreeSurface(src);
    }
    pu::sdl2::TextureHandle::Ref GlassShell::Banner(u64 id) {
        if(banner_checked && banner_id==id)return banner;
        banner_checked=true;banner_id=id;banner.reset();
        char name[120]={};
        snprintf(name,sizeof(name),"sdmc:/ulaunch/glassnx/art/%016llx.jpg",static_cast<unsigned long long>(id));
        auto fp=fopen(name,"rb");if(!fp)return {};
        const bool seek_ok=fseek(fp,0,SEEK_END)==0;
        const long length=seek_ok?ftell(fp):-1;fclose(fp);
        if(length<=0||length>8*1024*1024)return {};
        auto tex=pu::ui::render::LoadImageFromFile(name);
        if(tex)banner=pu::sdl2::TextureHandle::New(tex);
        return banner;
    }
    void GlassShell::Cover(Renderer &r,const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h) {
        if(!t||!t->Get())return;
        int tw=0,th=0;if(SDL_QueryTexture(t->Get(),nullptr,nullptr,&tw,&th)!=0||tw<=0||th<=0)return;
        SDL_Rect source{0,0,tw,th};
        if(static_cast<long long>(tw)*h>static_cast<long long>(th)*w){source.w=th*w/h;source.x=(tw-source.w)/2;}
        else {source.h=tw*h/w;source.y=(th-source.h)/2;}
        // Use SDL's crop directly to preserve artwork aspect ratio.
        SDL_Rect dest{x,y,w,h};
        SDL_RenderCopy(pu::ui::render::GetMainRenderer(),t->Get(),&source,&dest);
        (void)r;
    }

    void GlassShell::SetPage(Page p) {
        if(p==page) return;
        if(p==Page::Control || p==Page::Notifications) previous=page;
        page=p; focus=0;offset=0;filter=0;search.clear();hits.clear();transition=std::chrono::steady_clock::now();
    }
    std::vector<u32> GlassShell::List() {
        std::vector<u32> result;
        auto &all=entries->GetEntries();
        for(u32 i=0;i<all.size();++i) {
            auto &e=all[i];
            if(e.Is<EntryType::Invalid>())continue;
            if(!search.empty() && Fold(EntryName(e)).find(Fold(search))==std::string::npos)continue;
            if(page==Page::Apps) {
                if((filter==0 && (e.Is<EntryType::Homebrew>()||e.IsSpecial())) ||
                    (filter==1 && e.Is<EntryType::Homebrew>()) ||
                    (filter==2 && e.IsSpecial() && !e.Is<EntryType::SpecialEntryAlbum>() && !e.Is<EntryType::SpecialEntryWebBrowser>()) ||
                    (filter==3 && (e.Is<EntryType::SpecialEntryAlbum>()||e.Is<EntryType::SpecialEntryWebBrowser>())))result.push_back(i);
            } else if(page==Page::Library) {
                if((filter==0 && (e.Is<EntryType::Application>()||e.Is<EntryType::Folder>())) ||
                    (filter==1 && e.Is<EntryType::Application>() && Favorite(e.app_info.app_id)) ||
                    (filter==2 && e.Is<EntryType::Folder>()) ||
                    (filter==3 && e.Is<EntryType::Application>() && std::find(recent.begin(),recent.end(),e.app_info.app_id)!=recent.end()))result.push_back(i);
            } else if(e.Is<EntryType::Application>() || e.Is<EntryType::Homebrew>()) result.push_back(i);
        }
        if(page==Page::Library && filter==3)std::stable_sort(result.begin(),result.end(),[&](u32 a,u32 b){return std::find(recent.begin(),recent.end(),all[a].app_info.app_id)<std::find(recent.begin(),recent.end(),all[b].app_info.app_id);});
        else if(sort_name && page!=Page::Home)std::stable_sort(result.begin(),result.end(),[&](u32 a,u32 b){return Fold(EntryName(all[a]))<Fold(EntryName(all[b]));});
        if(page==Page::Home) {
            const auto it=std::find_if(result.begin(),result.end(),[&](u32 i){return g_GlobalSettings.IsEntrySuspended(all[i]);});
            if(it!=result.end()) std::rotate(result.begin(),it,it+1);
        }
        return result;
    }
    void GlassShell::Picture(Renderer &r,const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h) {
        if(t && t->Get()) r->RenderTexture(t->Get(),x,y,pu::ui::render::TextureRenderOptions({},w,h,{},{},{}));
    }
    pu::sdl2::TextureHandle::Ref GlassShell::Material(int x,int y,int w,int h,bool selected) {
        const auto key=std::to_string(x)+"/"+std::to_string(y)+"/"+std::to_string(w)+"/"+std::to_string(h)+"/"+std::to_string(theme)+"/"+std::to_string(glass)+"/"+std::to_string(blur)+"/"+std::to_string(selected);
        auto it=materials.find(key);if(it!=materials.end())return it->second;
        // Cache at half logical resolution; the GPU composites the translucent
        // textured bevel. No per-frame rasterization or framebuffer readback.
        const int tw=(w+2)/3,th=(h+2)/3;
        const float radius=std::min(11.f,th/3.f);
        const auto tint=GlassColors[theme];
        std::vector<u8> rgba(static_cast<size_t>(tw)*th*4);
        auto byte=[](float v){return static_cast<u8>(std::clamp(v,0.f,255.f));};
        for(int py=0;py<th;py++) for(int px=0;px<tw;px++) {
            const float qx=std::abs(px+.5f-tw*.5f)-(tw*.5f-radius-1);
            const float qy=std::abs(py+.5f-th*.5f)-(th*.5f-radius-1);
            const float ox=std::max(qx,0.f),oy=std::max(qy,0.f);
            const float d=std::sqrt(ox*ox+oy*oy)+std::min(std::max(qx,qy),0.f)-radius;
            const float cover=std::clamp(.8f-d,0.f,1.f);
            const float v=float(py)/th;
            const float rim=std::max(0.f,1.f-std::abs(d+1.6f)/3.4f);
            const float light=std::max(0.f,1.f-std::abs(d+5.f)/7.f);
            const float shine=(1.f-v)*.12f+rim*(selected?.8f:.58f)+light*.15f;
            const auto p=(static_cast<size_t>(py)*tw+px)*4;
            // Sample the real wallpaper with a low-frequency blur and edge
            // displacement. Cached per panel position, never reads the GPU framebuffer.
            const float edge=std::max(0.f,1.f-std::abs(d)/6.f);
            const int sx=std::clamp(int((x+px*3+(px-tw*.5f)*edge*.11f)/8.f),0,239);
            const int sy=std::clamp(int((y+py*3+(py-th*.5f)*edge*.11f)/8.f),0,134);
            const int radius_sample=blur/15;
            for(int k=0;k<3;k++) {
                const float tc=k==0?tint.r:k==1?tint.g:tint.b;
                float bg=tc;
                if(!backdrop.empty()) {
                    unsigned sum=0;
                    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
                        sum+=backdrop[(std::clamp(sy+dy*radius_sample,0,134)*240+std::clamp(sx+dx*radius_sample,0,239))*4+k];
                    bg=sum/9.f;
                    const auto shade=Tints[theme];
                    const float sc=k==0?shade.r:k==1?shade.g:shade.b;
                    bg=bg*(1-shade.a/255.f)+sc*(shade.a/255.f);
                }
                const float colored=bg*.68f+tc*.32f;
                rgba[p+k]=byte(colored+(255-colored)*shine);
            }
            rgba[p+3]=byte(cover*((100-glass)*1.65f+90+22*(1-v)+rim*(selected?185:130)+light*32));
        }
        auto tex=SDL_CreateTexture(pu::ui::render::GetMainRenderer(),SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,tw,th);
        if(!tex)return {};
        if(SDL_UpdateTexture(tex,nullptr,rgba.data(),tw*4)!=0 || SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND)!=0) {SDL_DestroyTexture(tex);return {};}
        if(materials.size()>=96)materials.clear();
        auto handle=pu::sdl2::TextureHandle::New(tex);materials.emplace(key,handle);return handle;
    }
    void GlassShell::Card(Renderer &r,int x,int y,int w,int h,bool selected) {
        const int radius=std::min(34,h/3);
        r->RenderRoundedRectangleFill({23,26,65,55},x+2,y+6,w,h,radius);
        const auto tex=Material(x,y,w,h,selected);
        if(tex)Picture(r,tex,x,y,w,h);
        else {
            const auto c=GlassColors[theme];
            r->RenderRoundedRectangleFill({c.r,c.g,c.b,100},x,y,w,h,radius);
            r->RenderRoundedRectangle({233,239,255,135},x,y,w,h,radius);
        }
        if(selected) {
            r->RenderRoundedRectangle({185,228,255,115},x-5,y-5,w+10,h+10,radius+5);
            r->RenderRoundedRectangle({238,248,255,255},x-2,y-2,w+4,h+4,radius+2);
        }
    }
    void GlassShell::Label(Renderer &r,const std::string &s,int x,int y,int size,int maxw,bool center) {
        if(s.empty())return;
        const auto key=std::to_string(size)+"/"+std::to_string(maxw)+"/"+s;
        auto it=texts.find(key);
        if(it==texts.end()) {
            if(texts.size()>=160) texts.clear();
            auto t=pu::ui::elm::TextBlock::New(0,0,s);
            t->SetFont(pu::ui::MakeDefaultFontName(size));
            t->SetColor({255,255,255,255});
            if(maxw>0)t->SetClampWidth(maxw);
            it=texts.emplace(key,t).first;
        }
        const auto &t=it->second;
        t->OnRender(r,center?x-t->GetWidth()/2:x,y);
    }
    void GlassShell::Button(Renderer &r,int x,int y,int w,int h,const std::string &s,bool selected,std::function<void()> cb) {
        Card(r,x,y,w,h,selected); Label(r,s,x+w/2,y+(h-37)/2,30,w-24,true);
        hits.push_back({x,y,w,h,cb});
    }
    void GlassShell::Glyph(Renderer &r,const std::string &kind,int x,int y,int size) {
        const pu::ui::Color white{246,249,255,255};
        auto line=[&](int a,int b,int c,int d) {
            auto rr=pu::ui::render::GetMainRenderer();
            SDL_SetRenderDrawColor(rr,246,249,255,255);
            for(int i=-1;i<=1;i++)SDL_RenderDrawLine(rr,x+a*size/24+i,y+b*size/24,x+c*size/24+i,y+d*size/24);
        };
        auto box=[&](int a,int b,int w,int h){r->RenderRoundedRectangle(white,x+a*size/24,y+b*size/24,w*size/24,h*size/24,std::max(2,size/10));};
        auto dot=[&](int a,int b,int rad){r->RenderCircleFill(white,x+a*size/24,y+b*size/24,std::max(2,rad*size/24));};
        if(kind=="home"){line(2,11,12,2);line(12,2,22,11);box(5,10,14,12);line(12,16,12,22);}
        else if(kind=="games"){box(2,6,20,13);line(6,10,6,16);line(3,13,9,13);dot(16,11,1);dot(19,15,1);}
        else if(kind=="apps"){for(int a:{3,13})for(int b:{3,13})box(a,b,8,8);}
        else if(kind=="settings"){r->RenderCircle(white,x+size/2,y+size/2,size/4);for(int i=0;i<8;i++){float a=i*3.14159265f/4;line(12+int(7*cosf(a)),12+int(7*sinf(a)),12+int(11*cosf(a)),12+int(11*sinf(a)));}dot(12,12,2);}
        else if(kind=="wifi"){for(int i=0;i<3;i++){int span=10-i*3;line(12-span,6+i*4,12,3+i*4);line(12,3+i*4,12+span,6+i*4);}dot(12,20,2);}
        else if(kind=="bluetooth"){line(12,2,12,22);line(12,2,19,8);line(19,8,5,19);line(5,5,19,16);line(19,16,12,22);}
        else if(kind=="sun"){r->RenderCircle(white,x+size/2,y+size/2,size/5);for(int i=0;i<8;i++){float a=i*3.14159265f/4;line(12+int(7*cosf(a)),12+int(7*sinf(a)),12+int(11*cosf(a)),12+int(11*sinf(a)));}}
        else if(kind=="volume"){box(2,9,5,6);line(7,9,13,4);line(13,4,13,20);line(13,20,7,15);line(17,7,21,12);line(21,12,17,17);}
        else if(kind=="lock"){box(5,10,14,12);line(8,10,8,5);line(8,5,16,5);line(16,5,16,10);dot(12,16,1);}
        else if(kind=="power"){r->RenderCircle(white,x+size/2,y+size/2,size*2/5);line(12,1,12,12);}
        else if(kind=="bell"){box(5,4,14,14);line(2,18,22,18);dot(12,21,1);}
        else if(kind=="moon"){r->RenderCircle(white,x+size/2,y+size/2,size*2/5);line(15,3,9,12);line(9,12,18,20);}
        else if(kind=="album"){box(2,3,20,18);line(3,19,10,10);line(10,10,15,16);line(15,16,19,11);dot(17,7,2);}
        else if(kind=="battery"){box(2,6,18,12);r->RenderRectangleFill(white,x+size*20/24,y+size*10/24,size*2/24,size*4/24);}
        else if(kind=="storage"){box(3,5,18,14);line(5,14,19,14);dot(17,17,1);}
        else if(kind=="search"){r->RenderCircle(white,x+size*9/24,y+size*9/24,size*7/24);line(14,14,22,22);}
        else {box(3,3,18,18);dot(12,12,2);}
    }
    void GlassShell::IconButton(Renderer &r,int x,int y,int w,int h,const std::string &kind,const std::string &label,bool selected,std::function<void()> cb) {
        Card(r,x,y,w,h,selected);
        const int sz=std::min(35,h/2);
        Glyph(r,kind,x+15,y+(h-sz)/2,sz);
        Label(r,label,x+sz+26,y+(h-37)/2,30,w-sz-40);
        hits.push_back({x,y,w,h,cb});
    }
    void GlassShell::Tile(Renderer &r,u32 index,int x,int y,int w,int h,bool selected) {
        Card(r,x,y,w,h,selected);
        auto &e=entries->GetEntries().at(index);
        auto icon=entries->GlassIcon(index,icon_budget);
        if(!icon && e.Is<EntryType::Application>()) icon=GetDefaultApplicationIconTexture();
        int side=std::min(w-20,h-48);
        if(icon)Picture(r,icon,x+(w-side)/2,y+7,side,side);
        else Glyph(r,e.Is<EntryType::Folder>()?"storage":"apps",x+w/2-34,y+h/2-55,68);
        const auto name=EntryName(e);
        Label(r,name,x+w/2,y+h-40,27,w-24,true);
        if(e.Is<EntryType::Application>() && Favorite(e.app_info.app_id)) {
            r->RenderCircleFill({50,49,98,220},x+w-30,y+30,23);
            Label(r,"★",x+w-30,y+11,27,0,true);
        }
        hits.push_back({x,y,w,h,[this,index](){launch(index);}});
    }
    void GlassShell::ToggleRadio(bool wifi) {
        bool &value=wifi?g_GlobalSettings.wireless_lan_enabled:g_GlobalSettings.bluetooth_enabled;
        const bool next=!value;
        const auto rc=wifi?setsysSetWirelessLanEnableFlag(next):setsysSetBluetoothEnableFlag(next);
        if(R_SUCCEEDED(rc)) {value=next;g_MenuApplication->ShowNotification(std::string(wifi?"Wi-Fi ":"Bluetooth ")+(next?"enabled":"disabled"));}
        else g_MenuApplication->ShowNotification("Unable to change system setting");
    }
    void GlassShell::ReadMetrics() {
        const auto now=std::chrono::steady_clock::now();
        if(now<next_metrics)return;
        next_metrics=now+std::chrono::seconds(2);
        brightness=volume=-1;temperature=-1;free_bytes=total_bytes=-1;
        if(R_SUCCEEDED(lblInitialize())) {
            float v=0;if(R_SUCCEEDED(lblGetCurrentBrightnessSetting(&v)) && std::isfinite(v))brightness=std::clamp(v,0.f,1.f);
            lblExit();
        }
        if(R_SUCCEEDED(audctlInitialize())) {
            float v=0;if(R_SUCCEEDED(audctlGetSystemOutputMasterVolume(&v)) && std::isfinite(v))volume=std::clamp(v,0.f,1.f);
            audctlExit();
        }
        if(R_SUCCEEDED(tsInitialize())) {
            s32 v=0;if(R_SUCCEEDED(tsGetTemperature(TsLocation_External,&v)) && v>=0 && v<=120)temperature=v;
            tsExit();
        }
        auto fs=fsdevGetDeviceFileSystem("sdmc");
        if(fs) {
            s64 value=0;if(R_SUCCEEDED(fsFsGetFreeSpace(fs,"/",&value)) && value>=0)free_bytes=value;
            if(R_SUCCEEDED(fsFsGetTotalSpace(fs,"/",&value)) && value>0)total_bytes=value;
        }
    }
    void GlassShell::AdjustSlider(bool display,float target) {
        // Keep the screen readable; never request a completely black display.
        target=std::clamp(target,display?.10f:0.f,1.f);
        bool ok=false;
        if(display) {
            if(brightness<0)return;
            if(R_SUCCEEDED(lblInitialize())) {
                if(R_SUCCEEDED(lblSetCurrentBrightnessSetting(target)))ok=R_SUCCEEDED(lblApplyCurrentBrightnessSettingToBacklight());
                lblExit();
            }
            if(ok)brightness=target;
        } else {
            if(volume<0)return;
            if(R_SUCCEEDED(audctlInitialize())) {
                ok=R_SUCCEEDED(audctlSetSystemOutputMasterVolume(target));audctlExit();
            }
            if(ok)volume=target;
        }
        if(!ok)g_MenuApplication->ShowNotification("Control is unavailable");
        next_metrics=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    }
    void GlassShell::Slider(Renderer &r,int x,int y,int w,int h,const std::string &name,float value,bool selected) {
        Card(r,x,y,w,h,selected);
        Label(r,name,x+26,y+22,37);
        Label(r,value<0?"N/A":std::to_string(int(value*100+.5f))+"%",x+w/2,y+70,30,0,true);
        const int left=x+46,top=y+115,width=w-92,height=h-166;
        r->RenderRoundedRectangleFill({32,38,74,110},left,top,width,height,std::min(34,width/2));
        if(value>=0) {
            const int amount=int(height*value);
            if(amount>0)r->RenderRoundedRectangleFill({235,240,255,240},left,top+height-amount,width,amount,std::min(34,amount/2));
        }
        Glyph(r,name=="Brightness"?"sun":"volume",x+w/2-22,y+h-44,44);
    }
    std::vector<GlassShell::Choice> GlassShell::Choices() {
        std::vector<Choice> c;
        if(page==Page::Settings) {
            c={{466,480,1360,90,"Glass transparency",std::to_string(glass)+"%",[this](){glass=glass>=85?35:glass+10;materials.clear();SavePreferences();}},
               {466,580,1360,90,"Blur strength",std::to_string(blur)+"%",[this](){blur=blur>=100?0:std::min(100,blur+20);materials.clear();SavePreferences();}},
               {466,680,1360,90,"UI Motion",motion?"On":"Off",[this](){motion=!motion;SavePreferences();}},
               {466,780,650,90,"Lock screen","Open now",[](){g_MenuApplication->LoadMenu(MenuType::Lockscreen);}},
               {1136,780,690,90,"System settings","Native controls",[](){ShowSettingsMenu();}},
               {466,880,650,90,"Installed themes","Theme manager",[](){ShowThemesMenu();}},
               {1136,880,690,90,"Recovery","Legacy menu",legacy}};
        } else if(page==Page::Control) {
            c={{780,255,240,475,"Brightness","",[this](){AdjustSlider(true,brightness>=.95f?.10f:brightness+.10f);}},
               {1040,255,240,475,"Volume","",[this](){AdjustSlider(false,volume>=.95f?0.f:volume+.10f);}},
               {100,270,195,190,"Wi-Fi",g_GlobalSettings.wireless_lan_enabled?"On":"Off",[this](){ToggleRadio(true);}},
               {320,270,195,190,"Bluetooth",g_GlobalSettings.bluetooth_enabled?"On":"Off",[this](){ToggleRadio(false);}},
               {540,270,195,190,"Controllers","",[](){ShowController();}},
               {100,485,195,190,"Power","",[](){ShowPowerDialog();}},
               {320,485,195,190,"Quiet",g_MenuApplication->IsQuietMode()?"On":"Off",[](){g_MenuApplication->SetQuietMode(!g_MenuApplication->IsQuietMode());}},
               {540,485,195,190,"Lock","",[](){g_MenuApplication->LoadMenu(MenuType::Lockscreen);}},
               {100,770,635,175,"Notifications","Recent alerts",[this](){SetPage(Page::Notifications);}},
               {1310,630,510,100,"System settings","",[](){ShowSettingsMenu();}}};
        } else if(page==Page::Notifications) {
            c={{430,905,510,90,"View / clear history","",[](){g_MenuApplication->ShowNotificationHistory();}},
               {980,905,510,90,"Back","",[this](){SetPage(previous);}}};
        }
        return c;
    }
    void GlassShell::Move(int amount) {
        int count=0;
        if(page==Page::Home || page==Page::Library || page==Page::Apps)count=static_cast<int>(List().size());
        else count=static_cast<int>(Choices().size());
        if(count==0){focus=0;offset=0;return;}
        focus=std::clamp(focus+amount,0,count-1);
        const int cap=page==Page::Home?5:12;
        if(focus<offset)offset=(focus/cap)*cap;
        if(focus>=offset+cap)offset=(focus/cap)*cap;
    }
    void GlassShell::Activate() {
        if(page==Page::Home || page==Page::Library || page==Page::Apps) {
            auto list=List();
            if(focus>=0 && focus<static_cast<int>(list.size()))launch(list[focus]);
        } else {
            auto choices=Choices();
            if(focus>=0 && focus<static_cast<int>(choices.size()))choices[focus].action();
        }
    }
    void GlassShell::HandleInput(u64 down,pu::ui::TouchPoint touch) {
        if(touch.IsEmpty())touched=false;
        else if(!touched) {
            touched=true;
            if(page==Page::Control) {
                if(touch.HitsRegion(780,255,240,475)){focus=0;AdjustSlider(true,float(679-touch.y)/309.f);return;}
                if(touch.HitsRegion(1040,255,240,475)){focus=1;AdjustSlider(false,float(679-touch.y)/309.f);return;}
            }
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)) {auto action=hit.action;action();return;}
        }
        if(down&HidNpadButton_Minus){legacy();return;}
        if(down&(HidNpadButton_ZL|HidNpadButton_ZR)){SetPage(page==Page::Control?Page::Home:Page::Control);return;}
        if(down&HidNpadButton_Plus){SetPage(page==Page::Notifications?Page::Home:Page::Notifications);return;}
        if(down&HidNpadButton_B){SetPage(Page::Home);return;}
        if(down&HidNpadButton_L){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+3)%4));return;}
        if(down&HidNpadButton_R){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+1)%4));return;}
        if(down&HidNpadButton_Y){
            if(page==Page::Library || page==Page::Apps){filter=(filter+1)%4;focus=0;offset=0;}return;
        }
        if(down&HidNpadButton_X) {
            if(page==Page::Library||page==Page::Apps){Search();return;}
        }
        if((page==Page::Home||page==Page::Library) && (down&HidNpadButton_StickL)){ToggleFavorite();return;}
        if(page==Page::Settings && (down&HidNpadButton_X)){SetTheme((theme+1)%4);return;}
        if(page==Page::Control && focus<2 && (down&(HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_StickLLeft|HidNpadButton_StickLRight))) {
            const float delta=(down&(HidNpadButton_Right|HidNpadButton_StickLRight))?.05f:-.05f;
            AdjustSlider(focus==0,(focus==0?brightness:volume)+delta);
            return;
        }
        if(down&(HidNpadButton_Left|HidNpadButton_StickLLeft))Move(-1);
        else if(down&(HidNpadButton_Right|HidNpadButton_StickLRight))Move(1);
        else if(down&(HidNpadButton_Up|HidNpadButton_StickLUp))Move(page==Page::Library||page==Page::Apps?-6:-1);
        else if(down&(HidNpadButton_Down|HidNpadButton_StickLDown))Move(page==Page::Library||page==Page::Apps?6:1);
        else if(down&HidNpadButton_A)Activate();
    }
    void GlassShell::OnRender(Renderer &r,const s32,const s32) {
        hits.clear();icon_budget=true;
        if(page==Page::Home || page==Page::Control)ReadMetrics();
        Picture(r,GetBackgroundTexture(),0,0,1920,1080);
        if(Tints[theme].a)r->RenderRectangleFill(Tints[theme],0,0,1920,1080);
        Picture(r,GetSelectedUserIconTexture(),44,28,68,68);
        Label(r,"GlassNX",128,34,45);
        Card(r,555,25,810,78);
        const char *tabs[]={"Home","Library","Apps","Settings"};
        const char *navicons[]={"home","games","apps","settings"};
        for(int i=0;i<4;++i)IconButton(r,565+i*198,32,190,64,navicons[i],tabs[i],static_cast<int>(page)==i,[this,i](){SetPage(static_cast<Page>(i));});
        Label(r,Clock(),1480,43,30);
        if(g_GlobalSettings.wireless_lan_enabled)Glyph(r,"wifi",1600,40,34);
        else Label(r,"Offline",1580,43,27);
        Label(r,Battery(),1690,43,30);
        Glyph(r,"battery",1805,40,38);

        if(page==Page::Home) {
            auto list=List();
            focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/5)*5;
            Card(r,70,140,1160,435);
            Card(r,1260,140,590,205);Card(r,1260,365,590,210);
            Glyph(r,"battery",1298,167,32);Label(r,"System",1345,166,30);
            Label(r,Battery(),1300,214,64);Label(r,temperature<0?"Temperature N/A":std::to_string(temperature)+" C",1530,241,30,275);
            Glyph(r,"storage",1298,392,32);Label(r,"Storage",1345,391,30);
            Label(r,free_bytes<0?"Unavailable":std::to_string(free_bytes/(1024LL*1024LL*1024LL))+" GiB free",1300,446,45,500);
            if(total_bytes>0 && free_bytes>=0) {
                r->RenderRoundedRectangleFill({32,38,74,100},1300,520,500,17,8);
                const auto used=std::clamp(1.0-double(free_bytes)/double(total_bytes),0.0,1.0);
                if(used>0)r->RenderRoundedRectangleFill({235,240,255,225},1300,520,int(500*used),17,8);
            }
            if(!list.empty()) {
                const auto idx=list[focus];auto &e=entries->GetEntries()[idx];
                const bool suspended=g_GlobalSettings.IsEntrySuspended(e);
                auto wide=e.Is<EntryType::Application>()?Banner(e.app_info.app_id):pu::sdl2::TextureHandle::Ref{};
                if(wide) {
                    Cover(r,wide,85,155,1130,405);
                    for(int i=0;i<24;i++)r->RenderRectangleFill({24,25,59,static_cast<u8>(185-i*7)},85+i*32,155,32,405);
                }
                Label(r,suspended?"CONTINUE PLAYING":"SELECTED GAME / APP",112,185,30);
                Label(r,EntryName(e),112,260,64,680);
                Label(r,suspended?"Currently suspended":"Ready to open",112,362,30,680);
                Button(r,112,445,250,78,suspended?"Resume":"Play",false,[this,idx](){launch(idx);});
                auto icon=entries->GlassIcon(idx,icon_budget);if(!icon&&e.Is<EntryType::Application>())icon=GetDefaultApplicationIconTexture();
                if(!wide)Picture(r,icon,845,175,360,360);
            } else {
                Label(r,"Your home awaits",112,245,64);
                Label(r,"Add or organize entries in the legacy menu.",112,355,30);
                Button(r,112,445,360,78,"Manage entries",false,legacy);
            }
            Label(r,"Your games & apps",75,600,37);
            for(int i=0;i<5&&offset+i<static_cast<int>(list.size());++i)Tile(r,list[offset+i],70+i*360,665,340,250,focus==offset+i);
            Card(r,392,954,1136,87);
            IconButton(r,404,960,215,74,"bell","Alerts",false,[this](){SetPage(Page::Notifications);});
            IconButton(r,628,960,210,74,"sun","Control",false,[this](){SetPage(Page::Control);});
            IconButton(r,847,960,210,74,"album","Album",false,[](){ShowAlbum();});
            IconButton(r,1066,960,210,74,"settings","Settings",false,[this](){SetPage(Page::Settings);});
            IconButton(r,1285,960,228,74,"power","Power",false,[](){ShowPowerDialog();});
        } else if(page==Page::Library || page==Page::Apps) {
            const bool games=page==Page::Library;
            auto list=List();focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/12)*12;
            Label(r,games?"Game Library":"App Library",70,125,64);
            Label(r,games?"Your collection":"Homebrew, media and utilities",72,208,30);
            IconButton(r,1410,145,435,76,"search",search.empty()?"Search":search,false,[this](){Search();});
            const char *gameTabs[]={"All Games","Favorites","Collections","Recently Played"};
            const char *appTabs[]={"Apps","Homebrew","Tools","Media"};
            for(int i=0;i<4;i++)Button(r,70+i*347,275,330,75,games?gameTabs[i]:appTabs[i],filter==i,[this,i](){filter=i;focus=offset=0;});
            Button(r,1475,275,370,75,sort_name?"A-Z":"Default order",false,[this](){sort_name=!sort_name;focus=offset=0;SavePreferences();});
            Card(r,70,375,1775,580);
            if(list.empty())Label(r,"No matching entries",960,585,45,0,true);
            for(int i=0;i<12&&offset+i<static_cast<int>(list.size());++i)Tile(r,list[offset+i],88+(i%6)*293,392+(i/6)*275,275,258,focus==offset+i);
            Label(r,"L/R Pages    Y Filter    X Search    L Stick Favorite    A Open    - Legacy",70,1005,27,1580);
            if(list.size()>12)Label(r,std::to_string(offset/12+1)+"/"+std::to_string((list.size()+11)/12),1720,1005,30);
        } else if(page==Page::Settings || page==Page::Control || page==Page::Notifications) {
            const bool settings=page==Page::Settings,control=page==Page::Control;
            Label(r,settings?"Settings":control?"Control Center":"Notifications",70,137,64);
            if(settings) {
                Card(r,70,210,360,760);
                IconButton(r,90,240,320,70,"settings","Appearance",true,[](){});
                IconButton(r,90,321,320,70,"settings","System",false,[](){ShowSettingsMenu();});
                IconButton(r,90,402,320,70,"games","Controls",false,[](){ShowController();});
                IconButton(r,90,483,320,70,"wifi","Network",false,[](){ShowSettingsMenu();});
                IconButton(r,90,564,320,70,"power","Power",false,[](){ShowPowerDialog();});
                IconButton(r,90,645,320,70,"storage","Storage",false,[](){ShowSettingsMenu();});
                IconButton(r,90,726,320,70,"apps","Homebrew",false,[this](){SetPage(Page::Apps);});
                IconButton(r,90,807,320,70,"bell","About",false,[](){ShowAboutDialog();});
                Label(r,"X  Change theme",95,895,27);
                Label(r,"Appearance",466,205,45);
                Label(r,"Theme",466,265,30);
                for(int i=0;i<4;i++) {
                    const int x=466+i*344;
                    Card(r,x,315,320,125,theme==i);
                    Picture(r,GetBackgroundTexture(),x+6,321,308,113);
                    if(Tints[i].a)r->RenderRectangleFill(Tints[i],x+6,321,308,113);
                    Label(r,Themes[i],x+160,443,27,305,true);
                    hits.push_back({x,315,320,125,[this,i](){SetTheme(i);}});
                }
            } else if(control) {
                Card(r,70,225,695,505);Card(r,1310,255,510,355);
                r->RenderRoundedRectangleFill({245,245,255,145},890,117,140,9,4);
                Label(r,"Performance",1340,280,37);
                Label(r,"Stock clocks",1340,351,37);
                Label(r,"Profiles unavailable",1340,422,30);
                Label(r,"No clocks changed",1340,490,27);
                Card(r,780,770,500,175);Label(r,"Battery",810,790,30);Label(r,Battery(),810,842,64);
                Card(r,1310,770,510,175);Label(r,"Temperature",1340,790,30);Label(r,temperature<0?"Unavailable":std::to_string(temperature)+" C",1340,842,45);
                Label(r,"Fan: unavailable",1340,909,27);
            } else {
                const auto &history=g_MenuApplication->GetNotificationHistory();
                if(history.empty()) {Card(r,430,270,1060,170);Label(r,"You're all caught up.",960,325,45,0,true);}
                for(size_t i=0;i<std::min<size_t>(history.size(),4);++i) {
                    int y=245+static_cast<int>(i)*155;Card(r,430,y,1060,130);
                    Label(r,"GlassNX",465,y+12,27);Label(r,history[history.size()-1-i],465,y+53,37,970);
                }
            }
            auto choices=Choices();focus=std::clamp(focus,0,std::max(0,static_cast<int>(choices.size())-1));
            for(size_t i=0;i<choices.size();++i) {
                auto &c=choices[i];
                if(control && i<2) {Slider(r,c.x,c.y,c.w,c.h,c.label,i==0?brightness:volume,focus==static_cast<int>(i));continue;}
                if(control && i>=2 && i<=7) {
                    const char *icons[]={"wifi","bluetooth","games","power","moon","lock"};
                    const bool on=(i==2&&g_GlobalSettings.wireless_lan_enabled)||(i==3&&g_GlobalSettings.bluetooth_enabled)||(i==6&&g_MenuApplication->IsQuietMode());
                    Card(r,c.x,c.y,c.w,c.h,focus==static_cast<int>(i)||on);
                    if(on)r->RenderCircleFill({103,155,242,210},c.x+c.w/2,c.y+65,45);
                    Glyph(r,icons[i-2],c.x+c.w/2-26,c.y+39,52);
                    Label(r,c.label,c.x+c.w/2,c.y+117,27,c.w-14,true);
                    Label(r,c.detail,c.x+c.w/2,c.y+148,27,c.w-14,true);
                    hits.push_back({c.x,c.y,c.w,c.h,c.action});continue;
                }
                Card(r,c.x,c.y,c.w,c.h,focus==static_cast<int>(i));
                Label(r,c.label,c.x+25,c.y+15,37,c.w-50);
                Label(r,c.detail,c.x+25,c.y+(settings?55:65),27,c.w-50);
                if(settings && i<2) {
                    const int x=c.x+750,y=c.y+42,w=530;
                    const float value=(i==0?glass:blur)/100.f;
                    r->RenderRoundedRectangleFill({30,38,75,110},x,y,w,12,6);
                    if(value>0)r->RenderRoundedRectangleFill({236,238,255,240},x,y,int(w*value),12,6);
                    r->RenderCircleFill({255,255,255,255},x+int(w*value),y+6,17);
                }
                if(settings && i==2) {
                    r->RenderRoundedRectangleFill(motion?pu::ui::Color{99,161,238,255}:pu::ui::Color{70,77,120,255},c.x+c.w-180,c.y+22,125,48,24);
                    r->RenderCircleFill({255,255,255,255},c.x+c.w-(motion?81:156),c.y+46,21);
                }
                hits.push_back({c.x,c.y,c.w,c.h,c.action});
            }
            Label(r,"A  Select     B  Home     ZL/ZR  Control     +  Notifications",70,1005,30);
        }
        if(motion) {
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-transition).count();
            if(elapsed>=0 && elapsed<220) {
                const float t=1.f-static_cast<float>(elapsed)/220.f;
                r->RenderRectangleFill({32,31,68,static_cast<u8>(75*t*t)},0,110,1920,970);
            }
        }
    }
}
