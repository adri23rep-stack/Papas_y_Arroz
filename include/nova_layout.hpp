#pragma once
// NOVA UI: layout only. No SDL, network, account or playback dependencies.
#include "catalog.hpp"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstddef>
#include <vector>

namespace xs {
struct NovaTile {
    size_t index = 0;
    int x=0, y=0, w=0, h=0;
    enum Kind { Featured, Channel, Movie, Series, Poster, Browse } kind=Browse;
    bool contains(int px,int py) const {
        return px>=x && px<x+w && py>=y && py<y+h;
    }
};
inline bool nova_is_home(const Catalog& c){ return c.title=="Inicio"; }
inline bool nova_is_posters(const Catalog& c){
    if(nova_is_home(c))return false;
    size_t posters=0,other=0;
    for(const auto& e:c.entries){
        if(e.kind==EntryKind::Asset)++posters;
        else if(e.kind!=EntryKind::VodPage)++other;
        if(posters+other>=12)break;
    }
    return posters>0 && posters>=other;
}
inline int nova_columns(const Catalog&){return 6;}
inline int nova_page_size(const Catalog& c){
    return nova_is_home(c)?60:(nova_is_posters(c)?6:12);
}
inline std::vector<NovaTile> nova_tiles(const Catalog& c,size_t selected,int last_index=-1){
    std::vector<NovaTile> result;
    if(nova_is_home(c)){
        if(last_index>=0 && static_cast<size_t>(last_index)<c.entries.size())
            result.push_back({static_cast<size_t>(last_index),247,98,974,226,NovaTile::Featured});
        int channels=0,movies=0,series=0;
        for(size_t i=0;i<c.entries.size();++i){
            if(static_cast<int>(i)==last_index)continue;
            const auto& e=c.entries[i];
            if((e.kind==EntryKind::Channel||e.kind==EntryKind::LiveSection||
                e.kind==EntryKind::Category)&&channels<6){
                result.push_back({i,247+channels*163,371,150,96,NovaTile::Channel});
                ++channels;
            }else if(((e.kind==EntryKind::Asset||e.kind==EntryKind::Section)&&
                    e.program_type=="1")&&movies<4){
                result.push_back({i,247+movies*117,523,106,139,NovaTile::Movie});
                ++movies;
            }else if(((e.kind==EntryKind::Asset||e.kind==EntryKind::Section)&&
                    e.program_type=="0")&&series<4){
                result.push_back({i,754+series*117,523,106,139,NovaTile::Series});
                ++series;
            }
        }
    }else{
        const bool posters=nova_is_posters(c);
        const int page=posters?6:12;
        const size_t start=(selected/static_cast<size_t>(page))*static_cast<size_t>(page);
        for(size_t i=start;i<c.entries.size()&&i<start+static_cast<size_t>(page);++i){
            const int k=static_cast<int>(i-start);
            if(posters)result.push_back({i,247+k*163,378,151,274,NovaTile::Poster});
            else result.push_back({i,247+(k%6)*163,383+(k/6)*136,151,124,NovaTile::Browse});
        }
    }
    return result;
}
// A left press on the first visible rail card opens the NOVA sidebar.
inline bool nova_left_edge(const Catalog& c,size_t selected,int last_index){
    for(const auto& tile:nova_tiles(c,selected,last_index))
        if(tile.index==selected)return tile.x<=247;
    return selected%6==0;
}
inline int nova_hit(const Catalog& c,size_t selected,int last_index,int x,int y){
    for(const auto& tile:nova_tiles(c,selected,last_index))
        if(tile.contains(x,y))return static_cast<int>(tile.index);
    return -1;
}
// Directional navigation follows visible card positions, including the hero.
inline size_t nova_move(const Catalog& c,size_t selected,int last_index,int dx,int dy){
    const auto tiles=nova_tiles(c,selected,last_index);
    const auto current=std::find_if(tiles.begin(),tiles.end(),
        [&](const NovaTile& t){return t.index==selected;});
    if(current==tiles.end())return selected;
    const int cx=current->x+current->w/2,cy=current->y+current->h/2;
    size_t nearest=selected;
    long best=LONG_MAX;
    for(const auto& tile:tiles){
        if(tile.index==selected)continue;
        const int sx=tile.x+tile.w/2-cx,sy=tile.y+tile.h/2-cy;
        if((dx>0&&sx<=5)||(dx<0&&sx>=-5)||
           (dy>0&&sy<=5)||(dy<0&&sy>=-5))continue;
        const long forward=dx?static_cast<long>(sx)*dx:static_cast<long>(sy)*dy;
        const long sideways=dx?std::abs(sy):std::abs(sx);
        const long score=forward*4+sideways*3;
        if(score<best){best=score;nearest=tile.index;}
    }
    return nearest;
}
} // namespace xs
