#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <SDL.h>
#undef main
#include "remote_server.hpp"
#include "json.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <filesystem>
#include <windows.h>

static std::string post(const std::string& path,const std::string& body,
                        const std::string& token=""){
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET)return "";
    sockaddr_in address{};
    address.sin_family=AF_INET;address.sin_port=htons(47831);
    inet_pton(AF_INET,"127.0.0.1",&address.sin_addr);
    if(connect(s,reinterpret_cast<const sockaddr*>(&address),sizeof(address))!=0){
        closesocket(s);return "";
    }
    const std::string auth=token.empty()?"":"Authorization: Bearer "+token+"\r\n";
    const std::string request="POST "+path+" HTTP/1.1\r\nHost: 127.0.0.1\r\n"+
        auth+"Content-Type: application/json\r\nConnection: close\r\n"+
        "Content-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
    size_t offset=0;
    while(offset<request.size()){
        const int count=send(s,request.data()+offset,static_cast<int>(request.size()-offset),0);
        if(count<=0){closesocket(s);return "";}
        offset+=count;
    }
    std::string response;char b[1024];
    while(true){
        const int n=recv(s,b,sizeof(b),0);
        if(n<=0)break;
        response.append(b,static_cast<size_t>(n));
        if(response.size()>10000)break;
    }
    closesocket(s);return response;
}
static bool okay(const std::string& response){
    return response.find("HTTP/1.1 200 OK")==0;
}
static std::string payload(const std::string& response){
    const size_t start=response.find("\r\n\r\n");
    return start==std::string::npos?"":response.substr(start+4);
}
// Keep the LAN protocol test offline: no service credentials or network
// catalogue calls are used in CI.
namespace xs {
struct ServiceSession {};
BrowseResult service_search_remote(const std::shared_ptr<ServiceSession>&,const std::string&){
    return {};
}
BrowseResult service_browse_remote(const std::shared_ptr<ServiceSession>&,const Entry& entry){
    BrowseResult out;out.ok=true;out.catalog.title=entry.title;
    if(entry.kind==EntryKind::Category){
        Entry sample;sample.kind=EntryKind::Channel;
        sample.title="HTTPS directo";sample.channel_code="direct";
        out.catalog.entries.push_back(sample);
        sample.title="Autorización especial";sample.channel_code="locked";
        out.catalog.entries.push_back(sample);
    }
    return out;
}
SourceResult service_source_remote(const std::shared_ptr<ServiceSession>&,
                                   const Entry& entry,int){
    SourceResult out;out.ok=true;
    out.source="https://example.org/demo.m3u8";
    if(entry.channel_code=="locked")
        out.headers={{"Content-License","test-secret-header"}};
    return out;
}
}
#define REQUIRE(condition) do{std::fprintf(stderr,"Checking %d: %s\\n",__LINE__,#condition);std::fflush(stderr);if(!(condition)){std::fprintf(stderr,"FAILED at %d: %s\\n",__LINE__,#condition);return 1;}}while(0)
int main(){
    // CI must never alter a real Windows user's remembered phone pairing.
    wchar_t temp_dir[MAX_PATH]{};
    REQUIRE(GetTempPathW(MAX_PATH,temp_dir)>0);
    const auto root=std::filesystem::path(temp_dir)/
        (L"xuper-remote-server-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    REQUIRE(SetEnvironmentVariableW(L"XUPER_PAIRING_TEST_DIR",root.c_str())!=0);
    REQUIRE(SDL_Init(SDL_INIT_EVENTS)==0);
    auto& server=xs::RemoteServer::shared();
    REQUIRE(server.start());
    const auto info=server.info();
    REQUIRE(info.running&&info.pin.size()==6);
    REQUIRE(!okay(post("/input",R"({"action":"right"})")));
    REQUIRE(!okay(post("/pair",R"({"pin":"111wrong"})")));
    const auto pair=post("/pair",std::string("{\"pin\":\"")+info.pin+"\"}");
    REQUIRE(okay(pair));
    auto data=nlohmann::json::parse(payload(pair),nullptr,false);
    REQUIRE(data.is_object()&&data.contains("token")&&data["token"].is_string());
    const std::string token=data["token"].get<std::string>();
    REQUIRE(token.size()==64);
    REQUIRE(!okay(post("/input",R"({"action":"youtube_open"})"))); // unpaired
    REQUIRE(!okay(post("/input",R"({"action":"youtube_search","text":"gatos"})",token))); // open browser first
    REQUIRE(!okay(post("/input",R"({"action":"youtube_pause"})",token))); // outside YouTube mode

    REQUIRE(!okay(post("/catalog/search",R"({"query":"test"})")));
    REQUIRE(!okay(post("/catalog/search",R"({"query":"test"})",token))); // No service session yet
    REQUIRE(!okay(post("/catalog/open",R"({"generation":1,"index":0})",token)));
    REQUIRE(!okay(post("/catalog/browse/start",R"({})")));
    REQUIRE(!okay(post("/catalog/browse/start",R"({})",token))); // no session
    REQUIRE(!okay(post("/mobile/catalog/start",R"({})"))); // pairing required
    REQUIRE(!okay(post("/mobile/catalog/start",R"({})",token))); // no session
    REQUIRE(!okay(post("/mobile/source",R"({"generation":1,"index":0})",token)));

    REQUIRE(!okay(post("/catalog/browse/start",R"({"provider":"netflix"})"))); // unauthenticated
    REQUIRE(!okay(post("/catalog/browse/start",R"({"provider":"notreal"})",token)));
    REQUIRE(!okay(post("/catalog/browse/start",R"({"provider":5})",token)));
    REQUIRE(!okay(post("/catalog/browse/select",R"({"generation":1,"index":0})",token)));
    REQUIRE(!okay(post("/settings/get",R"({})")));
    const auto settings=post("/settings/get",R"({})",token);
    REQUIRE(okay(settings));
    const auto settings_json=nlohmann::json::parse(payload(settings),nullptr,false);
    REQUIRE(settings_json.contains("settings")&&settings_json["settings"].size()==11);
    // Check the new frame interpolation setting is exposed and validated.
    REQUIRE(okay(post("/settings/set",R"({"key":"motion_mode","value":2})",token)));
    REQUIRE(!okay(post("/settings/set",R"({"key":"motion_mode","value":3})",token)));
    REQUIRE(!okay(post("/settings/set",R"({"key":"unknown","value":15})",token)));
    REQUIRE(!okay(post("/settings/set",R"({"key":"volume","value":101})",token)));
    REQUIRE(!okay(post("/settings/set",R"({"key":"speed","value":137})",token)));
    REQUIRE(okay(post("/settings/set",R"({"key":"speed","value":125})",token)));
    bool saw_setting=false;
    SDL_Event setting_event{};
    while(SDL_PollEvent(&setting_event))
        if(setting_event.type==SDL_USEREVENT&&setting_event.user.code==xs::REMOTE_SETTING&&
           static_cast<int>(reinterpret_cast<intptr_t>(setting_event.user.data1))==
                xs::remote_setting_index("speed")&&
           static_cast<int>(reinterpret_cast<intptr_t>(setting_event.user.data2))==125)
            saw_setting=true;
    REQUIRE(saw_setting);
    REQUIRE(server.setting_value(xs::remote_setting_index("speed"))==125);
    REQUIRE(!okay(post("/player/select",R"({"generation":1,"kind":"audio","id":2})",token)));
    const auto idle=nlohmann::json::parse(payload(post("/player/options",R"({})",token)));
    REQUIRE(idle["active"]==false);
    server.begin_playback("Prueba de audio");
    xs::RemotePlaybackSnapshot snapshot;
    snapshot.active=true;snapshot.title="Prueba de audio";
    snapshot.audio={{1,"audio","Portugués",true},{2,"audio","Español",false}};
    snapshot.subtitles={{-1,"subtitle","Desactivados",true},
                        {5,"subtitle","Español",false},
                        {0,"external_subtitle","Servicio: español",false}};
    snapshot.sources={{0,"source","Automático",true},{1,"source","HD",false}};
    server.publish_playback_options(std::move(snapshot));
    const auto track_json=nlohmann::json::parse(payload(post("/player/options",R"({})",token)));
    REQUIRE(track_json["active"]==true&&track_json["audio"].size()==2);
    REQUIRE(track_json["subtitles"].size()==3&&track_json["sources"].size()==2);
    const auto track_generation=track_json["generation"].get<int>();
    const auto choose=[&](const std::string& kind,int id,int gen){
        return post("/player/select",nlohmann::json{
           {"kind",kind},{"id",id},{"generation",gen}}.dump(),token);
    };
    REQUIRE(!okay(choose("audio",99,track_generation)));
    REQUIRE(!okay(choose("audio",2,track_generation-1)));
    REQUIRE(okay(choose("audio",2,track_generation)));
    bool got_audio=false;SDL_Event track_event{};
    while(SDL_PollEvent(&track_event))
        if(track_event.type==SDL_USEREVENT&&track_event.user.code==xs::REMOTE_PLAYBACK_SELECT&&
           reinterpret_cast<intptr_t>(track_event.user.data1)==1&&
           reinterpret_cast<intptr_t>(track_event.user.data2)==2)got_audio=true;
    REQUIRE(got_audio);
    REQUIRE(okay(choose("subtitle",-1,track_generation)));
    REQUIRE(okay(choose("external_subtitle",0,track_generation)));
    REQUIRE(okay(choose("source",1,track_generation)));
    server.end_playback();
    REQUIRE(!okay(choose("audio",2,track_generation)));
    REQUIRE(nlohmann::json::parse(payload(post("/player/options",R"({})",token)))["active"]==false);

    // Veyra controls are conditional, report engine-confirmed state, and reject
    // multiplier gaps even if the numeric value lies inside the slider range.
    REQUIRE(!okay(post("/settings/set",R"({"key":"veyra_fg","value":1})",token)));
    server.begin_playback("Veyra fixture");
    xs::RemotePlaybackSnapshot veyra;
    veyra.engine="veyra";
    veyra.veyraSettings={{0,1,0,0,4}};
    veyra.veyraMultipliers={2,4};
    server.publish_playback_options(veyra);
    const auto veyra_json=nlohmann::json::parse(payload(post("/settings/get",R"({})",token)));
    bool foundFg=false,foundMultiplier=false;
    for(const auto& setting:veyra_json["settings"]){
        REQUIRE(setting["key"]!="video_preset" && setting["key"]!="motion_mode");
        if(setting["key"]=="veyra_fg"){foundFg=true;REQUIRE(setting["value"]==0);}
        if(setting["key"]=="veyra_multiplier"){
            foundMultiplier=true;
            REQUIRE(setting["min"]==2 && setting["max"]==4 && setting["value"]==4);
            REQUIRE(setting["choices"]==nlohmann::json::array({2,4}));
        }
    }
    REQUIRE(foundFg && foundMultiplier);
    REQUIRE(!okay(post("/settings/set",R"({"key":"veyra_multiplier","value":3})",token)));
    REQUIRE(okay(post("/settings/set",R"({"key":"veyra_multiplier","value":4})",token)));
    REQUIRE(okay(post("/settings/set",R"({"key":"veyra_fg","value":1})",token)));
    REQUIRE(server.playback_options().veyraSettings[0]==0); // a request isn't an acknowledgement
    server.end_playback();
    REQUIRE(!okay(post("/settings/set",R"({"key":"veyra_fg","value":1})",token)));

    xs::Catalog mock_root;
    xs::Entry live;live.kind=xs::EntryKind::Category;
    live.title="TV";live.column_id=5;
    mock_root.entries={live};
    server.set_session(std::make_shared<xs::ServiceSession>(),mock_root);
    xs::Entry movie, series;
    movie.kind=series.kind=xs::EntryKind::Asset;
    movie.title="Prueba cine";movie.content_id="movie";movie.program_type="1";
    series.title="Prueba serie";series.content_id="series";series.program_type="0";
    server.index_assets({movie,series});
    const auto search=post("/catalog/search",R"({"query":"prueba"})",token);
    REQUIRE(okay(search));
    const auto ticket=nlohmann::json::parse(payload(search)).at("generation");
    nlohmann::json hits;
    for(int i=0;i<50;++i){
        const auto response=post("/catalog/results",nlohmann::json({{"generation",ticket}}).dump(),token);
        hits=nlohmann::json::parse(payload(response));
        if(hits["state"]=="ready")break;
        SDL_Delay(10);
    }
    REQUIRE(hits["items"].size()==2);
    REQUIRE(hits["items"][0]["kind"]=="movies" && hits["items"][1]["kind"]=="series");
    const auto mobile_start=post("/mobile/catalog/start",R"({})",token);
    REQUIRE(okay(mobile_start));
    const auto first=nlohmann::json::parse(payload(mobile_start));
    auto generation=first["generation"].get<uint32_t>();
    const auto mobile_page=post("/mobile/catalog/results",
        std::string("{\"generation\":")+std::to_string(generation)+"}",token);
    REQUIRE(okay(mobile_page));
    const auto page=nlohmann::json::parse(payload(mobile_page));
    REQUIRE(page["items"].is_array());
    // Index 1 is the category "TV en vivo".
    const auto live_start=post("/mobile/catalog/select",
        std::string("{\"generation\":")+std::to_string(generation)+",\"index\":1}",token);
    REQUIRE(okay(live_start));
    generation=nlohmann::json::parse(payload(live_start))["generation"].get<uint32_t>();
    for(int tries=0;tries<50;++tries){
        const auto reply=post("/mobile/catalog/results",
            std::string("{\"generation\":")+std::to_string(generation)+"}",token);
        REQUIRE(okay(reply));
        if(nlohmann::json::parse(payload(reply))["state"]=="ready")break;
        SDL_Delay(10);
    }
    const auto select_cat=post("/mobile/catalog/select",
        std::string("{\"generation\":")+std::to_string(generation)+",\"index\":0}",token);
    REQUIRE(okay(select_cat));
    generation=nlohmann::json::parse(payload(select_cat))["generation"].get<uint32_t>();
    for(int tries=0;tries<50;++tries){
        const auto reply=post("/mobile/catalog/results",
            std::string("{\"generation\":")+std::to_string(generation)+"}",token);
        REQUIRE(okay(reply));
        if(nlohmann::json::parse(payload(reply))["state"]=="ready")break;
        SDL_Delay(10);
    }
    const std::string direct=std::string("{\"generation\":")+
       std::to_string(generation)+",\"index\":0}";
    const auto selected=post("/mobile/catalog/select",direct,token);
    REQUIRE(okay(selected)&&nlohmann::json::parse(payload(selected))["state"]=="playable");
    REQUIRE(!okay(post("/mobile/source",direct))); // source must be authenticated
    const auto source=post("/mobile/source",direct,token);
    REQUIRE(okay(source)&&nlohmann::json::parse(payload(source))["url"]==
            "https://example.org/demo.m3u8");
    const std::string restricted=std::string("{\"generation\":")+
       std::to_string(generation)+",\"index\":1}";
    REQUIRE(!okay(post("/mobile/source",restricted,token)));
    REQUIRE(payload(post("/mobile/source",restricted,token)).find("test-secret-header")==
            std::string::npos);
    SDL_Event no_mobile_event{};
    bool played_on_pc=false;
    while(SDL_PollEvent(&no_mobile_event))
        if(no_mobile_event.type==SDL_USEREVENT&&
           no_mobile_event.user.code==xs::REMOTE_OPEN_BROWSE)played_on_pc=true;
    REQUIRE(!played_on_pc);



    REQUIRE(okay(post("/input",R"({"action":"right"})",token)));
    SDL_Event e{};bool saw_right=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_RIGHT)saw_right=true;
    REQUIRE(saw_right);
    REQUIRE(okay(post("/input",R"({"action":"volume","value":42})",token)));
    bool saw_volume=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_USEREVENT&&e.user.code==xs::REMOTE_VOLUME&&
        static_cast<int>(reinterpret_cast<intptr_t>(e.user.data1))==42)saw_volume=true;
    REQUIRE(saw_volume&&server.volume()==42);
    REQUIRE(okay(post("/input",R"({"action":"pointer_move","dx":42,"dy":-21})",token)));
    bool saw_move=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_MOUSEMOTION&&e.motion.x==682&&e.motion.y==339)saw_move=true;
    REQUIRE(saw_move);
    REQUIRE(okay(post("/input",R"({"action":"text","text":"hola"})",token)));
    REQUIRE(server.take_text()=="hola");
    REQUIRE(okay(post("/input",R"({"action":"tv"})",token)));
    bool saw_tv=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_F5)saw_tv=true;
    REQUIRE(saw_tv);
    REQUIRE(okay(post("/input",R"({"action":"fullscreen"})",token)));
    bool saw_fullscreen=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_F11)saw_fullscreen=true;
    REQUIRE(saw_fullscreen);
    REQUIRE(okay(post("/input",R"({"action":"wheel","value":-2})",token)));
    bool saw_wheel=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_USEREVENT&&e.user.code==xs::REMOTE_WHEEL&&static_cast<int>(reinterpret_cast<intptr_t>(e.user.data1))==-2)saw_wheel=true;
    REQUIRE(saw_wheel);
    REQUIRE(!okay(post("/input",R"({"action":"wheel_drop"})",token)));
    server.stop();
    SDL_Quit();
    std::error_code cleanup;
    std::filesystem::remove_all(root,cleanup);
    SetEnvironmentVariableW(L"XUPER_PAIRING_TEST_DIR",nullptr);
    return 0;
}
