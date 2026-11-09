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
BrowseResult service_search_remote(const std::shared_ptr<ServiceSession>&,const std::string&){
    return {};
}
}
#define REQUIRE(condition) do{if(!(condition)){std::fprintf(stderr,"FAILED at %d: %s\n",__LINE__,#condition);return 1;}}while(0)
int main(){
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
    REQUIRE(!okay(post("/catalog/search",R"({"query":"test"})")));
    REQUIRE(!okay(post("/catalog/search",R"({"query":"test"})",token))); // No service session yet
    REQUIRE(!okay(post("/catalog/open",R"({"generation":1,"index":0})",token)));

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
    bool saw_text=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_TEXTINPUT&&std::strcmp(e.text.text,"hola")==0)saw_text=true;
    REQUIRE(saw_text);
    REQUIRE(okay(post("/input",R"({"action":"tv"})",token)));
    bool saw_tv=false;
    while(SDL_PollEvent(&e))if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_F5)saw_tv=true;
    REQUIRE(saw_tv);
    server.stop();
    SDL_Quit();
    std::remove("remote_secret.txt");
    return 0;
}
