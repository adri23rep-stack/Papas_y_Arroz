#include "view.hpp"
#ifdef _WIN32
#include "cinematic_layout.hpp"
#include "remote_server.hpp"
#include <algorithm>
#include <string>

namespace xs {
namespace {
static constexpr SDL_Color white{249,249,252,255};
static constexpr SDL_Color subtle{176,188,208,255};
static constexpr SDL_Color pink{241,48,108,255};
static constexpr SDL_Color navy{18,24,40,255};
static constexpr SDL_Color dim{11,15,29,255};
static const char* navigation[]={"Inicio","TV en vivo","Películas","Series","Favoritos","Apoyar"};
const char* kind_label(const Entry& e){
    switch(e.kind){
        case EntryKind::Channel:return "● EN VIVO";
        case EntryKind::Asset:return e.program_type=="0"?"SERIE":"PELÍCULA";
        case EntryKind::Episode:return "EPISODIO";
        case EntryKind::Section:return e.program_type=="0"?"SERIES":"PELÍCULAS";
        case EntryKind::LiveSection:return "TV EN VIVO";
        case EntryKind::Category:return "CANALES";
        case EntryKind::VodCategory:return "COLECCIÓN";
        default:return "EXPLORAR";
    }
}
}
bool View::image_cover(const std::string& url,const std::string& key,SDL_Rect dst){
    if(!images||key.empty())return false;
    SDL_Texture* texture=images->get(url,key);
    if(!texture)return false;
    int w=0,h=0;SDL_QueryTexture(texture,nullptr,nullptr,&w,&h);
    if(w<=0||h<=0)return false;
    const double source_ratio=static_cast<double>(w)/h;
    const double dest_ratio=static_cast<double>(dst.w)/dst.h;
    SDL_Rect src{0,0,w,h};
    if(source_ratio>dest_ratio){
        src.w=std::max(1,static_cast<int>(h*dest_ratio));
        src.x=(w-src.w)/2;
    }else{
        src.h=std::max(1,static_cast<int>(w/dest_ratio));
        src.y=(h-src.h)/2;
    }
    SDL_RenderCopy(renderer,texture,&src,&dst);
    return true;
}
void View::draw_cinematic(const Catalog& c,size_t selected,const std::string& status){
    const bool home=is_home(c);
    const bool any=!c.entries.empty();
    const Entry* chosen=any?&c.entries[std::min(selected,c.entries.size()-1)]:nullptr;
    background();
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);

    // Atmospheric dark TV chrome with a strong focus indicator, no duplicate
    // sidebar fighting for remote/trackpad space.
    rect({0,0,1280,68},{7,11,23,255});
    rect({0,67,1280,2},{62,27,48,255});
    rect({19,13,47,43},{233,39,92,255});
    text(31,13,"X",white,37,heading,41);
    text(77,17,"Xuper TV",white,148,heading,37);
    for(int i=0;i<6;++i){
        int x=224+i*143;
        bool active=active_nav==i,focus=nav_focus==i;
        if(active||focus)rect({x,13,134,42},focus?pink:SDL_Color{85,24,56,255});
        text(x+12,23,navigation[i],(active||focus)?white:subtle,119,small,30);
    }
    rect({1092,13,168,43},{42,34,60,255});
    text(1100,24,"⛶ Pantalla completa",white,153,small,28);

    const int hero_y=79,hero_height=home?213:303;
    rect({19,hero_y,1242,hero_height},navy);
    if(chosen&&!chosen->artwork_cache.empty()){
        // Covers and logos can be portrait or landscape. The hero fills
        // available space by cropping, not shrinking into a blank square.
        image_cover(chosen->artwork,chosen->artwork_cache,
                    {660,hero_y+5,596,hero_height-10});
    }else if(!c.artwork_cache.empty()){
        image_cover(c.artwork,c.artwork_cache,
                    {660,hero_y+5,596,hero_height-10});
    }else{
        rect({682,hero_y+5,569,hero_height-10},{38,27,61,255});
        text(839,hero_y+73,"XUPER TV",{132,75,128,255},330,large,100);
    }
    // Darkened information column and soft-ish vertical falloff.
    for(int j=0;j<14;++j){
        const int x=580+j*18;
        rect({x,hero_y+5,19,hero_height-10},
             {9,12,24,static_cast<Uint8>(225-std::min(j*17,216))});
    }
    rect({19,hero_y,568,hero_height},{9,13,26,231});
    rect({19,hero_y,4,hero_height},pink);
    const std::string type=chosen?kind_label(*chosen):"XUPER TV";
    text(38,hero_y+14,type,pink,510,small,24);
    text(36,hero_y+47,chosen?chosen->title:"Tu entretenimiento en un solo lugar",
         white,590,home?heading:large,home?76:106);
    std::string description;
    if(chosen)description=chosen->description;
    if(description.empty())description=c.description;
    if(description.empty())description=chosen&&chosen->kind==EntryKind::Channel?
        "Emisión en directo. Abre el canal para ver la programación.":
        "Explora los canales, las películas y las series de tu catálogo.";
    text(38,hero_y+(home?102:154),description,subtle,575,small,home?47:65);

    const int buttons_y=hero_y+hero_height-45;
    rect({36,buttons_y,172,37},pink);
    text(53,buttons_y+6,chosen&&chosen->kind==EntryKind::Channel?"▶ Ver canal":"▶ Ver ahora",white,154,small,27);
    rect({220,buttons_y,183,37},{47,49,69,255});
    text(230,buttons_y+6,"⛶ Pantalla completa",white,170,small,27);
    rect({414,buttons_y,120,37},{46,43,62,255});
    text(430,buttons_y+6,"ⓘ Detalles",white,107,small,27);
    if(show_details){
        rect({660,hero_y+10,591,hero_height-20},{8,12,24,243});
        text(680,hero_y+21,"DETALLES",pink,530,small,27);
        text(680,hero_y+55,chosen?chosen->title:c.title,white,525,heading,69);
        text(681,hero_y+115,description,subtle,535,small,hero_height-130);
    }

    if(home){
        text(30,295,"◷  Último canal visto",white,850,font,28);
        const auto cards=cinematic_tiles(c,selected,home_last_index);
        bool live_heading=false,movie_heading=false,series_heading=false;
        for(const auto& tile:cards){
            if(tile.kind==CinematicTile::Channel)live_heading=true;
            if(tile.kind==CinematicTile::Movie)movie_heading=true;
            if(tile.kind==CinematicTile::Series)series_heading=true;
        }
        // The last-viewed card is backed by a persistent channel ID; do not
        // invent EPG program titles, progress or ratings.
        if(home_last_index<0){
            rect({29,318,858,73},{28,33,52,255});
            text(49,336,"Todavía no has visto un canal. Entra a TV en vivo.",subtle,803,font,52);
        }
        for(const CinematicTile& card:cards){
            const Entry& e=c.entries[card.index];
            const bool focus=card.index==selected&&nav_focus<0;
            rect({card.x-2,card.y-2,card.w+4,card.h+4},
                 focus?pink:SDL_Color{44,54,77,255});
            rect({card.x,card.y,card.w,card.h},dim);
            if(card.kind==CinematicTile::Last){
                rect({card.x+5,card.y+4,208,card.h-8},{35,33,58,255});
                image_cover(e.artwork,e.artwork_cache,
                            {card.x+8,card.y+6,201,card.h-12});
                text(card.x+230,card.y+7,"CONTINUAR TV EN VIVO",pink,550,small,25);
                text(card.x+230,card.y+32,e.title,white,595,font,37);
                text(card.x+790,card.y+20,"▶",pink,38,heading,38);
                continue;
            }
            const bool poster=card.kind==CinematicTile::Movie||card.kind==CinematicTile::Series;
            const int art_h=poster?card.h-3:card.h-5;
            rect({card.x+3,card.y+3,card.w-6,art_h},{32,37,56,255});
            if(!image_cover(e.artwork,e.artwork_cache,{card.x+3,card.y+3,card.w-6,art_h})){
                text(card.x+9,card.y+12,kind_label(e),pink,card.w-18,small,35);
            }
            rect({card.x+3,card.y+card.h-40,card.w-6,37},{8,12,23,210});
            text(card.x+11,card.y+card.h-35,e.title,white,card.w-20,small,36);
            if(!poster)text(card.x+11,card.y+5,kind_label(e),pink,card.w-20,small,21);
        }
        if(live_heading)text(29,395,"●  Canales en vivo",white,1150,font,29);
        else text(29,397,"●  Canales en vivo  ·  Selecciona TV para explorar",subtle,1150,font,29);
        if(movie_heading)text(29,513,"◉  Películas destacadas",white,590,font,27);
        else text(29,513,"◉  Películas",white,590,font,27);
        if(series_heading)text(655,513,"▤  Series populares",white,595,font,27);
        else text(655,513,"▤  Series",white,595,font,27);
    }else{
        const bool posters=is_poster_catalog(c);
        text(29,397,c.title.empty()?"EXPLORAR":c.title,white,930,font,29);
        text(1089,398,any?
             std::to_string(std::min(selected+1,c.entries.size()))+" / "+std::to_string(c.entries.size()):
             "Sin contenido",subtle,170,small,27);
        for(const CinematicTile& card:cinematic_tiles(c,selected)){
            const Entry& e=c.entries[card.index];
            const bool focus=card.index==selected&&nav_focus<0;
            rect({card.x-3,card.y-3,card.w+6,card.h+6},
                 focus?pink:SDL_Color{43,53,73,255});
            rect({card.x,card.y,card.w,card.h},navy);
            const int image_h=posters?card.h-46:card.h-4;
            rect({card.x+4,card.y+4,card.w-8,image_h-4},{35,41,61,255});
            if(!image_cover(e.artwork,e.artwork_cache,{card.x+4,card.y+4,card.w-8,image_h-4})){
                text(card.x+12,card.y+19,kind_label(e),pink,card.w-23,small,35);
            }
            if(posters){
                text(card.x+8,card.y+image_h+2,e.title,white,card.w-16,small,45);
            }else{
                rect({card.x+4,card.y+card.h-40,card.w-8,36},{9,12,25,226});
                text(card.x+12,card.y+card.h-37,e.title,white,card.w-25,small,35);
            }
        }
        if(!any)text(39,471,"No hay títulos disponibles en esta sección.",subtle,1100,font,66);
    }
    rect({20,684,1240,1},{56,55,73,255});
    text(26,690,status,subtle,755,small,28);
    const auto remote=RemoteServer::shared().info();
    const std::string pairing=!remote.running?"Control iPhone no disponible":
        remote.connected?"● iPhone conectado":("iPhone: "+remote.address+":47831  |  PIN: "+remote.pin);
    text(790,689,pairing,remote.connected?SDL_Color{118,233,179,255}:white,466,small,29);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    RemoteServer::shared().draw_cursor(renderer);
    SDL_RenderPresent(renderer);
}
int View::hit_cinematic(const Catalog& c,size_t selected,int x,int y)const{
    // Negative numbers are routed by main.cpp, never mistaken for indices.
    if(y>=13&&y<=56&&x>=224&&x<224+6*143){
        const int column=(x-224)/143;
        if((x-224)%143<134)return -2-column;
    }
    if(y>=13&&y<=56&&x>=1092&&x<1260)return -10;
    const bool home=is_home(c);
    const int hero_bottom=home?292:382;
    const int y_button=hero_bottom-45;
    if(y>=y_button&&y<y_button+38){
        if(x>=36&&x<209&&(!c.entries.empty()))return -9;
        if(x>=220&&x<404)return -10;
        if(x>=414&&x<535)return -11;
    }
    return cinematic_hit(c,selected,home?home_last_index:-1,x,y);
}
}
#endif
