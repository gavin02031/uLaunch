#include <ul/man/ui/ui_MainApplication.hpp>
#include <ul/glassnx/Brand.hpp>
#include <ul/glassnx/Surface.hpp>
#include <ul/menu/menu_Cache.hpp>
#include <ul/menu/menu_Entries.hpp>
#include <ul/acc/acc_Accounts.hpp>
#include <ul/util/util_Json.hpp>
#include <ul/man/man_Manager.hpp>
#include <ul/man/man_Network.hpp>
#include <ul/man/man_Artwork.hpp>
#include <ul/cfg/cfg_Config.hpp>
#include <ul/os/os_Applications.hpp>
#include <ul/os/os_System.hpp>
#include <ul/util/util_Zip.hpp>

extern ul::man::ui::MainApplication::Ref g_MainApplication;
extern ul::util::JSON g_DefaultLanguage;
extern ul::util::JSON g_MainLanguage;

namespace ul::man::ui {

    namespace {

        class ManagerBackdrop : public pu::ui::elm::Element {
        public:
            ManagerBackdrop()=default;
            PU_SMART_CTOR(ManagerBackdrop)
            s32 GetX() override{return 0;}
            s32 GetY() override{return 0;}
            s32 GetWidth() override{return 1920;}
            s32 GetHeight() override{return 1080;}
            void OnRender(pu::ui::render::Renderer::Ref &r,const s32,const s32) override {
                // Bounded drawing only; recovery must remain independent of GPU target effects.
                for(int i=0;i<24;i++) {
                    const int y=i*45;
                    r->RenderRectangleFill({static_cast<u8>(31+i),static_cast<u8>(36+i),static_cast<u8>(79+i*2),255},0,y,1920,45);
                }
                r->RenderRoundedRectangleFill({165,132,216,36},-95,290,810,810,280);
                r->RenderRoundedRectangleFill({99,176,231,32},1250,-230,910,770,250);
                auto glass=[&](int x,int y,int w,int h) {
                    ul::glassnx::DrawSurface(r,x,y,w,h,40,{143,161,223,84});
                };
                glass(70,230,550,680);glass(665,230,1185,680);
                r->RenderRoundedRectangleFill({122,145,218,80},118,282,155,155,42);
                r->RenderRoundedRectangleFill({220,232,255,145},145,310,82,80,25);
                r->RenderRoundedRectangleFill({110,150,220,220},177,342,75,76,23);
                r->RenderRoundedRectangle({255,255,255,170},177,342,75,76,23);
                r->RenderRoundedRectangleFill({135,157,224,60},650,945,620,76,38);
            }
            void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override{}
        };

        inline std::string GetLanguageString(const std::string &name) {
            return cfg::GetLanguageString(g_MainLanguage, g_DefaultLanguage, name);
        }

        inline std::string GetStatus() {
            std::string status = "GlassNX: ";
            if(IsBasePresent()) {
                if(IsSystemActive()) {
                    status += "Enabled on SD";
                }
                else {
                    status += "Not enabled (A to enable)";
                }
            }
            else {
                status += "Installation incomplete";
            }

            return status;
        }

        inline void RebootSystem() {
            UL_RC_ASSERT(spsmInitialize());
            spsmShutdown(true);
        }


    }

    void MainMenuLayout::ResetInfoText() {
        std::string info = std::string(ul::glassnx::Manager)+" v"+ul::glassnx::Version;
        if(g_MainApplication->IsAvailable()) {
            const auto ver = g_MainApplication->GetVersion();
            info += " | HOME service version " + std::to_string((u32)ver.major) + "." + std::to_string((u32)ver.minor) + "." + std::to_string((u32)ver.micro);
            if(!g_MainApplication->IsVersionMatch()) {
                info += " (unexpected version)";
            }
        }
        this->info_text->SetText(info);
    }

    MainMenuLayout::MainMenuLayout() : pu::ui::Layout() {
        this->Add(ManagerBackdrop::New());
        auto label=[&](int x,int y,const char *value,int size) {
            auto t=pu::ui::elm::TextBlock::New(x,y,value);
            t->SetColor({238,244,255,255});t->SetFont(pu::ui::MakeDefaultFontName(size));
            this->Add(t);
        };
        label(75,48,"GlassNX",64);
        label(78,136,"System management",35);
        label(118,480,"Recovery",45);
        label(118,557,"Manage HOME",35);
        label(118,613,"Restore or repair",35);
        label(118,669,"Keep your saves",35);
        label(118,780,"Native Switch runtime",35);
        label(705,268,"System actions",45);
        label(735,965,"A  Select       +  Exit",35);
        this->info_text = pu::ui::elm::TextBlock::New(0, InfoTextY, "...");
        this->ResetInfoText();
        this->info_text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::MediumLarge));
        this->info_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->info_text->SetColor(InfoTextColor);
        this->Add(this->info_text);

        this->update_download_bar = pu::ui::elm::ProgressBar::New(0, UpdateDownloadBarY, UpdateDownloadBarWidth, UpdateDownloadBarHeight, 1.0f);
        this->update_download_bar->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->update_download_bar->SetVisible(false);
        this->Add(this->update_download_bar);

        this->options_menu = pu::ui::elm::Menu::New(695, MenuY, 1125, MenuColor, MenuFocusColor, MenuItemSize, MenuItemCount);
        
        this->activate_menu_item = pu::ui::elm::MenuItem::New(GetStatus());
        this->activate_menu_item->SetColor(MenuItemColor);
        this->activate_menu_item->AddOnKey(std::bind(&MainMenuLayout::activate_DefaultKey, this));
        this->options_menu->AddItem(this->activate_menu_item);

        this->reset_menu_menu_item = pu::ui::elm::MenuItem::New("Reset library layout");
        this->reset_menu_menu_item->SetColor(MenuItemColor);
        this->reset_menu_menu_item->AddOnKey(std::bind(&MainMenuLayout::resetMenu_DefaultKey, this));
        this->options_menu->AddItem(this->reset_menu_menu_item);

        this->reset_cache_menu_item = pu::ui::elm::MenuItem::New("Rebuild artwork cache");
        this->reset_cache_menu_item->SetColor(MenuItemColor);
        this->reset_cache_menu_item->AddOnKey(std::bind(&MainMenuLayout::resetCache_DefaultKey, this));
        this->options_menu->AddItem(this->reset_cache_menu_item);
        
        this->update_menu_item = pu::ui::elm::MenuItem::New("Update / Recovery / Safe Mode");
        this->update_menu_item->SetColor(MenuItemColor);
        this->update_menu_item->AddOnKey(std::bind(&MainMenuLayout::update_DefaultKey,this));
        this->options_menu->AddItem(this->update_menu_item);

        this->artwork_menu_item=pu::ui::elm::MenuItem::New("SteamGridDB artwork");
        this->artwork_menu_item->SetColor(MenuItemColor);
        this->artwork_menu_item->AddOnKey(std::bind(&MainMenuLayout::artwork_DefaultKey,this));
        this->options_menu->AddItem(this->artwork_menu_item);
        this->Add(this->options_menu);

        this->SetBackgroundColor(BackgroundColor);
    }

    void MainMenuLayout::activate_DefaultKey() {
        if(GlassUpdatePending()) {
            g_MainApplication->CreateShowDialog("GlassNX","Interrupted update. Restore previous version using Update / Recovery first.",{"OK"},true);
            return;
        }
        if(IsBasePresent()) {
            const bool ok=IsSystemActive()?DeactivateSystem():ActivateSystem();
            if(!ok) {
                g_MainApplication->CreateShowDialog("GlassNX","Could not change HOME override. If another exefs.nsp is installed, back it up and remove that file only before enabling GlassNX.",{"OK"},true);
                return;
            }

            this->activate_menu_item->SetName(GetStatus());
            this->ReloadMenu();

            g_MainApplication->CreateShowDialog(GetLanguageString("activate_changes_title"), GetLanguageString("activate_changes"), { GetLanguageString("reboot") }, true);
            g_MainApplication->FadeOut();
            RebootSystem();
        }
        else {
            g_MainApplication->CreateShowDialog(GetLanguageString("activate_changes_title"), GetLanguageString("activate_not_present"), { GetLanguageString("ok") }, true);
        }
    }

    void MainMenuLayout::resetMenu_DefaultKey() {
        const auto option = g_MainApplication->CreateShowDialog(GetLanguageString("reset_menu_title"), GetLanguageString("reset_menu_conf"), { GetLanguageString("yes"), GetLanguageString("cancel") }, true);
        if(option == 0) {
            const auto is_emummc = os::IsEmuMMC();

            std::vector<AccountUid> uids;
            UL_RC_ASSERT(acc::ListAccounts(uids));

            for(const auto &uid : uids) {
                const auto menu_path = menu::MakeMenuPath(is_emummc, uid);
                fs::DeleteDirectory(menu_path);
            }

            // When returning to uMenu it will automatically regenerate the menu entries

            g_MainApplication->ShowNotification(GetLanguageString("reset_menu_success"));
        }
    }

    void MainMenuLayout::resetCache_DefaultKey() {
        const auto option = g_MainApplication->CreateShowDialog(GetLanguageString("reset_cache_title"), GetLanguageString("reset_cache_conf"), { GetLanguageString("yes"), GetLanguageString("cancel") }, true);
        if(option == 0) {
            // Regenerate cache
            const auto cur_app_recs = os::ListApplicationRecords();
            menu::CacheApplications(cur_app_recs);
            menu::CacheHomebrew();

            cfg::RemoveActiveThemeCache();

            g_MainApplication->ShowNotification(GetLanguageString("reset_cache_success"));
        }
    }

    void MainMenuLayout::artwork_DefaultKey() {
        const auto titles=ReadArtworkTitles();
        if(titles.empty()){
            g_MainApplication->CreateShowDialog("SteamGridDB","Export titles from GlassNX Settings > Homebrew first.",{"OK"},true);return;
        }
        size_t page=0;
        ArtworkTitle chosen;
        for(;;){
            std::vector<std::string> options;
            const auto start=page*4,end=std::min(start+4,titles.size());
            for(size_t i=start;i<end;i++)options.push_back(titles[i].name);
            options.push_back("Next page");options.push_back("Cancel");
            const int c=g_MainApplication->CreateShowDialog("SteamGridDB","Choose a title from the exported library.",options,true);
            if(c<0||c==int(options.size())-1)return;
            if(c==int(end-start)){page=(page+1)%((titles.size()+3)/4);continue;}
            if(c>=0&&c<int(end-start)){chosen=titles[start+c];break;}
            return;
        }
        std::string err;
        std::vector<ArtworkGame> games;
        if(!FindArtworkGames(chosen.name,games,err)){
            g_MainApplication->CreateShowDialog("SteamGridDB",err,{"OK"},true);return;
        }
        std::vector<std::string> opts;
        for(const auto &game:games)opts.push_back(game.name);
        opts.push_back("Cancel");
        const int game=g_MainApplication->CreateShowDialog("Confirm match","Choose the matching SteamGridDB game.",opts,true);
        if(game<0||game>=int(games.size()))return;
        std::vector<ArtworkHero> heroes;
        if(!FindArtworkHeroes(games[game].id,heroes,err)){
            g_MainApplication->CreateShowDialog("SteamGridDB",err,{"OK"},true);return;
        }
        opts.clear();
        for(size_t i=0;i<heroes.size();i++)opts.push_back("Hero "+std::to_string(i+1)+" ("+std::to_string(heroes[i].width)+"x"+std::to_string(heroes[i].height)+")");
        opts.push_back("Cancel");
        const int hero=g_MainApplication->CreateShowDialog("Choose artwork","Hero artwork • Center crop to 226:81\nGlassNX exports 1130 x 405. Choose a sharp source.",opts,true);
        if(hero<0||hero>=int(heroes.size()))return;
        if(g_MainApplication->CreateShowDialog("Import banner","Replace the existing custom hero for "+chosen.name+"?",{"Import","Cancel"},true)!=0)return;
        const bool ok=InstallArtwork(chosen,heroes[hero],err);
        g_MainApplication->CreateShowDialog("SteamGridDB",ok?"Saved. Restart GlassNX to refresh featured artwork.":err,{"OK"},true);
    }

    void MainMenuLayout::update_DefaultKey() {
        const auto choice=g_MainApplication->CreateShowDialog("GlassNX Update & Recovery",
            "Install a complete release from /glassnx/update.zip. Close all games before updating. Do not power off during installation.",
            {"Install ZIP","Restore previous","Toggle safe mode","Cancel"},true);
        if(choice==2){
            const char *path="sdmc:/glassnx/safe-mode";
            bool ok=false;
            if(fs::ExistsFile(path))ok=remove(path)==0;
            else if(auto fp=fopen(path,"wb"))ok=fclose(fp)==0;
            g_MainApplication->CreateShowDialog("Safe rendering",ok?"Setting changed. Restart the console to apply.":"Unable to change setting",{"OK"},true);
            return;
        }
        if(choice!=0&&choice!=1)return;
        const auto confirm=g_MainApplication->CreateShowDialog(choice==0?"Install update?":"Restore previous version?",
            "Confirm that all games are closed. Keep your SD card inserted and console charged. Your saves and themes will not be replaced.",{"Continue","Cancel"},true);
        if(confirm!=0)return;
        this->options_menu->SetVisible(false);
        this->info_text->SetText(choice==0?"Validating and installing update...":"Restoring previous files...");
        g_MainApplication->CallForRender();
        std::string message;
        const bool ok=choice==0?InstallGlassUpdate(message):RestoreGlassUpdate(message);
        this->ResetInfoText();this->options_menu->SetVisible(true);
        this->activate_menu_item->SetName(GetStatus());this->ReloadMenu();
        if(ok) {
            const auto reboot=g_MainApplication->CreateShowDialog("GlassNX",message,{"Restart now"},true);
            if(reboot==0){g_MainApplication->FadeOut();RebootSystem();}
        } else g_MainApplication->CreateShowDialog("GlassNX",message,{"OK"},true);
    }
}
