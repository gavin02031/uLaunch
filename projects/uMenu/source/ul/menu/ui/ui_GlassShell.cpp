#include <ul/menu/ui/ui_GlassShell.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/menu/ui/ui_Common.hpp>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <sys/statvfs.h>
#include <SDL2/SDL_image.h>
#include <switch/runtime/devices/fs_dev.h>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {
    namespace {
        constexpr const char *Themes[] = {"Pastel Dream","Midnight Glass","Frosted White","Aurora"};
        constexpr pu::ui::Color Tints[] = {{36,32,74,0},{12,18,46,158},{196,206,242,40},{21,83,95,100}};
        constexpr pu::ui::Color GlassColors[] = {{93,87,155,255},{40,59,106,255},{115,132,184,255},{43,110,125,255}};
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
    GlassShell::GlassShell(EntryMenu::Ref e,std::function<void(u32)> l,std::function<void()> old,std::function<void()> back,std::function<void()> close)
        :entries(e),launch(l),legacy(old),folder_back(back),close_suspended(close) {LoadPreferences();LoadBackdrop();ReadRecent();LoadCategories();}

    float GlassShell::StepSpring(float value,float &velocity,float target,float dt) {
        // Critically damped analytic spring; independent of frame rate.
        constexpr float omega=19.f;
        const float change=value-target;
        const float term=velocity+omega*change;
        const float decay=expf(-omega*dt);
        velocity=(velocity-omega*term*dt)*decay;
        return target+(change+term*dt)*decay;
    }
    void GlassShell::UpdateMotion() {
        const auto now=std::chrono::steady_clock::now();
        const float dt=animation_tick.time_since_epoch().count()==0?.016f:std::clamp(std::chrono::duration<float>(now-animation_tick).count(),0.f,.05f);
        animation_tick=now;
        const int p=static_cast<int>(page);
        const float target=p<4?float(p):float(std::clamp(static_cast<int>(previous),0,3));
        if(animated_focus!=focus){animated_focus=focus;focus_lift=0;focus_velocity=0;}
        if(!motion){nav_position=target;nav_velocity=0;focus_lift=0;focus_velocity=0;return;}
        nav_position=StepSpring(nav_position,nav_velocity,target,dt);
        focus_lift=StepSpring(focus_lift,focus_velocity,1.f,dt);
    }
    void GlassShell::ScrollAlerts(int amount) {
        const int n=static_cast<int>(g_MenuApplication->GetNotificationHistory().size());
        alert_offset=std::clamp(alert_offset+amount,0,std::max(0,n-4));
    }
    void GlassShell::Back() {
        if(page==Page::Control||page==Page::Notifications){
            SetPage(previous);focus=previous_focus;offset=previous_offset;filter=previous_filter;setting_section=previous_section;search=previous_search;return;
        }
        if(page==Page::Settings&&setting_section!=0){Section(0);return;}
        if((page==Page::Library||page==Page::Home||page==Page::Apps)&&!entries->IsInRoot()){folder_back();Invalidate();return;}
        SetPage(Page::Home);
    }

    namespace {
        const char *PrefsPath() {
            return g_GlobalSettings.ams_is_emummc?"sdmc:/glassnx/glassnx-emu.prefs":"sdmc:/glassnx/glassnx-sys.prefs";
        }
        std::string Fold(std::string v) {
            for(auto &c:v)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return v;
        }
    }
    void GlassShell::LoadCategories() {
        const auto path=std::string(PrefsPath())+".categories";
        auto fp=fopen(path.c_str(),"rb");if(!fp)return;
        unsigned long long id=0;int category=0;
        for(unsigned n=0;n<1024 && fscanf(fp,"%llx %d",&id,&category)==2;n++)
            if(id && (category==1||category==2))title_categories[static_cast<u64>(id)]=category;
        fclose(fp);
    }
    bool GlassShell::SaveCategories() {
        const auto path=std::string(PrefsPath())+".categories",temp=path+".tmp";
        auto fp=fopen(temp.c_str(),"wb");if(!fp)return false;
        bool ok=true;
        for(const auto &p:title_categories)
            if(fprintf(fp,"%016llx %d\n",static_cast<unsigned long long>(p.first),p.second)<0)ok=false;
        if(fclose(fp)!=0)ok=false;
        if(ok && rename(temp.c_str(),path.c_str())==0)return true;
        remove(temp.c_str());return false;
    }
    void GlassShell::TitleOptions() {
        auto list=List();
        if(focus<0||focus>=static_cast<int>(list.size()))return;
        auto &e=entries->GetEntries()[list[focus]];
        if(!e.Is<EntryType::Application>()){
            g_MenuApplication->ShowNotification("Title categories apply to installed applications.");return;
        }
        const auto id=e.app_info.app_id;
        char hex[24]={};snprintf(hex,sizeof(hex),"%016llx",static_cast<unsigned long long>(id));
        const std::string detail=std::string("Title ID: ")+hex+"\nUse Game or App to organize your library.";
        const auto choice=g_MenuApplication->DisplayDialog(EntryName(e),detail,{"Game","App","Automatic","Cancel"},true);
        if(choice<0||choice>2)return;
        const auto old=title_categories;
        if(choice==2)title_categories.erase(id);
        else {
            if(title_categories.size()>=1024 && !title_categories.count(id))return;
            title_categories[id]=choice==0?1:2;
        }
        if(!SaveCategories()){
            title_categories=old;g_MenuApplication->ShowNotification("Could not save title category");return;
        }
        focus=offset=0;g_MenuApplication->ShowNotification("Library category saved");
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
        swkbdConfigSetInitialText(&cfg,search.c_str());
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
        snprintf(name,sizeof(name),"sdmc:/glassnx/art/%016llx.jpg",static_cast<unsigned long long>(id));
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

    void GlassShell::Section(int i) {setting_section=std::clamp(i,0,7);focus=0;hits.clear();transition=std::chrono::steady_clock::now();}

    std::string GlassShell::StorageText() const {
        if(free_bytes<0)return "Unavailable";
        if(free_bytes==0)return "0 B free";
        const double gib=double(free_bytes)/1073741824.0;
        char out[48]={};
        if(gib<1)snprintf(out,sizeof(out),"%.0f MiB free",double(free_bytes)/1048576.0);
        else snprintf(out,sizeof(out),"%.1f GiB free",gib);
        return out;
    }
    void GlassShell::SetPage(Page p) {
        if(p==page) return;
        if((p==Page::Control || p==Page::Notifications) && static_cast<int>(page)<4) {
            previous=page;previous_focus=focus;previous_offset=offset;previous_filter=filter;previous_section=setting_section;previous_search=search;
        }
        if(p==Page::Settings)setting_section=0;
        page=p; focus=0;offset=0;filter=0;alert_offset=0;animated_focus=-1;search.clear();hits.clear();transition=std::chrono::steady_clock::now();
    }
    std::vector<u32> GlassShell::List() {
        std::vector<u32> result;
        auto &all=entries->GetEntries();
        for(u32 i=0;i<all.size();++i) {
            auto &e=all[i];
            if(e.Is<EntryType::Invalid>())continue;
            if(!search.empty() && Fold(EntryName(e)).find(Fold(search))==std::string::npos)continue;
            // Installed forwarders can be reported as applications by Horizon.
            // Keep known utilities out of the game shelf; never hide unknown titles.
            const auto name=Fold(EntryName(e));
            bool utility=e.Is<EntryType::Application>() &&
                (name=="sphaira" || name=="switchdrop" || name=="opennow" || name=="horizon" ||
                 name=="youtube tv" || name=="youtube" || name=="glassnx manager" || name=="umanager" || name=="hbmenu" || name=="homebrew menu");
            if(e.Is<EntryType::Application>()) {
                const auto it=title_categories.find(e.app_info.app_id);
                if(it!=title_categories.end())utility=it->second==2;
            }
            const bool game=e.Is<EntryType::Application>()&&!utility;
            if(page==Page::Apps) {
                if((filter==0 && (e.Is<EntryType::Homebrew>()||e.IsSpecial()||utility)) ||
                    (filter==1 && e.Is<EntryType::Homebrew>()) ||
                    (filter==2 && (utility||(e.IsSpecial() && !e.Is<EntryType::SpecialEntryAlbum>() && !e.Is<EntryType::SpecialEntryWebBrowser>()))) ||
                    (filter==3 && (e.Is<EntryType::SpecialEntryAlbum>()||e.Is<EntryType::SpecialEntryWebBrowser>()||name=="youtube tv"||name=="youtube")))result.push_back(i);
            } else if(page==Page::Library) {
                if((filter==0 && (game||e.Is<EntryType::Folder>())) ||
                    (filter==1 && game && Favorite(e.app_info.app_id)) ||
                    (filter==2 && e.Is<EntryType::Folder>()) ||
                    (filter==3 && game && std::find(recent.begin(),recent.end(),e.app_info.app_id)!=recent.end()))result.push_back(i);
            } else if(game || g_GlobalSettings.IsEntrySuspended(e)) result.push_back(i);
        }
        if(page==Page::Library && filter==3)std::stable_sort(result.begin(),result.end(),[&](u32 a,u32 b){return std::find(recent.begin(),recent.end(),all[a].app_info.app_id)<std::find(recent.begin(),recent.end(),all[b].app_info.app_id);});
        else if(sort_name && page!=Page::Home)std::stable_sort(result.begin(),result.end(),[&](u32 a,u32 b){return Fold(EntryName(all[a]))<Fold(EntryName(all[b]));});
        if(page==Page::Home) {
            std::stable_sort(result.begin(),result.end(),[&](u32 a,u32 b){
                auto rank=[&](u32 i){return all[i].Is<EntryType::Application>()?std::find(recent.begin(),recent.end(),all[i].app_info.app_id):recent.end();};
                return rank(a)<rank(b);
            });
            const auto it=std::find_if(result.begin(),result.end(),[&](u32 i){return g_GlobalSettings.IsEntrySuspended(all[i]);});
            if(it!=result.end()) std::rotate(result.begin(),it,it+1);
        }
        return result;
    }
    void GlassShell::Picture(Renderer &r,const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h) {
        if(t && t->Get()) r->RenderTexture(t->Get(),x,y,pu::ui::render::TextureRenderOptions({},w,h,{},{},{}));
    }
    pu::sdl2::TextureHandle::Ref GlassShell::Material(int x,int y,int w,int h,bool selected) {
        // GPU bevels are position independent; reuse one for equal-size panels.
        const bool gpu=liquid_glass.IsReady();
        const auto key=std::to_string(gpu?0:x)+"/"+std::to_string(gpu?0:y)+"/"+std::to_string(w)+"/"+std::to_string(h)+"/"+std::to_string(theme)+"/"+std::to_string(glass)+"/"+std::to_string(blur)+"/"+std::to_string(selected)+"/"+std::to_string(gpu);
        auto it=materials.find(key);if(it!=materials.end())return it->second;
        // Cache at one-third logical resolution; the GPU composites the translucent
        // textured bevel. No per-frame rasterization or framebuffer readback.
        const int tw=(w+2)/3,th=(h+2)/3;
        const float radius=std::min(13.f,th/2.f-1.f);
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
            const float rim=std::max(0.f,1.f-std::abs(d+.65f)/1.25f);
            const float light=std::max(0.f,1.f-std::abs(d+2.5f)/3.f);
            const float angle=.28f+.72f*(1-v)*(1-v);
            const float reflection=std::max(0.f,1.f-std::abs(float(px)/tw+v*.55f-.4f)/.23f);
            const float shine=(1.f-v)*.045f+reflection*.035f+rim*(selected?.68f:.42f)*angle+light*.065f;
            const auto p=(static_cast<size_t>(py)*tw+px)*4;
            // CPU wallpaper sampling is only the compatibility fallback. On the
            // GPU path this texture contains tint and specular lighting alone.
            const float edge=std::max(0.f,1.f-std::abs(d)/6.f);
            const int sx=std::clamp(int((x+px*3+(px-tw*.5f)*edge*.11f)/8.f),0,239);
            const int sy=std::clamp(int((y+py*3+(py-th*.5f)*edge*.11f)/8.f),0,134);
            const int radius_sample=blur/15;
            for(int k=0;k<3;k++) {
                const float tc=k==0?tint.r:k==1?tint.g:tint.b;
                float bg=tc;
                if(!gpu && !backdrop.empty()) {
                    unsigned sum=0;
                    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
                        sum+=backdrop[(std::clamp(sy+dy*radius_sample,0,134)*240+std::clamp(sx+dx*radius_sample,0,239))*4+k];
                    bg=sum/9.f;
                    const auto shade=Tints[theme];
                    const float sc=k==0?shade.r:k==1?shade.g:shade.b;
                    bg=bg*(1-shade.a/255.f)+sc*(shade.a/255.f);
                }
                const float colored=bg*.62f+tc*.38f;
                rgba[p+k]=byte(colored+(255-colored)*shine);
            }
            const float base=gpu?30.f:70.f;
            rgba[p+3]=byte(cover*((100-glass)*1.15f+base+14*(1-v)+rim*(selected?105:56)+light*10));
        }
        auto tex=SDL_CreateTexture(pu::ui::render::GetMainRenderer(),SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,tw,th);
        if(!tex)return {};
        if(SDL_UpdateTexture(tex,nullptr,rgba.data(),tw*4)!=0 || SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND)!=0) {SDL_DestroyTexture(tex);return {};}
        if(materials.size()>=96)materials.clear();
        auto handle=pu::sdl2::TextureHandle::New(tex);materials.emplace(key,handle);return handle;
    }
    void GlassShell::Card(Renderer &r,int x,int y,int w,int h,bool selected) {
        const int radius=std::min(40,h/2);
        r->RenderRoundedRectangleFill({29,25,66,18},x-4,y+12,w+8,h,radius+4);
        r->RenderRoundedRectangleFill({18,18,47,32},x,y+5,w,h,radius);
        liquid_glass.Draw(pu::ui::render::GetMainRenderer(),x,y,w,h);
        const auto tex=Material(x,y,w,h,selected);
        if(tex)Picture(r,tex,x,y,w,h);
        else {
            const auto c=GlassColors[theme];
            r->RenderRoundedRectangleFill({c.r,c.g,c.b,100},x,y,w,h,radius);
            r->RenderRoundedRectangle({233,239,255,135},x,y,w,h,radius);
        }
        if(selected) {
            r->RenderRoundedRectangle({141,219,255,42},x-7,y-7,w+14,h+14,radius+7);
            r->RenderRoundedRectangle({178,203,255,155},x-3,y-3,w+6,h+6,radius+3);
            r->RenderRoundedRectangle({237,243,255,225},x,y,w,h,radius);
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
            t->SetColor(size<30?pu::ui::Color{231,238,255,235}:pu::ui::Color{255,255,255,255});
            // Fit once when caching rather than letting long titles marquee
            // into an arbitrary cropped middle. Preserve UTF-8 boundaries.
            if(maxw>0 && t->GetWidth()>maxw) {
                if(size>=45) {
                    for(int candidate:{45,37,30}) {
                        if(candidate>=size)continue;
                        t->SetFont(pu::ui::MakeDefaultFontName(candidate));
                        if(t->GetWidth()<=maxw)break;
                    }
                }
                if(t->GetWidth()>maxw) {
                    std::vector<size_t> boundaries{0};
                    for(size_t i=1;i<s.size();i++)
                        if((static_cast<unsigned char>(s[i])&0xc0)!=0x80)boundaries.push_back(i);
                    boundaries.push_back(s.size());
                    size_t lo=0,hi=boundaries.size()-1;
                    while(lo<hi) {
                        const size_t mid=(lo+hi+1)/2;
                        t->SetText(s.substr(0,boundaries[mid])+"…");
                        if(t->GetWidth()<=maxw)lo=mid;else hi=mid-1;
                    }
                    t->SetText(s.substr(0,boundaries[lo])+"…");
                    if(t->GetWidth()>maxw)t->SetClampWidth(maxw);
                }
            }
            it=texts.emplace(key,t).first;
        }
        const auto &t=it->second;
        t->OnRender(r,center?x-t->GetWidth()/2:x,y);
    }
    void GlassShell::Button(Renderer &r,int x,int y,int w,int h,const std::string &s,bool selected,std::function<void()> cb) {
        if(selected)r->RenderRoundedRectangleFill({151,171,234,130},x,y,w,h,h/2);
        else r->RenderRoundedRectangleFill({29,32,79,42},x,y,w,h,h/2);
        Label(r,s,x+w/2,y+(h-37)/2,30,w-24,true);
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
        else if(kind=="clock"){r->RenderCircle(white,x+size/2,y+size/2,size*2/5);line(12,5,12,12);line(12,12,17,15);}
        else if(kind=="star"){for(int i=0;i<10;i++){float a=(-90+i*36)*3.14159265f/180,b=(-90+(i+1)*36)*3.14159265f/180;float ra=i%2?4.5f:10.f,rb=i%2?10.f:4.5f;line(12+int(ra*cosf(a)),12+int(ra*sinf(a)),12+int(rb*cosf(b)),12+int(rb*sinf(b)));}}
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
    void GlassShell::Segment(Renderer &r,int x,int y,int w,int h,const std::string &kind,const std::string &label,bool selected,std::function<void()> cb) {
        if(selected) {
            r->RenderRoundedRectangleFill({171,185,241,120},x,y,w,h,h/2);
            r->RenderRoundedRectangle({218,229,255,135},x,y,w,h,h/2);
        }
        Glyph(r,kind,x+13,y+(h-32)/2,32);
        Label(r,label,x+54,y+(h-37)/2,30,w-59);
        hits.push_back({x,y,w,h,cb});
    }
    void GlassShell::RoundedArt(Renderer &r,const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h,int radius) {
        if(!t || !t->Get())return;
        int tw=0,th=0;
        if(SDL_QueryTexture(t->Get(),nullptr,nullptr,&tw,&th)!=0 || tw<=0 || th<=0)return;
        float uw=1.f,vh=1.f;
        if(float(tw)/th>float(w)/h)uw=float(w)*th/(float(h)*tw);
        else vh=float(h)*tw/(float(w)*th);
        const float u0=(1-uw)*.5f,v0=(1-vh)*.5f;
        const float rad=float(std::min(radius,std::min(w,h)/2));
        SDL_Vertex vertices[33]={};
        int inds[96]={};
        auto vertex=[&](float px,float py){return SDL_Vertex{{x+px,y+py},{255,255,255,255},{u0+uw*px/w,v0+vh*py/h}};};
        vertices[0]=vertex(w*.5f,h*.5f);
        const float cx[]={w-rad,w-rad,rad,rad},cy[]={rad,h-rad,h-rad,rad};
        for(int c=0;c<4;c++)for(int i=0;i<8;i++) {
            const float a=(-90.f+c*90.f+i*90.f/7.f)*3.1415926535f/180.f;
            vertices[1+c*8+i]=vertex(cx[c]+rad*cosf(a),cy[c]+rad*sinf(a));
        }
        for(int i=0;i<32;i++){inds[i*3]=0;inds[i*3+1]=1+i;inds[i*3+2]=1+(i+1)%32;}
        auto rr=pu::ui::render::GetMainRenderer();
        if(SDL_RenderGeometry(rr,t->Get(),vertices,33,inds,96)==0)return;
        // Some Switch SDL backends do not implement textured geometry.
        // Clip using horizontal texture bands instead of falling back to square art.
        const int corner=std::max(1,int(rad));
        auto band=[&](int y0,int y1,int inset) {
            if(y1<=y0||w<=2*inset)return;
            const int sx0=std::clamp(int((u0+uw*float(inset)/w)*tw),0,tw-1);
            const int sx1=std::clamp(int(std::ceil((u0+uw*float(w-inset)/w)*tw)),sx0+1,tw);
            const int sy0=std::clamp(int((v0+vh*float(y0)/h)*th),0,th-1);
            const int sy1=std::clamp(int(std::ceil((v0+vh*float(y1)/h)*th)),sy0+1,th);
            SDL_Rect src{sx0,sy0,sx1-sx0,sy1-sy0};
            SDL_Rect dst{x+inset,y+y0,w-2*inset,y1-y0};
            SDL_RenderCopy(rr,t->Get(),&src,&dst);
        };
        const int step=std::max(1,corner/12);
        for(int a=0;a<corner;a+=step) {
            const int b=std::min(a+step,corner);
            const float dy=corner-(a+b)*.5f;
            const int inset=std::max(0,int(std::ceil(corner-std::sqrt(std::max(0.f,float(corner*corner)-dy*dy)))));
            band(a,b,inset);band(h-b,h-a,inset);
        }
        band(corner,h-corner,0);
        (void)r;
    }
    void GlassShell::Tile(Renderer &r,u32 index,int x,int y,int w,int h,bool selected) {
        if(selected)r->RenderRoundedRectangleFill({150,168,229,static_cast<u8>(motion?45+40*focus_lift:85)},x,y,w,h,32);
        auto &e=entries->GetEntries().at(index);
        auto icon=entries->GlassIcon(index,icon_budget);
        if(!icon && e.Is<EntryType::Application>()) icon=GetDefaultApplicationIconTexture();
        const int base_side=std::min(w-20,h-54);
        const int lift=selected&&motion?int(5*focus_lift):0;
        const int side=base_side+lift*2;
        const int ix=x+(w-side)/2,iy=y+5-lift;
        const int rounding=std::max(20,side/6);
        r->RenderRoundedRectangleFill({15,20,45,65},ix,iy+5,side,side,rounding);
        if(icon)RoundedArt(r,icon,ix,iy,side,side,rounding);
        else {Card(r,ix,iy,side,side);Glyph(r,e.Is<EntryType::Folder>()?"storage":"apps",x+w/2-34,y+h/2-55,68);}
        if(selected) {
            r->RenderRoundedRectangle({125,212,255,48},ix-8,iy-8,side+16,side+16,rounding+8);
            r->RenderRoundedRectangle({164,225,255,115},ix-5,iy-5,side+10,side+10,rounding+5);
            r->RenderRoundedRectangle({240,249,255,255},ix-2,iy-2,side+4,side+4,rounding+2);
        }
        const auto name=EntryName(e);
        Label(r,name,x+w/2,y+h-35,27,w-10,true);
        if(e.Is<EntryType::Application>() && Favorite(e.app_info.app_id)) {
            r->RenderCircleFill({50,49,98,220},x+w-30,y+30,23);
            Label(r,"★",x+w-30,y+11,27,0,true);
        }
        hits.push_back({x,y,w,h,[this,index](){
            const auto list=List();const auto it=std::find(list.begin(),list.end(),index);
            if(it==list.end())return;
            const int next=static_cast<int>(it-list.begin());
            if(next!=focus){focus=next;return;}
            launch(index);
        }});
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
            if(hosversionAtLeast(10,0,0)) {
                TsSession session{};
                if(R_SUCCEEDED(tsOpenSession(&session,TsDeviceCode_LocationExternal))) {
                    float v=0;if(R_SUCCEEDED(tsSessionGetTemperature(&session,&v))&&std::isfinite(v)&&v>=0&&v<=120)temperature=int(v+.5f);
                    tsSessionClose(&session);
                }
            }
            if(temperature<0){s32 v=0;if(R_SUCCEEDED(tsGetTemperature(TsLocation_External,&v)) && v>=0 && v<=120)temperature=v;}
            tsExit();
        }
        struct statvfs sv{};
        if(statvfs("sdmc:/",&sv)==0 && sv.f_frsize>0 && sv.f_blocks>0) {
            free_bytes=static_cast<s64>(sv.f_bavail)*sv.f_frsize;
            total_bytes=static_cast<s64>(sv.f_blocks)*sv.f_frsize;
        }
        if(total_bytes<=0 || free_bytes<0) {
            FsFileSystem sd{};
            if(R_SUCCEEDED(fsOpenSdCardFileSystem(&sd))) {
                s64 v=0;if(R_SUCCEEDED(fsFsGetFreeSpace(&sd,"/",&v))&&v>=0)free_bytes=v;
                if(R_SUCCEEDED(fsFsGetTotalSpace(&sd,"/",&v))&&v>0)total_bytes=v;
                fsFsClose(&sd);
            }
        }
        if(total_bytes<=0 || free_bytes<0 || free_bytes>total_bytes)free_bytes=total_bytes=-1;
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
        Glyph(r,name=="Brightness"?"sun":"volume",x+28,y+46,48);
        Label(r,name,x+100,y+18,27);
        Label(r,value<0?"N/A":std::to_string(int(value*100+.5f))+"%",x+w-85,y+58,30);
        const int left=x+100,top=y+65,width=w-220;
        r->RenderRoundedRectangleFill({45,51,105,95},left,top,width,32,16);
        if(value>=0) {
            const int amount=int(width*value);
            if(amount>0)r->RenderRoundedRectangleFill({230,238,255,235},left,top,amount,32,std::min(16,amount/2));
            r->RenderCircleFill({35,43,91,35},left+amount,top+20,25);
            r->RenderCircleFill({255,255,255,255},left+amount,top+16,22);
        }
    }
    std::vector<GlassShell::Choice> GlassShell::Choices() {
        std::vector<Choice> c;
        if(page==Page::Settings) {
            if(setting_section!=0) {
                auto row=[&](const std::string &name,const std::string &detail,std::function<void()> action){
                    c.push_back({466,300+int(c.size())*116,1360,98,name,detail,action});
                };
                auto noop=[](){};
                switch(setting_section) {
                    case 1:
                        row("Console firmware",g_GlobalSettings.fw_version.display_version,noop);
                        row("Atmosphere",g_GlobalSettings.ams_version.Format(),noop);
                        row("Environment",g_GlobalSettings.ams_is_emummc?"emuMMC":"sysMMC",noop);
                        row("Advanced system options","Full settings",[](){ShowSettingsMenu();});break;
                    case 2:
                        row("Controllers","Connect, update or remap",[](){ShowController();});
                        row("Touchscreen","Tap items to navigate",noop);
                        row("Controller navigation","L/R pages, ZL/ZR Control Center",noop);break;
                    case 3:
                        row("Wi-Fi",g_GlobalSettings.wireless_lan_enabled?"On":"Off",[this](){ToggleRadio(true);});
                        row("Bluetooth",g_GlobalSettings.bluetooth_enabled?"On":"Off",[this](){ToggleRadio(false);});
                        row("Internet settings","Connection setup",[](){ShowNetConnect();});
                        row("Advanced network options","Full settings",[](){ShowSettingsMenu();});break;
                    case 4:
                        row("Power options","Sleep, restart, shutdown",[](){ShowPowerDialog();});
                        row("Control Center","Display and sound",[this](){SetPage(Page::Control);});
                        row("Lock screen","Lock now",[](){g_MenuApplication->LoadMenu(MenuType::Lockscreen);});break;
                    case 5:
                        row("SD card",StorageText(),noop);
                        row("Total capacity",total_bytes>0?std::to_string(total_bytes/1073741824LL)+" GiB":"Unavailable",noop);
                        row("Refresh storage","Read card statistics",[this](){next_metrics={};ReadMetrics();});break;
                    case 6:
                        row("App Library","Browse installed homebrew",[this](){SetPage(Page::Apps);});
                        row("Manage entries","Folders and shortcuts",legacy);
                        row("Installed themes","Theme manager",[](){ShowThemesMenu();});break;
                    case 7:
                        row("GlassNX","Version 0.9",[](){ShowAboutDialog();});
                        row("Backend","Based on XorTroll's uLaunch",noop);
                        row("Recovery","Legacy interface",legacy);break;
                }
                return c;
            }
            c={{466,480,1360,90,"Glass transparency",std::to_string(glass)+"%",[this](){glass=glass>=85?35:glass+10;materials.clear();SavePreferences();}},
               {466,580,1360,90,"Blur strength",std::to_string(blur)+"%",[this](){blur=blur>=100?0:std::min(100,blur+20);materials.clear();SavePreferences();}},
               {466,680,1360,90,"UI Motion",motion?"On":"Off",[this](){motion=!motion;SavePreferences();}},
               {466,780,650,90,"Lock screen","Open now",[](){g_MenuApplication->LoadMenu(MenuType::Lockscreen);}},
               {1136,780,690,90,"System settings","Native controls",[](){ShowSettingsMenu();}},
               {466,880,650,90,"Installed themes","Theme manager",[](){ShowThemesMenu();}},
               {1136,880,690,90,"Recovery","Legacy menu",legacy}};
        } else if(page==Page::Control) {
            c={{480,225,790,150,"Brightness","",[this](){AdjustSlider(true,brightness>=.95f?.10f:brightness+.10f);}},
               {480,405,790,150,"Volume","",[this](){AdjustSlider(false,volume>=.95f?0.f:volume+.10f);}},
               {70,605,280,190,"Wi-Fi",g_GlobalSettings.wireless_lan_enabled?"On":"Off",[this](){ToggleRadio(true);}},
               {370,605,280,190,"Bluetooth",g_GlobalSettings.bluetooth_enabled?"On":"Off",[this](){ToggleRadio(false);}},
               {670,605,280,190,"Controllers","",[](){ShowController();}},
               {970,605,280,190,"Power","",[](){ShowPowerDialog();}},
               {1270,605,280,190,"Quiet",g_MenuApplication->IsQuietMode()?"On":"Off",[](){g_MenuApplication->SetQuietMode(!g_MenuApplication->IsQuietMode());}},
               {1570,605,280,190,"Lock","",[](){g_MenuApplication->LoadMenu(MenuType::Lockscreen);}},
               {70,835,880,110,"Notifications","Recent alerts",[this](){SetPage(Page::Notifications);}},
               {970,835,880,110,"System settings","",[](){ShowSettingsMenu();}}};
        } else if(page==Page::Notifications) {
            c={{430,905,510,90,"Clear alerts","",[this](){
                   const auto result=g_MenuApplication->DisplayDialog("Clear notifications?","Remove all saved GlassNX alerts.",{"Clear","Cancel"},true);
                   if(result==0){g_MenuApplication->ClearNotificationHistory();alert_offset=0;}
               }},
               {980,905,510,90,"Back","",[this](){Back();}}};
        }
        return c;
    }
    void GlassShell::Move(int amount) {
        int count=0;
        if(page==Page::Home || page==Page::Library || page==Page::Apps)count=static_cast<int>(List().size());
        else count=static_cast<int>(Choices().size());
        if(count==0){focus=0;offset=0;return;}
        focus=std::clamp(focus+amount,0,count-1);
        const int cap=page==Page::Home?5:page==Page::Library?10:12;
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
        if(touch.IsEmpty()) {
            touched=false;drag_slider=-1;
        } else if(touched && drag_slider>=0 && page==Page::Control) {
            const auto now=std::chrono::steady_clock::now();
            if(now-slider_tick>=std::chrono::milliseconds(85)) {
                slider_tick=now;
                const float value=std::clamp(float(touch.x-580)/570.f,0.f,1.f);
                if(std::abs(value-(drag_slider==0?brightness:volume))>.025f)AdjustSlider(drag_slider==0,value);
            }
            return;
        } else if(!touched) {
            touched=true;
            if(page==Page::Settings && setting_section==0) {
                if(touch.HitsRegion(1216,480,530,90)){glass=std::clamp(int((touch.x-1216)*100/530),35,85);materials.clear();SavePreferences();return;}
                if(touch.HitsRegion(1216,580,530,90)){blur=std::clamp(int((touch.x-1216)*100/530),0,100);materials.clear();SavePreferences();return;}
            }
            if(page==Page::Control) {
                if(touch.HitsRegion(480,225,790,150)){focus=0;drag_slider=0;slider_tick=std::chrono::steady_clock::now();AdjustSlider(true,float(touch.x-580)/570.f);return;}
                if(touch.HitsRegion(480,405,790,150)){focus=1;drag_slider=1;slider_tick=std::chrono::steady_clock::now();AdjustSlider(false,float(touch.x-580)/570.f);return;}
            }
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)) {auto action=hit.action;action();return;}
        }
        if(down&HidNpadButton_Minus){legacy();return;}
        if(down&(HidNpadButton_ZL|HidNpadButton_ZR)){if(page==Page::Control)Back();else SetPage(Page::Control);return;}
        if(down&HidNpadButton_Plus){if(page==Page::Notifications)Back();else SetPage(Page::Notifications);return;}
        if(down&HidNpadButton_B){Back();return;}
        if(page==Page::Notifications) {
            if(down&(HidNpadButton_Up|HidNpadButton_StickLUp)){ScrollAlerts(-1);return;}
            if(down&(HidNpadButton_Down|HidNpadButton_StickLDown)){ScrollAlerts(1);return;}
        }
        if(down&HidNpadButton_L){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+3)%4));return;}
        if(down&HidNpadButton_R){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+1)%4));return;}
        if(down&HidNpadButton_Y){
            if(page==Page::Settings){Section((setting_section+1)%8);return;}
            if(page==Page::Library || page==Page::Apps){filter=(filter+1)%4;focus=0;offset=0;}return;
        }
        if((page==Page::Home||page==Page::Library||page==Page::Apps) && (down&HidNpadButton_StickR)){TitleOptions();return;}
        if(down&HidNpadButton_X) {
            if(page==Page::Home && g_GlobalSettings.IsSuspended()){close_suspended();return;}
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
        else if(down&(HidNpadButton_Up|HidNpadButton_StickLUp))Move(page==Page::Library?-5:page==Page::Apps?-6:-1);
        else if(down&(HidNpadButton_Down|HidNpadButton_StickLDown))Move(page==Page::Library?5:page==Page::Apps?6:1);
        else if(down&HidNpadButton_A)Activate();
    }
    void GlassShell::HeroShade(int x,int y,int w,int h) {
        auto rr=pu::ui::render::GetMainRenderer();
        const float l=float(x),m=l+w*.67f,right=l+w,top=float(y),bottom=top+h;
        const SDL_Color solid{14,18,40,230},mid{14,18,40,190},clear{14,18,40,8};
        const SDL_Vertex v[]={{{l,top},solid,{0,0}},{{m,top},mid,{0,0}},{{right,top},clear,{0,0}},
                              {{l,bottom},solid,{0,0}},{{m,bottom},mid,{0,0}},{{right,bottom},clear,{0,0}}};
        const int inds[]={0,1,3,1,4,3,1,2,4,2,5,4};
        SDL_BlendMode old=SDL_BLENDMODE_NONE;
        SDL_GetRenderDrawBlendMode(rr,&old);
        SDL_SetRenderDrawBlendMode(rr,SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(rr,nullptr,v,6,inds,12);
        SDL_SetRenderDrawBlendMode(rr,old);
    }
    void GlassShell::BatteryGlyph(Renderer &r,int x,int y,int size) {
        const auto pct=std::clamp(int(os::GetBatteryLevel()),0,100);
        const int width=size*4/5,height=size/2;
        r->RenderRoundedRectangle({240,245,255,200},x,y+size/4,width,height,5);
        r->RenderRectangleFill({240,245,255,200},x+width+2,y+size*3/8,3,size/4);
        const int fill=(width-8)*pct/100;
        if(fill>0)r->RenderRoundedRectangleFill(pct<=15?pu::ui::Color{255,122,138,255}:pu::ui::Color{235,243,255,245},x+4,y+size/4+4,fill,height-8,std::min(4,fill/2));
    }
    void GlassShell::OnRender(Renderer &r,const s32,const s32) {
        hits.clear();icon_budget=true;UpdateMotion();
        const auto wallpaper=GetBackgroundTexture();
        const auto tint=Tints[theme];
        liquid_glass.Prepare(pu::ui::render::GetMainRenderer(),wallpaper?wallpaper->Get():nullptr,blur,{tint.r,tint.g,tint.b,tint.a});
        if(page==Page::Home || page==Page::Control || (page==Page::Settings && setting_section==5))ReadMetrics();
        Picture(r,GetBackgroundTexture(),0,0,1920,1080);
        if(Tints[theme].a)r->RenderRectangleFill(Tints[theme],0,0,1920,1080);
        RoundedArt(r,GetSelectedUserIconTexture(),44,24,76,76,38);
        Label(r,"GlassNX",128,25,45);
        Label(r,"v0.9",130,76,27);
        hits.push_back({44,24,76,76,[](){ShowUserPage();}});
        Card(r,555,25,810,78);
        const char *tabs[]={"Home","Library","Apps","Settings"};
        const char *navicons[]={"home","games","apps","settings"};
        const int pill=565+int(nav_position*198);
        r->RenderRoundedRectangleFill({171,185,241,112},pill,32,190,64,32);
        r->RenderRoundedRectangle({218,229,255,115},pill,32,190,64,32);
        for(int i=0;i<4;++i)Segment(r,565+i*198,32,190,64,navicons[i],tabs[i],false,[this,i](){SetPage(static_cast<Page>(i));});
        Label(r,Clock(),1480,43,30);
        if(g_GlobalSettings.wireless_lan_enabled)Glyph(r,"wifi",1600,40,34);
        else Label(r,"Offline",1580,43,27);
        Label(r,Battery(),1690,43,30);
        BatteryGlyph(r,1805,40,38);
        hits.push_back({1470,25,380,80,[this](){SetPage(Page::Control);}});

        if(page==Page::Home) {
            auto list=List();
            focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/5)*5;
            Card(r,70,140,1160,435);
            Card(r,1260,140,590,205);Card(r,1260,365,590,210);
            Glyph(r,"battery",1298,167,32);Label(r,"System",1345,166,30);
            Label(r,Battery(),1300,214,64);Label(r,temperature<0?"Temperature N/A":std::to_string(temperature)+" C",1530,241,30,275);
            Glyph(r,"storage",1298,392,32);Label(r,"Storage",1345,391,30);
            Label(r,StorageText(),1300,446,45,500);
            if(total_bytes>0 && free_bytes>=0) {
                r->RenderRoundedRectangleFill({32,38,74,100},1300,520,500,17,8);
                const auto used=std::clamp(1.0-double(free_bytes)/double(total_bytes),0.0,1.0);
                if(used>0)r->RenderRoundedRectangleFill({235,240,255,225},1300,520,int(500*used),17,8);
            }
            if(!list.empty()) {
                const auto idx=list[focus];auto &e=entries->GetEntries()[idx];
                const bool suspended=g_GlobalSettings.IsEntrySuspended(e);
                auto wide=e.Is<EntryType::Application>()?Banner(e.app_info.app_id):pu::sdl2::TextureHandle::Ref{};
                auto icon=entries->GlassIcon(idx,icon_budget);if(!icon&&e.Is<EntryType::Application>())icon=GetDefaultApplicationIconTexture();
                if(wide) {
                    RoundedArt(r,wide,85,155,1130,405,32);
                    HeroShade(105,175,1090,365);
                } else if(icon) {
                    // Installed icon as artwork-derived backdrop, not invented wide banner.
                    auto tex=icon->Get();
                    if(tex) {
                        Uint8 rr=255,gg=255,bb=255;
                        SDL_GetTextureColorMod(tex,&rr,&gg,&bb);
                        SDL_SetTextureColorMod(tex,102,108,146);
                        Cover(r,icon,105,175,1090,365);
                        SDL_SetTextureColorMod(tex,rr,gg,bb);
                        HeroShade(105,175,1090,365);
                    }
                    RoundedArt(r,icon,805,165,390,390,58);
                }
                Label(r,suspended?"CONTINUE PLAYING":"FEATURED GAME",112,185,30);
                Label(r,EntryName(e),112,260,64,650);
                Label(r,suspended?"Currently suspended":"Ready to play",112,362,30,680);
                Button(r,112,445,250,78,suspended?"Resume":"Play",true,[this,idx](){launch(idx);});
                if(suspended) {
                    r->RenderRoundedRectangleFill({126,56,89,160},380,445,300,78,39);
                    r->RenderRoundedRectangle({255,198,211,130},380,445,300,78,39);
                    Label(r,"X  Close Game",530,465,30,270,true);
                    hits.push_back({380,445,300,78,close_suspended});
                }
                else if(e.Is<EntryType::Application>() && Favorite(e.app_info.app_id))Label(r,"Favorite",405,466,27);
            } else {
                Label(r,"Your home awaits",112,245,64);
                Label(r,"Add or organize entries in the legacy menu.",112,355,30);
                Button(r,112,445,360,78,"Manage entries",false,legacy);
            }
            Label(r,"Your games",75,600,37);
            Label(r,"R Stick  Options",370,606,27,330);
            Label(r,std::to_string(list.size())+" titles",1635,606,27,205);
            if(g_GlobalSettings.IsSuspended())Label(r,"X  Close suspended game",1110,606,27,500);
            for(int i=0;i<5&&offset+i<static_cast<int>(list.size());++i)Tile(r,list[offset+i],70+i*360,648,340,276,focus==offset+i);
            const int dots=std::min(9,int((list.size()+4)/5));
            for(int i=0;i<dots;++i)r->RenderCircleFill({227,235,255,static_cast<u8>(i==offset/5?230:80)},960-(dots-1)*9+i*18,938,4);
            Card(r,392,954,1136,87);
            Segment(r,404,960,215,74,"bell","Alerts",false,[this](){SetPage(Page::Notifications);});
            const auto alert_count=g_MenuApplication->GetNotificationHistory().size();
            if(alert_count>0){
                r->RenderCircleFill({125,146,218,255},602,964,18);
                Label(r,std::to_string(alert_count),602,945,27,0,true);
            }
            Segment(r,628,960,210,74,"sun","Control",false,[this](){SetPage(Page::Control);});
            Segment(r,847,960,210,74,"album","Album",false,[](){ShowAlbum();});
            Segment(r,1066,960,210,74,"settings","Settings",false,[this](){SetPage(Page::Settings);});
            Segment(r,1285,960,228,74,"power","Power",false,[](){ShowPowerDialog();});
        } else if(page==Page::Library || page==Page::Apps) {
            const bool games=page==Page::Library;
            const int capacity=games?10:12;
            auto list=List();focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/capacity)*capacity;
            Label(r,games?"Game Library":"App Library",70,125,64);
            Label(r,games?"Your collection":"Homebrew, media and utilities",72,208,30);
            IconButton(r,1410,145,435,76,"search",search.empty()?"Search":search,false,[this](){Search();});
            const char *gameTabs[]={"All Games","Favorites","Collections","Recently Played"};
            const char *appTabs[]={"Apps","Homebrew","Tools","Media"};
            for(int i=0;i<4;i++)Button(r,70+i*347,275,330,75,games?gameTabs[i]:appTabs[i],filter==i,[this,i](){filter=i;focus=offset=0;});
            Button(r,1475,275,370,75,sort_name?"A-Z":"Default order",false,[this](){sort_name=!sort_name;focus=offset=0;SavePreferences();});
            if(games) {
                Card(r,40,375,340,580);
                const char *icons[]={"games","star","storage","clock"};
                for(int i=0;i<4;i++)Segment(r,55,400+i*85,310,72,icons[i],gameTabs[i],filter==i,[this,i](){filter=i;focus=offset=0;});
                Label(r,"Sort collection",75,785,27);
                Button(r,60,835,300,75,sort_name?"A–Z":"Default order",false,[this](){sort_name=!sort_name;focus=offset=0;SavePreferences();});
            } else Card(r,70,375,1775,580);
            if(!entries->IsInRoot())Button(r,1050,145,325,76,"Back to folder",false,[this](){Back();});
            if(list.empty()){
                Label(r,"No matching entries",960,550,45,0,true);
                Label(r,"Try another category or clear search.",960,625,30,0,true);
                Button(r,775,700,370,75,"Clear filters",true,[this](){search.clear();filter=0;focus=offset=0;});
            }
            for(int i=0;i<capacity&&offset+i<static_cast<int>(list.size());++i) {
                const int columns=games?5:6;
                Tile(r,list[offset+i],(games?400:88)+(i%columns)*(games?290:293),389+(i/columns)*280,275,270,focus==offset+i);
            }
            Label(r,games?"L/R Pages   Y Filter   X Search   L Stick Favorite   R Stick Options   A Open":"L/R Pages   Y Filter   X Search   R Stick Options   A Open",70,1005,27,1580);
            if(static_cast<int>(list.size())>capacity)Label(r,std::to_string(offset/capacity+1)+"/"+std::to_string((list.size()+capacity-1)/capacity),1720,1005,30);
        } else if(page==Page::Settings || page==Page::Control || page==Page::Notifications) {
            const bool settings=page==Page::Settings,control=page==Page::Control;
            Label(r,settings?"Settings":control?"Control Center":"Notifications",70,137,64);
            if(settings) {
                Card(r,70,210,360,760);
                const char *sections[]={"Appearance","System","Controls","Network","Power","Storage","Homebrew","About"};
                const char *icons[]={"settings","settings","games","wifi","power","storage","apps","bell"};
                for(int i=0;i<8;i++)Segment(r,90,240+i*81,320,70,icons[i],sections[i],setting_section==i,[this,i](){Section(i);});
                Label(r,"Y  Categories",95,895,27);
                Label(r,sections[setting_section],466,205,45);
                if(setting_section==0) {
                    Label(r,"Theme",466,265,30);
                    for(int i=0;i<4;i++) {
                        const int x=466+i*344;
                        RoundedArt(r,GetBackgroundTexture(),x+6,321,308,113,22);
                        if(Tints[i].a)r->RenderRoundedRectangleFill(Tints[i],x+6,321,308,113,22);
                        if(theme==i)r->RenderRoundedRectangle({233,240,255,255},x+3,318,314,119,24);
                        Label(r,Themes[i],x+160,443,27,305,true);
                        hits.push_back({x,315,320,125,[this,i](){SetTheme(i);}});
                    }
                } else Card(r,450,280,1390,670);
            } else if(control) {
                Card(r,70,225,380,330);Card(r,1310,225,540,330);
                Glyph(r,"battery",100,260,42);Label(r,Battery(),165,245,64);
                Label(r,"Battery",100,335,27);
                r->RenderRectangleFill({235,242,255,50},100,386,320,2);
                Label(r,temperature<0?"Unavailable":std::to_string(temperature)+" C",100,414,45);
                Label(r,"System temperature",100,485,27);
                Label(r,"Performance",1340,255,37);
                Label(r,"Stock clocks",1340,330,45);
                Label(r,"Profiles unavailable",1340,410,30);
                Label(r,"No clocks changed",1340,480,27);
            } else {
                const auto &history=g_MenuApplication->GetNotificationHistory();
                if(history.empty()) {Card(r,430,270,1060,170);Label(r,"You're all caught up.",960,325,45,0,true);}
                const int total=static_cast<int>(history.size());
                alert_offset=std::clamp(alert_offset,0,std::max(0,total-4));
                for(int i=0;i<4&&alert_offset+i<total;++i) {
                    int y=245+i*155;Card(r,430,y,1060,130);
                    Glyph(r,"bell",460,y+30,48);
                    Label(r,"GlassNX",525,y+12,27);Label(r,history[total-1-alert_offset-i],525,y+53,37,920);
                }
                if(total>0)Label(r,std::to_string(alert_offset+1)+"–"+std::to_string(std::min(alert_offset+4,total))+" of "+std::to_string(total)+" alerts",960,860,27,0,true);
                if(total>4) {
                    Button(r,1520,245,290,75,"Newer",false,[this](){ScrollAlerts(-4);});
                    Button(r,1520,335,290,75,"Older",false,[this](){ScrollAlerts(4);});
                }
            }
            auto choices=Choices();focus=std::clamp(focus,0,std::max(0,static_cast<int>(choices.size())-1));
            for(size_t i=0;i<choices.size();++i) {
                auto &c=choices[i];
                if(control && i<2) {Slider(r,c.x,c.y,c.w,c.h,c.label,i==0?brightness:volume,focus==static_cast<int>(i));continue;}
                if(control && i>=2 && i<=7) {
                    const char *icons[]={"wifi","bluetooth","games","power","moon","lock"};
                    const bool on=(i==2&&g_GlobalSettings.wireless_lan_enabled)||(i==3&&g_GlobalSettings.bluetooth_enabled)||(i==6&&g_MenuApplication->IsQuietMode());
                    Card(r,c.x,c.y,c.w,c.h,focus==static_cast<int>(i));
                    if(on)r->RenderCircleFill({103,155,242,210},c.x+c.w/2,c.y+65,45);
                    Glyph(r,icons[i-2],c.x+c.w/2-26,c.y+39,52);
                    Label(r,c.label,c.x+c.w/2,c.y+117,27,c.w-14,true);
                    Label(r,c.detail,c.x+c.w/2,c.y+148,27,c.w-14,true);
                    hits.push_back({c.x,c.y,c.w,c.h,c.action});continue;
                }
                if(settings) {
                    if(focus==static_cast<int>(i))r->RenderRoundedRectangleFill({132,155,223,105},c.x,c.y,c.w,c.h,26);
                    else r->RenderRoundedRectangleFill({31,37,76,48},c.x,c.y,c.w,c.h,26);
                } else Card(r,c.x,c.y,c.w,c.h,focus==static_cast<int>(i));
                Label(r,c.label,c.x+25,c.y+12,37,c.w-50);
                Label(r,c.detail,c.x+25,c.y+(settings?51:65),27,c.w-50);
                if(settings && setting_section==0 && i<2) {
                    const int x=c.x+750,y=c.y+42,w=530;
                    const float value=(i==0?glass:blur)/100.f;
                    r->RenderRoundedRectangleFill({30,38,75,110},x,y,w,12,6);
                    if(value>0)r->RenderRoundedRectangleFill({236,238,255,240},x,y,int(w*value),12,6);
                    r->RenderCircleFill({255,255,255,255},x+int(w*value),y+6,17);
                }
                if(settings && setting_section==0 && i==2) {
                    r->RenderRoundedRectangleFill(motion?pu::ui::Color{99,161,238,255}:pu::ui::Color{70,77,120,255},c.x+c.w-180,c.y+22,125,48,24);
                    r->RenderCircleFill({255,255,255,255},c.x+c.w-(motion?81:156),c.y+46,21);
                }
                hits.push_back({c.x,c.y,c.w,c.h,c.action});
            }
            Label(r,"A  Select     B  Home     ZL/ZR  Control     +  Notifications",70,1005,30);
        }
        if(motion) {
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-transition).count();
            if(elapsed>=0 && elapsed<260) {
                const float progress=static_cast<float>(elapsed)/260.f;
                const float t=1.f-progress*progress*(3.f-2.f*progress);
                r->RenderRectangleFill({47,42,91,static_cast<u8>(58*t)},0,110,1920,970);
            }
        }
    }
}
