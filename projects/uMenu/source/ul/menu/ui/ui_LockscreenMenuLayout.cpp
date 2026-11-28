#include <ul/menu/ui/ui_LockscreenMenuLayout.hpp>
#include <ul/menu/ui/ui_MenuApplication.hpp>
#include <ul/glassnx/LockscreenPrefs.hpp>

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
        this->prism_wallpaper=GlassSystemWallpaper::New();
        this->Add(this->prism_wallpaper);
        auto custom=pu::ui::render::LoadImageFromFile("sdmc:/glassnx/lockscreen.jpg");
        if(custom)this->custom_wallpaper=pu::sdl2::TextureHandle::New(custom);
        auto brand=pu::ui::elm::TextBlock::New(60,45,"GlassNX  /  Lock screen");
        brand->SetColor({255,255,255,255});
        brand->SetFont(pu::ui::MakeDefaultFontName(37));
        this->Add(brand);
        this->Add(GlassSystemPanel::New(600,875,720,120));
        auto key=pu::ui::elm::TextBlock::New(0,0,"A");
        key->SetColor({35,48,73,255});
        key->SetFont(pu::ui::MakeDefaultFontName(37));
        // Draw a white physical button behind the legend in a compact element.
        class UnlockKey : public pu::ui::elm::Element {
            pu::ui::elm::TextBlock::Ref label;
        public:
            UnlockKey(pu::ui::elm::TextBlock::Ref value):label(value){}
            s32 GetX() override{return 755;}
            s32 GetY() override{return 912;}
            s32 GetWidth() override{return 46;}
            s32 GetHeight() override{return 46;}
            void OnRender(pu::ui::render::Renderer::Ref &r,const s32 x,const s32 y) override{
                r->RenderCircleFill({247,249,255,255},x+23,y+23,23);
                label->OnRender(r,x+23-label->GetWidth()/2,y+3);
            }
            void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override{}
        };
        this->Add(std::make_shared<UnlockKey>(key));
        this->info_text=pu::ui::elm::TextBlock::New(835,909,"Unlock");
        this->info_text->SetColor({255,255,255,255});
        this->info_text->SetFont(pu::ui::MakeDefaultFontName(45));
        this->Add(this->info_text);

        this->connection_top_icon = pu::ui::elm::Image::New(0, 0, TryFindLoadImageHandle("ui/Main/TopIcon/Connection/None"));
        g_GlobalSettings.ApplyConfigForElement("lockscreen_menu", "connection_top_icon", this->connection_top_icon);
        this->Add(this->connection_top_icon);

        this->InitializeTimeText(this->time_mtext, "lockscreen_menu", "time_text");
        for(auto &part : this->time_mtext->GetAll()) part->SetFont(pu::ui::MakeDefaultFontName(220));
        this->time_mtext->UpdatePositionsSizes();

        this->Add(GlassSystemPanel::New(480,500,960,145));
        this->older_panel=GlassSystemPanel::New(480,665,960,145);
        this->older_panel->SetVisible(false);this->Add(this->older_panel);
        this->notification_heading = pu::ui::elm::TextBlock::New(0, 513, "GlassNX  ·  Notifications");
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

    void LockscreenMenuLayout::Enter(const pu::sdl2::TextureHandle::Ref &wallpaper){
        prism_wallpaper->SetWallpaper(custom_wallpaper?custom_wallpaper:wallpaper);
        touch_latched=true;
        hide_previews=ul::glassnx::HideLockscreenPreviews();
        // Clear previous visible content before the first frame after entry.
        notification_heading->SetText("GlassNX  ·  Activity");
        notification_text->SetText(hide_previews?"Notification previews hidden":"You're all caught up.");
        older_panel->SetVisible(false);older_notification->SetVisible(false);
    }
    void LockscreenMenuLayout::OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        const auto &history = g_MenuApplication->GetNotificationHistory();
        const auto message = history.empty() ? std::string("You're all caught up.") : hide_previews?std::string("Notification previews hidden"):history.back();
        const auto heading=history.empty()?std::string("GlassNX  ·  Notifications"):"GlassNX  ·  "+std::to_string(history.size())+" saved alerts";
        if(this->notification_heading->GetText()!=heading)this->notification_heading->SetText(heading);
        if(this->notification_text->GetText() != message) this->notification_text->SetText(message);
        const bool has_older=!hide_previews&&history.size()>1;
        if(this->older_panel)this->older_panel->SetVisible(has_older);
        this->older_notification->SetVisible(has_older);
        if(has_older && this->older_notification->GetText()!=history[history.size()-2])this->older_notification->SetText(history[history.size()-2]);
        const bool unlock_touch=!touch_latched && touch_pos.HitsRegion(600, 875, 720, 120);
        touch_latched=!touch_pos.IsEmpty();
        if((keys_down & HidNpadButton_A) || unlock_touch) {
            UnlockScreen();
            return;
        }

        this->UpdateConnectionTopIcon(this->connection_top_icon);
        this->UpdateTimeText(this->time_mtext);
        this->time_mtext->Get(1)->SetVisible(true);
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
