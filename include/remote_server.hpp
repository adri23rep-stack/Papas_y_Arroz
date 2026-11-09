#pragma once
#ifdef _WIN32
#include <SDL.h>
#include "remote_catalog.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <utility>

namespace xs {
struct RemoteInfo {
    std::string address;
    std::string pin;
    bool running=false;
    bool connected=false;
};
class RemoteServer {
public:
    static RemoteServer& shared();
    bool start();
    void stop();
    RemoteInfo info()const;
    // Text typed on iPhone is transferred to the UI thread, never injected
    // into SDL from a background HTTP worker.
    std::string take_text(){
        std::lock_guard<std::mutex> lock(text_mutex_);
        std::string value=std::move(pending_text_);
        pending_text_.clear();
        return value;
    }

    void set_session(std::shared_ptr<ServiceSession> session){catalog_.set_session(std::move(session));}
    bool selected_result(uint32_t generation,size_t index,Catalog& out)const{return catalog_.select(generation,index,out);}
    bool cursor_visible()const;
    int cursor_x()const{return pointer_x_.load();}
    int cursor_y()const{return pointer_y_.load();}
    int volume()const{return volume_.load();}
    int brightness()const{return brightness_.load();}
    void draw_cursor(SDL_Renderer* renderer)const;
    ~RemoteServer(){stop();}
private:
    RemoteServer()=default;
    RemoteServer(const RemoteServer&)=delete;
    RemoteServer& operator=(const RemoteServer&)=delete;
    void serve();
    void handle(uintptr_t connection);
    void dispatch(const std::string& action,int dx,int dy,int value);
    void push_key(SDL_Keycode key);
    void push_mouse(Uint32 kind);
    void push_value(int code,int value);
    std::atomic<bool> running_{false};
    std::atomic<int> pointer_x_{640},pointer_y_{360};
    std::atomic<int> volume_{100},brightness_{50};
    std::atomic<Uint32> pointer_at_{0},active_at_{0};
    std::thread worker_;
    RemoteCatalog catalog_;
    std::mutex text_mutex_;
    std::string pending_text_;
    uintptr_t listener_=static_cast<uintptr_t>(~uintptr_t{0});
    std::string address_="Sin red",pin_,secret_;
    int wrong_pins_=0;
    Uint32 lock_until_=0;
};
constexpr int REMOTE_VOLUME=0x585501;
constexpr int REMOTE_BRIGHTNESS=0x585502;
constexpr int REMOTE_OPEN_CATALOG=0x585503;
}
#endif
