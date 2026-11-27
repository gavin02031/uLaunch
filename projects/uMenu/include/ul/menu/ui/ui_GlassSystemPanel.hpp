#pragma once
#include <pu/Plutonium>

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
                r->RenderRoundedRectangleFill({14,20,46,78},px,py+8,w,h,48);
                r->RenderRoundedRectangleFill({49,65,106,106},px,py,w,h,48);
                r->RenderRoundedRectangle({230,238,255,105},px,py,w,h,48);
                r->RenderRoundedRectangle({236,241,255,28},px+3,py+3,w-6,h-6,45);
            }
            void OnInput(const u64,const u64,const u64,const pu::ui::TouchPoint) override{}
    };
}
