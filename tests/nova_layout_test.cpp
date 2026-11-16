#include "nova_layout.hpp"
#include <array>
#include <cassert>
#include <set>

int main(){
    xs::Catalog home;home.title="Inicio";
    xs::Entry last;last.kind=xs::EntryKind::Channel;
    last.title="Último canal";home.entries.push_back(last);
    for(int i=0;i<10;++i){
        xs::Entry e;e.kind=xs::EntryKind::Channel;e.title="TV";
        home.entries.push_back(e);
    }
    for(int i=0;i<10;++i){
        xs::Entry e;e.kind=xs::EntryKind::Asset;e.program_type="0";
        e.title="Serie";home.entries.push_back(e);
    }
    for(int i=0;i<10;++i){
        xs::Entry e;e.kind=xs::EntryKind::Asset;e.program_type="1";
        e.title="Película";home.entries.push_back(e);
    }
    assert(xs::nova_is_home(home));
    assert(xs::nova_columns(home)==6);
    assert(xs::nova_page_size(home)==120);
    assert(xs::nova_row_indices(home,0).size()==11);
    assert(xs::nova_row_indices(home,1).size()==10);
    assert(xs::nova_row_indices(home,2).size()==10);
    assert(xs::nova_row_of(home.entries[11])==1);
    assert(xs::nova_row_of(home.entries[21])==2);
    assert(xs::nova_row_at(xs::nova_row_y(2)+5)==2);
    assert(xs::nova_row_at(250)==-1);

    const auto visible=xs::nova_tiles(home,0,0);
    assert(visible.size()==18); // Six full cards per row.
    for(const auto& tile:visible){
        assert(tile.x>=xs::nova_card_x);
        assert(tile.x+tile.w<=xs::nova_carousel_right);
        assert(tile.y>=0&&tile.y+tile.h<=720);
        assert(xs::nova_hit(home,0,0,tile.x+5,tile.y+5)==static_cast<int>(tile.index));
    }
    assert(xs::nova_hit(home,0,0,4,400)==-1);
    assert(xs::nova_move(home,0,0,1,0)==1);
    assert(xs::nova_move(home,10,0,1,0)==10);
    assert(xs::nova_move(home,3,0,0,1)==14);
    assert(xs::nova_move(home,14,0,0,1)==24);
    assert(xs::nova_move(home,21,0,0,-1)==11);

    std::array<int,3> scroll{{xs::nova_card_step*4,0,0}};
    auto shifted=xs::nova_tiles(home,0,0,scroll);
    assert(shifted.front().index==4);
    assert(shifted.front().x==xs::nova_card_x);
    assert(xs::nova_hit(home,0,0,xs::nova_card_x+6,
                         xs::nova_row_y(0)+6,scroll)==4);
    assert(xs::nova_clamp_scroll(home,0,999999)==xs::nova_max_scroll(home,0));

    xs::CinematicHomeContent preview;
    preview.channels.assign(10,home.entries[1]);
    preview.movies.assign(10,home.entries[21]);
    preview.series.assign(10,home.entries[11]);
    int last_index=-1;
    const auto assembled=xs::make_nova_home(preview,{},last_index);
    assert(assembled.entries.size()==30);
    assert(xs::nova_row_of(assembled.entries.front())==0);
    assert(xs::nova_row_of(assembled.entries[10])==1);
    assert(xs::nova_row_of(assembled.entries[20])==2);

    xs::Catalog browse;browse.title="Canales";
    for(int i=0;i<17;++i){
        xs::Entry e;e.kind=xs::EntryKind::Channel;browse.entries.push_back(e);
    }
    assert(xs::nova_page_size(browse)==12);
    assert(xs::nova_tiles(browse,13).front().index==12);
    assert(xs::nova_hit(browse,13,-1,250,385)==12);
    xs::Catalog movies;movies.title="Películas";
    for(int i=0;i<15;++i){
        xs::Entry e;e.kind=xs::EntryKind::Asset;movies.entries.push_back(e);
    }
    assert(xs::nova_is_posters(movies));
    assert(xs::nova_page_size(movies)==6);
    assert(xs::nova_tiles(movies,8).front().index==6);
    return 0;
}
