#pragma once
// NOVA layout and home carousel logic. Pure C++: no SDL or network requests.
#include "catalog.hpp"
#include "cinematic_home.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace xs {
struct NovaTile {
    size_t index=0;
    int x=0,y=0,w=0,h=0;
    enum Kind { Featured, Channel, Movie, Series, Poster, Browse } kind=Browse;
    bool contains(int px,int py)const{
        return px>=x&&px<x+w&&py>=y&&py<y+h;
    }
};

inline bool nova_is_home(const Catalog& c){return c.title=="Inicio";}
inline bool nova_is_posters(const Catalog& c){
    if(nova_is_home(c))return false;
    size_t posters=0,other=0;
    for(const auto& e:c.entries){
        if(e.kind==EntryKind::Asset)++posters;
        else if(e.kind!=EntryKind::VodPage)++other;
        if(posters+other>=12)break;
    }
    return posters>0&&posters>=other;
}
inline int nova_columns(const Catalog&){return 6;}
inline int nova_page_size(const Catalog& c){
    return nova_is_home(c)?120:(nova_is_posters(c)?6:12);
}

// Home rows are always TV, series, movies, in that order.
inline int nova_row_of(const Entry& e){
    if(e.kind==EntryKind::Channel||e.kind==EntryKind::LiveSection||
       e.kind==EntryKind::Category)return 0;
    if(e.kind==EntryKind::Asset||e.kind==EntryKind::Section){
        if(e.program_type=="0")return 1;
        if(e.program_type=="1")return 2;
    }
    return -1;
}
inline std::vector<size_t> nova_row_indices(const Catalog& c,int row){
    std::vector<size_t> indices;
    if(row<0||row>2)return indices;
    for(size_t i=0;i<c.entries.size();++i)
        if(nova_row_of(c.entries[i])==row)indices.push_back(i);
    return indices;
}
constexpr int nova_card_x=32;
constexpr int nova_card_step=202;
constexpr int nova_card_w=184;
constexpr int nova_card_h=105;
constexpr int nova_carousel_right=1240;
inline int nova_row_y(int row){return 296+155*row;}
inline int nova_row_at(int y){
    for(int row=0;row<3;++row)
        if(y>=nova_row_y(row)&&y<nova_row_y(row)+nova_card_h)return row;
    return -1;
}
inline int nova_max_scroll(const Catalog& c,int row){
    const int count=static_cast<int>(nova_row_indices(c,row).size());
    return std::max(0,(count-6)*nova_card_step);
}
inline int nova_clamp_scroll(const Catalog& c,int row,int pixels){
    return std::max(0,std::min(nova_max_scroll(c,row),pixels));
}

// NOVA uses the EXISTING catalogue's actual artwork and entries, not mock data.
// Extra previews are included only in Nova; F8's old home stays unchanged.
inline Catalog make_nova_home(const CinematicHomeContent& home,
                              const std::vector<Entry>& root,int& last_index){
    Catalog catalog;catalog.title="Inicio";last_index=-1;
    if(home.has_last&&home.last_channel.kind==EntryKind::Channel){
        last_index=static_cast<int>(catalog.entries.size());
        catalog.entries.push_back(home.last_channel);
    }
    int live=0;
    for(const auto& e:home.channels){
        if(e.kind!=EntryKind::Channel)continue;
        if(home.has_last&&e.channel_code==home.last_channel.channel_code)continue;
        catalog.entries.push_back(e);
        if(++live>=24)break;
    }
    if(live==0&&last_index<0){
        Entry e;e.title="Ver todos los canales";e.kind=EntryKind::LiveSection;
        catalog.entries.push_back(e);
    }
    const auto append=[&](const std::vector<Entry>& source,const char* type,const char* fallback){
        int count=0;
        for(const auto& e:source){
            if(e.kind!=EntryKind::Asset||e.program_type!=type)continue;
            catalog.entries.push_back(e);
            if(++count>=20)break;
        }
        if(count==0){
            const auto it=std::find_if(root.begin(),root.end(),[&](const Entry& e){
                return e.kind==EntryKind::Section&&e.program_type==type;
            });
            if(it!=root.end())catalog.entries.push_back(*it);
            else{
                Entry e;e.title=fallback;e.kind=EntryKind::Section;e.program_type=type;
                catalog.entries.push_back(e);
            }
        }
    };
    append(home.series,"0","Explorar series");
    append(home.movies,"1","Explorar películas");
    return catalog;
}

inline std::vector<NovaTile> nova_tiles(const Catalog& c,size_t selected,
                                        int last_index=-1,
                                        const std::array<int,3>& offsets={{0,0,0}}){
    (void)last_index;
    std::vector<NovaTile> result;
    if(nova_is_home(c)){
        for(int row=0;row<3;++row){
            const auto entries=nova_row_indices(c,row);
            const int scroll=nova_clamp_scroll(c,row,offsets[static_cast<size_t>(row)]);
            for(size_t ordinal=0;ordinal<entries.size();++ordinal){
                const int x=nova_card_x+static_cast<int>(ordinal)*nova_card_step-scroll;
                if(x>=nova_carousel_right||x+nova_card_w<=nova_card_x)continue;
                result.push_back({entries[ordinal],x,nova_row_y(row),nova_card_w,
                                  nova_card_h,row==0?NovaTile::Channel:
                                    row==1?NovaTile::Series:NovaTile::Movie});
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
inline bool nova_left_edge(const Catalog& c,size_t selected,int last_index){
    (void)last_index;
    if(nova_is_home(c)){
        if(selected>=c.entries.size())return false;
        const auto entries=nova_row_indices(c,nova_row_of(c.entries[selected]));
        return !entries.empty()&&entries.front()==selected;
    }
    return selected%6==0;
}
inline int nova_hit(const Catalog& c,size_t selected,int last_index,int x,int y,
                    const std::array<int,3>& offsets={{0,0,0}}){
    if(nova_is_home(c)&&(x<nova_card_x||x>=nova_carousel_right))return -1;
    for(const auto& tile:nova_tiles(c,selected,last_index,offsets))
        if(tile.contains(x,y))return static_cast<int>(tile.index);
    return -1;
}
inline size_t nova_move(const Catalog& c,size_t selected,int last_index,int dx,int dy){
    (void)last_index;
    if(nova_is_home(c)){
        if(selected>=c.entries.size())return selected;
        const int row=nova_row_of(c.entries[selected]);
        if(row<0)return selected;
        const auto current=nova_row_indices(c,row);
        auto it=std::find(current.begin(),current.end(),selected);
        if(it==current.end())return selected;
        const size_t pos=static_cast<size_t>(it-current.begin());
        if(dx>0)return current[std::min(pos+1,current.size()-1)];
        if(dx<0)return current[pos?pos-1:0];
        if(dy==0)return selected;
        const int next=row+(dy>0?1:-1);
        const auto targets=nova_row_indices(c,next);
        return targets.empty()?selected:targets[std::min(pos,targets.size()-1)];
    }
    // Retain the existing browsing navigation until its own UI is redesigned.
    const auto tiles=nova_tiles(c,selected);
    const auto it=std::find_if(tiles.begin(),tiles.end(),
        [&](const NovaTile& tile){return tile.index==selected;});
    if(it==tiles.end())return selected;
    const int cx=it->x+it->w/2,cy=it->y+it->h/2;
    size_t best_index=selected;
    long best=2147483647L;
    for(const auto& t:tiles){
        if(t.index==selected)continue;
        const int sx=t.x+t.w/2-cx,sy=t.y+t.h/2-cy;
        if((dx>0&&sx<=5)||(dx<0&&sx>=-5)||
           (dy>0&&sy<=5)||(dy<0&&sy>=-5))continue;
        const long forward=dx?static_cast<long>(sx)*dx:static_cast<long>(sy)*dy;
        const long sideways=dx?std::abs(sy):std::abs(sx);
        const long score=forward*4+sideways*3;
        if(score<best){best=score;best_index=t.index;}
    }
    return best_index;
}
} // namespace xs
