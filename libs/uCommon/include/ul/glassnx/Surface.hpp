#pragma once
#include <pu/Plutonium>
#include <algorithm>
namespace ul::glassnx {
    // Shared non-GPU-target glass material for boot, recovery and compact controls.
    // No allocations, no framebuffer capture, no new services.
    inline void DrawSurface(pu::ui::render::Renderer::Ref &r,int x,int y,int w,int h,int radius,pu::ui::Color tint,bool selected=false){
        radius=std::max(1,std::min(radius,std::min(w,h)/2));
        r->RenderRoundedRectangleFill({10,16,37,60},x,y+7,w,h,radius);
        r->RenderRoundedRectangleFill(tint,x,y,w,h,radius);
        r->RenderRoundedRectangle({221,234,255,static_cast<u8>(selected?205:100)},x,y,w,h,radius);
        if(w>12&&h>12)r->RenderRoundedRectangle({248,250,255,static_cast<u8>(selected?85:38)},x+3,y+3,w-6,h-6,std::max(1,radius-3));
        if(w>2*radius+8)r->RenderRoundedRectangleFill({255,255,255,static_cast<u8>(selected?100:48)},x+radius,y+3,w-2*radius,3,1);
        if(w>2*radius+8)r->RenderRoundedRectangleFill({171,194,255,40},x+radius,y+h-5,w-2*radius,2,1);
    }
}
