#include "view.hpp"
#ifdef _WIN32
#include "nova_layout.hpp"
#include "remote_server.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>

namespace xs {
namespace {
constexpr SDL_Color black{0,0,0,255};
constexpr SDL_Color white{247,248,251,255};
constexpr SDL_Color muted{193,198,211,255};
constexpr SDL_Color focus{181,143,255,255};
constexpr SDL_Color card{15,16,20,255};
constexpr SDL_Color icon_fill{26,27,32,245};

// High-resolution LANDSCAPE stills from 2026 film and TV titles in
// Rotten Tomatoes' year-end guides. These are fetched from their original
// online hosts, never embedded inside the Windows ZIP. Unlike MetaHub's
// sometimes-offline thumbnails, these sources point to wide movie stills.
constexpr std::array<const char*,7> best_of_2026_wallpapers{{
    // The Odyssey (2026), Eye Filmmuseum — 1920x1080
    "https://assets.eyefilm.nl/images/production/_1920x1080_crop_center-center_none/1632957/still_The-Odyssey-Christopher-Nolan-US-GB-2026-1.webp",
    // Wildwood (2026), 1920x1080 original promotional backdrop
    "https://d32qys9a6wm9no.cloudfront.net/images/movies/backdrop/ZVofAle5TZmIEieUj7WVjT2NYNyBAux8_1920x1080.jpg?t=1778746400",
    // Project Hail Mary (2026), promotional landscape still
    "https://www.mensjournal.com/.image/c_fill%2Cg_faces%3Acenter/NDI6MDAwMDAwMDAxMTE2OTA4/project-hail-mary-1-header.jpg",
    // VisionQuest (2026), series promotional still — 1536x646
    "https://cdn.observatoriodocinema.com.br/2025/10/vision-quest-1536x646.jpeg",
    // Wildwood (2026), TMDb-origin original landscape key art
    "https://img.kinobaza.com.ua/t/p/original/sl5NJ1vtyhP9Vr88bB4blULsJYp.jpg",
    // The Odyssey (2026), Seattle International Film Festival — 1600x900
    "https://www.siff.net/images/CINEMA/2026/Films%20Jun-Dec/CIN_TheOdyssey_1600x900.png",
    // Godzilla Minus Zero (2026), wide theatrical image
    "https://www.slashfilm.com/img/gallery/godzilla-minus-zero-will-do-something-no-godzilla-movie-has-done-in-over-20-years/l-intro-1762215147.jpg"
}};
constexpr int wallpaper_count=static_cast<int>(best_of_2026_wallpapers.size());

std::string wallpaper_url(int n){
    const int index=(n%wallpaper_count+wallpaper_count)%wallpaper_count;
    return best_of_2026_wallpapers[static_cast<size_t>(index)];
}

int corner_inset(int y,int height,int radius){
    const int near=std::min(y,height-1-y);
    if(near>=radius)return 0;
    const int dy=radius-1-near;
    return radius-static_cast<int>(std::sqrt(static_cast<double>(radius*radius-dy*dy)));
}
void round_fill(SDL_Renderer* renderer,SDL_Rect b,int radius,SDL_Color color){
    if(b.w<1||b.h<1)return;
    radius=std::min(radius,std::min(b.w,b.h)/2);
    SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a);
    for(int y=0;y<b.h;++y){
        const int edge=corner_inset(y,b.h,radius);
        SDL_RenderDrawLine(renderer,b.x+edge,b.y+y,b.x+b.w-1-edge,b.y+y);
    }
}
// Crop the actual artwork to the rounded tile without relying on
// OpenGL, SDL3 or new dependencies. The surrounding rows are solid black.
void round_mask(SDL_Renderer* renderer,SDL_Rect b,int radius){
    SDL_SetRenderDrawColor(renderer,0,0,0,255);
    for(int y=0;y<radius;++y){
        const int edge=corner_inset(y,b.h,radius);
        for(int x=0;x<edge;++x){
            SDL_RenderDrawPoint(renderer,b.x+x,b.y+y);
            SDL_RenderDrawPoint(renderer,b.x+b.w-1-x,b.y+y);
            SDL_RenderDrawPoint(renderer,b.x+x,b.y+b.h-1-y);
            SDL_RenderDrawPoint(renderer,b.x+b.w-1-x,b.y+b.h-1-y);
        }
    }
}

void heading_symbol(SDL_Renderer* renderer,int row,int x,int y){
    SDL_SetRenderDrawColor(renderer,237,230,255,255);
    if(row==0){
        SDL_Rect screen{x+5,y+5,21,15};
        SDL_RenderDrawRect(renderer,&screen);
        SDL_RenderDrawLine(renderer,x+14,y+21,x+14,y+25);
        SDL_RenderDrawLine(renderer,x+10,y+25,x+18,y+25);
    }else if(row==1){
        SDL_Rect cover{x+6,y+5,17,20};
        SDL_RenderDrawRect(renderer,&cover);
        SDL_RenderDrawLine(renderer,x+10,y+9,x+19,y+9);
        SDL_RenderDrawLine(renderer,x+10,y+13,x+19,y+13);
        SDL_RenderDrawLine(renderer,x+10,y+17,x+17,y+17);
    }else{
        SDL_Rect screen{x+5,y+12,22,14};
        SDL_RenderDrawRect(renderer,&screen);
        SDL_RenderDrawLine(renderer,x+5,y+12,x+27,y+12);
        SDL_RenderDrawLine(renderer,x+5,y+8,x+27,y+3);
        SDL_RenderDrawLine(renderer,x+5,y+8,x+5,y+12);
        SDL_RenderDrawLine(renderer,x+12,y+7,x+17,y+11);
        SDL_RenderDrawLine(renderer,x+21,y+5,x+25,y+9);
    }
}
} // namespace

void View::scroll_nova_row(const Catalog& c,int row,int delta_pixels){
    if(!nova_is_home(c)||row<0||row>2)return;
    auto& offset=nova_row_pixels[static_cast<size_t>(row)];
    offset=nova_clamp_scroll(c,row,offset+delta_pixels);
}
void View::ensure_nova_visible(const Catalog& c,size_t selected){
    if(!nova_is_home(c)||selected>=c.entries.size())return;
    const int row=nova_row_of(c.entries[selected]);
    if(row<0)return;
    const auto entries=nova_row_indices(c,row);
    const auto it=std::find(entries.begin(),entries.end(),selected);
    if(it==entries.end())return;
    const int ordinal=static_cast<int>(it-entries.begin());
    auto& offset=nova_row_pixels[static_cast<size_t>(row)];
    const int x=nova_card_x+ordinal*nova_card_step-offset;
    if(x<nova_card_x)offset=ordinal*nova_card_step;
    else if(x+nova_card_w>nova_carousel_right)
        offset=ordinal*nova_card_step+nova_card_w-(nova_carousel_right-nova_card_x);
    offset=nova_clamp_scroll(c,row,offset);
}

void View::draw_nova_home(const Catalog& c,size_t selected){
    poll_images();
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    rect({0,0,1280,720},black);

    if(nova_backdrop_url.empty()){
        std::mt19937 random(std::random_device{}());
        nova_backdrop_choice=std::uniform_int_distribution<int>(0,wallpaper_count-1)(random);
        nova_backdrop_attempt=0;
    }

    // On a missing/offline wallpaper try another 2026 title; don't
    // accidentally enlarge catalogue thumbnails as the backdrop.
    SDL_Texture* wallpaper=nullptr;
    for(;nova_backdrop_attempt<wallpaper_count;++nova_backdrop_attempt){
        const std::string candidate=wallpaper_url(nova_backdrop_choice+nova_backdrop_attempt);
        const std::string key=artwork_key(candidate);
        if(images->failed(key))continue;
        nova_backdrop_url=candidate;
        nova_backdrop_key=key;
        wallpaper=images->get(candidate,key,true);
        if(!wallpaper)break; // Async artwork download is still underway.
        int width=0,height=0;
        SDL_QueryTexture(wallpaper,nullptr,nullptr,&width,&height);
        if(width>=900&&height>=450)break;
        wallpaper=nullptr; // Skip tiny images even if the server returns one.
    }
    if(wallpaper){
        int width=0,height=0;
        SDL_QueryTexture(wallpaper,nullptr,nullptr,&width,&height);
        SDL_Rect from{0,0,width,height};
        const double dst_ratio=1280.0/315.0;
        const double image_ratio=static_cast<double>(width)/height;
        if(image_ratio>dst_ratio){
            from.w=std::max(1,static_cast<int>(height*dst_ratio));
            from.x=(width-from.w)/2;
        }else{
            from.h=std::max(1,static_cast<int>(width/dst_ratio));
            from.y=(height-from.h)/2;
        }
        const SDL_Rect to{0,0,1280,315};
        SDL_RenderCopy(renderer,wallpaper,&from,&to);
    }

    // True black fade. Reaches fully opaque black BEFORE first TV cards.
    for(int y=96;y<297;y+=3){
        const int opacity=std::min(255,(y-96)*255/198);
        rect({0,y,1280,3},{0,0,0,static_cast<Uint8>(opacity)});
    }
    rect({0,297,1280,423},black);
    round_fill(renderer,{24,20,5,44},2,focus);
    text(42,25,"XUPER TV",white,320,heading,44);

    const char* names[]={"TV en vivo","Series","Películas"};
    const auto tiles=nova_tiles(c,selected,home_last_index,nova_row_pixels);
    for(int row=0;row<3;++row){
        const int y=nova_row_y(row)-37;
        round_fill(renderer,{30,y,34,31},8,icon_fill);
        heading_symbol(renderer,row,32,y);
        text(75,y-2,names[row],white,278,font,34);

        const SDL_Rect clip{28,nova_row_y(row)-4,1214,nova_card_h+8};
        SDL_RenderSetClipRect(renderer,&clip);
        for(const auto& tile:tiles){
            if(nova_row_of(c.entries[tile.index])!=row)continue;
            const Entry& e=c.entries[tile.index];
            const bool selected_tile=nav_focus<0&&selected==tile.index;
            if(selected_tile)
                round_fill(renderer,{tile.x-2,tile.y-2,tile.w+4,tile.h+4},15,focus);
            round_fill(renderer,{tile.x,tile.y,tile.w,tile.h},13,card);
            const SDL_Rect artwork{tile.x+1,tile.y+1,tile.w-2,tile.h-2};
            const bool drawn=row==0
                ?image(e.artwork,e.artwork_cache,artwork)
                :image_cover(e.artwork,e.artwork_cache,artwork);
            if(!drawn)
                text(tile.x+11,tile.y+18,row==0?"TV":row==1?"SERIE":"PELÍCULA",
                     muted,tile.w-20,small,31);
            // Minimal translucent label rather than a heavy framed box.
            for(int j=0;j<35;j+=5)
                rect({tile.x+1,tile.y+tile.h-35+j,tile.w-2,5},
                     {0,0,0,static_cast<Uint8>(150+j*2)});
            text(tile.x+9,tile.y+tile.h-28,e.title,white,tile.w-18,small,24);
            round_mask(renderer,{tile.x,tile.y,tile.w,tile.h},13);
        }
        SDL_RenderSetClipRect(renderer,nullptr);
        if(nova_row_indices(c,row).empty())
            text(34,nova_row_y(row)+28,"Sin contenido disponible",muted,450,small,31);
    }
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    RemoteServer::shared().draw_cursor(renderer);
    if(pairing_visible)draw_pairing_overlay();
    SDL_RenderPresent(renderer);
}
} // namespace xs
#endif
