#include <ul/menu/ui/ui_GlassShell.hpp>
#include <ul/menu/ui/ui_ArtworkClient.hpp>
#include <ul/glassnx/Artwork.hpp>
#include <ul/glassnx/Surface.hpp>
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
        :entries(e),launch(l),legacy(old),folder_back(back),close_suspended(close) {LoadPreferences();LoadBackdrop();ReadRecent();LoadCategories();
        if(auto fp=fopen("sdmc:/glassnx/safe-mode","rb")){safe_mode=true;fclose(fp);}
        if(auto fp=fopen("sdmc:/glassnx/enhanced-glass","rb")){enhanced_glass=true;fclose(fp);}
        last_conservative=Conservative();}

    bool GlassShell::Conservative() const {return safe_mode||g_GlobalSettings.IsSuspended();}
    void GlassShell::ClearMaterials(){materials.clear();material_bytes=0;}
    void GlassShell::PrepareTransition(){
        CloseHub();CloseGallery();keyboard_open=false;keyboard_draft.clear();close_sheet=false;dock_focus=-1;
        liquid_glass.Release();ClearMaterials();texts.clear();custom_icons.clear();banner.reset();hero_frost.reset();banner_checked=false;
        theme_previews.clear();drag_slider=-1;focus_lift=0;focus_velocity=0;nav_velocity=0;dock_lift.fill(0);dock_velocity.fill(0);
        next_metrics=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    }
    void GlassShell::ToggleEnhancedGlass(){
        if(enhanced_glass) {
            if(remove("sdmc:/glassnx/enhanced-glass")!=0){g_MenuApplication->ShowNotification("Could not save glass mode");return;}
            enhanced_glass=false;
        } else {
            if(g_MenuApplication->DisplayDialog("Experimental GPU glass","Not yet tested on Switch hardware. Disable if unstable. Safe Mode always overrides this option.",{"Enable","Cancel"},true)!=0)return;
            auto fp=fopen("sdmc:/glassnx/enhanced-glass","wb");
            if(!fp){g_MenuApplication->ShowNotification("Could not save glass mode");return;}
            fclose(fp);enhanced_glass=true;
        }
        PrepareTransition();
    }
    void GlassShell::ToggleSafeMode(){
        if(safe_mode){
            if(remove("sdmc:/glassnx/safe-mode")!=0){g_MenuApplication->ShowNotification("Could not disable safe mode");return;}
            safe_mode=false;
        } else {
            auto fp=fopen("sdmc:/glassnx/safe-mode","wb");
            if(!fp){g_MenuApplication->ShowNotification("Could not save safe mode");return;}
            fclose(fp);safe_mode=true;
        }
        PrepareTransition();
        g_MenuApplication->ShowNotification(safe_mode?"Safe rendering enabled":"Standard rendering enabled");
    }
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
        if(!motion||Conservative()){nav_position=target;nav_velocity=0;focus_lift=0;focus_velocity=0;dock_lift.fill(0);dock_velocity.fill(0);return;}
        for(int i=0;i<5;i++)dock_lift[i]=std::clamp(StepSpring(dock_lift[i],dock_velocity[i],page==Page::Home&&dock_focus==i?1.f:0.f,dt),0.f,1.f);
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
        if(page==Page::Library&&filter==4){SetPage(Page::Home);return;}
        if((page==Page::Library||page==Page::Home||page==Page::Apps)&&!entries->IsInRoot()){folder_back();Invalidate();return;}
        SetPage(Page::Home);
    }

    namespace {
        const char *PrefsPath() {
            return g_GlobalSettings.ams_is_emummc?"sdmc:/glassnx/glassnx-emu.prefs":"sdmc:/glassnx/glassnx-sys.prefs";
        }
        constexpr const char *DockShortcutNames[]={"Album","Search","Apps","Library","Lock"};
        constexpr const char *DockShortcutIcons[]={"album","search","apps","games","lock"};
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
    bool GlassShell::SetArtworkKey() {
        SwkbdConfig cfg;if(R_FAILED(swkbdCreate(&cfg,0)))return false;
        swkbdConfigMakePresetDefault(&cfg);
        swkbdConfigSetType(&cfg,SwkbdType_All);
        swkbdConfigSetGuideText(&cfg,"Enter your SteamGridDB API key");
        char result[129]={};const auto rc=ShowSwkbd(&cfg,result,sizeof(result));swkbdClose(&cfg);
        if(R_FAILED(rc))return false;
        const std::string key=result;
        if(key.size()<16||key.size()>128||!std::all_of(key.begin(),key.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_';})){
            g_MenuApplication->ShowNotification("Invalid API key format");return false;
        }
        const char *path="sdmc:/glassnx/steamgriddb.key",*tmp="sdmc:/glassnx/steamgriddb.key.tmp";
        auto fp=fopen(tmp,"wb");if(!fp)return false;
        const bool wrote=fwrite(key.data(),1,key.size(),fp)==key.size(),closed=fclose(fp)==0;
        if(!wrote||!closed||rename(tmp,path)!=0){remove(tmp);return false;}
        return true;
    }
    void GlassShell::EditArtwork(u64 id,const std::string &name){
        if(Conservative()){
            g_MenuApplication->DisplayDialog("Artwork","Close any running game and disable Safe Mode before downloading artwork.",{"OK"},true);return;
        }
        char hex[24]={};snprintf(hex,sizeof(hex),"%016llx",static_cast<unsigned long long>(id));
        const std::string root=std::string("sdmc:/glassnx/art/")+hex;
        const int action=g_MenuApplication->DisplayDialog("Change artwork",name,{"Game cover","Hero banner","Restore defaults","API key","Cancel"},true);
        if(action==3){if(SetArtworkKey())g_MenuApplication->ShowNotification("SteamGridDB key saved");return;}
        if(action==2){
            if(g_MenuApplication->DisplayDialog("Restore default artwork?","Remove custom cover and banner for this title.",{"Restore","Cancel"},true)==0){
                remove((root+".cover1024").c_str());remove((root+".cover").c_str());remove((root+".hero").c_str());remove((root+".jpg").c_str());
                custom_icons.erase(id);banner.reset();hero_frost.reset();banner_checked=false;
            }
            return;
        }
        if(action!=0&&action!=1)return;
        const bool hero=action==1;
        auto key=fopen("sdmc:/glassnx/steamgriddb.key","rb");
        if(key)fclose(key);
        else if(!SetArtworkKey())return;
        PrepareTransition();
        gallery.active=true;gallery.hero=hero;gallery.id=id;gallery.title=name;
        if(!art::FindArtworkGames(name,gallery.games,gallery.error))gallery.games.clear();
        hits.clear();touched=true;
    }
    void GlassShell::CloseGallery(){
        if(!gallery.active)return;
        gallery=ArtworkGallery{};
        hits.clear();touched=true;
    }
    void GlassShell::GalleryMove(int delta){
        const size_t n=gallery.matching?gallery.games.size():gallery.pictures.size();
        if(n==0)return;
        if(n==1)return;
        gallery.index=(gallery.index+n+(delta<0?n-1:1))%n;
        if(!gallery.matching)GalleryPreview();
    }
    void GlassShell::GalleryPreview(){
        gallery.image.reset();gallery.preview.reset();gallery.error.clear();
        if(Conservative()||gallery.index>=gallery.pictures.size())return;
        std::string data;
        if(!art::DownloadPicture(gallery.pictures[gallery.index],gallery.hero,data,gallery.error))return;
        auto rw=SDL_RWFromConstMem(data.data(),int(data.size()));
        auto source=rw?IMG_Load_RW(rw,1):nullptr;
        const bool valid=source&&(gallery.hero?ul::glassnx::IsHeroSource(source->w,source->h):ul::glassnx::IsSquareCover(source->w,source->h));
        SDL_Surface *normalized=nullptr;
        if(valid)normalized=ul::glassnx::NormalizeArtwork(source,gallery.hero,source->w==1024?1024:512,gallery.error);
        else gallery.error="Artwork does not meet the required dimensions.";
        if(source)SDL_FreeSurface(source);
        if(!normalized)return;
        gallery.image=std::shared_ptr<SDL_Surface>(normalized,SDL_FreeSurface);
        // Only one full source and one bounded preview reside in memory.
        const float scale=std::min(960.f/normalized->w,600.f/normalized->h);
        auto small=SDL_CreateRGBSurfaceWithFormat(0,std::max(1,int(normalized->w*scale)),std::max(1,int(normalized->h*scale)),32,SDL_PIXELFORMAT_RGBA32);
        SDL_Texture *tex=nullptr;
        if(small&&SDL_BlitScaled(normalized,nullptr,small,nullptr)==0)tex=SDL_CreateTextureFromSurface(pu::ui::render::GetMainRenderer(),small);
        if(small)SDL_FreeSurface(small);
        if(tex)gallery.preview=pu::sdl2::TextureHandle::New(tex);
        else {gallery.image.reset();gallery.error="Unable to create preview.";}
    }
    void GlassShell::GallerySelect(){
        if(Conservative()){CloseGallery();return;}
        if(gallery.matching){
            if(gallery.games.empty()){
                gallery.error.clear();art::FindArtworkGames(gallery.title,gallery.games,gallery.error);return;
            }
            if(gallery.index>=gallery.games.size())return;
            gallery.pictures.clear();
            gallery.error.clear();
            if(!art::FindPictures(gallery.games[gallery.index].id,gallery.hero,gallery.pictures,gallery.error))return;
            gallery.matching=false;gallery.index=0;GalleryPreview();return;
        }
        if(!gallery.image){GalleryPreview();return;}
        char hex[24]={};snprintf(hex,sizeof(hex),"%016llx",static_cast<unsigned long long>(gallery.id));
        const std::string root=std::string("sdmc:/glassnx/art/")+hex;
        bool saved=false;
        if(gallery.hero)saved=ul::glassnx::ExportArtwork(gallery.image.get(),root+".hero",gallery.error);
        else{
            auto tile=ul::glassnx::NormalizeArtwork(gallery.image.get(),false,512,gallery.error);
            if(tile){
                saved=ul::glassnx::ExportArtwork(tile,root+".cover",gallery.error);
                SDL_FreeSurface(tile);
                if(saved){
                    std::string optional_error;
                    if(gallery.image->w!=1024||!ul::glassnx::ExportArtwork(gallery.image.get(),root+".cover1024",optional_error))
                        remove((root+".cover1024").c_str());
                }
            }
        }
        if(saved){
            const auto id=gallery.id;const bool hero=gallery.hero;
            CloseGallery();custom_icons.erase(id);banner.reset();hero_frost.reset();banner_checked=false;
            g_MenuApplication->ShowNotification(hero?"Hero applied • 1130 x 405":"Square cover applied");
        }
    }
    void GlassShell::GalleryInput(u64 down,pu::ui::TouchPoint touch){
        if(Conservative()||(down&HidNpadButton_B)){CloseGallery();return;}
        if(touch.IsEmpty())touched=false;
        else if(!touched){
            touched=true;
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)){
                auto action=hit.action;action();return;
            }
        }
        if(down&(HidNpadButton_Left|HidNpadButton_StickLLeft|HidNpadButton_L))GalleryMove(-1);
        else if(down&(HidNpadButton_Right|HidNpadButton_StickLRight|HidNpadButton_R))GalleryMove(1);
        else if(down&HidNpadButton_A)GallerySelect();
    }
    void GlassShell::RenderGallery(Renderer &r){
        r->RenderRectangleFill({13,20,40,125},0,0,1920,1080);
        Card(r,64,38,1792,104);
        Glyph(r,"album",100,65,48);Label(r,"Artwork Studio",170,61,45);
        Label(r,"SteamGridDB",1470,71,30,320);
        Label(r,gallery.title,78,173,45,1720);
        Label(r,gallery.matching?"Choose the matching game":gallery.hero?"Hero banners":"Square game covers",80,235,30);
        if(gallery.matching){
            Card(r,240,325,1440,440);
            if(!gallery.games.empty()){
                Label(r,"MATCH "+std::to_string(gallery.index+1)+" / "+std::to_string(gallery.games.size()),960,383,30,0,true);
                Label(r,gallery.games[gallery.index].name,960,475,64,1280,true);
                Label(r,"Confirm the title before browsing artwork.",960,590,30,1200,true);
            }
        }else{
            Card(r,72,300,1120,620);
            if(gallery.preview){
                int w=0,h=0;SDL_QueryTexture(gallery.preview->Get(),nullptr,nullptr,&w,&h);
                if(w>0&&h>0){
                    const float scale=std::min(1050.f/w,580.f/h);
                    const int dw=int(w*scale),dh=int(h*scale);
                    RoundedArt(r,gallery.preview,632-dw/2,610-dh/2,dw,dh,36);
                }
            }else Label(r,"Preview unavailable",632,560,45,1000,true);
            Card(r,1220,300,630,620);
            Label(r,"ARTWORK "+std::to_string(gallery.index+1)+" / "+std::to_string(gallery.pictures.size()),1260,345,30,550);
            Label(r,gallery.hero?"Cinematic hero":"Original square",1260,405,45,550);
            if(gallery.index<gallery.pictures.size()){
                const auto &p=gallery.pictures[gallery.index];
                Label(r,"Source",1260,490,30);
                Label(r,std::to_string(p.width)+" × "+std::to_string(p.height),1260,535,37,540);
            }
            Label(r,"GlassNX output",1260,610,30);
            Label(r,gallery.hero?"1130 × 405":gallery.image&&gallery.image->w==1024?"512 + 1024 square":"512 × 512",1260,655,37,550);
            Label(r,"Original game data is unchanged.",1260,775,27,550);
        }
        if(!gallery.error.empty()){
            r->RenderRoundedRectangleFill({105,47,69,235},110,795,1700,92,30);
            Label(r,gallery.error,140,825,30,1640);
        }
        Button(r,78,960,250,82,"B  Back",false,[this](){CloseGallery();});
        Button(r,375,960,295,82,"L  Previous",false,[this](){GalleryMove(-1);});
        Button(r,700,960,520,82,gallery.matching?(gallery.games.empty()?"A  Retry search":"A  Choose game"):gallery.image?"A  Apply artwork":"A  Retry preview",true,[this](){GallerySelect();});
        Button(r,1250,960,295,82,"R  Next",false,[this](){GalleryMove(1);});
    }
    pu::sdl2::TextureHandle::Ref GlassShell::CustomIcon(u64 id){
        if(Conservative())return {};
        const auto it=custom_icons.find(id);if(it!=custom_icons.end())return it->second;
        if(!icon_budget)return {};
        if(custom_icons.size()>=32)custom_icons.erase(custom_icons.begin());
        char name[120]={};snprintf(name,sizeof(name),"sdmc:/glassnx/art/%016llx.cover",static_cast<unsigned long long>(id));
        auto fp=fopen(name,"rb");if(!fp){custom_icons.emplace(id,pu::sdl2::TextureHandle::Ref{});return {};}
        const auto size=fseek(fp,0,SEEK_END)==0?ftell(fp):-1;fclose(fp);
        if(size<=0||size>3*1024*1024){custom_icons.emplace(id,pu::sdl2::TextureHandle::Ref{});return {};}
        icon_budget=false;
        auto image=IMG_Load(name);if(!image){custom_icons.emplace(id,pu::sdl2::TextureHandle::Ref{});return {};}
        SDL_Texture *tex=nullptr;
        if(image->w>0&&image->h>0&&image->w<=1024&&image->h<=1024){
            auto small=SDL_CreateRGBSurfaceWithFormat(0,256,256,32,SDL_PIXELFORMAT_RGBA32);
            if(small){
                const int side=std::min(image->w,image->h);
                SDL_Rect crop{(image->w-side)/2,(image->h-side)/2,side,side};
                if(SDL_BlitScaled(image,&crop,small,nullptr)==0)tex=SDL_CreateTextureFromSurface(pu::ui::render::GetMainRenderer(),small);
                SDL_FreeSurface(small);
            }
        }
        SDL_FreeSurface(image);
        auto value=tex?pu::sdl2::TextureHandle::New(tex):pu::sdl2::TextureHandle::Ref{};
        custom_icons.emplace(id,value);return value;
    }
    int GlassShell::HubEntry(){
        auto &all=entries->GetEntries();
        for(size_t i=0;i<all.size();++i)
            if(all[i].Is<EntryType::Application>()&&all[i].app_info.app_id==hub_id)return static_cast<int>(i);
        return -1;
    }
    void GlassShell::CloseHub(){
        if(!hub_mode)return;
        hub_mode=0;hub_focus=0;hub_id=0;hits.clear();touched=true;
    }
    void GlassShell::TitleOptions(){
        const auto list=List();
        if(focus<0||focus>=static_cast<int>(list.size()))return;
        const auto &e=entries->GetEntries()[list[focus]];
        if(!e.Is<EntryType::Application>()){
            g_MenuApplication->ShowNotification("Game Hub is available for installed titles.");return;
        }
        hub_id=e.app_info.app_id;hub_mode=1;hub_focus=0;hits.clear();touched=true;
    }
    void GlassShell::OpenProfileOverview(){
        hub_mode=2;hub_focus=0;hub_id=0;hits.clear();touched=true;
    }
    void GlassShell::SetHubCategory(int category){
        const u64 id=hub_id;
        if(!id||HubEntry()<0||category<0||category>2)return;
        const auto old=title_categories;
        if(category==0)title_categories.erase(id);
        else{
            if(title_categories.size()>=1024&&!title_categories.count(id)){
                g_MenuApplication->ShowNotification("Library category limit reached");return;
            }
            title_categories[id]=category;
        }
        if(!SaveCategories()){
            title_categories=old;g_MenuApplication->ShowNotification("Could not save title category");return;
        }
        CloseHub();focus=offset=0;
        g_MenuApplication->ShowNotification("Library category saved");
    }
    void GlassShell::HubAction(int action){
        if(hub_mode==4){
            CloseHub();
            if(action==1){g_MenuApplication->ClearNotificationHistory();alert_offset=0;}
            return;
        }
        if(hub_mode==2){
            if(action==0){CloseHub();ShowUserPage();}
            else if(action==1){CloseHub();SetPage(Page::Library);filter=1;search.clear();focus=offset=0;}
            else if(action==2){CloseHub();SetPage(Page::Library);filter=3;search.clear();focus=offset=0;}
            else CloseHub();
            return;
        }
        if(hub_mode==3){
            if(action>=0&&action<=2)SetHubCategory(action);
            else {hub_mode=1;hub_focus=3;hits.clear();touched=true;}
            return;
        }
        const int idx=HubEntry();
        if(idx<0){CloseHub();return;}
        auto &e=entries->GetEntries()[idx];
        const u64 id=hub_id;
        if(action==0){CloseHub();launch(static_cast<u32>(idx));}
        else if(action==1){
            const auto it=std::find(favorites.begin(),favorites.end(),id);
            if(it!=favorites.end()){favorites.erase(it);g_MenuApplication->ShowNotification("Removed from favorites");}
            else if(favorites.size()<512){favorites.push_back(id);g_MenuApplication->ShowNotification("Added to favorites");}
            else{g_MenuApplication->ShowNotification("Favorite limit reached");return;}
            SavePreferences();
        }else if(action==2){
            if(Conservative()){g_MenuApplication->ShowNotification("Close the game and disable Safe Mode to edit artwork");return;}
            const auto name=EntryName(e);CloseHub();EditArtwork(id,name);
        }else if(action==3){hub_mode=3;hub_focus=0;hits.clear();touched=true;}
        else CloseHub();
    }
    void GlassShell::HubInput(u64 down,pu::ui::TouchPoint touch){
        if(down&HidNpadButton_B){
            if(hub_mode==3){hub_mode=1;hub_focus=3;hits.clear();touched=true;}
            else CloseHub();
            return;
        }
        if(touch.IsEmpty())touched=false;
        else if(!touched){
            touched=true;
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)){
                auto cb=hit.action;cb();return;
            }
        }
        const int last=hub_mode==1?4:hub_mode==4?1:3;
        if(down&(HidNpadButton_Up|HidNpadButton_StickLUp|HidNpadButton_Left|HidNpadButton_StickLLeft))hub_focus=std::max(0,hub_focus-1);
        else if(down&(HidNpadButton_Down|HidNpadButton_StickLDown|HidNpadButton_Right|HidNpadButton_StickLRight))hub_focus=std::min(last,hub_focus+1);
        else if(down&HidNpadButton_A)HubAction(hub_focus);
    }
    void GlassShell::RenderHub(Renderer &r){
        hits.clear();
        r->RenderRectangleFill({11,19,42,145},0,0,1920,1080);
        ul::glassnx::DrawSurface(r,95,80,1730,920,48,{23,34,61,232});
        const auto row=[&](int i,const std::string &label,const std::string &detail){
            const int x=1070,y=235+i*132,w=695,h=110;
            ul::glassnx::DrawSurface(r,x,y,w,h,26,hub_focus==i?pu::ui::Color{79,117,175,235}:pu::ui::Color{43,58,91,210},hub_focus==i);
            Label(r,label,x+28,y+12,37,w-60);
            Label(r,detail,x+28,y+61,27,w-60);
            hits.push_back({x,y,w,h,[this,i](){hub_focus=i;HubAction(i);}});
        };
        Button(r,1490,112,275,82,"B  Back",false,[this](){if(hub_mode==3){hub_mode=1;hub_focus=3;hits.clear();touched=true;}else CloseHub();});
        if(hub_mode==4){
            Label(r,"Clear Activity?",155,123,64,1200);
            Glyph(r,"bell",175,290,120);
            Label(r,"Remove saved alerts",155,445,45,855);
            Label(r,"This clears the notification history.",155,535,37,855);
            Label(r,"New system alerts will still appear.",155,605,30,855);
            Hint(r,155,925,180,"A","Select");Hint(r,350,925,220,"B","Cancel");
            row(0,"Keep alerts","Return without deleting anything");
            row(1,"Clear history","Remove all saved alerts");
            return;
        }
        if(hub_mode==2){
            Label(r,"Your collection",155,123,64,1200);
            RoundedArt(r,GetSelectedUserIconTexture(),180,260,230,230,114);
            Label(r,"GlassNX profile",155,535,45,870);
            Label(r,"Collection overview",155,600,37,870);
            Label(r,std::to_string(favorites.size())+" saved favorites",155,685,37,870);
            Label(r,std::to_string(recent.size())+" recent unique titles",155,755,37,870);
            Label(r,"Shared within this console environment",155,840,27,870);
            row(0,"Nintendo profile","Open account and profile settings");
            row(1,"Favorites","Browse your saved games");
            row(2,"Recently played","Browse recorded launch history");
            row(3,"Back","Return to your previous screen");
            Hint(r,155,925,180,"A","Select");Hint(r,350,925,180,"B","Back");Hint(r,545,925,240,"D","Navigate");
            return;
        }
        const int idx=HubEntry();
        if(idx<0){CloseHub();return;}
        auto &e=entries->GetEntries()[idx];
        Label(r,hub_mode==3?"Library category":"Game Hub",155,123,64,1200);
        const bool suspended=g_GlobalSettings.IsEntrySuspended(e);
        auto wide=Conservative()?pu::sdl2::TextureHandle::Ref{}:Banner(hub_id);
        auto icon=CustomIcon(hub_id);
        if(!icon)icon=entries->GlassIcon(static_cast<u32>(idx),icon_budget);
        if(!icon)icon=GetDefaultApplicationIconTexture();
        ul::glassnx::DrawSurface(r,155,235,855,360,36,{39,52,86,180});
        if(wide)RoundedArt(r,wide,165,245,835,340,28);
        else if(icon)RoundedArt(r,icon,412,250,340,340,48);
        Label(r,EntryName(e),155,635,45,855);
        Label(r,suspended?"Suspended":std::find(recent.begin(),recent.end(),hub_id)!=recent.end()?"In recent launch history":"Installed title",155,704,37,855);
        char hex[24]={};snprintf(hex,sizeof(hex),"%016llx",static_cast<unsigned long long>(hub_id));
        Label(r,std::string("Title ID  ")+hex,155,770,27,855);
        const auto category=title_categories.find(hub_id);
        const int value=category==title_categories.end()?0:category->second;
        const std::string category_name=value==1?"Game":value==2?"App":"Automatic";
        Label(r,"Category  /  "+category_name,155,825,30,855);
        Hint(r,155,925,180,"A","Select");Hint(r,350,925,180,"B","Back");Hint(r,545,925,240,"D","Navigate");
        if(hub_mode==3){
            row(0,"Automatic",value==0?"Current category":"Use detected title type");
            row(1,"Game",value==1?"Current category":"Show in game library");
            row(2,"App",value==2?"Current category":"Show in app library");
            row(3,"Cancel","Keep current category");
        }else{
            row(0,suspended?"Resume":"Launch","Open this title");
            row(1,Favorite(hub_id)?"Remove favorite":"Add favorite","Personalize your collection");
            row(2,"Artwork Studio",Conservative()?"Unavailable in conservative mode":"Cover and hero customization");
            row(3,"Library category",category_name);
            row(4,"Back","Return to your previous screen");
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
            // Optional trailing field keeps the v1 preference format backwards compatible.
            int shortcut=0;if(fscanf(fp," Dock %d",&shortcut)==1 && shortcut>=0&&shortcut<5)dock_shortcut=shortcut;
        }
        fclose(fp);
    }
    void GlassShell::SavePreferences() {
        const std::string temp=std::string(PrefsPath())+".tmp";
        auto fp=fopen(temp.c_str(),"wb");if(!fp){g_MenuApplication->ShowNotification("Could not save appearance settings");return;}
        bool ok=fprintf(fp,"GlassNX 1 %d %d %d %d %d %u\n",theme,glass,blur,int(motion),int(sort_name),static_cast<unsigned>(favorites.size()))>0;
        for(auto id:favorites)if(fprintf(fp,"%016llx\n",static_cast<unsigned long long>(id))<0)ok=false;
        if(fprintf(fp,"Dock %d\n",dock_shortcut)<0)ok=false;
        if(fclose(fp)!=0)ok=false;
        if(ok && rename(temp.c_str(),PrefsPath())==0)return;
        remove(temp.c_str());g_MenuApplication->ShowNotification("Could not save appearance settings");
    }
    pu::sdl2::TextureHandle::Ref GlassShell::LoadWallpaper(int index,bool preview) {
        static const char *names[]={"PastelDream","MidnightGlass","FrostedWhite","Aurora"};
        const auto path=std::string("sdmc:/glassnx/wallpapers/")+names[std::clamp(index,0,3)]+(preview?"-preview.jpg":".jpg");
        auto fp=fopen(path.c_str(),"rb");if(!fp)return {};
        const long bytes=fseek(fp,0,SEEK_END)==0?ftell(fp):-1;fclose(fp);
        if(bytes<=0||bytes>(preview?128*1024:2*1024*1024))return {};
        // The supplied pack has fixed dimensions. Reject unexpected decoded sizes.
        auto surface=IMG_Load(path.c_str());
        if(!surface)return {};
        const int w=preview?320:1280,h=preview?180:720;
        SDL_Texture *tex=nullptr;
        if(surface->w==w&&surface->h==h)tex=SDL_CreateTextureFromSurface(pu::ui::render::GetMainRenderer(),surface);
        SDL_FreeSurface(surface);
        return tex?pu::sdl2::TextureHandle::New(tex):pu::sdl2::TextureHandle::Ref{};
    }
    pu::sdl2::TextureHandle::Ref GlassShell::ActiveWallpaper() {
        return theme_wallpaper?theme_wallpaper:GetBackgroundTexture();
    }
    pu::ui::Color GlassShell::WallpaperTint() const {
        // Subtle scrim on bright wallpapers keeps white system labels legible.
        if(theme_wallpaper){
            constexpr pu::ui::Color scrims[]={{31,27,53,115},{0,0,0,0},{27,34,52,150},{10,24,38,80}};
            return scrims[theme];
        }
        return Tints[theme];
    }
    void GlassShell::FrameDiagnostics(){
        const auto now=std::chrono::steady_clock::now();
        if(!diagnostics){frame_tick={};last_diag_log={};return;}
        if(frame_tick.time_since_epoch().count()==0){frame_tick=frame_window=now;frame_count=frame_late=0;frame_bins.fill(0);frame_max=0;return;}
        const float ms=std::chrono::duration<float,std::milli>(now-frame_tick).count();
        frame_tick=now;
        ++frame_count;frame_max=std::max(frame_max,ms);
        if(ms>18.f)++frame_late;
        ++frame_bins[static_cast<unsigned>(std::clamp(ms,0.f,80.f))];
        const float seconds=std::chrono::duration<float>(now-frame_window).count();
        if(seconds>=2.f){display_fps=frame_count/seconds;display_max=frame_max;
            display_late=100.f*frame_late/frame_count;
            const unsigned rank=(frame_count*95+99)/100;unsigned cumulative=0;
            for(size_t i=0;i<frame_bins.size();++i){cumulative+=frame_bins[i];if(cumulative>=rank){display_p95=float(i+1);break;}}
            frame_count=frame_late=0;frame_bins.fill(0);frame_max=0;frame_window=now;
            if(last_diag_log.time_since_epoch().count()==0||now-last_diag_log>=std::chrono::seconds(10)){
                UL_LOG_INFO("GlassNX frame sample: page=%d fps=%.1f max_interval_ms=%.1f p95_ms_upper=%.1f late_pct=%.1f material_bytes=%lu conservative=%d",int(page),double(display_fps),double(display_max),double(display_p95),double(display_late),static_cast<unsigned long>(material_bytes),int(Conservative()));
                last_diag_log=now;
            }
        }
    }
    void GlassShell::SetTheme(int index) {
        liquid_glass.Release();theme_wallpaper.reset();theme=std::clamp(index,0,3);LoadBackdrop();ClearMaterials();texts.clear();SavePreferences();
        transition=std::chrono::steady_clock::now();
        if(!theme_wallpaper)g_MenuApplication->ShowNotification("Theme wallpaper missing. Install the GlassNX wallpaper pack at /glassnx/wallpapers.");
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
    void GlassShell::CycleDockShortcut(){
        dock_shortcut=(dock_shortcut+1)%5;SavePreferences();
    }
    void GlassShell::ApplySearch(const std::string &query){
        // HOME search spans all entries in the current folder; never recurse the SD card.
        if(page==Page::Home){
            if(query.empty())return;
            SetPage(Page::Library);filter=4;
        } else if(page==Page::Library&&filter==4&&query.empty())filter=0;
        search=query;focus=offset=0;
    }
    void GlassShell::Search() {
        if(safe_mode){NativeSearch(search);return;}
        keyboard_draft=search;keyboard_shift=false;keyboard_symbols=false;key_focus=10;
        keyboard_open=true;hits.clear();touched=true;
    }
    void GlassShell::NativeSearch(const std::string &initial) {
        SwkbdConfig cfg;if(R_FAILED(swkbdCreate(&cfg,0)))return;
        swkbdConfigMakePresetDefault(&cfg);
        swkbdConfigSetType(&cfg,SwkbdType_All);
        swkbdConfigSetGuideText(&cfg,"Search your library");
        swkbdConfigSetInitialText(&cfg,initial.c_str());
        char result[128]={};auto rc=ShowSwkbd(&cfg,result,sizeof(result));swkbdClose(&cfg);
        if(R_SUCCEEDED(rc))ApplySearch(result);
    }
    namespace {
        constexpr const char *KeyboardRows[]={"1234567890","qwertyuiop","asdfghjkl'","zxcvbnm,.-"};
        constexpr const char *KeyboardSymbolRows[]={"1234567890","!@#$%^&*()","[]{}<>+=_-",":;\"'/?\\|,."};
        constexpr const char *KeyboardActions[]={"Shift","Space","Delete","Clear","Search"};
        void EraseCharacter(std::string &value){
            if(value.empty())return;
            size_t pos=value.size()-1;
            while(pos>0&&(static_cast<unsigned char>(value[pos])&0xc0)==0x80)--pos;
            value.erase(pos);
        }
    }
    void GlassShell::FinishKeyboard(bool apply){
        if(apply){const auto query=keyboard_draft;ApplySearch(query);}
        keyboard_open=false;keyboard_draft.clear();hits.clear();touched=true;
    }
    void GlassShell::KeyboardKey(int key){
        if(key<0||key>=45)return;
        key_focus=key;
        if(key<40){
            char ch=(keyboard_symbols?KeyboardSymbolRows:KeyboardRows)[key/10][key%10];
            if(keyboard_shift)ch=static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            if(keyboard_draft.size()<127)keyboard_draft+=ch;
        }else switch(key){
            case 40:keyboard_shift=!keyboard_shift;break;
            case 41:if(keyboard_draft.size()<127)keyboard_draft+=' ';break;
            case 42:EraseCharacter(keyboard_draft);break;
            case 43:keyboard_draft.clear();break;
            case 44:FinishKeyboard(true);break;
        }
    }
    void GlassShell::KeyboardInput(u64 down,pu::ui::TouchPoint touch){
        if(down&HidNpadButton_B){FinishKeyboard(false);return;}
        if(down&HidNpadButton_Plus){FinishKeyboard(true);return;}
        if(down&HidNpadButton_X){const auto initial=keyboard_draft;FinishKeyboard(false);NativeSearch(initial);return;}
        if(down&HidNpadButton_Y){EraseCharacter(keyboard_draft);return;}
        if(down&HidNpadButton_L){keyboard_shift=!keyboard_shift;return;}
        if(down&HidNpadButton_R){keyboard_symbols=!keyboard_symbols;return;}
        if(touch.IsEmpty())touched=false;
        else if(!touched){
            touched=true;
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)){
                auto cb=hit.action;cb();return;
            }
        }
        int row=key_focus/10,col=key_focus%10;
        if(down&(HidNpadButton_Left|HidNpadButton_StickLLeft))col=std::max(0,col-1);
        else if(down&(HidNpadButton_Right|HidNpadButton_StickLRight))col=std::min(row==4?4:9,col+1);
        else if(down&(HidNpadButton_Up|HidNpadButton_StickLUp)){
            if(row>0){if(row==4)col*=2;--row;}
        }else if(down&(HidNpadButton_Down|HidNpadButton_StickLDown)){
            if(row<4){++row;if(row==4)col/=2;}
        }else if(down&HidNpadButton_A){KeyboardKey(key_focus);return;}
        key_focus=row*10+col;
    }
    void GlassShell::RenderKeyboard(Renderer &r){
        hits.clear();
        r->RenderRectangleFill({9,16,38,155},0,0,1920,1080);
        // Legible under all four wallpapers, independent of experimental GPU targets.
        ul::glassnx::DrawSurface(r,135,175,1650,805,48,{19,30,56,235});
        Glyph(r,"search",178,209,42);Label(r,page==Page::Home||filter==4?"Search games & apps":"Search your library",238,201,45,950);
        Button(r,1180,192,235,74,keyboard_symbols?"ABC":"Symbols",false,[this](){keyboard_symbols=!keyboard_symbols;});
        Hint(r,1490,209,245,"B","Cancel");
        hits.push_back({1450,188,300,84,[this](){FinishKeyboard(false);}});
        ul::glassnx::DrawSurface(r,175,278,1570,105,32,{13,23,48,200});
        Label(r,keyboard_draft.empty()?"Type a game or app name":keyboard_draft+" |",205,309,37,1370);
        Label(r,std::to_string(keyboard_draft.size())+"/127",1605,320,27,115,true);
        for(int i=0;i<45;i++){
            const bool action=i>=40;
            const int x=action?175+(i-40)*316:175+(i%10)*158;
            const int y=410+(i/10)*91,w=action?300:142;
            const bool selected=i==key_focus;
            const bool active=i==40&&keyboard_shift;
            const auto color=selected?pu::ui::Color{66,115,174,255}:active?pu::ui::Color{79,113,176,245}:i==44?pu::ui::Color{36,105,157,245}:pu::ui::Color{62,79,119,225};
            r->RenderRoundedRectangleFill(color,x,y,w,78,20);
            r->RenderRoundedRectangle({230,239,255,static_cast<u8>(selected?230:90)},x,y,w,78,20);
            if(selected){
                r->RenderRoundedRectangle({246,250,255,170},x+3,y+3,w-6,72,17);
                r->RenderRoundedRectangleFill({226,241,255,245},x+w/2-16,y+68,32,4,1);
            }
            std::string key;
            if(action)key=KeyboardActions[i-40];
            else {char ch=(keyboard_symbols?KeyboardSymbolRows:KeyboardRows)[i/10][i%10];if(keyboard_shift)ch=static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));key=std::string(1,ch);}
            Label(r,key,x+w/2,y+19,37,w-14,true);
            hits.push_back({x,y,w,78,[this,i](){KeyboardKey(i);}});
        }
        Hint(r,230,900,150,"A","Type");Hint(r,390,900,180,"Y","Delete");
        Hint(r,580,900,150,"L","Shift");Hint(r,740,900,205,"R","Symbols");
        Hint(r,960,900,180,"+","Search");Hint(r,1150,900,430,"X","System keyboard");
    }
    void GlassShell::OpenCloseSheet(){
        if(!g_GlobalSettings.IsSuspended())return;
        close_sheet=true;close_focus=0;hits.clear();touched=true;
    }
    void GlassShell::FinishCloseSheet(bool confirm){
        close_sheet=false;hits.clear();touched=true;
        if(confirm&&g_GlobalSettings.IsSuspended())close_suspended();
    }
    void GlassShell::CloseSheetInput(u64 down,pu::ui::TouchPoint touch){
        if(!g_GlobalSettings.IsSuspended()||(down&HidNpadButton_B)){FinishCloseSheet(false);return;}
        if(down&(HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_StickLLeft|HidNpadButton_StickLRight))close_focus=1-close_focus;
        else if(down&HidNpadButton_A){FinishCloseSheet(close_focus==1);return;}
        if(touch.IsEmpty())touched=false;
        else if(!touched){
            touched=true;
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)){
                auto cb=hit.action;cb();return;
            }
        }
    }
    void GlassShell::RenderCloseSheet(Renderer &r){
        hits.clear();
        r->RenderRectangleFill({7,12,30,128},0,0,1920,1080);
        // A suspended game uses conservative rendering: no GPU targets or capture.
        ul::glassnx::DrawSurface(r,460,250,1000,565,60,{35,47,81,207});
        r->RenderCircleFill({202,106,135,220},960,355,58);
        Glyph(r,"power",930,325,60);
        Label(r,"Close game?",960,440,64,850,true);
        Label(r,"Any unsaved progress will be lost.",960,535,37,900,true);
        Button(r,520,650,420,95,"Keep playing",close_focus==0,[this](){FinishCloseSheet(false);});
        ul::glassnx::DrawSurface(r,980,650,420,95,40,{166,66,100,static_cast<u8>(close_focus==1?240:180)},close_focus==1);
        Label(r,"Close game",1190,677,37,390,true);
        hits.push_back({980,650,420,95,[this](){FinishCloseSheet(true);}});
        Hint(r,650,762,210,"B","Cancel");Hint(r,880,762,430,"A","Confirm selection");
    }
    void GlassShell::ActivateDock(int index){
        dock_focus=-1;
        switch(index){
            case 0:SetPage(Page::Notifications);break;
            case 1:SetPage(Page::Control);break;
            case 2:
                switch(dock_shortcut){
                    case 1:Search();break;
                    case 2:SetPage(Page::Apps);break;
                    case 3:SetPage(Page::Library);break;
                    case 4:g_MenuApplication->LoadMenu(MenuType::Lockscreen);break;
                    default:ShowAlbum();break;
                }break;
            case 3:SetPage(Page::Settings);break;
            case 4:ShowPowerDialog();break;
        }
    }
    void GlassShell::RenderDock(Renderer &r){
        // Shared lens with icon-first shortcuts, a restrained raised selection,
        // and a detached silhouette. No additional full-screen render target.
        r->RenderRoundedRectangleFill({10,16,39,75},368,925,1184,134,58);
        Card(r,370,920,1180,132);
        // A second subtle edge, not another opaque card over the material.
        r->RenderRoundedRectangle({238,248,255,95},374,924,1172,124,50);
        r->RenderRoundedRectangleFill({241,246,255,60},420,925,1080,3,1);
        const char *icons[]={"bell","sun",DockShortcutIcons[dock_shortcut],"settings","power"};
        const char *names[]={"Alerts","Control",DockShortcutNames[dock_shortcut],"Settings","Power"};
        for(int i=0;i<5;i++){
            const int x=390+i*230;
            if(dock_focus==i)ul::glassnx::DrawSurface(r,x+12,934,196,108,42,{146,170,230,130},true);
            const bool selected=dock_focus==i;
            const float lift=motion&&!Conservative()?dock_lift[i]:(selected?1.f:0.f);
            const int side=68+int(22*lift),ix=x+110-side/2,iy=932-int(15*lift);
            if(auto tile=SystemSymbol(icons[i],true))Picture(r,tile,ix,iy,side,side);
            else {
                ul::glassnx::DrawSurface(r,ix,iy,side,side,20,{100,135,198,172},selected);
                const int glyph=side/2;
                Glyph(r,icons[i],ix+(side-glyph)/2,iy+(side-glyph)/2,glyph);
            }
            Label(r,names[i],x+110,1009,27,208,true);
            if(selected)r->RenderRoundedRectangleFill({230,241,255,210},x+95,1041,30,4,1);
            hits.push_back({x,928,220,116,[this,i](){ActivateDock(i);}});
        }
        const auto alerts=g_MenuApplication->GetNotificationHistory().size();
        if(alerts){
            const float lift=motion&&!Conservative()?dock_lift[0]:(dock_focus==0?1.f:0.f);
            const int bx=500+(68+int(22*lift))/2-2,by=937-int(15*lift);
            r->RenderCircleFill({147,101,162,255},bx,by,18);
            Label(r,alerts>9?"9+":std::to_string(alerts),bx,by-15,27,0,true);
        }
    }
    void GlassShell::LoadBackdrop() {
        backdrop.clear();
        theme_wallpaper=LoadWallpaper(theme,false);
        if(!theme_wallpaper)UL_LOG_WARN("GlassNX: theme %d wallpaper missing or invalid; using built-in fallback",theme);
        else UL_LOG_INFO("GlassNX: theme %d loaded from SD wallpaper pack",theme);
        static const char *names[]={"PastelDream","MidnightGlass","FrostedWhite","Aurora"};
        const auto path=theme_wallpaper?std::string("sdmc:/glassnx/wallpapers/")+names[theme]+".jpg":TryFindImage("ui/Background");
        if(path.empty())return;
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
        banner_checked=true;banner_id=id;banner.reset();hero_frost.reset();
        char name[120]={};
        snprintf(name,sizeof(name),"sdmc:/glassnx/art/%016llx.jpg",static_cast<unsigned long long>(id));
        char custom[120]={};snprintf(custom,sizeof(custom),"sdmc:/glassnx/art/%016llx.hero",static_cast<unsigned long long>(id));
        auto fp=fopen(custom,"rb");
        if(fp)snprintf(name,sizeof(name),"%s",custom);else fp=fopen(name,"rb");
        if(!fp)return {};
        const bool seek_ok=fseek(fp,0,SEEK_END)==0;
        const long length=seek_ok?ftell(fp):-1;fclose(fp);
        if(length<=0||length>3*1024*1024)return {};
        auto image=IMG_Load(name);if(!image)return {};
        if(image->w>0&&image->h>0&&image->w<=1920&&image->h<=1080){
            auto scaled=SDL_CreateRGBSurfaceWithFormat(0,1130,405,32,SDL_PIXELFORMAT_RGBA32);
            if(scaled){
                // Center-crop to the cached 226:81 texture, preserving aspect ratio.
                SDL_Rect crop{0,0,image->w,image->h};
                if(image->w*81>image->h*226){crop.w=image->h*226/81;crop.x=(image->w-crop.w)/2;}
                else {crop.h=image->w*81/226;crop.y=(image->h-crop.h)/2;}
                if(crop.w>0&&crop.h>0&&SDL_BlitScaled(image,&crop,scaled,nullptr)==0){
                    auto tex=SDL_CreateTextureFromSurface(pu::ui::render::GetMainRenderer(),scaled);
                    if(tex)banner=pu::sdl2::TextureHandle::New(tex);
                    // Precompute only the area under the information sheet from
                    // the actual hero, not the unrelated HOME wallpaper.
                    if(banner){
                        auto low=SDL_CreateRGBSurfaceWithFormat(0,66,25,32,SDL_PIXELFORMAT_RGBA32);
                        auto soft=SDL_CreateRGBSurfaceWithFormat(0,66,25,32,SDL_PIXELFORMAT_RGBA32);
                        SDL_Rect area{13,140,660,248};
                        if(low&&soft&&SDL_BlitScaled(scaled,&area,low,nullptr)==0){
                            const auto src=static_cast<const u8*>(low->pixels);
                            auto dst=static_cast<u8*>(soft->pixels);
                            for(int yy=0;yy<25;yy++)for(int xx=0;xx<66;xx++){
                                for(int k=0;k<3;k++){
                                    unsigned sum=0;
                                    for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)
                                        sum+=src[std::clamp(yy+dy,0,24)*low->pitch+std::clamp(xx+dx,0,65)*4+k];
                                    dst[yy*soft->pitch+xx*4+k]=static_cast<u8>(sum/25);
                                }
                                dst[yy*soft->pitch+xx*4+3]=255;
                            }
                            auto frost=SDL_CreateTextureFromSurface(pu::ui::render::GetMainRenderer(),soft);
                            if(frost){SDL_SetTextureScaleMode(frost,SDL_ScaleModeLinear);hero_frost=pu::sdl2::TextureHandle::New(frost);}
                        }
                        if(low)SDL_FreeSurface(low);
                        if(soft)SDL_FreeSurface(soft);
                    }
                }
                SDL_FreeSurface(scaled);
            }
        }
        SDL_FreeSurface(image);
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
        dock_focus=-1;
        if((p==Page::Control || p==Page::Notifications) && static_cast<int>(page)<4) {
            previous=page;previous_focus=focus;previous_offset=offset;previous_filter=filter;previous_section=setting_section;previous_search=search;
        }
        if(p==Page::Settings)setting_section=0;
        if(p==Page::Control&&!Conservative())next_metrics={};
        page=p; focus=0;offset=0;filter=0;alert_offset=0;animated_focus=-1;search.clear();hits.clear();transition=std::chrono::steady_clock::now();
    }
    std::vector<u32> GlassShell::List() {
        std::vector<u32> result;
        auto &all=entries->GetEntries();
        const auto needle=Fold(search);
        for(u32 i=0;i<all.size();++i) {
            auto &e=all[i];
            if(e.Is<EntryType::Invalid>())continue;
            const auto name=Fold(EntryName(e));
            if(!needle.empty() && name.find(needle)==std::string::npos)continue;
            // Installed forwarders can be reported as applications by Horizon.
            // Keep known utilities out of the game shelf; never hide unknown titles.
            auto utility_name=name;
            // Some forwarders include a stylized accent or surrounding whitespace.
            const auto first=utility_name.find_first_not_of(" \t\r\n");
            if(first!=std::string::npos)utility_name=utility_name.substr(first,utility_name.find_last_not_of(" \t\r\n")-first+1);
            bool utility=e.Is<EntryType::Application>() &&
                (utility_name=="sphaira" || utility_name=="sphaíra" || utility_name=="sphaïra" || utility_name=="sphaîra" || utility_name=="sphaira menu" || name=="switchdrop" || name=="opennow" || name=="horizon" ||
                 name=="youtube tv" || name=="youtube" || name=="streamfin" || name=="glassnx manager" || name=="umanager" || name=="hbmenu" || name=="homebrew menu");
            if(e.Is<EntryType::Application>()) {
                const auto it=title_categories.find(e.app_info.app_id);
                if(it!=title_categories.end())utility=it->second==2;
            }
            const bool game=e.Is<EntryType::Application>()&&!utility;
            if(page==Page::Library&&filter==4){result.push_back(i);continue;}
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
        if(Conservative())return {};
        const bool gpu=enhanced_glass&&liquid_glass.IsReady();
        const auto key=std::to_string(gpu?0:x)+"/"+std::to_string(gpu?0:y)+"/"+std::to_string(w)+"/"+std::to_string(h)+"/"+std::to_string(theme)+"/"+std::to_string(glass)+"/"+std::to_string(blur)+"/"+std::to_string(selected)+"/"+std::to_string(gpu);
        auto it=materials.find(key);if(it!=materials.end())return it->second;
        // Cache at half logical resolution; the GPU composites the translucent
        // textured bevel. No per-frame rasterization or framebuffer readback.
        const int tw=(w+3)/4,th=(h+3)/4;
        const size_t bytes=static_cast<size_t>(tw)*th*4;
        if(bytes>2*1024*1024||material_budget==0)return {};
        // Do not churn every cached panel when a large page fills the cache.
        while(!materials.empty()&&(material_bytes+bytes>6*1024*1024 || materials.size()>=48)){
            auto victim=materials.begin();int vw=0,vh=0;
            if(victim->second&&SDL_QueryTexture(victim->second->Get(),nullptr,nullptr,&vw,&vh)==0&&vw>0&&vh>0){
                const size_t used=size_t(vw)*vh*4;material_bytes-=std::min(material_bytes,used);
            }
            materials.erase(victim);
        }
        if(material_bytes+bytes>6*1024*1024)return {};
        --material_budget;
        const float radius=std::max(2.f,std::min(13.f,std::min(tw,th)/2.f-1.f));
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
            const float u=float(px)/tw;
            const float angle=.20f+.80f*(1-v)*(1-v);
            const float lower=std::max(0.f,(v-.70f)/.30f)*(0.26f+0.75f*u);
            const float edge_shadow=std::max(0.f,1.f-std::abs(d+2.f)/3.f)*lower;
            const float reflection=std::max(0.f,1.f-std::abs(float(px)/tw+v*.55f-.4f)/.23f);
            const float shine=(1.f-v)*.052f+reflection*.095f+rim*(selected?.78f:.61f)*angle+light*.065f+rim*lower*.20f;
            const auto p=(static_cast<size_t>(py)*tw+px)*4;
            // Sample the real wallpaper with a low-frequency blur and edge
            // displacement. Cached per panel position, never reads the GPU framebuffer.
            const float edge=std::clamp(1.f-std::abs(d+1.f)/7.f,0.f,1.f);
            const float lens=edge*edge;
            const float nx=px+.5f<tw*.5f?-1.f:1.f,ny=py+.5f<th*.5f?-1.f:1.f;
            const float gradx=ox>0.f?ox/std::max(1.f,std::sqrt(ox*ox+oy*oy)):qx>qy?1.f:0.f;
            const float grady=oy>0.f?oy/std::max(1.f,std::sqrt(ox*ox+oy*oy)):qy>=qx?1.f:0.f;
            const int sx=std::clamp(int((x+px*4-nx*gradx*lens*24.f)/8.f),0,239);
            const int sy=std::clamp(int((y+py*4-ny*grady*lens*24.f)/8.f),0,134);
            const int radius_sample=blur/15;
            for(int k=0;k<3;k++) {
                const float tc=k==0?tint.r:k==1?tint.g:tint.b;
                float bg=tc;
                if(!gpu&&!backdrop.empty()) {
                    unsigned sum=0;
                    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
                        sum+=backdrop[(std::clamp(sy+dy*radius_sample,0,134)*240+std::clamp(sx+dx*radius_sample,0,239))*4+k];
                    bg=sum/9.f;
                    const auto shade=WallpaperTint();
                    const float sc=k==0?shade.r:k==1?shade.g:shade.b;
                    bg=bg*(1-shade.a/255.f)+sc*(shade.a/255.f);
                }
                const float colored=bg*.86f+tc*.14f;
                rgba[p+k]=byte(colored*(1.f-.12f*edge_shadow)+(255-colored)*shine);
            }
            rgba[p+3]=byte(cover*((100-glass)*1.15f+(gpu?30.f:82.f)+18*(1-v)+rim*(selected?125:90)+light*18));
        }
        auto tex=SDL_CreateTexture(pu::ui::render::GetMainRenderer(),SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,tw,th);
        if(!tex)return {};
        if(SDL_UpdateTexture(tex,nullptr,rgba.data(),tw*4)!=0 || SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND)!=0) {SDL_DestroyTexture(tex);return {};}
        material_bytes+=bytes;
        auto handle=pu::sdl2::TextureHandle::New(tex);materials.emplace(key,handle);return handle;
    }
    void GlassShell::Card(Renderer &r,int x,int y,int w,int h,bool selected) {
        const int radius=std::min(50,std::min(w,h)/2);
        r->RenderRoundedRectangleFill({29,25,66,18},x-4,y+12,w+8,h,radius+4);
        r->RenderRoundedRectangleFill({18,18,47,32},x,y+5,w,h,radius);
        if(enhanced_glass&&!Conservative())liquid_glass.Draw(pu::ui::render::GetMainRenderer(),x,y,w,h);
        const auto tex=Material(x,y,w,h,selected);
        if(tex)Picture(r,tex,x,y,w,h);
        else {
            const auto c=GlassColors[theme];
            ul::glassnx::DrawSurface(r,x,y,w,h,radius,{c.r,c.g,c.b,125},selected);
        }
        if(selected) {
            r->RenderRoundedRectangle({178,203,255,130},x-3,y-3,w+6,h+6,radius+3);
            r->RenderRoundedRectangle({237,243,255,205},x,y,w,h,radius);
        }
    }
    void GlassShell::Label(Renderer &r,const std::string &s,int x,int y,int size,int maxw,bool center,bool dark) {
        if(s.empty())return;
        const auto key=std::to_string(size)+"/"+std::to_string(maxw)+(dark?"/dark/":"/light/")+s;
        auto it=texts.find(key);
        if(it==texts.end()) {
            if(texts.size()>=160) texts.erase(texts.begin());
            auto t=pu::ui::elm::TextBlock::New(0,0,s);
            t->SetFont(pu::ui::MakeDefaultFontName(size));
            t->SetColor(dark?pu::ui::Color{24,36,57,255}:size<30?pu::ui::Color{231,238,255,235}:pu::ui::Color{255,255,255,255});
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
    // Physical input silhouettes: bright controller hardware, dark legends,
    // separate action text. No hit regions or textures are allocated here.
    void GlassShell::Hint(Renderer &r,int x,int y,int w,const std::string &key,const std::string &action){
        const pu::ui::Color white{247,249,255,255},ink{35,48,73,255},muted{142,155,177,255};
        int used=0;
        auto face=[&](const std::string &legend,bool stick){
            const int cx=x+used+19,cy=y+19;
            r->RenderCircleFill({10,18,36,100},cx,cy+2,18);
            r->RenderCircleFill(white,cx,cy,18);
            if(stick)r->RenderCircle(ink,cx,cy,15);
            Label(r,legend,cx,y+3,27,34,true,true);
            used+=40;
        };
        auto shoulder=[&](const std::string &legend){
            const int width=legend.size()==2?51:38;
            r->RenderRoundedRectangleFill({10,18,36,100},x+used,y+5,width,32,8);
            r->RenderRoundedRectangleFill(white,x+used,y+2,width,32,8);
            Label(r,legend,x+used+width/2,y+2,27,width-4,true,true);
            used+=width+5;
        };
        if(key=="LR"){shoulder("L");shoulder("R");}
        else if(key=="ZLZR"){shoulder("ZL");shoulder("ZR");}
        else if(key=="L"||key=="R"||key=="ZL"||key=="ZR")shoulder(key);
        else if(key=="LS"||key=="RS")face(key=="LS"?"L":"R",true);
        else if(key=="+"||key=="-"){
            const int cx=x+19,cy=y+19;
            r->RenderRoundedRectangleFill({10,18,36,100},cx-16,cy-4,32,12,3);
            if(key=="+")r->RenderRoundedRectangleFill({10,18,36,100},cx-6,cy-14,12,32,3);
            r->RenderRoundedRectangleFill(white,cx-16,cy-6,32,12,3);
            if(key=="+")r->RenderRoundedRectangleFill(white,cx-6,cy-16,12,32,3);
            used=40;
        }
        else if(key=="D"||key=="UD"||key=="LRD"||key=="DOWN"||key=="UP"){
            const int cx=x+20,cy=y+19;
            // Cross-shaped directional pad; only relevant arms are bright.
            r->RenderRoundedRectangleFill(muted,cx-7,cy-19,14,38,3);
            r->RenderRoundedRectangleFill(muted,cx-19,cy-7,38,14,3);
            if(key=="D"||key=="UD"||key=="UP")r->RenderRoundedRectangleFill(white,cx-7,cy-19,14,15,3);
            if(key=="D"||key=="UD"||key=="DOWN")r->RenderRoundedRectangleFill(white,cx-7,cy+4,14,15,3);
            if(key=="D"||key=="LRD"){
                r->RenderRoundedRectangleFill(white,cx-19,cy-7,15,14,3);
                r->RenderRoundedRectangleFill(white,cx+4,cy-7,15,14,3);
            }
            r->RenderRoundedRectangleFill(ink,cx-5,cy-5,10,10,3);
            used=42;
        }else face(key,false);
        if(!action.empty()&&w>used+8)Label(r,action,x+used+6,y+3,27,w-used-6);
    }
    void GlassShell::Button(Renderer &r,int x,int y,int w,int h,const std::string &s,bool selected,std::function<void()> cb) {
        ul::glassnx::DrawSurface(r,x,y,w,h,h/2,selected?pu::ui::Color{151,171,234,150}:pu::ui::Color{29,32,79,62},selected);
        if(s.size()>3 && s[1]==' ' && s[2]==' ' && std::string("ABXYLR+-").find(s[0])!=std::string::npos)
            Hint(r,x+20,y+(h-38)/2,w-40,s.substr(0,1),s.substr(3));
        else Label(r,s,x+w/2,y+(h-37)/2,30,w-24,true);
        hits.push_back({x,y,w,h,cb});
    }
    pu::sdl2::TextureHandle::Ref GlassShell::SystemSymbol(const std::string &kind,bool tile){
        const auto key=std::string(tile?"Icons/":"Glyphs/")+kind;
        const auto it=system_symbols.find(key);if(it!=system_symbols.end())return it->second;
        if(Conservative()||symbol_budget==0||system_symbols.size()>=64)return {};
        --symbol_budget;
        auto t=TryFindLoadImageHandle("ui/GlassNX/"+key);
        if(!t||!t->Get())t.reset();
        system_symbols.emplace(key,t);return t;
    }
    void GlassShell::Glyph(Renderer &r,const std::string &kind,int x,int y,int size) {
        if(auto t=SystemSymbol(kind,false)){Picture(r,t,x,y,size,size);return;}
        const pu::ui::Color white{246,249,255,255};
        auto arc=[&](float cx,float cy,float rad,float start,float end){
            auto rr=pu::ui::render::GetMainRenderer();
            SDL_SetRenderDrawColor(rr,246,249,255,255);
            for(int j=0;j<16;j++){
                const float a=(start+(end-start)*j/16.f)*3.14159265f/180.f;
                const float b=(start+(end-start)*(j+1)/16.f)*3.14159265f/180.f;
                for(int k=-1;k<=1;k++)SDL_RenderDrawLine(rr,x+int((cx+rad*cosf(a))*size/24)+k,y+int((cy+rad*sinf(a))*size/24),x+int((cx+rad*cosf(b))*size/24)+k,y+int((cy+rad*sinf(b))*size/24));
            }
        };
        auto line=[&](int a,int b,int c,int d) {
            auto rr=pu::ui::render::GetMainRenderer();
            SDL_SetRenderDrawColor(rr,246,249,255,255);
            for(int i=-1;i<=1;i++)SDL_RenderDrawLine(rr,x+a*size/24+i,y+b*size/24,x+c*size/24+i,y+d*size/24);
        };
        auto box=[&](int a,int b,int w,int h){r->RenderRoundedRectangle(white,x+a*size/24,y+b*size/24,w*size/24,h*size/24,std::max(2,size/10));};
        auto dot=[&](int a,int b,int rad){r->RenderCircleFill(white,x+a*size/24,y+b*size/24,std::max(2,rad*size/24));};
        if(kind=="home"){line(2,11,12,2);line(12,2,22,11);box(5,10,14,12);line(12,16,12,22);}
        else if(kind=="games"){box(2,5,20,14);line(7,9,7,15);line(4,12,10,12);dot(16,10,1);dot(19,13,1);dot(13,13,1);dot(16,16,1);}
        else if(kind=="apps"){for(int a:{3,13})for(int b:{3,13})r->RenderRoundedRectangleFill({246,249,255,235},x+a*size/24,y+b*size/24,8*size/24,8*size/24,std::max(2,size/10));}
        else if(kind=="settings"){r->RenderCircle(white,x+size/2,y+size/2,size/4);for(int i=0;i<8;i++){float a=i*3.14159265f/4;line(12+int(7*cosf(a)),12+int(7*sinf(a)),12+int(11*cosf(a)),12+int(11*sinf(a)));}r->RenderCircle(white,x+size/2,y+size/2,std::max(2,size/9));}
        else if(kind=="wifi"){arc(12,19,15,225,315);arc(12,19,10,225,315);arc(12,19,5,225,315);dot(12,20,1);}
        else if(kind=="bluetooth"){line(12,2,12,22);line(12,2,19,8);line(19,8,5,19);line(5,5,19,16);line(19,16,12,22);}
        else if(kind=="sun"){r->RenderCircle(white,x+size/2,y+size/2,size/5);for(int i=0;i<8;i++){float a=i*3.14159265f/4;line(12+int(7*cosf(a)),12+int(7*sinf(a)),12+int(11*cosf(a)),12+int(11*sinf(a)));}}
        else if(kind=="volume"){box(2,9,5,6);line(7,9,13,4);line(13,4,13,20);line(13,20,7,15);arc(13,12,5,-50,50);arc(13,12,9,-50,50);}
        else if(kind=="lock"){box(5,10,14,12);arc(12,8,4,180,360);line(8,8,8,10);line(16,8,16,10);dot(12,16,1);}
        else if(kind=="power"){arc(12,13,9,-53,233);line(12,1,12,12);}
        else if(kind=="bell"){arc(12,10,6,180,360);line(6,10,6,15);line(18,10,18,15);line(6,15,3,19);line(18,15,21,19);line(3,19,21,19);arc(12,19,3,0,180);}
        else if(kind=="moon"){arc(12,12,10,38,280);arc(17,7,8,85,205);}
        else if(kind=="album"){box(2,3,20,18);line(3,19,10,10);line(10,10,15,16);line(15,16,19,11);dot(17,7,2);}
        else if(kind=="battery"){box(2,6,18,12);r->RenderRectangleFill(white,x+size*20/24,y+size*10/24,size*2/24,size*4/24);}
        else if(kind=="storage"){box(3,5,18,14);line(5,14,19,14);dot(17,17,1);}
        else if(kind=="clock"){r->RenderCircle(white,x+size/2,y+size/2,size*2/5);line(12,5,12,12);line(12,12,17,15);}
        else if(kind=="star"){for(int i=0;i<10;i++){float a=(-90+i*36)*3.14159265f/180,b=(-90+(i+1)*36)*3.14159265f/180;float ra=i%2?4.5f:10.f,rb=i%2?10.f:4.5f;line(12+int(ra*cosf(a)),12+int(ra*sinf(a)),12+int(rb*cosf(b)),12+int(rb*sinf(b)));}}
        else if(kind=="profile"){r->RenderCircle(white,x+size/2,y+size*7/24,size*4/24);arc(12,22,8,180,360);}
        else if(kind=="browser"){r->RenderCircle(white,x+size/2,y+size/2,size*10/24);line(8,16,15,6);line(15,6,16,15);line(16,15,8,16);dot(12,12,1);}
        else if(kind=="palette"){arc(12,12,10,30,330);dot(7,8,2);dot(12,5,2);dot(17,8,2);dot(7,15,2);arc(17,17,4,180,360);}
        else if(kind=="amiibo"){arc(12,12,9,10,160);arc(12,12,9,190,340);dot(12,12,3);dot(12,2,1);dot(12,22,1);}
        else if(kind=="mii"){r->RenderCircle(white,x+size/2,y+size/2,size*10/24);dot(8,9,1);dot(16,9,1);arc(12,13,5,15,165);}
        else if(kind=="folder"){box(2,7,20,14);line(3,7,3,4);line(3,4,10,4);line(10,4,13,7);line(5,11,19,11);}
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
        if(!Conservative()&&geometry_supported){
            if(SDL_RenderGeometry(rr,t->Get(),vertices,33,inds,96)==0)return;
            geometry_supported=false;UL_LOG_WARN("GlassNX: rounded geometry unavailable; using compatibility path");
        }
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
        const int step=std::max(1,corner/(Conservative()?5:10));
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
        const char *system_glyph=nullptr;
        pu::ui::Color accent{99,125,195,255};
        if(e.Is<EntryType::SpecialEntryMiiEdit>()){system_glyph="mii";accent={154,109,181,255};}
        else if(e.Is<EntryType::SpecialEntryWebBrowser>()){system_glyph="browser";accent={67,146,195,255};}
        else if(e.Is<EntryType::SpecialEntryUserPage>()){system_glyph="profile";accent={111,139,216,255};}
        else if(e.Is<EntryType::SpecialEntrySettings>()){system_glyph="settings";accent={98,114,153,255};}
        else if(e.Is<EntryType::SpecialEntryThemes>()){system_glyph="palette";accent={186,112,164,255};}
        else if(e.Is<EntryType::SpecialEntryControllers>()){system_glyph="games";accent={77,154,174,255};}
        else if(e.Is<EntryType::SpecialEntryAlbum>()){system_glyph="album";accent={91,146,210,255};}
        else if(e.Is<EntryType::SpecialEntryAmiibo>()){system_glyph="amiibo";accent={153,117,207,255};}
        else if(e.Is<EntryType::Folder>()){system_glyph="folder";accent={99,147,220,255};}
        auto icon=e.Is<EntryType::Application>()?CustomIcon(e.app_info.app_id):pu::sdl2::TextureHandle::Ref{};
        if(!icon&&!system_glyph)icon=entries->GlassIcon(index,icon_budget);
        if(!icon && e.Is<EntryType::Application>()) icon=GetDefaultApplicationIconTexture();
        const int base_side=std::min(w-20,h-54);
        const int lift=selected&&motion&&!Conservative()?int(5*focus_lift):0;
        const int side=base_side+lift*2;
        const int ix=x+(w-side)/2,iy=y+5-lift;
        const int rounding=std::max(20,side/6);
        r->RenderRoundedRectangleFill({15,20,45,65},ix,iy+5,side,side,rounding);
        if(system_glyph){
            if(auto t=SystemSymbol(system_glyph,true))Picture(r,t,ix,iy,side,side);
            else {
            ul::glassnx::DrawSurface(r,ix,iy,side,side,rounding,{accent.r,accent.g,accent.b,180},selected);
            r->RenderRoundedRectangleFill({229,239,255,32},ix+9,iy+9,side-18,side-18,rounding-4);
            r->RenderCircleFill({22,35,72,70},ix+side/2,iy+side/2,side*33/100);
            r->RenderCircle({232,244,255,90},ix+side/2,iy+side/2,side*33/100);
            const int glyph=side*48/100;
            Glyph(r,system_glyph,ix+(side-glyph)/2,iy+(side-glyph)/2,glyph);
            }
        } else if(icon)RoundedArt(r,icon,ix,iy,side,side,rounding);
        else {Card(r,ix,iy,side,side);Glyph(r,"apps",x+w/2-34,y+h/2-55,68);}
        if(selected)r->RenderRoundedRectangle({224,236,255,225},ix-3,iy-3,side+6,side+6,rounding+3);
        const auto name=EntryName(e);
        Label(r,name,x+w/2,y+h-35,27,w-10,true);
        if(e.Is<EntryType::Application>() && Favorite(e.app_info.app_id)) {
            r->RenderCircleFill({50,49,98,220},x+w-30,y+30,23);
            Label(r,"★",x+w-30,y+11,27,0,true);
        }
        hits.push_back({x,y,w,h,[this,index](){
            const bool from_dock=dock_focus>=0;
            dock_focus=-1;
            const auto list=List();const auto it=std::find(list.begin(),list.end(),index);
            if(it==list.end())return;
            const int next=static_cast<int>(it-list.begin());
            if(next!=focus||from_dock){focus=next;return;}
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
        if(now<next_metrics||Conservative())return;
        next_metrics=now+std::chrono::seconds(page==Page::Control?8:30);
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
        next_metrics=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    }
    void GlassShell::Slider(Renderer &r,int x,int y,int w,int h,const std::string &name,float value,bool selected) {
        Card(r,x,y,w,h,selected);
        Label(r,name,x+w/2,y+22,30,w-18,true);
        Label(r,value<0?"N/A":std::to_string(int(value*100+.5f))+"%",x+w/2,y+70,30,0,true);
        const int left=x+46,top=y+115,width=w-92,height=h-166;
        const int track_radius=std::max(1,std::min(width,height)/2-2);
        r->RenderRoundedRectangleFill({15,24,52,170},left,top,width,height,track_radius);
        r->RenderRectangleFill({15,24,52,255},left,top+track_radius,width,height-2*track_radius);
        r->RenderRoundedRectangle({217,232,255,80},left,top,width,height,track_radius);
        if(value>=0) {
            const int amount=int(height*std::clamp(value,0.f,1.f));
            if(amount>0){
                const int fw=width-6,fx=left+3,fy=top+height-amount;
                const auto fill=name=="Brightness"?pu::ui::Color{250,236,196,255}:pu::ui::Color{212,229,255,255};
                const int radius=std::min(fw/2-2,amount/2-1);
                // SDL_gfx can omit the stem at an exact half-width radius.
                if(radius>1){
                    r->RenderRoundedRectangleFill(fill,fx,fy,fw,amount,radius);
                    if(amount>2*radius)r->RenderRectangleFill(fill,fx,fy+radius,fw,amount-2*radius);
                }else r->RenderRectangleFill(fill,fx,fy,fw,amount);
                if(amount>24)r->RenderRoundedRectangle({255,255,255,120},fx+2,fy+2,fw-4,amount-4,std::max(1,radius-2));
            }
        }
        Glyph(r,name=="Brightness"?"sun":"volume",x+w/2-22,y+h-44,44);
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
                        row("Controller navigation","L/R pages, ZL/ZR Control Center",noop);
                        row("Dock center shortcut",std::string(DockShortcutNames[dock_shortcut])+"  ·  A to change",[this](){CycleDockShortcut();});break;
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
                        row("Installed themes","Theme manager",[](){ShowThemesMenu();});
                        row("Artwork & Discord","Optional integrations",[](){
                            g_MenuApplication->DisplayDialog("GlassNX integrations","SteamGridDB: select a game, press right stick, choose Change artwork. Enter your API key once.\nDiscord: optional external presence companion; not connected in this version.",{"OK"},true);
                        });
                        row("Export title list","For artwork imports",[this](){
                            auto fp=fopen("sdmc:/glassnx/titles.tsv.tmp","wb");
                            if(!fp){g_MenuApplication->ShowNotification("Could not export title list");return;}
                            bool ok=true;
                            for(auto &e:entries->GetEntries())if(e.Is<EntryType::Application>()){
                                auto name=EntryName(e);for(auto &ch:name)if(ch=='\t'||ch=='\n'||ch=='\r')ch=' ';
                                if(fprintf(fp,"%016llx\t%s\n",static_cast<unsigned long long>(e.app_info.app_id),name.c_str())<0)ok=false;
                            }
                            if(fclose(fp)!=0)ok=false;
                            if(ok&&rename("sdmc:/glassnx/titles.tsv.tmp","sdmc:/glassnx/titles.tsv")==0)
                                g_MenuApplication->ShowNotification("Exported current folder to /glassnx/titles.tsv");
                            else {remove("sdmc:/glassnx/titles.tsv.tmp");g_MenuApplication->ShowNotification("Title export failed");}
                        });break;
                    case 7:
                        row("GlassNX","Version 0.26",[](){ShowAboutDialog();});
                        row("Backend","Based on XorTroll's uLaunch",noop);
                        row("Safe rendering",safe_mode?"Enabled":"Disabled",[this](){ToggleSafeMode();});
                        row("Enhanced GPU glass",enhanced_glass?"Enabled (experimental)":"Disabled",[this](){ToggleEnhancedGlass();});
                        row("Performance overlay",diagnostics?"Enabled":"Disabled",[this](){diagnostics=!diagnostics;frame_tick={};display_fps=display_max=0;});
                        row("Recovery","Legacy interface",legacy);break;
                }
                return c;
            }
            c={{466,480,1360,90,"Glass transparency",std::to_string(glass)+"%",[this](){glass=glass>=85?35:glass+10;ClearMaterials();SavePreferences();}},
               {466,580,1360,90,"Blur strength",std::to_string(blur)+"%",[this](){blur=blur>=100?0:std::min(100,blur+20);ClearMaterials();SavePreferences();}},
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
            c={{430,905,510,90,"Clear alerts","",[this](){
                   hub_mode=4;hub_focus=0;hub_id=0;hits.clear();touched=true;
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
        if(gallery.active){GalleryInput(down,touch);return;}
        if(hub_mode){HubInput(down,touch);return;}
        if(close_sheet){CloseSheetInput(down,touch);return;}
        if(keyboard_open){KeyboardInput(down,touch);return;}
        if(touch.IsEmpty()) {
            touched=false;drag_slider=-1;
        } else if(touched && drag_slider>=0 && page==Page::Control) {
            const auto now=std::chrono::steady_clock::now();
            if(now-slider_tick>=std::chrono::milliseconds(200)) {
                slider_tick=now;
                const float value=std::clamp(float(679-touch.y)/309.f,0.f,1.f);
                if(std::abs(value-(drag_slider==0?brightness:volume))>.025f)AdjustSlider(drag_slider==0,value);
            }
            return;
        } else if(!touched) {
            touched=true;
            if(page==Page::Settings && setting_section==0) {
                if(touch.HitsRegion(1216,480,530,90)){glass=std::clamp(int((touch.x-1216)*100/530),35,85);ClearMaterials();SavePreferences();return;}
                if(touch.HitsRegion(1216,580,530,90)){blur=std::clamp(int((touch.x-1216)*100/530),0,100);ClearMaterials();SavePreferences();return;}
            }
            if(page==Page::Control) {
                if(touch.HitsRegion(780,255,240,475)){focus=0;drag_slider=0;slider_tick=std::chrono::steady_clock::now();AdjustSlider(true,float(679-touch.y)/309.f);return;}
                if(touch.HitsRegion(1040,255,240,475)){focus=1;drag_slider=1;slider_tick=std::chrono::steady_clock::now();AdjustSlider(false,float(679-touch.y)/309.f);return;}
            }
            for(const auto &hit:hits)if(touch.HitsRegion(hit.x,hit.y,hit.w,hit.h)) {auto action=hit.action;action();return;}
        }
        if(down&HidNpadButton_Minus){legacy();return;}
        if(down&(HidNpadButton_ZL|HidNpadButton_ZR)){if(page==Page::Control)Back();else SetPage(Page::Control);return;}
        if(down&HidNpadButton_Plus){if(page==Page::Notifications)Back();else SetPage(Page::Notifications);return;}
        if(down&HidNpadButton_B){if(dock_focus>=0){dock_focus=-1;return;}Back();return;}
        if(page==Page::Notifications) {
            if(down&(HidNpadButton_Up|HidNpadButton_StickLUp)){ScrollAlerts(-1);return;}
            if(down&(HidNpadButton_Down|HidNpadButton_StickLDown)){ScrollAlerts(1);return;}
        }
        if(down&HidNpadButton_L){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+3)%4));return;}
        if(down&HidNpadButton_R){int p=static_cast<int>(page);SetPage(static_cast<Page>(p>=4?0:(p+1)%4));return;}
        if(down&HidNpadButton_Y){
            if(page==Page::Home){dock_focus=-1;Search();return;}
            if(page==Page::Notifications){g_MenuApplication->SetQuietMode(!g_MenuApplication->IsQuietMode());return;}
            if(page==Page::Settings){Section((setting_section+1)%8);return;}
            if(page==Page::Library || page==Page::Apps){
                if(page==Page::Library&&filter==4){filter=0;search.clear();}
                else filter=(filter+1)%4;
                focus=0;offset=0;
            }return;
        }
        if((page==Page::Home||page==Page::Library||page==Page::Apps) && (down&HidNpadButton_StickR)){if(dock_focus<0)TitleOptions();return;}
        if(down&HidNpadButton_X) {
            if(page==Page::Home && g_GlobalSettings.IsSuspended()){OpenCloseSheet();return;}
            if(page==Page::Home){OpenProfileOverview();return;}
            if(page==Page::Library||page==Page::Apps){Search();return;}
        }
        if(page==Page::Home){
            if(dock_focus>=0){
                if(down&(HidNpadButton_Up|HidNpadButton_StickLUp)){dock_focus=-1;return;}
                if(down&(HidNpadButton_Left|HidNpadButton_StickLLeft)){dock_focus=std::max(0,dock_focus-1);return;}
                if(down&(HidNpadButton_Right|HidNpadButton_StickLRight)){dock_focus=std::min(4,dock_focus+1);return;}
                if(down&HidNpadButton_A){ActivateDock(dock_focus);return;}
                if(down&(HidNpadButton_Down|HidNpadButton_StickLDown))return;
            }else if(down&(HidNpadButton_Down|HidNpadButton_StickLDown)){dock_focus=0;return;}
        }
        if((page==Page::Home||page==Page::Library) && (down&HidNpadButton_StickL)){if(dock_focus<0)ToggleFavorite();return;}
        if(page==Page::Settings && (down&HidNpadButton_X)){SetTheme((theme+1)%4);return;}
        if(page==Page::Settings&&setting_section==0&&focus<2&&(down&(HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_StickLLeft|HidNpadButton_StickLRight))){
            const int delta=(down&(HidNpadButton_Right|HidNpadButton_StickLRight))?5:-5;
            if(focus==0)glass=std::clamp(glass+delta,35,85);else blur=std::clamp(blur+delta,0,100);
            ClearMaterials();SavePreferences();return;
        }
        if(page==Page::Control && focus<2 && (down&(HidNpadButton_Left|HidNpadButton_Right|HidNpadButton_StickLLeft|HidNpadButton_StickLRight))) {
            const float delta=(down&(HidNpadButton_Right|HidNpadButton_StickLRight))?.05f:-.05f;
            AdjustSlider(focus==0,(focus==0?brightness:volume)+delta);
            return;
        }
        if(page==Page::Control){
            // Focus follows the visible shortcut grid, not the underlying vector order.
            // Sliders reserve Left/Right for value adjustment above.
            constexpr int routes[10][4]={
                {0,0,4,1},{1,1,0,9},{2,3,2,5},{2,4,3,6},{3,0,4,7},
                {5,6,2,8},{5,7,3,8},{6,1,4,8},{8,9,6,8},{8,9,1,9}};
            focus=std::clamp(focus,0,9);
            if(down&(HidNpadButton_Left|HidNpadButton_StickLLeft))focus=routes[focus][0];
            else if(down&(HidNpadButton_Right|HidNpadButton_StickLRight))focus=routes[focus][1];
            else if(down&(HidNpadButton_Up|HidNpadButton_StickLUp))focus=routes[focus][2];
            else if(down&(HidNpadButton_Down|HidNpadButton_StickLDown))focus=routes[focus][3];
            else if(down&HidNpadButton_A)Activate();
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
        if(!geometry_supported||SDL_RenderGeometry(rr,nullptr,v,6,inds,12)!=0){
            // Small strip-based gradient on backends lacking geometry.
            for(int i=0;i<20;++i){const int left=x+i*w/20,right=x+(i+1)*w/20;
                const auto a=static_cast<Uint8>(225*(1.f-i/20.f));SDL_SetRenderDrawColor(rr,14,18,40,a);
                SDL_Rect strip{left,y,right-left,h};SDL_RenderFillRect(rr,&strip);}
        }
        SDL_SetRenderDrawBlendMode(rr,old);
    }
    void GlassShell::BatteryGlyph(Renderer &r,int x,int y,int size) {
        const auto pct=std::clamp(int(os::GetBatteryLevel()),0,100);
        const int width=size*4/5,height=size/2;
        r->RenderRoundedRectangle({240,245,255,200},x,y+size/4,width,height,5);
        r->RenderRectangleFill({240,245,255,200},x+width+2,y+size*3/8,3,size/4);
        const int fill=(width-8)*pct/100;
        if(fill>0){
            const auto color=pct<=15?pu::ui::Color{255,122,138,255}:pu::ui::Color{235,243,255,245};
            const int radius=std::min(4,std::min(fill,height-8)/2-1);
            if(radius>0)r->RenderRoundedRectangleFill(color,x+4,y+size/4+4,fill,height-8,radius);
            else r->RenderRectangleFill(color,x+4,y+size/4+4,fill,height-8);
        }
    }
    void GlassShell::OnRender(Renderer &r,const s32,const s32) {
        hits.clear();icon_budget=true;material_budget=2;symbol_budget=3;
        const bool conservative=Conservative();
        if(conservative!=last_conservative){
            last_conservative=conservative;PrepareTransition();
            if(!conservative)next_metrics={};
        }
        UpdateMotion();FrameDiagnostics();
        if(page!=Page::Settings||setting_section!=0)theme_previews.clear();
        if(enhanced_glass&&!Conservative()) {
            const auto wallpaper=ActiveWallpaper();const auto tint=WallpaperTint();
            liquid_glass.Prepare(pu::ui::render::GetMainRenderer(),wallpaper?wallpaper->Get():nullptr,blur,{tint.r,tint.g,tint.b,tint.a});
        } else if(liquid_glass.IsReady())liquid_glass.Release();
        if(page==Page::Home || page==Page::Control || (page==Page::Settings && setting_section==5))ReadMetrics();
        Picture(r,ActiveWallpaper(),0,0,1920,1080);
        const auto scrim=WallpaperTint();if(scrim.a)r->RenderRectangleFill(scrim,0,0,1920,1080);
        if(gallery.active){RenderGallery(r);return;}
        if(hub_mode){RenderHub(r);return;}
        RoundedArt(r,GetSelectedUserIconTexture(),44,24,76,76,37);
        r->RenderCircle({228,238,255,135},82,62,39);
        Label(r,"GlassNX",132,25,45,380);
        const char *contexts[]={"Your space","Game collection","Apps & utilities","Personalize","Quick controls","Activity"};
        Label(r,safe_mode?"SAFE MODE":contexts[static_cast<int>(page)],134,76,27,395);
        hits.push_back({44,24,430,76,[this](){OpenProfileOverview();}});
        Card(r,555,25,810,78);
        const char *tabs[]={"Home","Library","Apps","Settings"};
        const char *navicons[]={"home","games","apps","settings"};
        const int pill=565+int(nav_position*198);
        r->RenderRoundedRectangleFill({171,185,241,112},pill,32,190,64,32);
        r->RenderRoundedRectangle({218,229,255,155},pill,32,190,64,32);
        r->RenderRoundedRectangleFill({247,249,255,70},pill+25,34,140,3,1);
        for(int i=0;i<4;++i)Segment(r,565+i*198,32,190,64,navicons[i],tabs[i],false,[this,i](){SetPage(static_cast<Page>(i));});
        ul::glassnx::DrawSurface(r,1460,25,390,78,35,{33,43,75,80});
        Label(r,Clock(),1480,43,30);
        if(g_GlobalSettings.wireless_lan_enabled)Glyph(r,"wifi",1600,40,34);
        else Label(r,"Offline",1580,43,27);
        Label(r,Battery(),1690,43,30);
        BatteryGlyph(r,1805,40,38);
        hits.push_back({1470,25,380,80,[this](){SetPage(Page::Control);}});
        if(diagnostics){
            char stats[90]={};snprintf(stats,sizeof(stats),"%.1f FPS | %.0f%% late",display_fps,display_late);
            r->RenderRoundedRectangleFill({16,22,45,215},1420,102,430,45,18);
            Label(r,stats,1435,107,27,400);
        }

        if(page==Page::Home) {
            auto list=List();
            focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/5)*5;
            Card(r,70,140,1160,435);
            Card(r,1260,140,590,205);Card(r,1260,365,590,210);
            Glyph(r,"battery",1298,167,32);Label(r,"Device glance",1345,166,30);
            hits.push_back({1260,140,590,205,[this](){SetPage(Page::Control);}});
            hits.push_back({1260,365,590,210,[this](){SetPage(Page::Settings);Section(5);}});
            Label(r,Battery(),1300,214,64);
            const auto pct=std::clamp(int(os::GetBatteryLevel()),0,100);
            r->RenderRoundedRectangleFill({25,32,70,130},1300,302,500,12,6);
            if(pct>0){
                const auto color=pct<=15?pu::ui::Color{245,131,150,230}:pu::ui::Color{165,224,207,230};
                const int width=5*pct,radius=std::min(5,width/2-1);
                if(radius>0)r->RenderRoundedRectangleFill(color,1300,302,width,12,radius);
                else r->RenderRectangleFill(color,1300,302,width,12);
            }
            Label(r,temperature<0?"Temp unavailable":std::to_string(temperature)+" C",1530,241,30,275);
            Glyph(r,"storage",1298,392,32);Label(r,"Storage",1345,391,30);
            Label(r,StorageText(),1300,446,45,500);
            if(total_bytes>0&&free_bytes>=0)Label(r,free_bytes<1073741824LL?"Low space":"SD card",1620,405,27,200);
            if(total_bytes>0 && free_bytes>=0) {
                r->RenderRoundedRectangleFill({32,38,74,100},1300,520,500,17,8);
                const auto used=std::clamp(1.0-double(free_bytes)/double(total_bytes),0.0,1.0);
                const int width=int(500*used);
                if(width>0){
                    const int radius=std::min(7,width/2-1);
                    const auto color=free_bytes<1073741824LL?pu::ui::Color{246,156,141,235}:pu::ui::Color{235,240,255,225};
                    if(radius>0)r->RenderRoundedRectangleFill(color,1300,520,width,17,radius);
                    else r->RenderRectangleFill(color,1300,520,width,17);
                }
            }
            if(!list.empty()) {
                const auto idx=list[focus];auto &e=entries->GetEntries()[idx];
                const bool suspended=g_GlobalSettings.IsEntrySuspended(e);
                auto wide=e.Is<EntryType::Application>()&&!Conservative()?Banner(e.app_info.app_id):pu::sdl2::TextureHandle::Ref{};
                auto icon=e.Is<EntryType::Application>()?CustomIcon(e.app_info.app_id):pu::sdl2::TextureHandle::Ref{};
                if(!icon)icon=entries->GlassIcon(idx,icon_budget);
                if(!icon&&e.Is<EntryType::Application>())icon=GetDefaultApplicationIconTexture();
                if(wide) {
                    RoundedArt(r,wide,85,155,1130,405,32);

                } else if(icon) {
                    // Installed icon as artwork-derived backdrop, not invented wide banner.
                    auto tex=icon->Get();
                    if(tex&&!Conservative()) {
                        Uint8 rr=255,gg=255,bb=255;
                        SDL_GetTextureColorMod(tex,&rr,&gg,&bb);
                        SDL_SetTextureColorMod(tex,102,108,146);
                        Cover(r,icon,105,175,1090,365);
                        SDL_SetTextureColorMod(tex,rr,gg,bb);

                    }
                    RoundedArt(r,icon,805,165,390,390,58);
                }
                if(wide&&hero_frost&&!Conservative())RoundedArt(r,hero_frost,98,295,660,248,38);
                ul::glassnx::DrawSurface(r,98,295,660,248,38,{20,28,52,static_cast<u8>(wide&&hero_frost?132:168)});
                if(suspended){
                    r->RenderRoundedRectangleFill({94,156,139,205},122,320,14,14,9);
                    Label(r,"CONTINUE PLAYING",150,310,27,570);
                }else Label(r,"SELECTED GAME",122,310,27);
                Label(r,EntryName(e),122,350,45,610);
                Label(r,suspended?"Suspended":(e.Is<EntryType::Application>() && std::find(recent.begin(),recent.end(),e.app_info.app_id)!=recent.end()?"Recently played":"Ready to play"),122,410,27,610);
                Button(r,112,445,250,78,suspended?"Resume":"Play",true,[this,idx](){launch(idx);});
                if(suspended) {
                    ul::glassnx::DrawSurface(r,380,445,300,78,38,{144,62,96,180});
                    Hint(r,400,465,260,"X","Close game");
                    hits.push_back({380,445,300,78,[this](){OpenCloseSheet();}});
                } else Button(r,380,445,300,78,"Game Hub",false,[this](){TitleOptions();});
            } else {
                Label(r,"Your home awaits",112,245,64);
                Label(r,"Add games and organize your collection.",112,355,30);
                Button(r,112,445,360,78,"Manage entries",false,legacy);
            }
            Label(r,"Game shelf",75,600,37);
            if(dock_focus>=0){Hint(r,385,599,160,"A","Open");Hint(r,555,599,190,"UP","Games");Hint(r,755,599,180,"B","Back");}
            else {Hint(r,385,599,260,"RS","Game Hub");Hint(r,660,599,200,"DOWN","Dock");}
            ul::glassnx::DrawSurface(r,1000,590,240,52,24,{29,32,79,62});
            Hint(r,1012,597,215,"Y","Search");
            hits.push_back({1000,590,240,52,[this](){dock_focus=-1;Search();}});
            Label(r,std::to_string(list.size())+" titles",1635,606,27,205);
            Hint(r,1260,599,300,"X",g_GlobalSettings.IsSuspended()?"Close game":"Profile");
            for(int i=0;i<5&&offset+i<static_cast<int>(list.size());++i)Tile(r,list[offset+i],70+i*360,648,340,250,dock_focus<0&&focus==offset+i);
            const int pages=int((list.size()+4)/5);
            if(pages>9){
                const int track=160,thumb=std::max(12,track/pages);
                r->RenderRoundedRectangleFill({227,235,255,65},880,906,track,6,2);
                r->RenderRoundedRectangleFill({227,235,255,230},880+(track-thumb)*(offset/5)/(pages-1),906,thumb,6,2);
            }else for(int i=0;i<pages;++i)r->RenderCircleFill({227,235,255,static_cast<u8>(i==offset/5?230:80)},960-(pages-1)*9+i*18,909,4);
            RenderDock(r);
        } else if(page==Page::Library || page==Page::Apps) {
            const bool games=page==Page::Library;
            const int capacity=games?10:12;
            auto list=List();focus=std::clamp(focus,0,std::max(0,static_cast<int>(list.size())-1));offset=(focus/capacity)*capacity;
            const bool all_entries=games&&filter==4;
            Label(r,all_entries?"Search results":games?"Game Library":"App Library",70,125,64);
            Label(r,all_entries?std::to_string(list.size())+" matches in this folder":games?"Your collection":"Homebrew, media and utilities",72,208,30);
            IconButton(r,1410,145,435,76,"search",search.empty()?"Search":search,false,[this](){Search();});
            const char *gameTabs[]={"All Games","Favorites","Collections","Recently Played"};
            const char *appTabs[]={"Apps","Homebrew","Tools","Media"};
            if(!games)for(int i=0;i<4;i++)Button(r,70+i*347,275,330,75,games?gameTabs[i]:appTabs[i],filter==i,[this,i](){filter=i;focus=offset=0;});
            if(!games)Button(r,1475,275,370,75,sort_name?"A-Z":"Default order",false,[this](){sort_name=!sort_name;focus=offset=0;SavePreferences();});
            if(games) {
                Card(r,40,275,340,680);
                const char *icons[]={"games","star","storage","clock"};
                if(all_entries){
                    Glyph(r,"search",70,320,44);Label(r,"All entries",125,320,30,230);
                    Label(r,"Games, apps",70,400,30,285);Label(r,"and folders",70,440,30,285);
                    Button(r,55,535,310,75,"Clear search",false,[this](){filter=0;search.clear();focus=offset=0;});
                }else for(int i=0;i<4;i++)Segment(r,55,310+i*95,310,72,icons[i],gameTabs[i],filter==i,[this,i](){filter=i;focus=offset=0;});
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
                Tile(r,list[offset+i],(games?400:88)+(i%columns)*(games?290:293),(games?320:389)+(i/columns)*(games?310:280),275,games?292:270,focus==offset+i);
            }
            if(all_entries){Hint(r,70,1000,290,"Y","Clear search");Hint(r,380,1000,280,"X","Edit search");Hint(r,680,1000,220,"B","Home");Hint(r,920,1000,200,"A","Open");}
            else {Hint(r,70,1000,240,"LR","Pages");Hint(r,320,1000,190,"Y","Filter");Hint(r,520,1000,190,"X","Search");
                if(games){Hint(r,720,1000,240,"LS","Favorite");Hint(r,980,1000,240,"RS","Game Hub");Hint(r,1240,1000,200,"A","Open");}
                else {Hint(r,720,1000,240,"RS","Game Hub");Hint(r,980,1000,200,"A","Open");}
            }
            if(static_cast<int>(list.size())>capacity)Label(r,std::to_string(offset/capacity+1)+"/"+std::to_string((list.size()+capacity-1)/capacity),1720,1005,30);
        } else if(page==Page::Settings || page==Page::Control || page==Page::Notifications) {
            const bool settings=page==Page::Settings,control=page==Page::Control;
            Label(r,settings?"Settings":control?"Quick controls":"Activity",70,137,64);
            if(!settings&&!control){
                const bool quiet=g_MenuApplication->IsQuietMode();
                IconButton(r,1420,140,400,76,"moon",quiet?"Quiet on":"Quiet off",quiet,[](){g_MenuApplication->SetQuietMode(!g_MenuApplication->IsQuietMode());});
                Hint(r,1430,211,385,"Y","Toggle quiet mode");
            }
            if(settings) {
                Card(r,70,210,360,760);
                const char *sections[]={"Appearance","System","Controls","Network","Power","Storage","Homebrew","About"};
                const char *icons[]={"settings","settings","games","wifi","power","storage","apps","bell"};
                for(int i=0;i<8;i++)Segment(r,90,240+i*81,320,70,icons[i],sections[i],setting_section==i,[this,i](){Section(i);});
                Hint(r,95,890,290,"Y","Categories");
                Label(r,sections[setting_section],466,205,45);
                if(setting_section==0) {
                    Label(r,"Theme",466,265,30);
                    for(int i=0;i<4;i++) {
                        const int x=466+i*344;
                        auto it=theme_previews.find(i);
                        if(it==theme_previews.end())it=theme_previews.emplace(i,LoadWallpaper(i,true)).first;
                        RoundedArt(r,it->second?it->second:GetBackgroundTexture(),x+6,321,308,113,22);
                        if(!it->second&&Tints[i].a)r->RenderRoundedRectangleFill(Tints[i],x+6,321,308,113,22);
                        if(theme==i)r->RenderRoundedRectangle({233,240,255,255},x+3,318,314,119,24);
                        Label(r,Themes[i],x+160,443,27,305,true);
                        hits.push_back({x,315,320,125,[this,i](){SetTheme(i);}});
                    }
                } else Card(r,450,280,1390,670);
            } else if(control) {
                Card(r,70,225,695,505);Card(r,1310,255,510,355);
                Label(r,"Connections & shortcuts",100,235,27,635);
                Label(r,"Display & sound",790,212,27,490);
                Glyph(r,"games",1340,281,42);Label(r,"System state",1398,280,37,390);
                const bool suspended=g_GlobalSettings.IsSuspended();
                Label(r,suspended?"Game suspended":"Shell active",1340,351,45,455);
                Label(r,safe_mode?"Safe rendering":suspended?"Conservative rendering":enhanced_glass?(liquid_glass.IsReady()?"Enhanced glass (experimental)":"Enhanced glass requested"):"Standard Prism rendering",1340,422,30,455);
                Label(r,"Clock control unavailable",1340,490,27,455);
                Card(r,780,770,500,175);Label(r,"Battery",810,790,30);Label(r,Battery(),810,842,64);
                Card(r,1310,770,510,175);Label(r,"Temperature",1340,790,30);Label(r,temperature<0?"Unavailable":std::to_string(temperature)+" C",1340,842,45);
                Label(r,"Fan: unavailable",1340,909,27);
            } else {
                const auto &history=g_MenuApplication->GetNotificationHistory();
                if(history.empty()) {
                    Card(r,430,270,1060,310);
                    Glyph(r,"bell",926,315,68);
                    Label(r,"You're all caught up.",960,415,45,0,true);
                    Label(r,"System alerts will appear here.",960,485,30,0,true);
                    if(g_MenuApplication->IsQuietMode())Label(r,"Quiet mode is enabled",960,535,27,0,true);
                }
                const int total=static_cast<int>(history.size());
                alert_offset=std::clamp(alert_offset,0,std::max(0,total-4));
                for(int i=0;i<4&&alert_offset+i<total;++i) {
                    int y=245+i*155;Card(r,430,y,1060,130);
                    const auto message=history[total-1-alert_offset-i];
                    const bool battery_alert=message.rfind("Low battery:",0)==0;
                    r->RenderRoundedRectangleFill(battery_alert?pu::ui::Color{196,108,129,165}:pu::ui::Color{124,153,219,125},454,y+25,64,64,20);
                    Glyph(r,battery_alert?"battery":"bell",465,y+35,42);
                    Label(r,battery_alert?"Battery":"GlassNX",545,y+12,27);
                    Label(r,message,545,y+53,37,900);
                    hits.push_back({430,y,1060,130,[message](){g_MenuApplication->DisplayDialog("GlassNX notification",message,{"Done"},true);}});
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
                    const auto accent=i==2?pu::ui::Color{91,174,242,225}:i==3?pu::ui::Color{128,145,242,225}:pu::ui::Color{170,138,228,225};
                    const int ix=c.x+c.w/2-46,iy=c.y+19;
                    ul::glassnx::DrawSurface(r,ix,iy,92,92,30,on?accent:pu::ui::Color{45,58,99,150},focus==static_cast<int>(i));
                    if(on)r->RenderCircleFill({225,250,239,235},ix+80,iy+12,6);
                    Glyph(r,icons[i-2],c.x+c.w/2-26,c.y+39,52);
                    Label(r,c.label,c.x+c.w/2,c.y+117,27,c.w-14,true);
                    if(!c.detail.empty())Label(r,c.detail,c.x+c.w/2,c.y+148,27,c.w-14,true);
                    hits.push_back({c.x,c.y,c.w,c.h,[this,i,action=c.action](){focus=static_cast<int>(i);action();}});continue;
                }
                if(settings) {
                    const bool selected=focus==static_cast<int>(i);
                    ul::glassnx::DrawSurface(r,c.x,c.y,c.w,c.h,26,selected?pu::ui::Color{132,155,223,110}:pu::ui::Color{31,37,76,65},selected);
                } else Card(r,c.x,c.y,c.w,c.h,focus==static_cast<int>(i));
                Label(r,c.label,c.x+25,c.y+12,37,c.w-50);
                Label(r,c.detail,c.x+25,c.y+(settings?51:65),27,c.w-50);
                if(settings && setting_section==0 && i<2) {
                    if(focus==static_cast<int>(i))Hint(r,c.x+1010,c.y+5,290,"LRD","Adjust");
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
                hits.push_back({c.x,c.y,c.w,c.h,[this,i,action=c.action](){focus=static_cast<int>(i);action();}});
            }
            Hint(r,70,1000,190,"A","Select");
            if(control){
                Hint(r,275,1000,355,"LRD",focus<2?"Adjust slider":"Navigate");
                Hint(r,650,1000,290,"UD","Navigate");Hint(r,960,1000,200,"B","Back");
            }else{
                Hint(r,275,1000,190,"B","Back");Hint(r,485,1000,410,"ZLZR","Quick controls");Hint(r,915,1000,270,"+","Activity");
            }
        }
        if(close_sheet){RenderCloseSheet(r);return;}
        if(keyboard_open){RenderKeyboard(r);return;}
        if(motion&&!Conservative()) {
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-transition).count();
            if(elapsed>=0 && elapsed<260) {
                const float p=static_cast<float>(elapsed)/260.f;
                const float t=1.f-p*p*(3.f-2.f*p);
                r->RenderRectangleFill({47,42,91,static_cast<u8>(58*t)},0,110,1920,970);
            }
        }
    }
}
