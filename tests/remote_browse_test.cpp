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
BrowseResult service_browse_remote(const std::shared_ptr<ServiceSession>&,const Entry& entry,std::shared_ptr<ServiceRequest>){
    BrowseResult r;r.ok=true;r.catalog.title=entry.title;
    if(entry.kind==EntryKind::Category){
        Entry tv;tv.kind=EntryKind::Channel;tv.title="Canal de prueba";tv.channel_code="c1";
        r.catalog.entries.push_back(tv);
    }else if(entry.kind==EntryKind::Section){
        Entry action;action.kind=EntryKind::VodCategory;action.title="Acción";
        action.program_type=entry.program_type;action.column_id=10;
        r.catalog.entries.push_back(action);
        Entry found;found.kind=EntryKind::VodCategory;
        found.program_type=entry.program_type;
        if(entry.program_type=="1"){
            found.title="Netflix Peliculas";found.column_id=222;
        }else{
            found.title="Apple TV+ Series";found.column_id=333;
        }
        r.catalog.entries.push_back(found);
        if(entry.program_type=="1"){
            found.title="HBO Max Peliculas";found.column_id=444;
            r.catalog.entries.push_back(found);
        }
    }else if(entry.kind==EntryKind::VodCategory){
        Entry movie;movie.kind=EntryKind::Asset;movie.title="Contenido real";
        movie.program_type=entry.program_type;movie.content_id="asset1";
        r.catalog.entries.push_back(movie);
    }else if(entry.kind==EntryKind::Asset){
        Entry play;play.kind=EntryKind::Episode;play.title="Reproducir";
        play.content_id="episode1";r.catalog.entries.push_back(play);
    }
    return r;
}
}
static xs::RemoteBrowseSnapshot ready(xs::RemoteBrowse& browse,uint32_t gen){
    xs::RemoteBrowseSnapshot page;
    for(int i=0;i<250;++i){
        page=browse.snapshot(gen);
        if(page.state!="loading")return page;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(false&&"Timed out loading mock catalog");
    return page;
}
static bool saw_play_event(){
    SDL_Event ev{};
    for(int i=0;i<100;++i){
        while(SDL_PollEvent(&ev))
            if(ev.type==SDL_USEREVENT&&ev.user.code==xs::REMOTE_OPEN_BROWSE)
                return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}
int main(){
    using namespace xs;
    SDL_SetMainReady();assert(SDL_Init(SDL_INIT_EVENTS)==0);
    RemoteBrowse browse;
    Catalog root;root.title="Inicio";
    Entry tv;tv.kind=EntryKind::Category;tv.title="TV";tv.column_id=5;
    Entry movies;movies.kind=EntryKind::Section;movies.title="Peliculas";
    movies.program_type="1";movies.column_id=9;
    Entry series;series.kind=EntryKind::Section;series.title="Series";
    series.program_type="0";series.column_id=11;
    root.entries={tv,movies,series};
    browse.set_session(std::make_shared<ServiceSession>(),root);
    uint32_t gen=0,next=0;std::string err;
    assert(!browse.start(gen,err,"not_a_provider"));
    assert(browse.start(gen,err));
    auto page=ready(browse,gen);
    assert(page.state=="ready"&&page.items.size()==4);
    assert(page.items[0].second.kind==EntryKind::ProviderDirectory);
    assert(page.items[1].second.kind==EntryKind::LiveSection);
    assert(browse.select(gen,0,next,err)==BrowseSelection::Loading);
    page=ready(browse,next);
    assert(page.state=="ready"&&page.items.size()==3);
    assert(page.items[0].second.title=="Netflix");
    assert(page.items[1].second.title=="HBO Max");
    assert(page.items[2].second.title=="Apple TV");
    // A provider always points to real categories returned by the service.
    uint32_t category_gen=0;
    assert(browse.select(next,0,category_gen,err)==BrowseSelection::Loading);
    page=ready(browse,category_gen);
    assert(page.state=="ready"&&page.items.size()==1);
    assert(page.items[0].second.column_id==222);
    assert(page.items[0].second.kind==EntryKind::VodCategory);
    assert(browse.select(category_gen,0,next,err)==BrowseSelection::Loading);
    page=ready(browse,next);
    assert(page.state=="ready"&&page.items[0].second.kind==EntryKind::Asset);
    uint32_t episode_gen=0;
    assert(browse.select(next,0,episode_gen,err)==BrowseSelection::Loading);
    page=ready(browse,episode_gen);
    assert(page.auto_played&&page.items[0].second.kind==EntryKind::Episode);
    assert(saw_play_event());
    Catalog playable;assert(browse.selected_result(episode_gen,0,playable));
    // Direct opening from an existing iPhone brand tile works too.
    assert(browse.start(gen,err,"apple_tv"));
    page=ready(browse,gen);
    assert(page.state=="ready"&&page.title=="Apple TV");
    assert(page.items.size()==1&&page.items[0].second.column_id==333);
    // If the service has no matching provider, show an honest empty state.
    assert(browse.start(gen,err,"youtube"));
    page=ready(browse,gen);
    assert(page.state=="ready"&&page.items.empty());
    assert(page.message.find("no tiene una sección")!=std::string::npos);
    // Existing TV and generic movies browse remain available.
    assert(browse.start(gen,err));
    assert(browse.select(gen,1,next,err)==BrowseSelection::Loading);
    page=ready(browse,next);
    assert(page.state=="ready"&&page.items[0].second.kind==EntryKind::Category);
    assert(browse.select(next,0,category_gen,err)==BrowseSelection::Loading);
    page=ready(browse,category_gen);
    assert(page.items[0].second.kind==EntryKind::Channel);
    assert(browse.select(category_gen,0,next,err)==BrowseSelection::Play);
    assert(saw_play_event());
    // A private phone browser is entirely independent of Windows playback.
    xs::RemoteBrowse phone;
    phone.set_session(std::make_shared<ServiceSession>(),root);
    assert(phone.start(gen,err,"",true));
    assert(phone.select(gen,1,next,err)==BrowseSelection::Loading);
    page=ready(phone,next);
    assert(page.state=="ready");
    assert(phone.select(next,0,category_gen,err)==BrowseSelection::Loading);
    page=ready(phone,category_gen);
    assert(page.items[0].second.kind==EntryKind::Channel);
    assert(phone.select(category_gen,0,next,err)==BrowseSelection::Play);
    Entry direct;std::shared_ptr<ServiceSession> isolated;
    assert(phone.mobile_selection(next,0,direct,isolated));
    assert(direct.kind==EntryKind::Channel&&isolated!=nullptr);
    SDL_Event no_mobile_event{};bool unexpected=false;
    while(SDL_PollEvent(&no_mobile_event))
        if(no_mobile_event.type==SDL_USEREVENT&&
           no_mobile_event.user.code==REMOTE_OPEN_BROWSE)unexpected=true;
    assert(!unexpected);
    phone.stop();
    browse.stop();SDL_Quit();return 0;
}
