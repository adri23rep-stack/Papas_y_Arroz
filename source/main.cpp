#include "catalog.hpp"
#include "player.hpp"
#include "service.hpp"
#include "view.hpp"
#include "trace.hpp"
#include <fstream>
#include <cstdio>
#include <algorithm>
#include <utility>
#include <vector>
#include <cstring>
#include <cmath>
#include "playback_state.hpp"
#include "live_tv_recovery.hpp"
#include "saved_catalog.hpp"
#include <thread>
#include <atomic>
#if defined(_WIN32)
#include <windows.h>
#include <filesystem>
#include "remote_server.hpp"
#include "video_enhanced.hpp"
#include "cinematic_home.hpp"
#include "cinematic_layout.hpp"
#include "nova_layout.hpp"
#endif
#ifdef __SWITCH__
#include <switch.h>

#endif
static bool search_keyboard(xs::View& view,const xs::Catalog& catalog,size_t selected,bool full_memory,std::string& value) {
#ifdef __SWITCH__
    (void)view;(void)catalog;(void)selected;(void)full_memory;
    SwkbdConfig keyboard;char buffer[241]{};
    if(R_FAILED(swkbdCreate(&keyboard,0)))return false;
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard,"Buscar canales, peliculas y series");
    swkbdConfigSetGuideText(&keyboard,"Escribe parte del titulo");
    swkbdConfigSetStringLenMax(&keyboard,80);
    const Result result=swkbdShow(&keyboard,buffer,sizeof(buffer));swkbdClose(&keyboard);
    if(R_FAILED(result))return false;
    value=buffer;return !value.empty();
#else
    value.clear();SDL_StartTextInput();bool accepted=false,running=true;
    while(running){
#ifdef _WIN32
        // Drain typed text on the SDL UI thread, preserving compatibility with
        // the original iPhone keyboard feature without a cross-thread event.
        const std::string remote_text=xs::RemoteServer::shared().take_text();
        if(value.size()+remote_text.size()<=240)value+=remote_text;
#endif
        view.draw(catalog,selected,"Buscar: "+value+" | Enter: buscar; Esc: cancelar",full_memory);
        SDL_Event event;while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running=false;
            if(event.type==SDL_TEXTINPUT&&value.size()+std::strlen(event.text.text)<=240)value+=event.text.text;
            if(event.type==SDL_KEYDOWN){if(event.key.keysym.sym==SDLK_ESCAPE)running=false;
                if(event.key.keysym.sym==SDLK_RETURN){accepted=!value.empty();running=false;}
                if(event.key.keysym.sym==SDLK_BACKSPACE&&!value.empty()){
                    size_t cut=value.size()-1;while(cut>0&&(static_cast<unsigned char>(value[cut])&0xc0)==0x80)--cut;value.resize(cut);
                }
            }
        }SDL_Delay(10);
    }SDL_StopTextInput();return accepted;
#endif
}
int main(int argc,char** argv){
#if defined(_WIN32)
    // Launcher and HDMI shortcuts may start us from an unrelated working directory.
    // Keep the bundled font, demo catalog and local app state beside the executable.
    wchar_t executable[32768]{};
    const DWORD n=GetModuleFileNameW(nullptr,executable,32768);
    if(n>0&&n<32768){
        const auto directory=std::filesystem::path(executable).parent_path();
        std::error_code ec;
        std::filesystem::current_path(directory,ec);
        // A platform-neutral service.template.json is packaged with the
        // Windows build. load_installation() generates the per-device identity
        // at first start; no Android APK or setup executable is required.

    }
#endif
    xs::trace_reset();xs::trace("main:begin");
    const char* path=argc>1?argv[1]:
#ifdef __SWITCH__
        "sdmc:/switch/xuperswitch/catalog.m3u";
    Result rc=socketInitializeDefault();const bool network=R_SUCCEEDED(rc);
    PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);
    const auto type=appletGetAppletType();
    const bool full_memory=type==AppletType_Application||type==AppletType_SystemApplication;
#else
        "catalog.m3u";
    const bool full_memory=true;
#endif
    std::ifstream input(path);xs::Catalog catalog=xs::read_m3u(input);
    std::string status="Conectando...";
    if(!input.is_open())status="No se pudo abrir catalog.m3u.";
    xs::View view;std::string error;
    if(!view.open(error)){xs::trace("main:view_failed");std::fprintf(stderr,"Pantalla: %s\n",error.c_str());
#ifdef __SWITCH__
        if(network)socketExit();
#endif
        return 1;
    }
#ifdef _WIN32
    if(!xs::RemoteServer::shared().start())
        status="El control de iPhone no pudo iniciar. Comprueba firewall o puerto 47831.";
#endif
    xs::PlaybackHistory playback_history;
#ifdef _WIN32
    const auto& remote=xs::RemoteServer::shared();
    playback_history.volume=remote.setting_value(xs::remote_setting_index("volume"));
    playback_history.speed=remote.setting_value(xs::remote_setting_index("speed"))/100.0;
    playback_history.auto_next=remote.setting_value(xs::remote_setting_index("auto_next"))!=0;
#endif
    xs::SavedCatalog saved(
#ifdef __SWITCH__
        "sdmc:/switch/xuperswitch/library_state.json"
#else
        "library_state.json"
#endif
    );
    size_t selected=0;bool running=true,redraw=true,autoload=true,donating=false;
    int pending_remote_navigation=-1;
    int pending_remote_result=-1;
    uint32_t pending_remote_generation=0;
    bool pending_remote_is_browse=false;
    xs::Catalog root_catalog;
    std::atomic<bool> cancel_task{false};std::atomic<size_t> task_done{0},task_total{0};
    std::shared_ptr<xs::ServiceSession> session;
    std::vector<std::pair<xs::Catalog,size_t>> history;
#ifdef _WIN32
    xs::CinematicHomeContent home_data;
    bool home_prepared=false;
#endif
    auto home=[&]{
#ifdef _WIN32
        xs::remember_last_channel(home_data,saved.recent);
        return xs::make_cinematic_home(home_data,root_catalog.entries,view.home_last_index);
#else
        xs::Catalog c;c.title="Inicio";
        xs::Entry tv;tv.title="TV en vivo";tv.kind=xs::EntryKind::LiveSection;c.entries.push_back(tv);
        for(const char* type:{"1","0"}){auto i=std::find_if(root_catalog.entries.begin(),root_catalog.entries.end(),[&](const xs::Entry& e){return e.kind==xs::EntryKind::Section&&e.program_type==type;});if(i!=root_catalog.entries.end())c.entries.push_back(*i);}
        xs::Entry resume;resume.title="Continuar viendo";resume.kind=xs::EntryKind::Resume;c.entries.push_back(resume);
        xs::Entry fav;fav.title="Favoritos";fav.kind=xs::EntryKind::Favorites;c.entries.push_back(fav);
        xs::Entry donate;donate.title="Apoyar el desarrollo";donate.kind=xs::EntryKind::Donation;c.entries.push_back(donate);return c;
#endif
    };
#ifdef _WIN32
    auto prepare_home=[&]{
        if(!session||root_catalog.entries.empty()||home_prepared)return;
        home_prepared=true;
        const auto live_category=std::find_if(root_catalog.entries.begin(),root_catalog.entries.end(),
                    [](const xs::Entry& e){return e.kind==xs::EntryKind::Category;});
        if(live_category!=root_catalog.entries.end()){
            auto result=xs::service_channels(session,live_category->column_id);
            if(result.ok)for(const auto& e:result.catalog.entries)
                if(e.kind==xs::EntryKind::Channel){
                    home_data.channels.push_back(e);
                    if(home_data.channels.size()>=7)break;
                }
        }
        auto fetch_titles=[&](const char* type,std::vector<xs::Entry>& target){
            const auto section=std::find_if(root_catalog.entries.begin(),root_catalog.entries.end(),
                [&](const xs::Entry& e){return e.kind==xs::EntryKind::Section&&e.program_type==type;});
            if(section==root_catalog.entries.end())return;
            auto categories=xs::service_browse(session,*section);
            if(!categories.ok)return;
            // Obtain actual cover artwork, not screenshots or made-up posters.
            // Probe at most two categories; unavailable ones are skipped.
            int visited=0;
            for(const auto& category:categories.catalog.entries){
                if(target.size()>=4||visited>=2||cancel_task.load())break;
                if(category.kind==xs::EntryKind::Asset){
                    target.push_back(category);continue;
                }
                if(category.kind!=xs::EntryKind::VodCategory)continue;
                ++visited;
                auto page=xs::service_browse(session,category);
                if(!page.ok)continue;
                for(const auto& e:page.catalog.entries){
                    if(e.kind!=xs::EntryKind::Asset||e.program_type!=type)continue;
                    target.push_back(e);
                    if(target.size()>=4)break;
                }
            }
        };
        if(!cancel_task.load())fetch_titles("1",home_data.movies);
        if(!cancel_task.load())fetch_titles("0",home_data.series);
    };
#endif
    auto load=[&](auto task,const std::string& message,bool splash=false){
        cancel_task=false;task_done=0;task_total=0;std::atomic<bool> complete{false};
        const auto started=SDL_GetTicks();if(splash)view.splash(message);
        std::thread worker([&]{task();complete.store(true);});
        while(!complete.load()||(splash&&SDL_GetTicks()-started<1100)){
            SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT){running=false;cancel_task=true;}if(e.type==SDL_KEYDOWN&&(e.key.keysym.sym==SDLK_ESCAPE||e.key.keysym.sym==SDLK_b))cancel_task=true;}
#ifdef __SWITCH__
            if(!appletMainLoop()){running=false;cancel_task=true;}padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_B)cancel_task=true;
#endif
            const auto progress=task_total.load()?"  TV "+std::to_string(task_done.load())+"/"+std::to_string(task_total.load()):"";
            if(splash)view.splash(message);else view.draw(catalog,selected,message+progress+(cancel_task.load()?" - terminando consulta...":""),full_memory);
            SDL_Delay(16);
        }worker.join();
#ifdef __SWITCH__
        padUpdate(&pad);
#endif
    };
    while(running){
#ifdef __SWITCH__
        if(!appletMainLoop())break;
#endif
        if(view.poll_images())redraw=true;
        if(redraw){view.favorite=!catalog.entries.empty()&&saved.contains(catalog.entries[std::min(selected,catalog.entries.size()-1)]);if(donating)view.donation();else view.draw(catalog,selected,status,full_memory);redraw=false;}
        bool up=false,down=false,left=false,right=false,play=false,reload=false,login=autoload,search=false,back=false,next=false,previous=false,toggle_favorite=false;int nav_action=pending_remote_navigation;pending_remote_navigation=-1;autoload=false;SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running=false;
            if(event.type==SDL_WINDOWEVENT)redraw=true;
#ifdef _WIN32
            if(event.type==SDL_USEREVENT&&event.user.code==xs::REMOTE_YOUTUBE_RETURN){
                SDL_RaiseWindow(view.window);
                redraw=true;
            }
            if(event.type==SDL_USEREVENT&&
               (event.user.code==xs::REMOTE_OPEN_CATALOG||
                event.user.code==xs::REMOTE_OPEN_BROWSE)){
                pending_remote_result=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                pending_remote_generation=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.user.data2));
                pending_remote_is_browse=event.user.code==xs::REMOTE_OPEN_BROWSE;
            }
            if(event.type==SDL_USEREVENT&&event.user.code==xs::REMOTE_SETTING){
                const int id=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                const int value=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data2));
                if(xs::valid_remote_setting(id,value)){
                    if(id==xs::remote_setting_index("volume"))
                        playback_history.volume=value;
                    else if(id==xs::remote_setting_index("speed"))
                        playback_history.speed=value/100.0;
                    else if(id==xs::remote_setting_index("auto_next"))
                        playback_history.auto_next=value!=0;
                    else if(id==xs::remote_setting_index("video_preset"))
                        xs::save_video_preset(static_cast<xs::VideoPreset>(value));
                }
            }
#endif
#ifdef _WIN32
            if(event.type==SDL_MOUSEMOTION)redraw=true;
            if(event.type==SDL_USEREVENT&&event.user.code==xs::REMOTE_VOLUME)
                playback_history.volume=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
#endif
#ifdef _WIN32
            if(event.type==SDL_USEREVENT&&event.user.code==xs::REMOTE_WHEEL){
                const int ticks=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                previous|=ticks>0;next|=ticks<0;
            }
#endif
            if(event.type==SDL_MOUSEWHEEL){previous|=event.wheel.y>0;next|=event.wheel.y<0;}
            if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_FINGERDOWN){
                const int x=event.type==SDL_FINGERDOWN?int(event.tfinger.x*1280):event.button.x,y=event.type==SDL_FINGERDOWN?int(event.tfinger.y*720):event.button.y;
                if(donating){back=true;continue;}const int hit=view.hit(catalog,selected,x,y);
                if(hit==-13){view.pairing_visible=!view.pairing_visible;redraw=true;}
                else if(hit==-8)toggle_favorite=true;
                else if(hit==-9)play=true;
                else if(hit==-10){view.toggle_fullscreen();redraw=true;}
                else if(hit==-11){view.show_details=!view.show_details;redraw=true;}
                else if(hit==-12){search=true;redraw=true;}
                else if(hit<=-2)nav_action=-hit-2;
                else if(hit>=0){selected=static_cast<size_t>(hit);view.nav_focus=-1;view.show_details=false;play=true;}
            }
            if(event.type==SDL_KEYDOWN){
                switch(event.key.keysym.sym){
                    case SDLK_ESCAPE:
#ifdef _WIN32
                        if(view.pairing_visible){view.pairing_visible=false;redraw=true;break;}
#endif
                        if(!history.empty()||donating||view.nav_focus>=0)back=true;
                        else running=false;
                        break;
#ifdef _WIN32
                    case SDLK_F8:view.toggle_ui_mode();view.nav_focus=-1;view.show_details=false;
                        status=view.nova_enabled?"NOVA activada. F8: regresar a la vista anterior.":"Vista original activada. F8: probar NOVA.";
                        redraw=true;break;
                    case SDLK_F9:view.pairing_visible=!view.pairing_visible;redraw=true;break;
                    case SDLK_F4:nav_action=0;break;
                    case SDLK_F5:nav_action=1;break;
                    case SDLK_F6:nav_action=2;break;
                    case SDLK_F7:nav_action=3;break;
#endif
                    case SDLK_F11:view.toggle_fullscreen();redraw=true;break;
                    case SDLK_F10:view.move_to_next_display();redraw=true;break;
                    case SDLK_RETURN:if(event.key.keysym.mod&KMOD_ALT){view.toggle_fullscreen();redraw=true;}else play=true;break;
                    case SDLK_UP:up=true;break;
                    case SDLK_DOWN:down=true;break;
                    case SDLK_LEFT:left=true;break;
                    case SDLK_RIGHT:right=true;break;
                    case SDLK_f:toggle_favorite=true;break;
                    case SDLK_d:nav_action=5;break;
                    case SDLK_x:search=true;break;
                    case SDLK_l:reload=true;break;
                    case SDLK_y:login=true;break;
                    case SDLK_b:back=true;break;
                    case SDLK_PAGEDOWN:next=true;break;
                    case SDLK_PAGEUP:previous=true;break;
                }
            }
        }
#ifdef __SWITCH__
        padUpdate(&pad);u64 buttons=padGetButtonsDown(&pad);
        if(buttons&HidNpadButton_Plus)running=false;
        up|=(buttons&(HidNpadButton_Up|HidNpadButton_StickLUp))!=0;down|=(buttons&(HidNpadButton_Down|HidNpadButton_StickLDown))!=0;
        left|=(buttons&(HidNpadButton_Left|HidNpadButton_StickLLeft))!=0;right|=(buttons&(HidNpadButton_Right|HidNpadButton_StickLRight))!=0;
        toggle_favorite|=(buttons&HidNpadButton_StickR)!=0;if(buttons&HidNpadButton_Minus)nav_action=5;
        play|=(buttons&HidNpadButton_A)!=0;reload|=(buttons&HidNpadButton_ZL)!=0;search|=(buttons&HidNpadButton_X)!=0;
        login|=(buttons&HidNpadButton_Y)!=0;
        back|=(buttons&HidNpadButton_B)!=0;
        next|=(buttons&HidNpadButton_R)!=0;previous|=(buttons&HidNpadButton_L)!=0;
#endif
#ifdef _WIN32
        if(pending_remote_result>=0){
            xs::Catalog remote;
            if((pending_remote_is_browse?
                xs::RemoteServer::shared().selected_browse_result(pending_remote_generation,
                          static_cast<size_t>(pending_remote_result),remote):
                xs::RemoteServer::shared().selected_result(pending_remote_generation,
                          static_cast<size_t>(pending_remote_result),remote))){
                history.emplace_back(catalog,selected);
                catalog=std::move(remote);
                selected=static_cast<size_t>(pending_remote_result);
                view.active_nav=-1;view.nav_focus=-1;view.show_details=false;
                donating=false;search=false;back=false;nav_action=-1;play=true;redraw=true;
            }else{status="El resultado del iPhone ya no está disponible.";redraw=true;}
            pending_remote_result=-1;
            pending_remote_is_browse=false;
        }
#endif
        if(donating){if(back||play||login){donating=false;redraw=true;}SDL_Delay(10);continue;}
        if(view.nav_focus>=0){
#ifdef _WIN32
            if(view.nova_enabled){
                // NOVA uses a vertical sidebar, not the old horizontal bar.
                if(up){view.nav_focus=(view.nav_focus+5)%6;redraw=true;}
                if(down){view.nav_focus=(view.nav_focus+1)%6;redraw=true;}
                if(right||back){view.nav_focus=-1;redraw=true;back=false;}
            }else{
                if(left||up){view.nav_focus=(view.nav_focus+5)%6;redraw=true;}
                if(right){view.nav_focus=(view.nav_focus+1)%6;redraw=true;}
                if(down||back){view.nav_focus=-1;redraw=true;back=false;}
            }
#else
            if(up){view.nav_focus=(view.nav_focus+5)%6;redraw=true;}if(down){view.nav_focus=(view.nav_focus+1)%6;redraw=true;}
            if(right||back){view.nav_focus=-1;redraw=true;back=false;}
#endif
            if(play){nav_action=view.nav_focus;play=false;}
            up=down=left=right=false;
        }else if(left&&selected%static_cast<size_t>(view.columns(catalog))==0){view.nav_focus=view.active_nav;left=false;redraw=true;}
        if(nav_action==0)login=true;
        if(login){
            if(!session){
                xs::LoginAttempt attempt;xs::trace("bootstrap:begin");
                load([&]{attempt=xs::service_bootstrap(
#ifdef __SWITCH__
                    "sdmc:/switch/xuperswitch/service.json"
#else
                    "service.json"
#endif
                );},"Cargando tu catalogo...",true);
                xs::trace(attempt.authenticated?"bootstrap:accepted":"bootstrap:failed");
                session=attempt.session;status=attempt.status;root_catalog=std::move(attempt.catalog);
#ifdef _WIN32
                xs::RemoteServer::shared().set_session(session,root_catalog);
#endif
            }
            if(session&&root_catalog.entries.empty()){
                xs::BrowseResult result;load([&]{result=xs::service_root(session);},"Reintentando carga del catalogo...");status=result.status;
                if(result.ok){
                    root_catalog=std::move(result.catalog);
#ifdef _WIN32
                    xs::RemoteServer::shared().set_session(session,root_catalog);
#endif
                }
            }
#ifdef _WIN32
            if(session&&!root_catalog.entries.empty()&&!home_prepared){
                load([&]{prepare_home();},"Preparando portadas de TV, películas y series...");
            }
#endif
            if(session){catalog=home();if(!root_catalog.entries.empty())status="Explora TV, películas y series. X: buscar.";else status+=" Pulsa Y para reintentar el catálogo.";}else if(catalog.entries.empty()){catalog.title="Sin conexion";status+=" Pulsa Y para reintentar.";}
            else{catalog.title="Lista local";status+=" Lista M3U local disponible; L para recargar; Y para reintentar.";}
            selected=0;history.clear();view.active_nav=0;view.nav_focus=-1;redraw=true;
        }
        if(nav_action>0){
            view.show_details=false;
            if(nav_action==5){donating=true;redraw=true;continue;}
            if(nav_action==4){history.emplace_back(catalog,selected);catalog={};catalog.title="Favoritos";catalog.entries=saved.favorites;selected=0;view.active_nav=4;view.nav_focus=-1;redraw=true;}
            else if(session){
                if(nav_action==1){
                    history.emplace_back(catalog,selected);
                    catalog={};catalog.title="TV en vivo";
#ifdef _WIN32
                    xs::remember_last_channel(home_data,saved.recent);
                    if(home_data.has_last)catalog.entries.push_back(home_data.last_channel);
                    for(const auto& e:home_data.channels)
                        if(!home_data.has_last||e.channel_code!=home_data.last_channel.channel_code)
                            catalog.entries.push_back(e);
#endif
                    for(const auto& e:root_catalog.entries)if(e.kind==xs::EntryKind::Category)catalog.entries.push_back(e);
                    selected=0;
#ifdef _WIN32
                    // TV opens the actual last-watched channel, not a random category.
                    if(home_data.has_last)play=true;
#endif
                }
                else{const std::string type=nav_action==2?"1":"0";const auto i=std::find_if(root_catalog.entries.begin(),root_catalog.entries.end(),[&](const xs::Entry& e){return e.kind==xs::EntryKind::Section&&e.program_type==type;});
                    if(i!=root_catalog.entries.end()){xs::BrowseResult result;load([&]{result=xs::service_browse(session,*i);},"Cargando categorias...");status=result.status;if(result.ok){history.emplace_back(catalog,selected);catalog=std::move(result.catalog);catalog.title=i->title;selected=0;}}
                    else status="Esta seccion no esta disponible en el servicio.";
                }view.active_nav=nav_action;view.nav_focus=-1;redraw=true;
            }else{status="Sin sesion. Pulsa Y para reintentar la conexion.";redraw=true;}
        }
        if(toggle_favorite&&!catalog.entries.empty()){
            const auto entry=catalog.entries[selected];
            if(entry.kind!=xs::EntryKind::Channel)status="Selecciona un canal para guardarlo en Favoritos.";
            else if(!saved.toggle(entry))status="No se pudo guardar Favoritos en la SD.";
            else{status=saved.contains(entry)?"Canal guardado en Favoritos.":"Canal quitado de Favoritos.";if(catalog.title=="Favoritos"){catalog.entries=saved.favorites;selected=catalog.entries.empty()?0:std::min(selected,catalog.entries.size()-1);}}
            redraw=true;
        }
        if(search){
            if(!session)status="Pulsa Y para activar antes de buscar.";
            else{
                std::string query;
                if(search_keyboard(view,catalog,selected,full_memory,query)){
                    xs::BrowseResult result;xs::trace("catalog:search_begin");
                    load([&]{result=xs::service_search_all(session,query,[&](size_t done,size_t total){task_done=done;task_total=total;return !cancel_task.load();});},"Buscando canales, peliculas y series... B: detener TV");status=result.status;
                    xs::trace(result.ok?"catalog:search_loaded":"catalog:search_failed");
                    if(result.ok){history.emplace_back(std::move(catalog),selected);catalog=std::move(result.catalog);selected=0;}
                }
            }
#ifdef __SWITCH__
            padUpdate(&pad); // Consume the keyboard's confirmation button state.
#endif
            redraw=true;
        }
        if(back&&!history.empty()) {
            catalog=std::move(history.back().first);selected=history.back().second;history.pop_back();
            status="Elige un titulo o canal.";if(history.empty())view.active_nav=0;view.show_details=false;redraw=true;
        }
        if(!catalog.entries.empty()){
            const size_t cols=view.columns(catalog),page=view.page_size(catalog);
#ifdef _WIN32
            if(xs::is_home(catalog)){
                const auto move=[&](int dx,int dy){
                    return view.nova_enabled?
                        xs::nova_move(catalog,selected,view.home_last_index,dx,dy):
                        xs::cinematic_move(catalog,selected,view.home_last_index,dx,dy);
                };
                if(up)selected=move(0,-1);
                if(down)selected=move(0,1);
                if(left)selected=move(-1,0);
                if(right)selected=move(1,0);
                if(next||previous)selected=move(0,next?1:-1);
                if(up||down||left||right||next||previous)redraw=true;
            }else
#endif
            {
                if(up){selected=selected>=cols?selected-cols:selected;redraw=true;}
                if(down){selected=std::min(selected+cols,catalog.entries.size()-1);redraw=true;}
                if(left){if(selected>0)--selected;redraw=true;}if(right){selected=std::min(selected+1,catalog.entries.size()-1);redraw=true;}
                if(next){selected=std::min(selected+page,catalog.entries.size()-1);redraw=true;}
                if(previous){selected=selected>=page?selected-page:0;redraw=true;}
            }
            if(up||down||left||right||next||previous)view.show_details=false;
            if(play){
                const xs::Entry entry=catalog.entries[selected];
                if(entry.kind==xs::EntryKind::Donation){donating=true;redraw=true;continue;}
                if(entry.kind==xs::EntryKind::LiveSection||entry.kind==xs::EntryKind::Favorites||entry.kind==xs::EntryKind::Resume){
                    history.emplace_back(catalog,selected);catalog={};catalog.title=entry.title;
                    if(entry.kind==xs::EntryKind::LiveSection){for(const auto& e:root_catalog.entries)if(e.kind==xs::EntryKind::Category)catalog.entries.push_back(e);view.active_nav=1;}
                    else if(entry.kind==xs::EntryKind::Favorites){catalog.entries=saved.favorites;view.active_nav=4;}
                    else for(const auto& e:saved.recent)if(e.kind==xs::EntryKind::Episode&&playback_history.position(e.content_id)>0)catalog.entries.push_back(e);
                    selected=0;redraw=true;continue;
                }
                if(entry.kind==xs::EntryKind::Category||entry.kind==xs::EntryKind::Section||
                   entry.kind==xs::EntryKind::VodCategory||entry.kind==xs::EntryKind::Asset||
                   entry.kind==xs::EntryKind::VodPage||entry.kind==xs::EntryKind::SearchPage){
                    status="Cargando contenido...";view.draw(catalog,selected,status,full_memory);
                    xs::trace("catalog:browse_begin");
                    xs::BrowseResult result;load([&]{result=entry.kind==xs::EntryKind::Category?xs::service_channels(session,entry.column_id):xs::service_browse(session,entry);},"Cargando "+entry.title+"...");status=result.status;
                    if(result.catalog.title.empty())result.catalog.title=entry.title;
                    xs::trace(result.ok?"catalog:browse_loaded":"catalog:browse_failed");
                    if(result.ok){history.emplace_back(std::move(catalog),selected);catalog=std::move(result.catalog);selected=0;}
                }else if(!full_memory)status="Abre hbmenu manteniendo R sobre un juego para probar el video.";
                else{
                    int variant=0,live_failures=0;double start=0,speed=playback_history.speed,volume=playback_history.volume;bool auto_next=playback_history.auto_next,keep_paused=false;
                    std::vector<size_t> queue;
                    for(size_t i=0;i<catalog.entries.size();++i)if(catalog.entries[i].kind==entry.kind&&(entry.series_id.empty()||catalog.entries[i].series_id==entry.series_id))queue.push_back(i);
                    bool continue_playback=true;
                    while(continue_playback){continue_playback=false;const auto current=catalog.entries[selected];
                        std::string source=current.source;std::vector<std::pair<std::string,std::string>> headers;
                        xs::PlaybackOptions options;options.font=view.font;options.title=current.title;
                        options.variant=variant;options.start=start>0?start:current.kind==xs::EntryKind::Episode?playback_history.position(current.content_id):0;options.speed=speed;options.volume=volume;options.auto_next=auto_next;options.paused=keep_paused;
                        options.is_live=current.kind==xs::EntryKind::Channel;
                        if(current.kind==xs::EntryKind::Channel||current.kind==xs::EntryKind::Episode){
                            status="Solicitando fuente del video...";view.draw(catalog,selected,status,full_memory);
                            xs::SourceResult result;xs::trace("catalog:source_begin");load([&]{result=xs::service_source(session,current,variant);},"Preparando reproduccion...");status=result.status;
                            xs::trace(result.ok?"catalog:source_received":"catalog:source_unavailable");
                            source=result.ok?std::move(result.source):"";headers=std::move(result.headers);options.variants=std::move(result.variants);
                            const auto subtitles=result.subtitles;
                            for(const auto& sub:subtitles)options.subtitles.push_back(sub.language);
                            options.subtitle_loader=[session,subtitles](size_t i,std::string& data,std::string& error){
                                return i<subtitles.size()&&xs::service_subtitle(session,subtitles[i],data,error);
                            };
                        }
                        for(size_t i=0;i<queue.size();++i){
                            const auto& item=catalog.entries[queue[i]];
                            options.items.push_back(item.title);
                            if(current.kind==xs::EntryKind::Channel){
                                xs::LiveChannelTile tile;
                                tile.title=item.title;tile.artwork=item.artwork;
                                tile.artwork_cache=item.artwork_cache;
                                tile.description=item.description;
                                options.channels.push_back(std::move(tile));
                            }
                            if(queue[i]==selected)options.item=static_cast<int>(i);
                        }
                        if(current.kind==xs::EntryKind::Channel&&session&&false){
                            // Preview decoder temporarily disabled until the
                            // live-TV stability fix is verified on real hardware.
                            // Copy only the eligible channels; an asynchronous
                            // preview must not hold references to the mutable
                            // navigation catalogue or require another account.
                            std::vector<xs::Entry> preview_entries;
                            preview_entries.reserve(queue.size());
                            for(size_t channel_idx:queue)preview_entries.push_back(catalog.entries[channel_idx]);
                            options.preview_resolve=
                                [session,preview_entries=std::move(preview_entries)](
                                    size_t i,std::string& video,
                                    std::vector<std::pair<std::string,std::string>>& preview_headers,
                                    std::string& err)->bool {
                                    if(i>=preview_entries.size()||
                                       preview_entries[i].kind!=xs::EntryKind::Channel){
                                        err="Canal no disponible.";return false;
                                    }
                                    auto source_result=xs::service_source(session,preview_entries[i],0);
                                    if(!source_result.ok){err=source_result.status;return false;}
                                    video=std::move(source_result.source);
                                    preview_headers=std::move(source_result.headers);
                                    return !video.empty();
                                };
                        }
                        if(source.empty()&&options.is_live){
                            const auto retry=xs::retry_live_channel(true,false,true,false,
                                variant,static_cast<int>(options.variants.size()),live_failures);
                            if(retry.retry){
                                variant=retry.next_variant;live_failures=retry.next_attempt;
                                status="Reconectando TV en vivo ("+
                                    std::to_string(live_failures)+"/2)...";
                                xs::trace("live:source_retry");
                                SDL_Delay(static_cast<Uint32>(retry.delay_ms));
                                continue_playback=true;
                                continue;
                            }
                            if(status.empty())status="Canal no disponible. Prueba otra señal.";
                            xs::trace("live:source_unavailable_after_retries");
                        }
                        if(!source.empty()){
                            xs::PlaybackOutcome outcome;status=xs::play(source,view.renderer,headers,options,&outcome);xs::trace("main:video_returned");
                            speed=outcome.speed;volume=outcome.volume;auto_next=outcome.auto_next;
                            const auto recovery=xs::retry_live_channel(options.is_live,
                                outcome.requested_remote_result>=0||
                                    outcome.requested_navigation>=0||
                                    outcome.next_item>=0||outcome.next_variant>=0,
                                !status.empty(),outcome.ended,variant,
                                static_cast<int>(options.variants.size()),live_failures);
                            if(recovery.retry){
                                variant=recovery.next_variant;
                                live_failures=recovery.next_attempt;
                                xs::trace("live:playback_reconnect");
                                status="Se cortó la señal. Reconectando ("+
                                    std::to_string(live_failures)+"/2)...";
                                SDL_Delay(static_cast<Uint32>(recovery.delay_ms));
                                continue_playback=true;
                                continue;
                            }
#ifdef _WIN32
                            if(outcome.requested_navigation>=0)pending_remote_navigation=outcome.requested_navigation;
                            if(outcome.requested_remote_result>=0){
                                pending_remote_result=outcome.requested_remote_result;
                                pending_remote_generation=outcome.requested_remote_generation;
                                pending_remote_is_browse=outcome.requested_remote_browse;
                            }
#endif
                            if(status.empty()&&(current.kind==xs::EntryKind::Channel||outcome.position>3||outcome.ended)&&!saved.remember(current))xs::trace("catalog:recent_save_failed");
                            if(status.empty()&&!playback_history.save(current.kind==xs::EntryKind::Episode?current.content_id:"",outcome.position,outcome.speed,outcome.volume,outcome.auto_next,outcome.ended))xs::trace((std::string("play:history_save_failed stage=")+playback_history.last_error).c_str());
                            if(outcome.ended&&auto_next&&!current.series_id.empty()&&static_cast<size_t>(options.item+1)<queue.size())outcome.next_item=options.item+1;
                            if(pending_remote_result>=0){continue_playback=false;}
                            else if(outcome.next_variant>=0){variant=outcome.next_variant;start=outcome.position;keep_paused=outcome.paused;continue_playback=true;}
                            else if(outcome.next_item>=0&&static_cast<size_t>(outcome.next_item)<queue.size()){
                                selected=queue[outcome.next_item];variant=0;start=0;keep_paused=false;continue_playback=true;
                            }
                            if(status.empty())status="Video finalizado. A: reproducir; X: buscar.";
                        }
                    }

                }
                redraw=true;
            }
        }
        if(reload){std::ifstream file(path);catalog=xs::read_m3u(file);selected=0;history.clear();
            status=file.is_open()?"Lista local recargada.":"No se pudo abrir catalog.m3u.";redraw=true;}
        SDL_Delay(10);
    }
#ifdef _WIN32
    xs::RemoteServer::shared().stop();
#endif
    xs::trace("main:close_view");view.close();
#ifdef __SWITCH__
    if(network)socketExit();
#endif
    xs::trace("main:exit");return 0;
}
