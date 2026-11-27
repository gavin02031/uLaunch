#include <ul/menu/ui/ui_LiquidGlass.hpp>
#include <algorithm>
#include <cmath>
#include <array>

namespace ul::menu::ui {
    void LiquidGlass::Prepare(SDL_Renderer *rr,SDL_Texture *wallpaper,int strength,SDL_Color tint) {
        if(unavailable || !rr || !wallpaper)return;
        if(texture && source==wallpaper && blur==strength &&
           shade.r==tint.r && shade.g==tint.g && shade.b==tint.b && shade.a==tint.a)return;
        SDL_RendererInfo info{};
        if(SDL_GetRendererInfo(rr,&info)!=0 || !(info.flags&SDL_RENDERER_ACCELERATED) ||
           !SDL_RenderTargetSupported(rr) || !wallpaper) {unavailable=true;return;}
        // Two separable nine-tap passes at 1/8 logical resolution. Rebuild only
        // when blur changes; never read pixels back or blur game/system overlays.
        auto a=SDL_CreateTexture(rr,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,240,135);
        auto b=SDL_CreateTexture(rr,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,240,135);
        if(!a || !b){if(a)SDL_DestroyTexture(a);if(b)SDL_DestroyTexture(b);texture.reset();unavailable=true;return;}
        SDL_SetTextureScaleMode(a,SDL_ScaleModeLinear);
        SDL_SetTextureScaleMode(b,SDL_ScaleModeLinear);
        auto target=SDL_GetRenderTarget(rr);
        SDL_Rect viewport{},clip{};float scale_x=1,scale_y=1;
        SDL_RenderGetViewport(rr,&viewport);SDL_RenderGetClipRect(rr,&clip);
        const bool clipped=SDL_RenderIsClipEnabled(rr);
        SDL_RenderGetScale(rr,&scale_x,&scale_y);
        Uint8 red,green,blue,alpha;SDL_GetRenderDrawColor(rr,&red,&green,&blue,&alpha);
        SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(rr,&blend);
        bool ok=SDL_SetRenderTarget(rr,a)==0;
        if(!ok){SDL_DestroyTexture(a);SDL_DestroyTexture(b);Release();unavailable=true;return;}
        SDL_RenderSetScale(rr,1,1);SDL_RenderSetViewport(rr,nullptr);SDL_RenderSetClipRect(rr,nullptr);
        SDL_SetRenderDrawColor(rr,0,0,0,255);
        SDL_RenderClear(rr);
        SDL_Rect full{0,0,240,135};
        // The UI may have alpha-modulated this shared wallpaper for a fade.
        // Bake unmodulated pixels and leave the caller's texture state intact.
        Uint8 wr=255,wg=255,wb=255,wa=255;SDL_BlendMode wallpaper_blend=SDL_BLENDMODE_NONE;
        SDL_GetTextureColorMod(wallpaper,&wr,&wg,&wb);SDL_GetTextureAlphaMod(wallpaper,&wa);
        SDL_GetTextureBlendMode(wallpaper,&wallpaper_blend);
        SDL_SetTextureColorMod(wallpaper,255,255,255);SDL_SetTextureAlphaMod(wallpaper,255);
        SDL_SetTextureBlendMode(wallpaper,SDL_BLENDMODE_NONE);
        ok=ok && SDL_RenderCopy(rr,wallpaper,nullptr,&full)==0;
        SDL_SetTextureColorMod(wallpaper,wr,wg,wb);SDL_SetTextureAlphaMod(wallpaper,wa);
        SDL_SetTextureBlendMode(wallpaper,wallpaper_blend);
        if(tint.a) {
            SDL_SetRenderDrawBlendMode(rr,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(rr,tint.r,tint.g,tint.b,tint.a);
            ok=ok && SDL_RenderFillRect(rr,&full)==0;
            SDL_SetRenderDrawColor(rr,0,0,0,255);
        }
        const Uint8 weights[]={4,13,30,51,59,51,30,13,4}; // sum 255: preserve luminance
        const float step=std::clamp(strength,0,100)/100.f*1.25f;
        for(int axis=0;axis<2 && ok;axis++) {
            auto src=axis==0?a:b;auto dst=axis==0?b:a;
            ok=SDL_SetRenderTarget(rr,dst)==0;
            if(!ok)break;
            ok=SDL_RenderClear(rr)==0;
            ok=SDL_SetTextureBlendMode(src,SDL_BLENDMODE_ADD)==0 && ok;
            for(int tap=0;tap<9 && ok;tap++) {
                SDL_SetTextureColorMod(src,weights[tap],weights[tap],weights[tap]);
                const float shift=(tap-4)*step;
                SDL_FRect rect{axis==0?shift:0,axis==1?shift:0,240.f,135.f};
                ok=SDL_RenderCopyF(rr,src,nullptr,&rect)==0;
                // Extend the edge texel into uncovered strips, avoiding both
                // dark borders and the zoom introduced by oversized blur taps.
                if(shift!=0 && ok) {
                    SDL_Rect edge=axis==0?SDL_Rect{shift>0?0:239,0,1,135}:SDL_Rect{0,shift>0?0:134,240,1};
                    SDL_FRect strip=axis==0?SDL_FRect{shift>0?0:240+shift,0,std::abs(shift),135}:
                        SDL_FRect{0,shift>0?0:135+shift,240,std::abs(shift)};
                    ok=SDL_RenderCopyF(rr,src,&edge,&strip)==0;
                }
            }
            SDL_SetTextureColorMod(src,255,255,255);
        }
        SDL_SetRenderTarget(rr,target);
        SDL_RenderSetScale(rr,scale_x,scale_y);SDL_RenderSetViewport(rr,&viewport);
        SDL_RenderSetClipRect(rr,clipped?&clip:nullptr);
        SDL_SetRenderDrawColor(rr,red,green,blue,alpha);SDL_SetRenderDrawBlendMode(rr,blend);
        SDL_DestroyTexture(b);
        if(!ok){SDL_DestroyTexture(a);texture.reset();unavailable=true;return;}
        SDL_SetTextureBlendMode(a,SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(a,SDL_ScaleModeLinear);
        texture.reset(a);source=wallpaper;blur=strength;shade=tint;
    }
    bool LiquidGlass::Draw(SDL_Renderer *rr,int x,int y,int w,int h) {
        if(!texture || w<=0 || h<=0)return false;
        // Rounded inner fan and refractive outer ring: a curved lens at the rim,
        // smooth wallpaper at the center. Fixed geometry cost for every panel.
        SDL_Vertex v[97]{};
        static const auto perimeter=[] {
            std::array<SDL_FPoint,48> points{};
            for(int c=0;c<4;c++)for(int i=0;i<12;i++) {
                const float angle=(-90+c*90+i*90.f/11)*3.14159265f/180;
                points[c*12+i]={cosf(angle),sinf(angle)};
            }
            return points;
        }();
        static const auto indices=[] {
            std::array<int,432> values{};
            for(int i=0;i<48;i++) {
                const int next=(i+1)%48;
                const int tris[]={0,1+i,1+next,1+i,49+i,49+next,1+i,49+next,1+next};
                std::copy_n(tris,9,values.data()+i*9);
            }
            return values;
        }();
        const float radius=std::min(40.f,std::min(w,h)*.5f);
        const float inner_radius=radius-std::min(12.f,radius*.5f);
        auto vertex=[&](float px,float py,bool edge) {
            const float dx=edge?(px-w*.5f)*.018f:0;
            const float dy=edge?(py-h*.5f)*.018f:0;
            return SDL_Vertex{{x+px,y+py},{255,255,255,210},
                {std::clamp((x+px+dx)/1920.f,0.f,1.f),std::clamp((y+py+dy)/1080.f,0.f,1.f)}};
        };
        v[0]=vertex(w*.5f,h*.5f,false);
        const float cx[]={w-radius,w-radius,radius,radius},cy[]={radius,h-radius,h-radius,radius};
        for(int c=0;c<4;c++)for(int i=0;i<12;i++) {
            const auto point=perimeter[c*12+i];
            v[1+c*12+i]=vertex(cx[c]+inner_radius*point.x,cy[c]+inner_radius*point.y,false);
            v[49+c*12+i]=vertex(cx[c]+radius*point.x,cy[c]+radius*point.y,true);
        }
        if(SDL_RenderGeometry(rr,texture.get(),v,97,indices.data(),432)==0)return true;
        texture.reset();unavailable=true;return false;
    }
}
