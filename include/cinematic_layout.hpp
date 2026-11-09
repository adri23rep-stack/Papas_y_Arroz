#pragma once
#include "catalog.hpp"
#include <algorithm>
#include <cstddef>
#include <vector>

namespace xs {
struct CinematicTile {
    size_t index=0;
    int x=0,y=0,w=0,h=0;
    enum Kind { Last, Channel, Movie, Series, Browse, Poster } kind=Browse;
    bool contains(int px,int py)const{return px>=x&&px<x+w&&py>=y&&py<y+h;}
};
inline bool is_home(const Catalog& c){return c.title=="Inicio";}
inline bool is_poster_catalog(const Catalog& c){
    if(is_home(c))return false;
    size_t posters=0,others=0;
    for(const Entry& e:c.entries){
        if(e.kind==EntryKind::Asset)++posters;
        else if(e.kind!=EntryKind::VodPage)++others;
        if(posters+others>=12)break;
    }
    return posters>0&&posters>=others;
}
inline int cinematic_columns(const Catalog& c){
    return is_home(c)?6:is_poster_catalog(c)?8:6;
}
inline int cinematic_page_size(const Catalog& c){
    return is_home(c)?60:is_poster_catalog(c)?8:12;
}
inline std::vector<CinematicTile> cinematic_tiles(const Catalog& c,size_t selected,int last_index=-1){
    std::vector<CinematicTile> result;
    if(is_home(c)){
        int live=0,movies=0,series=0;
        for(size_t i=0;i<c.entries.size();++i){
            const Entry& e=c.entries[i];
            if(static_cast<int>(i)==last_index){
                result.push_back({i,30,318,858,73,CinematicTile::Last});continue;
            }
            if(e.kind==EntryKind::Channel&&live<6){
                result.push_back({i,29+live*207,424,195,83,CinematicTile::Channel});++live;
            }else if((e.kind==EntryKind::Asset&&e.program_type=="1"||
                      e.kind==EntryKind::Section&&e.program_type=="1")&&movies<4){
                result.push_back({i,29+movies*151,542,141,128,CinematicTile::Movie});++movies;
            }else if((e.kind==EntryKind::Asset&&e.program_type=="0"||
                      e.kind==EntryKind::Section&&e.program_type=="0")&&series<4){
                result.push_back({i,655+series*151,542,141,128,CinematicTile::Series});++series;
            }else if((e.kind==EntryKind::LiveSection||e.kind==EntryKind::Category)&&live<6){
                result.push_back({i,29+live*207,424,195,83,CinematicTile::Channel});++live;
            }
        }
    }else{
        const bool posters=is_poster_catalog(c);
        const int per_page=posters?8:12;
        const size_t start=(selected/static_cast<size_t>(per_page))*static_cast<size_t>(per_page);
        for(size_t i=start;i<c.entries.size()&&i<start+static_cast<size_t>(per_page);++i){
            const int k=static_cast<int>(i-start);
            if(posters)result.push_back({i,29+k*153,424,143,243,CinematicTile::Poster});
            else result.push_back({i,29+(k%6)*207,429+(k/6)*118,195,105,CinematicTile::Browse});
        }
    }
    return result;
}
inline int cinematic_hit(const Catalog& c,size_t selected,int last_index,int x,int y){
    for(const auto& tile:cinematic_tiles(c,selected,last_index))
        if(tile.contains(x,y))return static_cast<int>(tile.index);
    return -1;
}
}
