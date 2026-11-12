#pragma once
#ifdef _WIN32
#include <SDL.h>
#include <mpv/render.h>
#include <string>

namespace xs {
// Experimental GPU-rendered video with an offscreen OpenGL read-back.
// It preserves XuperTV's existing SDL2 interface and its cinematic overlays.
// This is NOT a zero-copy GPU compositor; readback is a deliberate bridge.
class GpuReadback {
public:
    GpuReadback()=default;
    ~GpuReadback();
    GpuReadback(const GpuReadback&)=delete;
    GpuReadback& operator=(const GpuReadback&)=delete;

    bool open(mpv_handle* mpv,int width,int height,std::string& diagnostic);
    bool render(void* rgba_pixels,std::string& diagnostic);
    bool make_current();
    mpv_render_context* context() const {return context_;}
    void close(); // frees mpv renderer while its OpenGL context is current
private:
    SDL_Window* window_=nullptr;
    SDL_GLContext gl_=nullptr;
    mpv_render_context* context_=nullptr; // owned by this bridge
    int width_=0,height_=0;
};
}
#endif
