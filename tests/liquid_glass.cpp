// Host smoke test for the production SDL material. Run with an accelerated SDL
// backend (Mesa offscreen is sufficient); this does not measure Switch FPS.
#include <ul/menu/ui/ui_LiquidGlass.hpp>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main() {
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    auto window=SDL_CreateWindow("GlassNX material smoke",0,0,1280,720,SDL_WINDOW_HIDDEN);
    assert(window);
    auto renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE);
    assert(renderer);
    auto wallpaper=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,240,135);
    assert(wallpaper);
    std::vector<Uint8> pixels(240*135*4);
    for(int y=0;y<135;y++)for(int x=0;x<240;x++) {
        auto p=(y*240+x)*4;
        pixels[p]=(x/4)%2?240:20;pixels[p+1]=y;pixels[p+2]=180;pixels[p+3]=255;
    }
    assert(SDL_UpdateTexture(wallpaper,nullptr,pixels.data(),240*4)==0);
    {
        ul::menu::ui::LiquidGlass material;
        assert(!material.Draw(renderer,0,0,100,100));
        // Production scale and a non-default viewport/clip must survive baking.
        SDL_RenderSetScale(renderer,2.f/3.f,2.f/3.f);
        SDL_Rect viewport{3,6,1800,1000},clip{9,12,1700,900};
        SDL_RenderSetViewport(renderer,&viewport);SDL_RenderSetClipRect(renderer,&clip);
        SDL_SetRenderDrawColor(renderer,15,25,35,45);
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        for(int strength:{0,65,100,65,65}) {
            material.Prepare(renderer,wallpaper,strength,{12,18,46,90});
            float sx=0,sy=0;SDL_RenderGetScale(renderer,&sx,&sy);
            assert(sx==2.f/3.f && sy==2.f/3.f);
            SDL_Rect actual{};SDL_RenderGetViewport(renderer,&actual);
            assert(actual.x==viewport.x && actual.y==viewport.y && actual.w==viewport.w && actual.h==viewport.h);
            SDL_RenderGetClipRect(renderer,&actual);
            assert(SDL_RenderIsClipEnabled(renderer) && actual.x==clip.x && actual.y==clip.y && actual.w==clip.w && actual.h==clip.h);
            assert(SDL_GetRenderTarget(renderer)==nullptr);
            Uint8 r,g,b,a;SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a);
            assert(r==15 && g==25 && b==35 && a==45);
            SDL_BlendMode mode;SDL_GetRenderDrawBlendMode(renderer,&mode);assert(mode==SDL_BLENDMODE_BLEND);
            assert(material.Draw(renderer,70,225,380,330));
        }
        // Draw into an isolated target to check rounded corner coverage.
        auto target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,240,135);
        assert(target && SDL_SetRenderTarget(renderer,target)==0);
        auto active_target=SDL_GetRenderTarget(renderer);
        SDL_RenderSetScale(renderer,1,1);SDL_RenderSetViewport(renderer,nullptr);SDL_RenderSetClipRect(renderer,nullptr);
        SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
        material.Prepare(renderer,wallpaper,65,{0,0,0,0});
        assert(SDL_GetRenderTarget(renderer)==active_target);
        assert(material.Draw(renderer,20,20,180,100));
        std::vector<Uint8> result(240*135*4);
        assert(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,result.data(),240*4)==0);
        auto p=[](int x,int y){return (y*240+x)*4;};
        assert(result[p(20,20)+2]==0); // rounded corner is outside the lens
        assert(result[p(110,70)+2]>100); // visible wallpaper in its center
        assert(!material.Draw(renderer,0,0,0,100));
        // A high-frequency wallpaper must lose contrast as blur increases.
        auto contrast=[&](int strength) {
            material.Prepare(renderer,wallpaper,strength,{0,0,0,0});
            SDL_RenderClear(renderer);SDL_RenderSetScale(renderer,.125f,.125f);
            assert(material.Draw(renderer,0,0,1920,1080));
            assert(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,result.data(),240*4)==0);
            int sum=0;
            for(int x=30;x<210;x++)sum+=std::abs(int(result[p(x,70)])-int(result[p(x-1,70)]));
            return sum;
        };
        const int sharp=contrast(0),soft=contrast(100);
        printf("Blur contrast: %d -> %d\n",sharp,soft);
        assert(sharp>0 && soft<sharp/2);
        SDL_SetRenderTarget(renderer,nullptr);SDL_DestroyTexture(target);
    }
    SDL_DestroyTexture(wallpaper);SDL_DestroyRenderer(renderer);
    auto surface=SDL_CreateRGBSurfaceWithFormat(0,240,135,32,SDL_PIXELFORMAT_RGBA32);
    assert(surface);
    auto software=SDL_CreateSoftwareRenderer(surface);assert(software);
    {
        ul::menu::ui::LiquidGlass fallback;
        fallback.Prepare(software,nullptr,65,{0,0,0,0});
        assert(!fallback.IsReady() && !fallback.Draw(software,0,0,100,100));
    }
    SDL_DestroyRenderer(software);SDL_FreeSurface(surface);
    SDL_DestroyWindow(window);SDL_Quit();
    puts("LiquidGlass smoke: renderer state, blur rebuild, target restore and rounded coverage passed");
}
