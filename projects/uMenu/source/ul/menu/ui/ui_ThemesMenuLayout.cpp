#include <ul/menu/ui/ui_GlassSystemPanel.hpp>
#include <ul/menu/ui/ui_ThemesMenuLayout.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/fs/fs_Stdio.hpp>
#include <ul/menu/smi/smi_Commands.hpp>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {

    ThemesMenuLayout::ThemesMenuLayout() : IMenuLayout() {
        auto brand=pu::ui::elm::TextBlock::New(70,38,"Theme studio");
        brand->SetFont(pu::ui::MakeDefaultFontName(45));brand->SetColor({238,244,255,225});this->Add(brand);
        this->Add(GlassSystemPanel::New(1600,24,250,82));
        auto back=pu::ui::elm::TextBlock::New(1650,46,"B  Back");
        back->SetFont(pu::ui::MakeDefaultFontName(30));back->SetColor({238,244,255,255});this->Add(back);
        this->Add(GlassSystemPanel::New(70,120,1780,930));
        this->info_text = pu::ui::elm::TextBlock::New(0, 0, GetLanguageString("theme_info_text"));
        this->info_text->SetColor(g_MenuApplication->GetTextColor());
        g_GlobalSettings.ApplyConfigForElement("themes_menu", "info_text", this->info_text);
        this->Add(this->info_text);

        this->themes_menu = pu::ui::elm::Menu::New(0, 0, ThemesMenuWidth, g_MenuApplication->GetMenuBackgroundColor(), g_MenuApplication->GetMenuFocusColor(), ThemesMenuItemSize, ThemesMenuItemsToShow);
        g_GlobalSettings.ApplyConfigForElement("themes_menu", "themes_menu", this->themes_menu);
        this->Add(this->themes_menu);
    }

    void ThemesMenuLayout::LoadSfx() {
        this->theme_change_sfx = pu::audio::LoadSfx(TryGetActiveThemeResource("sound/Themes/ThemeChange.wav"));
        this->back_sfx = pu::audio::LoadSfx(TryGetActiveThemeResource("sound/Themes/Back.wav"));
    }

    void ThemesMenuLayout::DisposeSfx() {
        pu::audio::DestroySfx(this->theme_change_sfx);
        pu::audio::DestroySfx(this->back_sfx);
    }

    void ThemesMenuLayout::OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        bool back_requested=(keys_down&HidNpadButton_B)!=0;
        if(touch_pos.IsEmpty())header_touch_latched=false;
        else if(!header_touch_latched){header_touch_latched=true;if(touch_pos.HitsRegion(1600,24,250,82))back_requested=true;}
        if(back_requested) {
            pu::audio::PlaySfx(this->back_sfx);

            g_MenuApplication->LoadMenu(MenuType::Main);
        }
    }

    bool ThemesMenuLayout::OnHomeButtonPress() {
        pu::audio::PlaySfx(this->back_sfx);

        g_MenuApplication->LoadMenu(MenuType::Main);
        return true;
    }

    void ThemesMenuLayout::Reload() {
        this->header_touch_latched=true;
        this->themes_menu->ClearItems();
        this->loaded_themes.clear();
        this->loaded_theme_icons.clear();
        
        this->loaded_themes = cfg::FindThemes();
        this->loaded_themes.insert(this->loaded_themes.begin(), cfg::Theme{}); // For the "default theme" entry

        // Move active theme to top
        for(u32 i = 0; i < this->loaded_themes.size(); i++) {
            const auto theme = this->loaded_themes.at(i);
            if(theme.IsSame(g_GlobalSettings.active_theme)) {
                this->loaded_themes.erase(this->loaded_themes.begin() + i);
                this->loaded_themes.insert(this->loaded_themes.begin(), theme);
                break;
            }
        }
        
        for(const auto &theme: this->loaded_themes) {
            if(theme.IsValid()) {
                std::string theme_icon_path;
                const auto rc = cfg::TryCacheLoadThemeIcon(theme, theme_icon_path);
                if(R_FAILED(rc)) {
                    UL_LOG_WARN("Theme '%s' unable to load image: %s", theme.name.c_str(), util::FormatResultDisplay(rc).c_str());
                }

                auto theme_icon = pu::sdl2::TextureHandle::New(pu::ui::render::LoadImageFromFile(theme_icon_path));
                this->loaded_theme_icons.push_back(theme_icon);

                auto theme_item = pu::ui::elm::MenuItem::New(std::string(theme.IsSame(g_GlobalSettings.active_theme)?"•  ":"")+theme.manifest.name + "  /  " + theme.manifest.author + "  ·  v" + theme.manifest.release);
                theme_item->AddOnKey(std::bind(&ThemesMenuLayout::theme_DefaultKey, this));
                theme_item->SetColor(g_MenuApplication->GetTextColor());
                theme_item->SetIcon(theme_icon);
                this->themes_menu->AddItem(theme_item);
            }
            else {
                this->loaded_theme_icons.emplace_back();
                auto theme_reset_item = pu::ui::elm::MenuItem::New(g_GlobalSettings.active_theme.IsValid()?"Restore default theme":"•  Default theme");
                theme_reset_item->AddOnKey(std::bind(&ThemesMenuLayout::theme_DefaultKey, this));
                theme_reset_item->SetColor(g_MenuApplication->GetTextColor());
                theme_reset_item->SetIcon(GetLogoTexture());
                this->themes_menu->AddItem(theme_reset_item);
            }
        }

        this->themes_menu->SetSelectedIndex(0);
    }

    void ThemesMenuLayout::theme_DefaultKey() {
        const auto idx = this->themes_menu->GetSelectedIndex();
        const auto &selected_theme = this->loaded_themes.at(idx);
        if(selected_theme.IsValid()) {
            if(selected_theme.IsSame(g_GlobalSettings.active_theme)) {
                g_MenuApplication->ShowNotification(GetLanguageString("theme_active_this"));
            }
            else {
                std::string theme_conf_msg = selected_theme.manifest.name + "\n";
                theme_conf_msg += selected_theme.manifest.description + "\n";
                theme_conf_msg += "(" + selected_theme.manifest.release + ", " + selected_theme.manifest.author + ")\n\n";
                theme_conf_msg += GetLanguageString("theme_set_conf");

                const auto option = g_MenuApplication->DisplayDialog(selected_theme.manifest.name, theme_conf_msg, { GetLanguageString("yes"), GetLanguageString("cancel") }, true, this->loaded_theme_icons.at(idx));
                if(option == 0) {
                    if(selected_theme.IsOutdated()) {
                        const auto option_2 = g_MenuApplication->DisplayDialog(selected_theme.manifest.name, GetLanguageString("theme_outdated"), { GetLanguageString("ok"), GetLanguageString("cancel") }, true);
                        if(option_2 != 0) {
                            return;
                        }
                    }

                    g_GlobalSettings.SetActiveTheme(selected_theme);
                    g_MenuApplication->ShowNotification(GetLanguageString("theme_cache"));

                    pu::audio::PlaySfx(this->theme_change_sfx);
                    g_MenuApplication->ShowNotification(GetLanguageString("theme_changed"));

                    g_MenuApplication->FadeOutToNonLibraryApplet();
                    UL_RC_ASSERT(ul::menu::smi::RestartMenu(true));
                    g_MenuApplication->Finalize();
                }
            }
        }
        else {
            if(g_GlobalSettings.active_theme.IsValid()) {
                const auto option = g_MenuApplication->DisplayDialog(GetLanguageString("theme_reset"), GetLanguageString("theme_reset_conf"), { GetLanguageString("yes"), GetLanguageString("cancel") }, true);
                if(option == 0) {
                    g_GlobalSettings.SetActiveTheme({});
                    g_MenuApplication->ShowNotification(GetLanguageString("theme_cache"));

                    pu::audio::PlaySfx(this->theme_change_sfx);
                    g_MenuApplication->ShowNotification(GetLanguageString("theme_changed"));

                    g_MenuApplication->FadeOutToNonLibraryApplet();
                    UL_RC_ASSERT(ul::menu::smi::RestartMenu(true));
                    g_MenuApplication->Finalize();
                }
            }
            else {
                g_MenuApplication->ShowNotification(GetLanguageString("theme_no_active"));
            }
        }
    }

}
