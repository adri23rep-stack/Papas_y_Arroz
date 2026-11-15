#pragma once
#include "catalog.hpp"
#include "artwork.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <memory>
namespace xs {
class View {
public:
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;TTF_Font* font=nullptr;
    int nav_focus=-1,active_nav=0;bool favorite=false;
    int home_last_index=-1;
    bool show_details=false;
    bool pairing_visible=false;
    bool open(std::string& error);
    void toggle_fullscreen();
    void move_to_next_display();
    void draw(const Catalog& catalog,size_t selected,const std::string& status,bool full_memory);
    void splash(const std::string& status);
    void donation();
    bool poll_images(){return images&&images->poll();}
    int columns(const Catalog& catalog)const;
    int page_size(const Catalog& catalog)const;
    // -8 favorite action; -2..-7 sidebar; -1 no hit; >=0 catalogue index.
    int hit(const Catalog& catalog,size_t selected,int x,int y)const;
    void close();~View(){close();}
private:
    TTF_Font *small=nullptr,*large=nullptr,*heading=nullptr;
    std::unique_ptr<Artwork> images;
    void text(int x,int y,const std::string& message,SDL_Color color,int width=1160,TTF_Font* type=nullptr,int max_height=0);
    void rect(SDL_Rect r,SDL_Color color);
    void background();void brand(int x,int y,bool big=false);
#ifdef _WIN32
    void draw_cinematic(const Catalog& catalog,size_t selected,const std::string& status);
    void draw_pairing_overlay();
    int hit_cinematic(const Catalog& catalog,size_t selected,int x,int y)const;
    bool image_cover(const std::string& url,const std::string& key,SDL_Rect bounds);
#endif
    bool image(const std::string& url,const std::string& key,SDL_Rect bounds);
};
}
