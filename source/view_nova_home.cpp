#include "view.hpp"
#ifdef _WIN32
#include "nova_layout.hpp"
#include "remote_server.hpp"
#include <algorithm>
#include <random>
#include <vector>

namespace xs {
namespace {
constexpr SDL_Color canvas{12,15,25,255};
constexpr SDL_Color white{249,248,251,255};
constexpr SDL_Color muted{193,198,211,255};
constexpr SDL_Color card{28,31,45,255};
constexpr SDL_Color focus{181,143,255,255};
constexpr SDL_Color icon_fill{45,47,67,238};

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
        SDL_RenderDrawLine(renderer,x+24,y+7,x+27,y+7);
        SDL_RenderDrawLine(renderer,x+24,y+22,x+27,y+22);
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
    rect({0,0,1280,720},canvas);

    // Pick one ACTUAL series/movie poster at random per app session.
    // Do not reshuffle when navigating a card or polling image downloads.
    if(nova_backdrop_key.empty()){
        std::vector<const Entry*> covers;
        for(const auto& e:c.entries){
            if(e.kind==EntryKind::Asset&&!e.artwork.empty()&&
               !e.artwork_cache.empty())covers.push_back(&e);
        }
        if(!covers.empty()){
            std::mt19937 generator(std::random_device{}());
            const auto* chosen=covers[std::uniform_int_distribution<size_t>(
                0,covers.size()-1)(generator)];
            nova_backdrop_url=chosen->artwork;
            nova_backdrop_key=chosen->artwork_cache;
        }
    }
    if(!nova_backdrop_key.empty())
        image_cover(nova_backdrop_url,nova_backdrop_key,{0,0,1280,315});

    // The backdrop fades into the app background exactly at the TV row.
    // It never darkens or obscures the series and movies below.
    for(int y=105;y<300;y+=3){
        const int alpha=std::min(255,(y-105)*255/195);
        rect({0,y,1280,3},{canvas.r,canvas.g,canvas.b,
             static_cast<Uint8>(alpha)});
    }
    rect({0,300,1280,420},canvas);

    rect({24,21,5,43},focus);
    text(42,25,"XUPER TV",white,320,heading,44);

    const char* names[]={"TV en vivo","Series","Películas"};
    for(int row=0;row<3;++row){
        const int y=nova_row_y(row)-37;
        rect({30,y,34,31},icon_fill);
        heading_symbol(renderer,row,32,y);
        text(75,y-2,names[row],white,278,font,34);
        const SDL_Rect clip{28,nova_row_y(row)-3,1214,nova_card_h+6};
        SDL_RenderSetClipRect(renderer,&clip);
        for(const auto& tile:nova_tiles(c,selected,home_last_index,nova_row_pixels)){
            if(nova_row_of(c.entries[tile.index])!=row)continue;
            const Entry& e=c.entries[tile.index];
            const bool is_focused=nav_focus<0&&selected==tile.index;
            rect({tile.x-3,tile.y-3,tile.w+6,tile.h+6},
                 is_focused?focus:SDL_Color{43,47,65,255});
            rect({tile.x,tile.y,tile.w,tile.h},card);
            const SDL_Rect artwork{tile.x+2,tile.y+2,tile.w-4,76};
            rect(artwork,{35,39,57,255});
            const bool drawn=row==0
                ?image(e.artwork,e.artwork_cache,artwork)
                :image_cover(e.artwork,e.artwork_cache,artwork);
            if(!drawn)text(tile.x+9,tile.y+22,row==0?"TV":row==1?"SERIE":"PELÍCULA",
                           muted,tile.w-17,small,31);
            rect({tile.x+2,tile.y+78,tile.w-4,tile.h-80},{17,19,30,255});
            text(tile.x+8,tile.y+80,e.title,white,tile.w-16,small,23);
        }
        SDL_RenderSetClipRect(renderer,nullptr);
        if(nova_row_indices(c,row).empty()){
            text(34,nova_row_y(row)+28,"Sin contenido disponible",muted,450,small,31);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    RemoteServer::shared().draw_cursor(renderer);
    if(pairing_visible)draw_pairing_overlay();
    SDL_RenderPresent(renderer);
}
} // namespace xs
#endif
