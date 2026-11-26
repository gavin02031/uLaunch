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
        this->info_text = pu::ui::elm::TextBlock::New(0, 0, GetLanguageString("lockscreen_info"));
        this->info_text->SetColor(g_MenuApplication->GetTextColor());
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "info_text", this->info_text);
        this->Add(this->info_text);

        this->connection_top_icon = pu::ui::elm::Image::New(0, 0, TryFindLoadImageHandle("ui/Main/TopIcon/Connection/None"));
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "connection_top_icon", this->connection_top_icon);
        this->Add(this->connection_top_icon);

        this->InitializeTimeText(this->time_mtext, "lockscreen_menu", "time_text");
        for(auto &part : this->time_mtext->GetAll()) part->SetFont(pu::ui::MakeDefaultFontName(112));
        this->time_mtext->UpdatePositionsSizes();

        this->notification_heading = pu::ui::elm::TextBlock::New(0, 594, "Notifications");
        this->notification_heading->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
        this->notification_heading->SetColor({255,255,255,255});
        this->notification_heading->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->Add(this->notification_heading);
        this->notification_text = pu::ui::elm::TextBlock::New(0, 650, "You're all caught up.");
        this->notification_text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
        this->notification_text->SetColor({255,255,255,255});
        this->notification_text->SetClampWidth(1100);
        this->notification_text->SetHorizontalAlign(pu::ui::elm::HorizontalAlign::Center);
        this->Add(this->notification_text);

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
        if((keys_down & HidNpadButton_A) || touch_pos.HitsRegion(640, 750, 640, 160)) {
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
