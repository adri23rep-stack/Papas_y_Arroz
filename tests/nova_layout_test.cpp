#include "nova_layout.hpp"
#include <cassert>
#include <set>
int main(){
    xs::Catalog home;home.title="Inicio";
    xs::Entry last;last.kind=xs::EntryKind::Channel;last.title="Último canal";
    home.entries.push_back(last);
    for(int i=0;i<6;++i){xs::Entry e;e.kind=xs::EntryKind::Channel;home.entries.push_back(e);}
    for(int i=0;i<4;++i){xs::Entry e;e.kind=xs::EntryKind::Asset;e.program_type="1";home.entries.push_back(e);}
    for(int i=0;i<4;++i){xs::Entry e;e.kind=xs::EntryKind::Asset;e.program_type="0";home.entries.push_back(e);}
    auto tiles=xs::nova_tiles(home,0,0);
    assert(tiles.size()==15);
    std::set<size_t> indices;
    for(const auto& tile:tiles){
        assert(tile.x>=220&&tile.x+tile.w<=1280);
        assert(tile.y>=0&&tile.y+tile.h<=675);
        assert(indices.insert(tile.index).second);
        assert(xs::nova_hit(home,0,0,tile.x+tile.w/2,tile.y+tile.h/2)==static_cast<int>(tile.index));
    }
    assert(xs::nova_hit(home,0,0,20,20)==-1);
    assert(xs::nova_left_edge(home,1,0));
    assert(!xs::nova_left_edge(home,2,0));
    assert(xs::nova_move(home,0,0,0,1)!=0);
    assert(xs::nova_move(home,1,0,1,0)==2);
    xs::Catalog movies;movies.title="Películas";
    for(int i=0;i<15;++i){xs::Entry e;e.kind=xs::EntryKind::Asset;movies.entries.push_back(e);}
    assert(xs::nova_is_posters(movies));
    assert(xs::nova_columns(movies)==6);
    assert(xs::nova_page_size(movies)==6);
    assert(xs::nova_tiles(movies,8).front().index==6);
    assert(xs::nova_hit(movies,8,-1,248,379)==6);
    xs::Catalog browse;browse.title="Canales";
    for(int i=0;i<17;++i){xs::Entry e;e.kind=xs::EntryKind::Channel;browse.entries.push_back(e);}
    assert(xs::nova_page_size(browse)==12);
    assert(xs::nova_tiles(browse,13).front().index==12);
    assert(xs::nova_hit(browse,13,-1,250,385)==12);
    return 0;
}
