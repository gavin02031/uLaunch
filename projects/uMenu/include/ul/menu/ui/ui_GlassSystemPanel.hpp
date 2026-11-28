#pragma once
#include <pu/Plutonium>
#include <ul/glassnx/Surface.hpp>

namespace ul::menu::ui {
    // Layout background images use native texture dimensions. Draw the shared
    // 1280x720 SD artwork to the full 1920x1080 logical canvas instead.
    class GlassSystemWallpaper : public pu::ui::elm::Element {
        pu::sdl2::TextureHandle::Ref wallpaper;
    public:
        GlassSystemWallpaper() = default;
        PU_SMART_CTOR(GlassSystemWallpaper)
        void SetWallpaper(const pu::sdl2::TextureHandle::Ref &value){wallpaper=value;}
        s32 GetX() override{return 0;}
        s32 GetY() override{return 0;}
        s32 GetWidth() override{return 1920;}
        s32 GetHeight() override{return 1080;}
        void OnRender(pu::ui::render::Renderer::Ref &r,const s32 x,const s32 y) override {
            r->RenderRectangleFill({15,24,48,255},x,y,1920,1080);
            if(wallpaper&&wallpaper->Get())
                r->RenderTexture(wallpaper->Get(),x,y,pu::ui::render::TextureRenderOptions({},1920,1080,{},{},{}));
            r->RenderRectangleFill({14,23,43,75},x,y,1920,1080);
        }
        void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override{}
    };

    // Lightweight shared material for advanced system pages.
    // No additional textures, render targets, services or input handlers.
    class GlassSystemPanel : public pu::ui::elm::Element {
        private:
            s32 x,y,w,h;
        public:
            GlassSystemPanel(s32 x,s32 y,s32 w,s32 h):x(x),y(y),w(w),h(h){}
            PU_SMART_CTOR(GlassSystemPanel)
            s32 GetX() override{return x;}
            s32 GetY() override{return y;}
            s32 GetWidth() override{return w;}
            s32 GetHeight() override{return h;}
            void OnRender(pu::ui::render::Renderer::Ref &r,const s32 px,const s32 py) override {
                ul::glassnx::DrawSurface(r,px,py,w,h,48,{49,65,106,130});
            }
            void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override{}
    };
}
