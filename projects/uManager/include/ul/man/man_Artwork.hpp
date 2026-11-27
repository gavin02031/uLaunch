#pragma once
#include <string>
#include <vector>
namespace ul::man {
    struct ArtworkTitle {std::string id,name;};
    struct ArtworkGame {int id;std::string name;};
    struct ArtworkHero {std::string url;int width,height;};
    std::vector<ArtworkTitle> ReadArtworkTitles();
    bool FindArtworkGames(const std::string &name,std::vector<ArtworkGame> &out,std::string &error);
    bool FindArtworkHeroes(int id,std::vector<ArtworkHero> &out,std::string &error);
    bool InstallArtwork(const ArtworkTitle &title,const ArtworkHero &hero,std::string &error);
}
