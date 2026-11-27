#pragma once
#include <string>
#include <vector>
namespace ul::menu::art {
    struct ArtworkGame {int id;std::string name;};
    struct ArtworkHero {std::string url;int width,height;};
    bool FindArtworkGames(const std::string &,std::vector<ArtworkGame> &,std::string &);
    bool FindPictures(int id,bool hero,std::vector<ArtworkHero> &,std::string &);
    bool DownloadPicture(const ArtworkHero &,bool hero,std::string &data,std::string &error);
    bool SavePicture(const std::string &path,const std::string &data,std::string &error);
}
