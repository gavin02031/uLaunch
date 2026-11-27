#pragma once
#include <SDL2/SDL.h>
#include <memory>

namespace ul::menu::ui {
    // Wallpaper-only lens, using the existing SDL GPU backend. No GL state sharing.
    class LiquidGlass {
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture{nullptr,SDL_DestroyTexture};
        SDL_Texture *source = nullptr;
        SDL_Color shade{};
        int blur = -1;
        bool unavailable = false;
    public:
        bool IsReady() const { return bool(texture); }
        void Release() { texture.reset(); source=nullptr; blur=-1; }
        void Prepare(SDL_Renderer *renderer,SDL_Texture *wallpaper,int strength,SDL_Color tint);
        bool Draw(SDL_Renderer *renderer,int x,int y,int w,int h);
    };
}
