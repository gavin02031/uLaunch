#include <ul/menu/ui/ui_LockscreenMenuLayout.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace ul::menu::ui {

    namespace {
        inline void UnlockScreen() {
            if(accountUidIsValid(&g_GlobalSettings.system_status.selected_user)) {
                g_MenuApplication->LoadMenu(MenuType::Main);
            }
            else {
                g_MenuApplication->LoadMenu(MenuType::Startup);
            }
        }

    }

    LockscreenMenuLayout::LockscreenMenuLayout() : IMenuLayout() {
        auto custom=pu::ui::render::LoadImageFromFile("sdmc:/glassnx/lockscreen.jpg");
        if(custom) this->SetBackgroundImage(pu::sdl2::TextureHandle::New(custom));
        else {
            auto scene=TryFindLoadImage("ui/GlassNX/Lockscreen");
            if(scene)this->SetBackgroundImage(pu::sdl2::TextureHandle::New(scene));
        }
        auto unlock_glass=TryFindLoadImage("ui/GlassNX/Unlock");
        if(unlock_glass)this->Add(pu::ui::elm::Image::New(600,875,pu::sdl2::TextureHandle::New(unlock_glass)));
        auto brand=pu::ui::elm::TextBlock::New(60,45,"GlassNX");
        brand->SetColor({255,255,255,255});
        brand->SetFont(pu::ui::MakeDefaultFontName(37));
        this->Add(brand);
        this->info_text = pu::ui::elm::TextBlock::New(0, 0, "Press A to unlock");
        this->info_text->SetColor(g_MenuApplication->GetTextColor());
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "info_text", this->info_text);
        this->Add(this->info_text);

        this->connection_top_icon = pu::ui::elm::Image::New(0, 0, TryFindLoadImageHandle("ui/Main/TopIcon/Connection/None"));
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "connection_top_icon", this->connection_top_icon);
        this->Add(this->connection_top_icon);

        this->InitializeTimeText(this->time_mtext, "lockscreen_menu", "time_text");
        for(auto &part : this->time_mtext->GetAll()) part->SetFont(pu::ui::MakeDefaultFontName(220));
        this->time_mtext->UpdatePositionsSizes();

        auto panel=TryFindLoadImage("ui/GlassNX/Alert");
        if(panel) {
            auto tex=pu::sdl2::TextureHandle::New(panel);
            this->Add(pu::ui::elm::Image::New(480,500,tex));
            this->older_panel=pu::ui::elm::Image::New(480,665,tex);
            this->older_panel->SetVisible(false);this->Add(this->older_panel);
        }
        this->notification_heading = pu::ui::elm::TextBlock::New(0, 513, "Notifications");
        this->notification_heading->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
        this->notification_heading->SetColor({255,255,255,255});
        this->notification_heading->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->Add(this->notification_heading);
        this->notification_text = pu::ui::elm::TextBlock::New(0, 563, "You're all caught up.");
        this->notification_text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
        this->notification_text->SetColor({255,255,255,255});
        this->notification_text->SetClampWidth(870);
        this->notification_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->Add(this->notification_text);
        this->older_notification=pu::ui::elm::TextBlock::New(0,710," ");
        this->older_notification->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
        this->older_notification->SetColor({255,255,255,255});
        this->older_notification->SetClampWidth(870);
        this->older_notification->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->older_notification->SetVisible(false);
        this->Add(this->older_notification);

        this->date_text = pu::ui::elm::TextBlock::New(0, 0, "...");
        this->date_text->SetColor(g_MenuApplication->GetTextColor());
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "date_text", this->date_text);
        this->Add(this->date_text);

        this->battery_text = pu::ui::elm::TextBlock::New(0, 0, "...");
        this->battery_text->SetColor(g_MenuApplication->GetTextColor());
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "battery_text", this->battery_text);
        this->Add(this->battery_text);

        this->battery_top_icon = pu::ui::elm::Image::New(0, 0, TryFindLoadImageHandle("ui/Main/TopIcon/Battery/100"));
        this->battery_charging_top_icon = pu::ui::elm::Image::New(0, 0, TryFindLoadImageHandle("ui/Main/TopIcon/Battery/Charging"));
        this->battery_charging_top_icon->SetVisible(false);
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "battery_top_icon", this->battery_top_icon);
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "battery_top_icon", this->battery_charging_top_icon);
        this->Add(this->battery_top_icon);
        this->Add(this->battery_charging_top_icon);
    }

    void LockscreenMenuLayout::OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        const auto &history = g_MenuApplication->GetNotificationHistory();
        const auto message = history.empty() ? std::string("You're all caught up.") : history.back();
        if(this->notification_text->GetText() != message) this->notification_text->SetText(message);
        const bool has_older=history.size()>1;
        if(this->older_panel)this->older_panel->SetVisible(has_older);
        this->older_notification->SetVisible(has_older);
        if(has_older && this->older_notification->GetText()!=history[history.size()-2])this->older_notification->SetText(history[history.size()-2]);
        if((keys_down & HidNpadButton_A) || touch_pos.HitsRegion(600, 875, 720, 120)) {
            UnlockScreen();
            return;
        }

        this->UpdateConnectionTopIcon(this->connection_top_icon);
        this->UpdateTimeText(this->time_mtext);
        this->UpdateDateText(this->date_text);
        this->UpdateBatteryTextAndTopIcons(this->battery_text, this->battery_top_icon, this->battery_charging_top_icon);
    }

    bool LockscreenMenuLayout::OnHomeButtonPress() {
        UnlockScreen();
        return true;
    }

    void LockscreenMenuLayout::LoadSfx() {
    }

    void LockscreenMenuLayout::DisposeSfx() {
    }

}
