#include <ul/glassnx/Artwork.hpp>
#include <cassert>
int main(){
 std::string error;
 auto source=SDL_CreateRGBSurfaceWithFormat(0,1800,1200,32,SDL_PIXELFORMAT_RGBA32);
 assert(source);
 assert(ul::glassnx::IsSquareCover(512,512));
 assert(ul::glassnx::IsSquareCover(1024,1024));
 assert(!ul::glassnx::IsSquareCover(600,900));
 assert(!ul::glassnx::IsSquareCover(768,768));
 assert(ul::glassnx::IsHeroSource(1920,620));
 assert(!ul::glassnx::IsHeroSource(512,512));
 SDL_FillRect(source,nullptr,SDL_MapRGBA(source->format,255,0,0,255));
 SDL_Rect middle{300,0,1200,1200};SDL_FillRect(source,&middle,SDL_MapRGBA(source->format,0,255,0,255));
 auto tile=ul::glassnx::NormalizeArtwork(source,false,1024,error);
 assert(tile && tile->w==1024 && tile->h==1024);
 Uint8 r,g,b,a;SDL_GetRGBA(*static_cast<Uint32*>(tile->pixels),tile->format,&r,&g,&b,&a);assert(g==255 && r==0);
 auto small=ul::glassnx::NormalizeArtwork(tile,false,512,error);assert(small && small->w==512 && small->h==512);
 auto hero=ul::glassnx::NormalizeArtwork(source,true,0,error);assert(hero && hero->w==1130 && hero->h==405);
 auto native=ul::glassnx::NormalizeArtwork(small,false,512,error);assert(native && native->w==512 && native->h==512);SDL_FreeSurface(native);
 assert(!ul::glassnx::NormalizeArtwork(small,false,1024,error));
 assert(!ul::glassnx::NormalizeArtwork(small,true,0,error));
 SDL_FreeSurface(hero);SDL_FreeSurface(small);SDL_FreeSurface(tile);SDL_FreeSurface(source);
}
