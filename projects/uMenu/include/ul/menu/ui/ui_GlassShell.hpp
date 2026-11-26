#pragma once
#include <ul/menu/ui/ui_EntryMenu.hpp>
#include <map>

namespace ul::menu::ui {
    class GlassShell : public pu::ui::elm::Element {
        enum class Page : int { Home, Library, Apps, Settings, Control, Notifications };
        using Renderer = pu::ui::render::Renderer::Ref;
        using Font = pu::ui::DefaultFontSize;
        EntryMenu::Ref entries;
        std::function<void(u32)> launch;
        std::function<void()> legacy;
        Page page = Page::Home;
        Page previous = Page::Home;
        int focus = 0;
        int offset = 0;
        int theme = 0;
        int glass = 65;
        int filter = 0;
        int blur = 65;
        bool motion = true;
        bool sort_name = false;
        std::string search;
        std::vector<u64> favorites;
        std::vector<u8> backdrop;
        std::chrono::steady_clock::time_point transition{};
        pu::sdl2::TextureHandle::Ref banner;
        u64 banner_id = 0;
        bool banner_checked = false;
        void LoadPreferences();
        void SavePreferences();
        void SetTheme(int index);
        void Search();
        void ToggleFavorite();
        bool Favorite(u64 id) const;
        void LoadBackdrop();
        pu::sdl2::TextureHandle::Ref Banner(u64 id);
        void Cover(Renderer &r,const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h);
        bool touched = false;
        bool icon_budget = true;
        float brightness = -1, volume = -1;
        int temperature = -1;
        s64 free_bytes = -1, total_bytes = -1;
        std::chrono::steady_clock::time_point next_metrics{};
        std::map<std::string, pu::sdl2::TextureHandle::Ref> materials;
        void ReadMetrics();
        void AdjustSlider(bool display, float target);
        void Slider(Renderer &r,int x,int y,int w,int h,const std::string &name,float value,bool selected);
        pu::sdl2::TextureHandle::Ref Material(int x,int y,int w,int h,bool selected);
        std::map<std::string, pu::ui::elm::TextBlock::Ref> texts;
        struct Hit { int x,y,w,h; std::function<void()> action; };
        std::vector<Hit> hits;
        struct Choice { int x,y,w,h; std::string label, detail; std::function<void()> action; };
        std::vector<u32> List();
        std::vector<Choice> Choices();
        void SetPage(Page p);
        void Activate();
        void Move(int amount);
        void Card(Renderer &r, int x,int y,int w,int h,bool selected=false);
        void Label(Renderer &r, const std::string &s,int x,int y,int size=30,int maxw=0,bool center=false);
        void Picture(Renderer &r, const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h);
        void Tile(Renderer &r,u32 index,int x,int y,int w,int h,bool selected);
        void Button(Renderer &r,int x,int y,int w,int h,const std::string &label,bool selected,std::function<void()> cb);
        void ToggleRadio(bool wifi);
    public:
        GlassShell(EntryMenu::Ref entries, std::function<void(u32)> launch, std::function<void()> legacy);
        PU_SMART_CTOR(GlassShell)
        s32 GetX() override {return 0;}
        s32 GetY() override {return 0;}
        s32 GetWidth() override {return 1920;}
        s32 GetHeight() override {return 1080;}
        void Invalidate() {focus=0;offset=0;texts.clear();hits.clear();banner.reset();banner_checked=false;}
        void HandleInput(u64 down, pu::ui::TouchPoint touch);
        void OnRender(Renderer &r,const s32 x,const s32 y) override;
        void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override {}
    };
}
