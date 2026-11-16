#pragma once
#ifdef _WIN32
#include <SDL.h>
#include "remote_catalog.hpp"
#include "remote_browse.hpp"
#include "remote_settings.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <utility>

namespace xs {
struct RemotePlaybackChoice {
    int id=0;
    std::string kind,label;
    bool selected=false;
};
struct RemotePlaybackSnapshot {
    bool active=false;
    uint32_t generation=0;
    std::string title;
    std::vector<RemotePlaybackChoice> audio,subtitles,sources;
};
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

    void set_session(std::shared_ptr<ServiceSession> session,const Catalog& root){
        catalog_.set_session(session);
        browse_.set_session(session,root);
        mobile_browse_.set_session(std::move(session),root);
    }
    void index_assets(const std::vector<Entry>& entries){catalog_.index_assets(entries);}
    bool selected_result(uint32_t generation,size_t index,Catalog& out)const{return catalog_.select(generation,index,out);}
    bool selected_browse_result(uint32_t generation,size_t index,Catalog& out)const{
        return browse_.selected_result(generation,index,out);
    }
    // Only titles and track labels are shared; never stream URLs or credentials.
    void begin_playback(const std::string& title){
        std::lock_guard<std::mutex> lock(playback_mutex_);
        ++playback_generation_;playback_={};
        playback_.active=true;playback_.generation=playback_generation_;
        playback_.title=title;
    }
    void publish_playback_options(RemotePlaybackSnapshot next){
        std::lock_guard<std::mutex> lock(playback_mutex_);
        if(!playback_.active)return;
        next.active=true;next.generation=playback_generation_;
        playback_=std::move(next);
    }
    void end_playback(){
        std::lock_guard<std::mutex> lock(playback_mutex_);
        ++playback_generation_;playback_={};
    }
    RemotePlaybackSnapshot playback_options()const{
        std::lock_guard<std::mutex> lock(playback_mutex_);
        return playback_;
    }
    int setting_value(int id)const{return settings_.get(id);}
    // Keep the desktop quality selector and the iPhone on the same persisted setting.
    bool set_setting_value(int id,int value){return settings_.put(id,value,true);}
    void publish_setting(int id,int value){settings_.put(id,value,false);}
    bool cursor_visible()const;
    int cursor_x()const{return pointer_x_.load();}
    int cursor_y()const{return pointer_y_.load();}
    bool youtube_active()const{return youtube_mode_.load();}
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
    std::atomic<bool> youtube_mode_{false};
    std::atomic<int> pointer_x_{640},pointer_y_{360};
    std::atomic<int> volume_{100},brightness_{50};
    std::atomic<Uint32> pointer_at_{0},active_at_{0};
    std::thread worker_;
    RemoteCatalog catalog_;
    RemoteBrowse browse_;
    RemoteBrowse mobile_browse_;
    RemoteSettingsStore settings_;
    std::mutex text_mutex_;
    std::string pending_text_;
    mutable std::mutex playback_mutex_;
    RemotePlaybackSnapshot playback_;
    uint32_t playback_generation_=0;
    uintptr_t listener_=static_cast<uintptr_t>(~uintptr_t{0});
    std::string address_="Sin red",pin_,secret_;
    int wrong_pins_=0;
    Uint32 lock_until_=0;
};
constexpr int REMOTE_PLAYBACK_SELECT=0x585509;
constexpr int REMOTE_VOLUME=0x585501;
constexpr int REMOTE_BRIGHTNESS=0x585502;
constexpr int REMOTE_OPEN_CATALOG=0x585503;
constexpr int REMOTE_YOUTUBE_START=0x585504;
constexpr int REMOTE_YOUTUBE_RETURN=0x585505;
constexpr int REMOTE_WHEEL=0x585506;
constexpr int REMOTE_OPEN_BROWSE=0x585507;
constexpr int REMOTE_SETTING=0x585508;
}
#endif
