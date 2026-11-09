#include "cinematic_layout.hpp"
#include <cassert>
int main(){
    xs::Catalog home;home.title="Inicio";
    xs::Entry last;last.title="Último";last.kind=xs::EntryKind::Channel;home.entries.push_back(last);
    for(int i=0;i<6;++i){xs::Entry e;e.kind=xs::EntryKind::Channel;home.entries.push_back(e);}
    for(int i=0;i<4;++i){xs::Entry e;e.kind=xs::EntryKind::Asset;e.program_type="1";home.entries.push_back(e);}
    for(int i=0;i<4;++i){xs::Entry e;e.kind=xs::EntryKind::Asset;e.program_type="0";home.entries.push_back(e);}
    const auto tiles=xs::cinematic_tiles(home,0,0);
    assert(tiles.size()==15);
    assert(xs::cinematic_hit(home,0,0,35,323)==0);
    assert(xs::cinematic_hit(home,0,0,35,432)==1);
    assert(xs::cinematic_hit(home,0,0,35,548)==7);
    assert(xs::cinematic_hit(home,0,0,660,548)==11);
    assert(xs::cinematic_hit(home,0,0,1240,690)==-1);
    xs::Catalog movies;movies.title="Películas";
    for(int i=0;i<16;++i){xs::Entry e;e.kind=xs::EntryKind::Asset;e.program_type="1";movies.entries.push_back(e);}
    assert(xs::is_poster_catalog(movies));
    assert(xs::cinematic_columns(movies)==8);
    assert(xs::cinematic_page_size(movies)==8);
    assert(xs::cinematic_tiles(movies,9).front().index==8);
    assert(xs::cinematic_hit(movies,9,-1,33,430)==8);
    return 0;
}
