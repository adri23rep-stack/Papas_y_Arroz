#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#endif
#include "remote_server.hpp"
#include "youtube_remote.hpp"
#ifdef _WIN32
// Windows desktop only. The Switch homebrew build does not link this file.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "json.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
namespace xs {
namespace {
constexpr int kPort=47831;
constexpr int kMaxBody=4096;
constexpr SOCKET invalid_socket=INVALID_SOCKET;
using Json=nlohmann::json;
bool random_bytes(unsigned char* bytes,size_t length){
    return BCryptGenRandom(nullptr,bytes,static_cast<ULONG>(length),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0;
}
std::string make_secret(){
    unsigned char bytes[32]{};
    if(!random_bytes(bytes,sizeof(bytes)))return "";
    const char* hex="0123456789abcdef";std::string value;value.reserve(64);
    for(unsigned char c:bytes){value+=hex[c>>4];value+=hex[c&15];}
    return value;
}
bool valid_secret(const std::string& s){
    return s.size()==64&&std::all_of(s.begin(),s.end(),[](unsigned char ch){return std::isxdigit(ch)!=0;});
}
bool same_secret(const std::string& a,const std::string& b){
    if(a.size()!=64||b.size()!=64)return false;
    unsigned char diff=0;
    for(size_t i=0;i<64;++i)diff|=static_cast<unsigned char>(a[i]^b[i]);
    return diff==0;
}
bool lan_address(const sockaddr_in& address){
    const uint32_t host=ntohl(address.sin_addr.s_addr);
    const uint8_t a=host>>24,b=(host>>16)&255;
    return a==10||a==127||a==192&&b==168||a==172&&b>=16&&b<=31||a==169&&b==254;
}
std::string local_address(){
    SOCKET socket_=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(socket_!=invalid_socket){
        sockaddr_in remote{};remote.sin_family=AF_INET;remote.sin_port=htons(53);
        inet_pton(AF_INET,"1.1.1.1",&remote.sin_addr);
        if(connect(socket_,reinterpret_cast<const sockaddr*>(&remote),sizeof(remote))==0){
            sockaddr_in local{};int size=sizeof(local);
            if(getsockname(socket_,reinterpret_cast<sockaddr*>(&local),&size)==0){
                char value[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET,&local.sin_addr,value,sizeof(value));
                closesocket(socket_);
                return value;
            }
        }
        closesocket(socket_);
    }
    char hostname[256]{};
    if(gethostname(hostname,sizeof(hostname))==0){
        addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
        addrinfo* result=nullptr;
        if(getaddrinfo(hostname,nullptr,&hints,&result)==0){
            for(addrinfo* p=result;p;p=p->ai_next){
                if(p->ai_addr&&lan_address(*reinterpret_cast<sockaddr_in*>(p->ai_addr))){
                    char ip[INET_ADDRSTRLEN]{};
                    inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr,ip,sizeof(ip));
                    freeaddrinfo(result);return ip;
                }
            }
            freeaddrinfo(result);
        }
    }
    return "127.0.0.1";
}
void reply(SOCKET socket_,int code,const Json& message){
    const std::string body=message.dump();
    std::ostringstream data;
    data<<"HTTP/1.1 "<<code<<(code==200?" OK":code==403?" Forbidden":code==401?" Unauthorized":code==429?" Too Many Requests":" Bad Request")<<"\r\n"
        <<"Content-Type: application/json; charset=utf-8\r\n"
        <<"Cache-Control: no-store\r\n"
        <<"Connection: close\r\n"
        <<"X-Content-Type-Options: nosniff\r\n"
        <<"Content-Length: "<<body.size()<<"\r\n\r\n"<<body;
    const std::string response=data.str();
    size_t sent=0;
    while(sent<response.size()){
        int bytes=send(socket_,response.data()+sent,
                       static_cast<int>(std::min<size_t>(response.size()-sent,8192)),0);
        if(bytes<=0)break;
        sent+=static_cast<size_t>(bytes);
    }
}
bool parse_request(SOCKET socket_,std::string& request,std::string& body){
    std::string raw;
    raw.reserve(2048);
    char block[2048];
    size_t header_end=std::string::npos;
    while(raw.size()<8192){
        int received=recv(socket_,block,sizeof(block),0);
        if(received<=0)return false;
        raw.append(block,static_cast<size_t>(received));
        header_end=raw.find("\r\n\r\n");
        if(header_end!=std::string::npos)break;
    }
    if(header_end==std::string::npos||header_end>4096)return false;
    request=raw.substr(0,header_end+4);
    std::istringstream headers(request);
    std::string line;
    int content_length=-1;
    std::getline(headers,line); // request line
    while(std::getline(headers,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        std::string lower=line;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return std::tolower(c);});
        if(lower.rfind("content-length:",0)==0){
            try {content_length=std::stoi(line.substr(15));}catch(...){return false;}
        }
        // Browser cross-origin requests are intentionally unsupported.
        if(lower.rfind("origin:",0)==0)return false;
    }
    if(content_length<0||content_length>kMaxBody)return false;
    body=raw.substr(header_end+4);
    while(body.size()<static_cast<size_t>(content_length)){
        int got=recv(socket_,block,static_cast<int>(std::min<size_t>(sizeof(block),content_length-body.size())),0);
        if(got<=0)return false;
        body.append(block,static_cast<size_t>(got));
    }
    if(body.size()!=static_cast<size_t>(content_length))body.resize(static_cast<size_t>(content_length));
    return true;
}
bool authorized(const std::string& request,const std::string& secret){
    std::istringstream input(request);std::string line;
    while(std::getline(input,line)){
        std::string lower=line;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return std::tolower(ch);});
        if(lower.rfind("authorization: bearer ",0)==0){
            std::string token=line.substr(22);
            while(!token.empty()&&(token.back()=='\r'||token.back()==' '))token.pop_back();
            return same_secret(token,secret);
        }
    }
    return false;
}
}
RemoteServer& RemoteServer::shared(){static RemoteServer instance;return instance;}
bool RemoteServer::start(){
    if(running_)return true;
    WSADATA ws{};
    if(WSAStartup(MAKEWORD(2,2),&ws)!=0)return false;
    std::ifstream stored("remote_secret.txt",std::ios::binary);
    std::getline(stored,secret_);
    if(!valid_secret(secret_)){
        secret_=make_secret();
        if(secret_.empty()){WSACleanup();return false;}
        std::ofstream saved("remote_secret.txt",std::ios::binary|std::ios::trunc);
        saved<<secret_<<"\n";
    }
    unsigned char digits[4]{};
    if(!random_bytes(digits,sizeof(digits))){WSACleanup();return false;}
    const uint32_t number=(static_cast<uint32_t>(digits[0])<<24)|
        (static_cast<uint32_t>(digits[1])<<16)|(static_cast<uint32_t>(digits[2])<<8)|digits[3];
    char pin[16]{};
    std::snprintf(pin,sizeof(pin),"%06u",number%1000000U);
    pin_=pin;
    address_=local_address();
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==invalid_socket){WSACleanup();return false;}
    BOOL exclusive=TRUE;
    setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(kPort);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(s,reinterpret_cast<const sockaddr*>(&addr),sizeof(addr))!=0||listen(s,8)!=0){
        closesocket(s);WSACleanup();return false;
    }
    listener_=static_cast<uintptr_t>(s);
    running_=true;
    worker_=std::thread(&RemoteServer::serve,this);
    return true;
}
void RemoteServer::stop(){
    if(!running_.exchange(false))return;
    youtube_mode_=false;
    catalog_.stop();
    if(listener_!=static_cast<uintptr_t>(~uintptr_t{0})){
        shutdown(static_cast<SOCKET>(listener_),SD_BOTH);
        closesocket(static_cast<SOCKET>(listener_));
        listener_=static_cast<uintptr_t>(~uintptr_t{0});
    }
    if(worker_.joinable())worker_.join();
    WSACleanup();
}
RemoteInfo RemoteServer::info()const{
    const Uint32 last=active_at_.load();
    return {address_,pin_,running_.load(),last!=0&&(SDL_GetTicks()-last)<30000};
}
bool RemoteServer::cursor_visible()const{
    const Uint32 last=pointer_at_.load();
    return last!=0&&(SDL_GetTicks()-last)<5000;
}
void RemoteServer::draw_cursor(SDL_Renderer* renderer)const{
    if(!renderer||youtube_mode_.load()||!cursor_visible())return;
    const int x=pointer_x_.load(),y=pointer_y_.load();
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,0,0,0,230);
    SDL_Rect shadow{x-3,y-3,24,24};SDL_RenderDrawRect(renderer,&shadow);
    SDL_SetRenderDrawColor(renderer,252,93,114,255);
    SDL_Rect box{x-1,y-1,20,20};SDL_RenderDrawRect(renderer,&box);
    SDL_RenderDrawLine(renderer,x+9,y-5,x+9,y+24);
    SDL_RenderDrawLine(renderer,x-5,y+9,x+24,y+9);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
}
void RemoteServer::push_key(SDL_Keycode key){
    SDL_Event event{};event.type=SDL_KEYDOWN;event.key.state=SDL_PRESSED;
    event.key.repeat=0;event.key.keysym.sym=key;
    SDL_PushEvent(&event);
}
void RemoteServer::push_mouse(Uint32 kind){
    SDL_Event event{};event.type=kind;
    event.button.type=kind;event.button.button=SDL_BUTTON_LEFT;
    event.button.state=kind==SDL_MOUSEBUTTONDOWN?SDL_PRESSED:SDL_RELEASED;
    event.button.x=pointer_x_.load();event.button.y=pointer_y_.load();
    SDL_PushEvent(&event);
}
void RemoteServer::push_value(int code,int value){
    SDL_Event event{};event.type=SDL_USEREVENT;event.user.code=code;
    event.user.data1=reinterpret_cast<void*>(static_cast<intptr_t>(value));
    SDL_PushEvent(&event);
}
void RemoteServer::dispatch(const std::string& action,int dx,int dy,int value){
    if(action=="up")push_key(SDLK_UP);
    else if(action=="down")push_key(SDLK_DOWN);
    else if(action=="left")push_key(SDLK_LEFT);
    else if(action=="right")push_key(SDLK_RIGHT);
    else if(action=="ok")push_key(SDLK_RETURN);
    else if(action=="back")push_key(SDLK_b);
    else if(action=="home")push_key(SDLK_F4);
    else if(action=="tv")push_key(SDLK_F5);
    else if(action=="movies")push_key(SDLK_F6);
    else if(action=="series")push_key(SDLK_F7);
    else if(action=="search")push_key(SDLK_x);
    else if(action=="play_pause")push_key(SDLK_SPACE);
    else if(action=="menu")push_key(SDLK_TAB);
    else if(action=="next")push_key(SDLK_PAGEDOWN);
    else if(action=="previous")push_key(SDLK_PAGEUP);
    else if(action=="volume"){volume_=value;push_value(REMOTE_VOLUME,value);}
    else if(action=="brightness"){brightness_=value;push_value(REMOTE_BRIGHTNESS,value);}
    else if(action=="pointer_move"){
        pointer_x_=std::clamp(pointer_x_.load()+dx,0,1279);
        pointer_y_=std::clamp(pointer_y_.load()+dy,0,719);
        pointer_at_=SDL_GetTicks();
        SDL_Event event{};event.type=SDL_MOUSEMOTION;
        event.motion.x=pointer_x_.load();event.motion.y=pointer_y_.load();
        event.motion.xrel=dx;event.motion.yrel=dy;SDL_PushEvent(&event);
    }
    else if(action=="pointer_down"||action=="pointer_up"||action=="click"){
        pointer_at_=SDL_GetTicks();
        if(action=="pointer_down"||action=="click")push_mouse(SDL_MOUSEBUTTONDOWN);
        if(action=="pointer_up"||action=="click")push_mouse(SDL_MOUSEBUTTONUP);
    }
}
void RemoteServer::handle(uintptr_t id){
    const SOCKET client=static_cast<SOCKET>(id);
    sockaddr_in peer{};int peer_length=sizeof(peer);
    if(getpeername(client,reinterpret_cast<sockaddr*>(&peer),&peer_length)!=0||
       peer.sin_family!=AF_INET||!lan_address(peer)){
        reply(client,403,{{"error","Only local networks are allowed"}});return;
    }
    const int timeout=2500;
    setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
    setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
    std::string request,body;
    if(!parse_request(client,request,body)){reply(client,400,{{"error","Invalid request"}});return;}
    const bool pairing=request.rfind("POST /pair HTTP/1.",0)==0;
    const bool input=request.rfind("POST /input HTTP/1.",0)==0;
    const bool catalog_search=request.rfind("POST /catalog/search HTTP/1.",0)==0;
    const bool catalog_results=request.rfind("POST /catalog/results HTTP/1.",0)==0;
    const bool catalog_open=request.rfind("POST /catalog/open HTTP/1.",0)==0;
    if(!pairing&&!input&&!catalog_search&&!catalog_results&&!catalog_open){
        reply(client,400,{{"error","Unknown endpoint"}});return;
    }
    Json payload=Json::parse(body,nullptr,false);
    if(!payload.is_object()){reply(client,400,{{"error","Expected JSON object"}});return;}
    if(pairing){
        const Uint32 now=SDL_GetTicks();
        if(lock_until_!=0&&static_cast<int32_t>(lock_until_-now)>0){
            reply(client,429,{{"error","Too many attempts. Try again in a minute"}});return;
        }
        auto found=payload.find("pin");
        const std::string pin=found!=payload.end()&&found->is_string()?found->get<std::string>():"";
        if(pin!=pin_){
            if(++wrong_pins_>=5){wrong_pins_=0;lock_until_=now+60000;}
            reply(client,403,{{"error","Incorrect PIN"}});return;
        }
        wrong_pins_=0;lock_until_=0;
        active_at_=SDL_GetTicks();
        reply(client,200,{{"token",secret_},{"protocol",1}});return;
    }
    if(!authorized(request,secret_)){reply(client,401,{{"error","Pairing required"}});return;}
    if(catalog_search){
        const auto q=payload.find("query");
        const std::string query=q!=payload.end()&&q->is_string()?q->get<std::string>():"";
        if(query.size()<2||query.size()>120){
            reply(client,400,{{"error","La búsqueda debe tener entre 2 y 120 caracteres."}});return;
        }
        uint32_t generation=0;std::string error;
        if(!catalog_.begin(query,generation,error)){
            reply(client,400,{{"error",error}});return;
        }
        reply(client,200,{{"generation",generation},{"state","loading"}});return;
    }
    if(catalog_results||catalog_open){
        const auto g=payload.find("generation");
        if(g==payload.end()||!g->is_number_unsigned()||g->get<uint64_t>()>UINT32_MAX){
            reply(client,400,{{"error","Invalid generation"}});return;
        }
        const uint32_t generation=g->get<uint32_t>();
        if(catalog_results){
            const auto snap=catalog_.snapshot(generation);
            Json items=Json::array();
            if(snap.state=="ready"){
                for(size_t i=0;i<snap.items.size();++i){
                    const Entry& item=snap.items[i];
                    if(item.kind!=EntryKind::Asset&&item.kind!=EntryKind::Channel)continue;
                    const char* kind=item.kind==EntryKind::Channel?"tv":
                                     item.program_type=="0"?"series":"movies";
                    // Only catalogue metadata leaves the PC, never session
                    // cookies, stream addresses, source headers or tokens.
                    std::string picture=item.artwork;
                    if(picture.rfind("https://",0)!=0||picture.size()>1800)picture.clear();
                    std::string description=item.description;
                    if(description.size()>240)description.resize(240);
                    items.push_back({{"index",i},{"title",item.title},
                        {"kind",kind},{"description",description},
                        {"image",picture}});
                }
            }
            reply(client,200,{{"generation",snap.generation},{"state",snap.state},
                {"message",snap.message},{"items",std::move(items)}});return;
        }
        const auto i=payload.find("index");
        if(i==payload.end()||!i->is_number_unsigned()||i->get<uint64_t>()>1000){
            reply(client,400,{{"error","Invalid result index"}});return;
        }
        const int index=i->get<int>();
        Catalog verify;
        if(!catalog_.select(generation,static_cast<size_t>(index),verify)){
            reply(client,400,{{"error","Result is no longer available"}});return;
        }
        SDL_Event event{};event.type=SDL_USEREVENT;
        event.user.code=REMOTE_OPEN_CATALOG;
        event.user.data1=reinterpret_cast<void*>(static_cast<intptr_t>(index));
        event.user.data2=reinterpret_cast<void*>(static_cast<intptr_t>(generation));
        if(SDL_PushEvent(&event)<0){
            reply(client,400,{{"error","Cannot reach Windows player"}});return;
        }
        reply(client,200,{{"ok",true}});return;
    }
    const auto action_it=payload.find("action");
    const std::string action=action_it!=payload.end()&&action_it->is_string()?action_it->get<std::string>():"";
    // YouTube uses the user's default Windows browser, not a downloaded
    // media stream or an unofficial YouTube API. Every request is paired.
    if(input&&action=="youtube_open"){
        if(!youtube_launch()){
            reply(client,400,{{"error","No se pudo abrir YouTube en el navegador."}});return;
        }
        youtube_mode_=true;
        push_value(REMOTE_YOUTUBE_START,0);
        active_at_=SDL_GetTicks();
        reply(client,200,{{"ok",true},{"mode","youtube"}});return;
    }
    if(input&&action=="youtube_search"){
        const auto value_it=payload.find("text");
        const std::string query=value_it!=payload.end()&&value_it->is_string()?
            value_it->get<std::string>():"";
        if(!youtube_mode_.load()||youtube_results_url(query).empty()){
            reply(client,400,{{"error","Abre YouTube y escribe una búsqueda válida."}});return;
        }
        if(!youtube_search(query)){
            reply(client,400,{{"error","No se pudo abrir la búsqueda de YouTube."}});return;
        }
        active_at_=SDL_GetTicks();
        reply(client,200,{{"ok",true}});return;
    }
    if(input&&action=="youtube_close"){
        youtube_mode_=false;
        push_value(REMOTE_YOUTUBE_RETURN,0);
        active_at_=SDL_GetTicks();
        reply(client,200,{{"ok",true},{"mode","xuper"}});return;
    }
    if(input&&youtube_mode_.load()){
        if(action=="ping"){
            active_at_=SDL_GetTicks();
            reply(client,200,{{"ok",true},{"mode","youtube"}});return;
        }
        if(action=="home"||action=="tv"||action=="movies"||action=="series"){
            youtube_mode_=false;
            push_value(REMOTE_YOUTUBE_RETURN,0);
            dispatch(action,0,0,0);
            active_at_=SDL_GetTicks();
            reply(client,200,{{"ok",true},{"mode","xuper"}});return;
        }
        int dx=0,dy=0;
        const auto displacement=[&](const char* key){
            const auto value=payload.find(key);
            return value!=payload.end()&&value->is_number_integer()?
                std::clamp(value->get<int>(),-1500,1500):0;
        };
        dx=displacement("dx");dy=displacement("dy");
        static const std::array<const char*,24> youtube_actions={
            "up","down","left","right","ok","play_pause","click",
            "pointer_move","pointer_down","pointer_up",
            "youtube_pause","youtube_fullscreen","youtube_mute",
            "youtube_seek_back","youtube_seek_forward","youtube_next",
            "youtube_previous","youtube_back","youtube_volume_up",
            "youtube_volume_down","previous","next","back","menu"};
        const bool allowed=std::any_of(youtube_actions.begin(),youtube_actions.end(),
            [&](const char* command){return action==command;});
        if(!allowed){
            reply(client,400,{{"error","Control no disponible en YouTube."}});return;
        }
        const std::string cmd=action=="back"?"youtube_back":
                              action=="next"?"youtube_next":
                              action=="previous"?"youtube_previous":
                              action=="menu"?"youtube_fullscreen":action;
        if(!youtube_input(cmd,dx,dy)){
            reply(client,400,{{"error","Activa la ventana de YouTube en Windows."}});return;
        }
        active_at_=SDL_GetTicks();
        reply(client,200,{{"ok",true},{"mode","youtube"}});return;
    }
    const auto number=[&](const char* name,int fallback){
        const auto it=payload.find(name);
        return it!=payload.end()&&it->is_number_integer()?
            std::clamp(it->get<int>(),-1500,1500):fallback;
    };
    static const std::array<const char*,20> actions={
        "up","down","left","right","ok","back","home","tv","movies","series",
        "search","play_pause","menu","next","previous","volume","brightness",
        "pointer_move","pointer_down","pointer_up"};
    const bool valid=action=="click"||action=="text"||action=="ping"||
        std::any_of(actions.begin(),actions.end(),[&](const char* x){return action==x;});
    if(!valid){reply(client,400,{{"error","Unsupported action"}});return;}
    if(action=="volume"||action=="brightness"){
        if(number("value",-1)<0||number("value",-1)>100){
            reply(client,400,{{"error","Value must be 0-100"}});return;
        }
    }
    if(action=="text"){
        const auto value_it=payload.find("text");
        const std::string value=value_it!=payload.end()&&value_it->is_string()?
            value_it->get<std::string>():"";
        if(value.empty()||value.size()>160||std::any_of(value.begin(),value.end(),
          [](unsigned char c){return c<32||c==127;})){
            reply(client,400,{{"error","Invalid search text"}});return;
        }
        // Network thread cannot safely modify the active SDL text input
        // session. The search screen drains this queue on the UI thread.
        {
            std::lock_guard<std::mutex> lock(text_mutex_);
            if(pending_text_.size()+value.size()<=240)pending_text_+=value;
        }
    }
    active_at_=SDL_GetTicks();
    if(action!="ping"&&action!="text")
        dispatch(action,number("dx",0),number("dy",0),number("value",0));
    reply(client,200,{{"ok",true}});
}
void RemoteServer::serve(){
    while(running_){
        fd_set set;FD_ZERO(&set);FD_SET(static_cast<SOCKET>(listener_),&set);
        timeval wait{0,200000};
        int status=select(0,&set,nullptr,nullptr,&wait);
        if(!running_)break;
        if(status<=0)continue;
        const SOCKET client=accept(static_cast<SOCKET>(listener_),nullptr,nullptr);
        if(client==invalid_socket)continue;
        handle(static_cast<uintptr_t>(client));
        closesocket(client);
    }
}
}
#endif
