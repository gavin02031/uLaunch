#include <ul/menu/ui/ui_StartupMenuLayout.hpp>
#include <algorithm>
#include <ul/glassnx/Surface.hpp>
#include <cmath>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/fs/fs_Stdio.hpp>
#include <ul/acc/acc_Accounts.hpp>
#include <ul/menu/smi/smi_Commands.hpp>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {

    class StartupMenuLayout::ProfileCards : public pu::ui::elm::Element {
        public:
            struct Profile {
                AccountUid uid{};
                pu::sdl2::TextureHandle::Ref icon;
                pu::ui::elm::TextBlock::Ref name;
                bool add=false;
            };
            std::vector<Profile> profiles;
            int selected=0;
            static constexpr int PerPage=4, CardW=320, CardH=345, Gap=35, Top=365;
            s32 GetX() override{return 0;}
            s32 GetY() override{return 0;}
            s32 GetWidth() override{return 1920;}
            s32 GetHeight() override{return 1080;}
            int Start() const{return selected/PerPage*PerPage;}
            int Count() const{return std::min(PerPage,int(profiles.size())-Start());}
            int Left() const{return (1920-(Count()*CardW+(Count()-1)*Gap))/2;}
            void Move(int delta){if(!profiles.empty())selected=std::clamp(selected+delta,0,int(profiles.size())-1);}
            int Hit(pu::ui::TouchPoint t) const {
                for(int i=0;i<Count();++i)if(t.HitsRegion(Left()+i*(CardW+Gap),Top,CardW,CardH))return Start()+i;
                return -1;
            }
            static void Avatar(const pu::sdl2::TextureHandle::Ref &icon,int x,int y,int size){
                if(!icon||!icon->Get())return;
                int w=0,h=0;
                auto tex=icon->Get();if(SDL_QueryTexture(tex,nullptr,nullptr,&w,&h)!=0||w<=0||h<=0)return;
                auto rr=pu::ui::render::GetMainRenderer();
                const int side=std::min(w,h),r=size/2;
                for(int a=0;a<size;a+=6){
                    const int b=std::min(size,a+6);
                    const float dy=(a+b)*.5f-r;
                    const int inset=std::max(0,int(std::ceil(r-std::sqrt(std::max(0.f,float(r*r)-dy*dy)))));
                    if(size<=2*inset)continue;
                    const int sx=(w-side)/2+inset*side/size,sy=(h-side)/2+a*side/size;
                    SDL_Rect src{sx,sy,std::max(1,(size-2*inset)*side/size),std::max(1,(b-a)*side/size)};
                    SDL_Rect dst{x+inset,y+a,size-2*inset,b-a};
                    SDL_RenderCopy(rr,tex,&src,&dst);
                }
            }
            void OnRender(pu::ui::render::Renderer::Ref &r,const s32,const s32) override {
                if(profiles.empty())return;
                for(int i=0;i<Count();++i){
                    auto &profile=profiles[Start()+i];
                    const int x=Left()+i*(CardW+Gap),y=Top;
                    const bool active=Start()+i==selected;
                    ul::glassnx::DrawSurface(r,x,y,CardW,CardH,48,active?pu::ui::Color{149,170,229,155}:pu::ui::Color{51,59,103,125},active);
                    r->RenderCircleFill({221,232,255,active?static_cast<u8>(125):static_cast<u8>(65)},x+160,y+138,105);
                    r->RenderCircleFill({47,57,104,210},x+160,y+138,96);
                    if(profile.icon)Avatar(profile.icon,x+68,y+46,184);
                    else if(profile.add){
                        r->RenderRoundedRectangleFill({245,248,255,245},x+125,y+132,70,12,6);
                        r->RenderRoundedRectangleFill({245,248,255,245},x+154,y+103,12,70,6);
                    }
                    if(profile.name)profile.name->OnRender(r,x+160-profile.name->GetWidth()/2,y+264);
                    if(active){
                        r->RenderRoundedRectangleFill({228,236,255,205},x+125,y+319,70,7,3);
                        r->RenderCircle({245,249,255,185},x+160,y+138,108);
                    }
                }
                const int pages=(int(profiles.size())+PerPage-1)/PerPage;
                for(int i=0;i<pages;++i)r->RenderCircleFill({239,245,255,static_cast<u8>(i==selected/PerPage?240:90)},960-(pages-1)*15+i*30,762,6);
            }
            void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override{}
    };

    void StartupMenuLayout::user_DefaultKey(const AccountUid uid) {
        // Note: menu loading is invoked below instead of here so that the main menu doesn't also register the button input which caused this action...
        this->load_menu = true;
        pu::audio::PlaySfx(this->user_select_sfx);
        g_GlobalSettings.SetSelectedUser(uid);

        auto &main_menu_lyt = g_MenuApplication->GetMainMenuLayout();
        if(main_menu_lyt != nullptr) {
            main_menu_lyt->NotifyNextReloadUserChanged();
        }
        g_MenuApplication->LoadMenu(MenuType::Main);
    }

    void StartupMenuLayout::create_DefaultKey() {
        pu::audio::PlaySfx(this->user_create_sfx);

        g_MenuApplication->FadeOutToLibraryApplet(AppletId_LibraryAppletMyPage);
        const auto rc=smi::OpenAddUser();
        if(R_FAILED(rc)){g_MenuApplication->FadeIn();g_MenuApplication->ShowNotification("Could not open user creation");return;}
        g_MenuApplication->Finalize();
    }

    StartupMenuLayout::StartupMenuLayout() : IMenuLayout() {
        this->load_menu = false;

        this->user_create_sfx = nullptr;
        this->user_select_sfx = nullptr;

        auto title=pu::ui::elm::TextBlock::New(0,92,"GlassNX");
        title->SetFont(pu::ui::MakeDefaultFontName(64));
        title->SetColor({255,255,255,255});
        title->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->Add(title);
        this->info_text = pu::ui::elm::TextBlock::New(0, 0, "Welcome. Choose your profile.");
        this->info_text->SetColor(g_MenuApplication->GetTextColor());
        g_GlobalSettings.ApplyConfigForElement("startup_menu", "info_text", this->info_text);
        this->Add(this->info_text);

        this->profile_cards=std::make_shared<ProfileCards>();
        this->Add(this->profile_cards);
        auto hint=pu::ui::elm::TextBlock::New(0,855,"Left / Right   Choose     A   Continue     L / R   Page");
        hint->SetFont(pu::ui::MakeDefaultFontName(30));
        hint->SetColor({235,242,255,255});hint->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->Add(hint);
        auto footer=pu::ui::elm::TextBlock::New(0,955,"Your games. Your space.");
        footer->SetFont(pu::ui::MakeDefaultFontName(37));
        footer->SetColor({235,242,255,210});footer->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->Add(footer);
    }

    void StartupMenuLayout::LoadSfx() {
        this->user_create_sfx = pu::audio::LoadSfx(TryGetActiveThemeResource("sound/Startup/UserCreate.wav"));
        this->user_select_sfx = pu::audio::LoadSfx(TryGetActiveThemeResource("sound/Startup/UserSelect.wav"));
    }
    
    void StartupMenuLayout::DisposeSfx() {
        pu::audio::DestroySfx(this->user_create_sfx);
        pu::audio::DestroySfx(this->user_select_sfx);
    }

    void StartupMenuLayout::OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        auto &grid=*this->profile_cards;
        if(grid.profiles.empty())return;
        if(keys_down&(HidNpadButton_Left|HidNpadButton_StickLLeft))grid.Move(-1);
        if(keys_down&(HidNpadButton_Right|HidNpadButton_StickLRight))grid.Move(1);
        if(keys_down&HidNpadButton_L)grid.Move(-ProfileCards::PerPage);
        if(keys_down&HidNpadButton_R)grid.Move(ProfileCards::PerPage);
        bool activate=(keys_down&HidNpadButton_A)!=0;
        if(touch_pos.IsEmpty())touch_latched=false;
        else if(!touch_latched){
            touch_latched=true;
            const int hit=grid.Hit(touch_pos);
            if(hit>=0){if(hit==grid.selected)activate=true;else grid.selected=hit;}
        }
        if(activate){
            const auto chosen=grid.profiles.at(grid.selected);
            if(chosen.add)this->create_DefaultKey();else this->user_DefaultKey(chosen.uid);
        }
    }

    bool StartupMenuLayout::OnHomeButtonPress() {
        // ...
        return true;
    }

    void StartupMenuLayout::ReloadMenu() {
        auto &grid=*this->profile_cards;
        grid.profiles.clear();grid.selected=0;
        // Require a fresh touch when entering the profile picker from another screen.
        touch_latched=true;
        auto label=[](const std::string &name){
            auto t=pu::ui::elm::TextBlock::New(0,0,name);
            t->SetFont(pu::ui::MakeDefaultFontName(37));t->SetColor({248,250,255,255});t->SetClampWidth(280);
            return t;
        };
        std::vector<AccountUid> user_ids;
        const auto rc=acc::ListAccounts(user_ids);
        if(R_SUCCEEDED(rc))for(const auto &uid:user_ids){
            std::string name;
            if(R_FAILED(acc::GetAccountName(uid,name)))continue;
            u8 *buf=nullptr;size_t len=0;
            pu::sdl2::TextureHandle::Ref icon;
            if(R_SUCCEEDED(acc::LoadAccountImage(uid,buf,len))&&buf&&len){
                auto tex=pu::ui::render::LoadImageFromBuffer(buf,len);
                if(tex)icon=pu::sdl2::TextureHandle::New(tex);
            }
            delete[] buf;
            grid.profiles.push_back({uid,icon,label(name),false});
        }
        grid.profiles.push_back({{}, {}, label("Add profile"),true});
    }

}
