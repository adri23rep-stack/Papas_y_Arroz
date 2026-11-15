#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "remote_browse.hpp"
#include "remote_server.hpp"
#include <cassert>
#include <chrono>
#include <memory>
#include <thread>
namespace xs {
struct ServiceSession {};
BrowseResult service_browse_remote(const std::shared_ptr<ServiceSession>&,const Entry& entry){
    BrowseResult r;r.ok=true;r.catalog.title=entry.title;
    if(entry.kind==EntryKind::Category){
        Entry a;a.kind=EntryKind::Channel;a.title="Canal de prueba";a.channel_code="c1";
        r.catalog.entries.push_back(a);
    }else if(entry.kind==EntryKind::Section){
        Entry a;a.kind=EntryKind::VodCategory;a.title="Accion";a.program_type=entry.program_type;a.column_id=10;
        r.catalog.entries.push_back(a);
    }else if(entry.kind==EntryKind::VodCategory){
        Entry a;a.kind=EntryKind::Asset;a.title="Pelicula";a.program_type="1";a.content_id="movie1";
        r.catalog.entries.push_back(a);
    }else if(entry.kind==EntryKind::Asset){
        Entry a;a.kind=EntryKind::Episode;a.title="Reproducir pelicula";a.content_id="ep1";
        r.catalog.entries.push_back(a);
    }
    return r;
}
}
int main(){
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_EVENTS)==0);
    xs::RemoteBrowse browse;
    xs::Catalog root;root.title="Raiz";
    xs::Entry cat;cat.title="Canales";cat.kind=xs::EntryKind::Category;cat.column_id=5;
    xs::Entry movie;movie.title="Peliculas";movie.kind=xs::EntryKind::Section;
    movie.program_type="1";movie.column_id=9;
    xs::Entry series;series.title="Series";series.kind=xs::EntryKind::Section;
    series.program_type="0";series.column_id=11;
    root.entries={cat,movie,series};
    browse.set_session(std::make_shared<xs::ServiceSession>(),root);
    uint32_t generation=0;std::string error;
    assert(browse.start(generation,error));
    const auto start=browse.snapshot(generation);
    assert(start.state=="ready"&&start.items.size()==3);
    assert(start.items[0].second.kind==xs::EntryKind::LiveSection);
    uint32_t next=0;
    assert(browse.select(generation,0,next,error)==xs::BrowseSelection::Loading);
    auto page=browse.snapshot(next);
    // Live category does not contact any third-party service.
    for(int i=0;i<50&&page.state=="loading";++i){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        page=browse.snapshot(next);
    }
    assert(page.state=="ready"&&page.items.size()==1);
    uint32_t channel_gen=0;
    assert(browse.select(next,0,channel_gen,error)==xs::BrowseSelection::Loading);
    for(int i=0;i<50;++i){
        page=browse.snapshot(channel_gen);
        if(page.state=="ready")break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(page.state=="ready"&&page.items[0].second.kind==xs::EntryKind::Channel);
    uint32_t play_gen=0;
    assert(browse.select(channel_gen,0,play_gen,error)==xs::BrowseSelection::Play);
    xs::Catalog playback;assert(browse.selected_result(play_gen,0,playback));
    SDL_Event ev{};assert(SDL_PollEvent(&ev)&&ev.type==SDL_USEREVENT&&
        ev.user.code==xs::REMOTE_OPEN_BROWSE);
    assert(browse.back(channel_gen,next,error));
    assert(browse.back(next,next,error));
    assert(browse.start(generation,error));
    assert(browse.select(generation,1,next,error)==xs::BrowseSelection::Loading);
    for(int i=0;i<50;++i){
        page=browse.snapshot(next);
        if(page.state=="ready")break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(page.state=="ready"&&page.items[0].second.kind==xs::EntryKind::VodCategory);
    uint32_t asset_gen=0;
    assert(browse.select(next,0,asset_gen,error)==xs::BrowseSelection::Loading);
    for(int i=0;i<50;++i){
        page=browse.snapshot(asset_gen);
        if(page.state=="ready")break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(page.state=="ready"&&page.items[0].second.kind==xs::EntryKind::Asset);
    uint32_t movie_gen=0;
    assert(browse.select(asset_gen,0,movie_gen,error)==xs::BrowseSelection::Loading);
    for(int i=0;i<100;++i){
        page=browse.snapshot(movie_gen);
        if(page.state=="ready")break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(page.state=="ready"&&page.auto_played);
    assert(page.items[0].second.kind==xs::EntryKind::Episode);
    assert(browse.selected_result(movie_gen,0,playback));
    assert(SDL_PollEvent(&ev)&&ev.user.code==xs::REMOTE_OPEN_BROWSE);
    browse.stop();
    SDL_Quit();
    return 0;
}
