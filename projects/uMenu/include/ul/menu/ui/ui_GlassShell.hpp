#pragma once
#include <ul/menu/ui/ui_EntryMenu.hpp>
#include <map>
#include <ul/menu/ui/ui_LiquidGlass.hpp>

namespace ul::menu::ui {
    class GlassShell : public pu::ui::elm::Element {
        enum class Page : int { Home, Library, Apps, Settings, Control, Notifications };
        using Renderer = pu::ui::render::Renderer::Ref;
        using Font = pu::ui::DefaultFontSize;
        EntryMenu::Ref entries;
        std::function<void(u32)> launch;
        std::function<void()> legacy;
        std::function<void()> folder_back;
        std::function<void()> close_suspended;
        int alert_offset=0, drag_slider=-1;
        float nav_position=0.f, nav_velocity=0.f, focus_lift=0.f, focus_velocity=0.f;
        int animated_focus=-1;
        std::chrono::steady_clock::time_point animation_tick{}, slider_tick{};
        float StepSpring(float current,float &velocity,float target,float dt);
        void UpdateMotion();
        void ScrollAlerts(int amount);
        void Back();
        Page page = Page::Home;
        Page previous = Page::Home;
        int previous_focus=0,previous_offset=0,previous_filter=0,previous_section=0;
        std::string previous_search;
        int focus = 0;
        int offset = 0;
        int theme = 0;
        int glass = 65;
        int filter = 0;
        int setting_section = 0;
        void Section(int i);
        void RoundedArt(Renderer &r,const pu::sdl2::TextureHandle::Ref &t,int x,int y,int w,int h,int radius);
        void Segment(Renderer &r,int x,int y,int w,int h,const std::string &kind,const std::string &label,bool selected,std::function<void()> cb);
        std::string StorageText() const;
        int blur = 65;
        bool motion = true;
        bool sort_name = false;
        std::string search;
        std::vector<u64> favorites;
        std::vector<u64> recent;
        std::map<u64,int> title_categories;
        void LoadCategories();
        bool SaveCategories();
        void TitleOptions();
        void HeroShade(int x,int y,int w,int h);
        void BatteryGlyph(Renderer &r,int x,int y,int size);
        void Glyph(Renderer &r,const std::string &kind,int x,int y,int size);
        void IconButton(Renderer &r,int x,int y,int w,int h,const std::string &kind,const std::string &label,bool selected,std::function<void()> cb);
        void ReadRecent();
        void SaveRecent();
        std::vector<u8> backdrop;
        pu::sdl2::TextureHandle::Ref theme_wallpaper;
        std::map<int,pu::sdl2::TextureHandle::Ref> theme_previews;
        pu::sdl2::TextureHandle::Ref LoadWallpaper(int index,bool preview);
        pu::sdl2::TextureHandle::Ref ActiveWallpaper();
        pu::ui::Color WallpaperTint() const;
        void FrameDiagnostics();
        bool diagnostics=false;
        std::chrono::steady_clock::time_point frame_tick{},frame_window{};
        unsigned frame_count=0;
        float frame_ms=0,frame_max=0,display_fps=0,display_max=0;
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
        bool safe_mode = false;
        bool enhanced_glass = false;
        LiquidGlass liquid_glass;
        void ToggleEnhancedGlass();
        size_t material_bytes = 0;
        void ClearMaterials();
        bool Conservative() const;
        void ToggleSafeMode();
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
        GlassShell(EntryMenu::Ref entries, std::function<void(u32)> launch, std::function<void()> legacy,std::function<void()> back,std::function<void()> close);
        PU_SMART_CTOR(GlassShell)
        s32 GetX() override {return 0;}
        s32 GetY() override {return 0;}
        s32 GetWidth() override {return 1920;}
        s32 GetHeight() override {return 1080;}
        void RecordLaunch(u64 id);
        void PrepareTransition();
        void Invalidate() {focus=0;offset=0;animated_focus=-1;alert_offset=0;texts.clear();hits.clear();banner.reset();banner_checked=false;}
        void HandleInput(u64 down, pu::ui::TouchPoint touch);
        void OnRender(Renderer &r,const s32 x,const s32 y) override;
        void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override {}
    };
}
