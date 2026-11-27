#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <cstdio>
#include <algorithm>
#include <sys/stat.h>
namespace ul::glassnx {
    // Featured artwork occupies 1130 x 405 logical pixels: exactly 226:81.
    constexpr int HeroWidth=1130, HeroHeight=405;
    inline SDL_Surface *NormalizeArtwork(SDL_Surface *source,bool hero,int size,std::string &error) {
        const int w=hero?HeroWidth:size,h=hero?HeroHeight:size;
        if(!source || source->w<=0 || source->h<=0){error="This artwork could not be decoded. Choose another image.";return nullptr;}
        SDL_Rect crop{0,0,source->w,source->h};
        if(int64_t(crop.w)*h>int64_t(crop.h)*w)crop.w=int(int64_t(crop.h)*w/h);
        else crop.h=int(int64_t(crop.w)*h/w);
        crop.x=(source->w-crop.w)/2;crop.y=(source->h-crop.h)/2;
        if(crop.w<w || crop.h<h){error=hero?"Choose a sharper Hero: the center crop must be at least 1130 x 405.":"Choose a sharper Cover: the square crop must be at least 1024 x 1024.";return nullptr;}
        auto output=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
        if(!output){error="Not enough memory to prepare artwork.";return nullptr;}
        SDL_SetSurfaceBlendMode(source,SDL_BLENDMODE_NONE);
        if(SDL_BlitScaled(source,&crop,output,nullptr)!=0){SDL_FreeSurface(output);error="Unable to resize artwork.";return nullptr;}
        return output;
    }
    inline bool ExportArtwork(SDL_Surface *image,const std::string &path,std::string &error) {
        mkdir("sdmc:/glassnx/art",0777);
        const auto temp=path+".tmp";
        if(IMG_SavePNG(image,temp.c_str())!=0 || rename(temp.c_str(),path.c_str())!=0){remove(temp.c_str());error="Could not save artwork. Your previous asset is retained.";return false;}
        return true;
    }
}
