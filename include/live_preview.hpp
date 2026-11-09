#pragma once
#ifdef _WIN32
#include <SDL.h>
#include <atomic>
#include <string>
#include <utility>
#include <vector>
#include <mpv/client.h>
#include <mpv/render.h>

namespace xs {
// Muted, low-resolution secondary decoder. Only created during a held
// channel preview; never changes the active full-screen playback.
class LivePreview {
public:
    explicit LivePreview(SDL_Renderer* renderer):renderer_(renderer){}
    ~LivePreview(){close();}
    bool open(const std::string& url,const std::vector<std::pair<std::string,std::string>>& headers,std::string& error);
    void poll();
    bool draw(const SDL_Rect& bounds)const;
    void close();
    bool ready()const{return has_frame_;}
    bool failed()const{return failed_;}
private:
    SDL_Renderer* renderer_=nullptr;
    mpv_handle* handle_=nullptr;
    mpv_render_context* context_=nullptr;
    SDL_Texture* texture_=nullptr;
    void* pixels_=nullptr;
    std::atomic<bool> dirty_{true};
    bool has_frame_=false,failed_=false;
    static void changed(void* opaque);
};
}
#endif
