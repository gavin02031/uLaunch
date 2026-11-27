#pragma once
#include <pu/Plutonium>
#include <ul/glassnx/Surface.hpp>

namespace ul::menu::ui {
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
