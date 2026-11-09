#pragma once
#include <SDL.h>
namespace xs {
// Move the player/window to the next attached display (HDMI TV included).
inline bool move_to_next_display(SDL_Window* window){
    if(!window)return false;
    const int count=SDL_GetNumVideoDisplays();
    if(count<2)return false;
    int current=SDL_GetWindowDisplayIndex(window);
    if(current<0)current=0;
    SDL_Rect bounds{};
    if(SDL_GetDisplayBounds((current+1)%count,&bounds)!=0)return false;
    const bool was_fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0;
    if(was_fullscreen)SDL_SetWindowFullscreen(window,0);
    int w=1280,h=720;
    SDL_GetWindowSize(window,&w,&h);
    SDL_SetWindowPosition(window,bounds.x+(bounds.w-w)/2,bounds.y+(bounds.h-h)/2);
    // Switching to HDMI should feel like a TV app, not an awkward small window.
    SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN_DESKTOP);
    return true;
}
}
