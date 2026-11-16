#include "view.hpp"
#ifdef _WIN32
#include "nova_layout.hpp"
#include "remote_server.hpp"
#include <algorithm>
#include <string>

namespace xs {
namespace {
constexpr SDL_Color ink{8,11,22,255};
constexpr SDL_Color sidebar{13,17,31,255};
constexpr SDL_Color canvas{18,23,41,255};
constexpr SDL_Color card{28,35,56,255};
constexpr SDL_Color focus_color{151,110,255,255};
constexpr SDL_Color cyan{116,231,236,255};
constexpr SDL_Color white{245,247,255,255};
constexpr SDL_Color muted{170,185,207,255};
const char* navigation[]={"Inicio","TV en vivo","Películas","Series","Favoritos","Apoyar"};
const char* category(const Entry& e) {
    switch(e.kind){
        case EntryKind::Channel:return "EN DIRECTO";
        case EntryKind::Asset:return e.program_type=="0"?"SERIE":"PELÍCULA";
        case EntryKind::Episode:return "EPISODIO";
        case EntryKind::Section:return e.program_type=="0"?"SERIES":"PELÍCULAS";
        case EntryKind::LiveSection:return "TV EN VIVO";
        case EntryKind::Category:return "CANALES";
        case EntryKind::Provider:return "PLATAFORMA";
        case EntryKind::ProviderDirectory:return "PLATAFORMAS";
        case EntryKind::VodCategory:return "COLECCIÓN";
        default:return "EXPLORAR";
    }
}
} // namespace

// Entirely new layout and visual hierarchy, sharing only existing catalogue,
// artwork cache, player controls and the already-paired remote infrastructure.
void View::draw_nova(const Catalog& c,size_t selected,const std::string& status){
    if(nova_is_home(c)){draw_nova_home(c,selected);return;}
    poll_images();
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    rect({0,0,1280,720},ink);
    rect({216,0,1064,720},canvas);

    // NOVA sidebar — intended for keyboard, TV remote and iPhone navigation.
    rect({0,0,216,720},sidebar);
    rect({215,0,1,720},{49,57,86,255});
    rect({24,24,46,46},focus_color);
    text(35,24,"X",white,34,heading,42);
    text(81,28,"XUPER",white,125,heading,42);
    text(26,84,"NOVA  /  INTERFAZ EXPERIMENTAL",muted,183,small,45);
    for(int i=0;i<6;++i){
        const int y=149+i*67;
        const bool focused=nav_focus==i;
        const bool active=active_nav==i;
        if(focused)rect({13,y,190,49},focus_color);
        else if(active)rect({13,y,190,49},{49,42,84,255});
        if(active&&!focused)rect({14,y+9,3,30},cyan);
        text(32,y+10,navigation[i],focused?ink:(active?white:muted),162,font,39);
    }
    rect({18,631,178,1},{53,62,87,255});
    text(25,647,"by Jona Ruacho",muted,180,small,29);
    text(25,676,"Windows  •  Vista NOVA",cyan,185,small,30);

    // Top bar always leaves search and pairing accessible.
    text(249,24,nova_is_home(c)?"Descubre algo para ver":(c.title.empty()?"Explorar":c.title),
         white,665,heading,45);
    rect({1000,21,112,45},{39,46,69,255});
    text(1018,31,"Buscar",white,93,font,32);
    rect({1122,21,134,45},pairing_visible?focus_color:SDL_Color{47,41,77,255});
    text(1132,32,"QR iPhone",white,119,font,31);

    const bool any=!c.entries.empty();
    const Entry* chosen=any?&c.entries[std::min(selected,c.entries.size()-1)]:nullptr;
    const bool home=nova_is_home(c);

    // Feature banner: original composition with a large art crop and a readable
    // gradient. All text and imagery come from the real catalogue.
    rect({246,91,983,235},{26,32,57,255});
    for(int j=0;j<26;++j){
        const int xx=633+j*23;
        rect({xx,92,24,233},
             {static_cast<Uint8>(30+j),static_cast<Uint8>(33+j/2),
              static_cast<Uint8>(61+j),255});
    }
    if(chosen&&!chosen->artwork_cache.empty())
        image_cover(chosen->artwork,chosen->artwork_cache,{852,96,371,224});
    else if(!c.artwork_cache.empty())
        image_cover(c.artwork,c.artwork_cache,{852,96,371,224});
    for(int j=0;j<18;++j){
        // Fade the photo into the information column; avoids a hard edge.
        const int alpha=std::max(0,232-j*14);
        rect({700+j*10,94,11,228},{17,22,40,static_cast<Uint8>(alpha)});
    }
    rect({246,91,5,235},focus_color);
    rect({274,110,137,26},{57,49,96,255});
    text(285,113,chosen?category(*chosen):"BIENVENIDO",cyan,119,small,26);
    text(274,147,chosen?chosen->title:"Todo tu entretenimiento",
         white,565,heading,72);
    std::string description=chosen?chosen->description:c.description;
    if(description.empty())description=home?
        "Canales, películas y series. Elige una portada para comenzar.":
        "Selecciona un título para ver detalles y empezar a reproducir.";
    text(275,205,description,muted,544,small,62);

    rect({273,279,158,36},any?focus_color:SDL_Color{80,77,105,255});
    text(287,282,chosen&&chosen->kind==EntryKind::Channel?"Ver canal":"Ver ahora",
         white,139,small,32);
    rect({441,279,158,36},{51,59,86,255});
    text(455,282,"Pantalla completa",white,140,small,32);
    rect({609,279,116,36},{51,59,86,255});
    text(623,282,"Detalles",white,103,small,32);

    auto draw_tile=[&](const NovaTile& tile){
        const Entry& e=c.entries[tile.index];
        if(tile.kind==NovaTile::Featured)return; // the hero is drawn above
        const bool focused=nav_focus<0&&selected==tile.index;
        rect({tile.x-3,tile.y-3,tile.w+6,tile.h+6},
             focused?focus_color:SDL_Color{51,59,81,255});
        rect({tile.x,tile.y,tile.w,tile.h},card);
        const bool portrait=tile.kind==NovaTile::Poster||
                            tile.kind==NovaTile::Movie||tile.kind==NovaTile::Series;
        const int art_height=portrait?tile.h-35:tile.h-36;
        rect({tile.x+3,tile.y+3,tile.w-6,art_height},{39,44,69,255});
        const bool art=image_cover(e.artwork,e.artwork_cache,
                                   {tile.x+3,tile.y+3,tile.w-6,art_height});
        if(!art)text(tile.x+11,tile.y+12,category(e),focused?cyan:muted,
                     tile.w-19,small,46);
        rect({tile.x+2,tile.y+art_height,tile.w-4,tile.h-art_height-2},
             {18,23,41,255});
        text(tile.x+8,tile.y+art_height+4,e.title,white,tile.w-15,small,33);
        if(tile.kind==NovaTile::Channel){
            rect({tile.x+6,tile.y+6,62,20},{16,22,41,211});
            text(tile.x+10,tile.y+5,"EN VIVO",cyan,57,small,24);
        }
    };

    const auto tiles=nova_tiles(c,selected,home?home_last_index:-1);
    if(home){
        text(249,336,"AHORA EN VIVO",white,700,font,30);
        text(250,490,"PELÍCULAS",white,435,font,30);
        text(756,490,"SERIES",white,420,font,30);
        for(const auto& tile:tiles)draw_tile(tile);
        if(tiles.empty())
            text(253,398,"El catálogo aparecerá cuando termine la conexión.",
                 muted,930,font,70);
    }else{
        text(249,338,c.title.empty()?"EXPLORAR":c.title,white,800,font,31);
        text(1071,339,any?
            std::to_string(std::min(selected+1,c.entries.size()))+" / "+
            std::to_string(c.entries.size()):"Sin contenido",muted,154,small,30);
        for(const auto& tile:tiles)draw_tile(tile);
        if(!any)text(255,410,"Todavía no hay contenido en esta sección.",
                     muted,870,font,55);
    }

    if(show_details){
        rect({778,99,442,218},{13,18,34,246});
        rect({778,99,4,218},cyan);
        text(797,114,"DETALLES",cyan,390,small,30);
        text(797,151,chosen?chosen->title:c.title,white,402,heading,68);
        text(797,217,description,muted,401,small,88);
    }

    rect({216,674,1064,1},{61,69,97,255});
    text(246,682,status,muted,656,small,30);
    const auto remote=RemoteServer::shared().info();
    const std::string connection=!remote.running?"Control no disponible":
        remote.connected?"iPhone conectado":"iPhone sin conectar";
    text(917,681,connection,remote.connected?cyan:muted,225,small,32);
    text(1165,682,"F8: Vista",muted,105,small,29);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    RemoteServer::shared().draw_cursor(renderer);
    if(pairing_visible)draw_pairing_overlay();
    SDL_RenderPresent(renderer);
}

int View::hit_nova(const Catalog& c,size_t selected,int x,int y)const{
    if(pairing_visible)return -13;
    if(nova_is_home(c)){
        // The icon AND the adjacent section heading open the full catalogue.
        for(int row=0;row<3;++row){
            const int top=nova_row_y(row)-37;
            if(x>=28&&x<340&&y>=top&&y<top+34)
                return row==0?-3:row==1?-5:-4;
        }
        return nova_hit(c,selected,home_last_index,x,y,nova_row_pixels);
    }
    if(x>=13&&x<203&&y>=149&&y<149+6*67){
        const int i=(y-149)/67;
        if((y-149)%67<49)return -2-i;
    }
    if(y>=21&&y<66&&x>=1000&&x<1112)return -12;
    if(y>=21&&y<66&&x>=1122&&x<1256)return -13;
    if(y>=279&&y<315){
        if(x>=273&&x<431&&!c.entries.empty())return -9;
        if(x>=441&&x<599)return -10;
        if(x>=609&&x<725)return -11;
    }
    return nova_hit(c,selected,nova_is_home(c)?home_last_index:-1,x,y);
}
} // namespace xs
#endif
