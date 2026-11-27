#include <ul/man/ui/ui_MainApplication.hpp>
#include <ul/menu/menu_Cache.hpp>
#include <ul/menu/menu_Entries.hpp>
#include <ul/acc/acc_Accounts.hpp>
#include <ul/util/util_Json.hpp>
#include <ul/man/man_Manager.hpp>
#include <ul/man/man_Network.hpp>
#include <ul/cfg/cfg_Config.hpp>
#include <ul/os/os_Applications.hpp>
#include <ul/os/os_System.hpp>
#include <ul/util/util_Zip.hpp>

extern ul::man::ui::MainApplication::Ref g_MainApplication;
extern ul::util::JSON g_DefaultLanguage;
extern ul::util::JSON g_MainLanguage;

namespace ul::man::ui {

    namespace {

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
        std::string info = "GlassNX Manager v0.11";
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

        this->options_menu = pu::ui::elm::Menu::New(0, MenuY, pu::ui::render::ScreenWidth, MenuColor, MenuFocusColor, MenuItemSize, MenuItemCount);
        
        this->activate_menu_item = pu::ui::elm::MenuItem::New(GetStatus());
        this->activate_menu_item->SetColor(MenuItemColor);
        this->activate_menu_item->AddOnKey(std::bind(&MainMenuLayout::activate_DefaultKey, this));
        this->options_menu->AddItem(this->activate_menu_item);

        this->reset_menu_menu_item = pu::ui::elm::MenuItem::New(GetLanguageString("reset_menu_item"));
        this->reset_menu_menu_item->SetColor(MenuItemColor);
        this->reset_menu_menu_item->AddOnKey(std::bind(&MainMenuLayout::resetMenu_DefaultKey, this));
        this->options_menu->AddItem(this->reset_menu_menu_item);

        this->reset_cache_menu_item = pu::ui::elm::MenuItem::New(GetLanguageString("reset_cache_item"));
        this->reset_cache_menu_item->SetColor(MenuItemColor);
        this->reset_cache_menu_item->AddOnKey(std::bind(&MainMenuLayout::resetCache_DefaultKey, this));
        this->options_menu->AddItem(this->reset_cache_menu_item);
        
        this->update_menu_item = pu::ui::elm::MenuItem::New("Update / Recovery / Safe Mode");
        this->update_menu_item->SetColor(MenuItemColor);
        this->update_menu_item->AddOnKey(std::bind(&MainMenuLayout::update_DefaultKey,this));
        this->options_menu->AddItem(this->update_menu_item);

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
