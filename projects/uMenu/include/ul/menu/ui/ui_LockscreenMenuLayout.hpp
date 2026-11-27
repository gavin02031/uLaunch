
#pragma once
#include <ul/menu/ui/ui_IMenuLayout.hpp>

namespace ul::menu::ui {

    class LockscreenMenuLayout : public IMenuLayout {
        private:
            bool touch_latched=true;
            pu::ui::elm::TextBlock::Ref info_text;
            pu::ui::elm::TextBlock::Ref notification_text;
            pu::ui::elm::TextBlock::Ref notification_heading;
            pu::ui::elm::TextBlock::Ref older_notification;
            pu::ui::elm::Image::Ref older_panel;
            pu::ui::elm::Image::Ref connection_top_icon;
            MultiTextBlock::Ref time_mtext;
            pu::ui::elm::TextBlock::Ref date_text;
            pu::ui::elm::TextBlock::Ref battery_text;
            pu::ui::elm::Image::Ref battery_top_icon;
            pu::ui::elm::Image::Ref battery_charging_top_icon;

        public:
            LockscreenMenuLayout();
            PU_SMART_CTOR(LockscreenMenuLayout)
            
            void OnMenuInput(const u64 keys_down, const u64 keys_up, const u64 keys_held, const pu::ui::TouchPoint touch_pos) override;
            bool OnHomeButtonPress() override;
            void LoadSfx() override;
            void DisposeSfx() override;
    };

}
