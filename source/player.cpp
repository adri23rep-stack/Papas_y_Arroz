#include "player.hpp"
#if defined(_WIN32)
#include "remote_server.hpp"
#endif
#include "channel_rail.hpp"
#if defined(_WIN32)
#include "live_preview.hpp"
#endif
#include "artwork.hpp"
#include <memory>
#include <thread>
#include <utility>
#include "display_tools.hpp"
#include "catalog.hpp"
#include "trace.hpp"
#include "playback_error.hpp"
#include "render_quality.hpp"
#ifdef _WIN32
#include "video_enhanced.hpp"
#include "gpu_readback.hpp"
#endif
#include <mpv/client.h>
#include <mpv/render.h>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#if defined(_WIN32)
#include <malloc.h>
#endif
#ifdef __SWITCH__
#include <switch.h>
#endif
namespace xs {
static void update(void* p){static_cast<std::atomic<bool>*>(p)->store(true);}
std::string play(const std::string& source,SDL_Renderer* renderer,const std::vector<std::pair<std::string,std::string>>& headers,const PlaybackOptions& options,PlaybackOutcome* outcome) {
    if(!renderer||!valid_source(source))return "Origen o pantalla no admitidos.";
    trace("play:begin");
    mpv_handle* mpv=mpv_create();mpv_render_context* context=nullptr;SDL_Texture* texture=nullptr;
    void* pixels=nullptr;std::atomic<bool> dirty{true};std::string error;bool subtitle_written=false;
#ifdef _WIN32
    std::unique_ptr<GpuReadback> gpu;
    bool gpu_active=false,shader_enabled=false,shader_ready=false;
    std::string gpu_diagnostic,shader_path;
#endif
#ifdef __SWITCH__
    const char* subtitle_cache="sdmc:/switch/xuperswitch/subtitle_cache.srt";
#else
    const char* subtitle_cache="subtitle_cache.srt";
#endif
    auto cleanup=[&]{
        trace("play:free_context_begin");
#ifdef _WIN32
        if(gpu){
            gpu->close(); // free libmpv renderer while its own GL context exists
            gpu.reset();
            context=nullptr;
        }
#endif
        if(context)mpv_render_context_free(context);
        trace("play:free_context_end");
        if(mpv)mpv_terminate_destroy(mpv);
        trace("play:mpv_destroyed");
        if(texture)SDL_DestroyTexture(texture);
#if defined(_WIN32)
        _aligned_free(pixels);
#else
        std::free(pixels);
#endif
        if(subtitle_written)std::remove(subtitle_cache);
#ifdef __SWITCH__
        appletSetMediaPlaybackState(false);
#endif
        trace("play:return_to_menu");
    };
    if(!mpv){cleanup();return "No se pudo iniciar libmpv.";}
    auto option=[&](const char* name,const char* value){int rc=mpv_set_option_string(mpv,name,value);
        if(rc<0&&error.empty())error=std::string(name)+": "+mpv_error_string(rc);};
    option("config","no");option("load-scripts","no");option("terminal","no");
    option("vo","libmpv");
#if defined(_WIN32)
    // Stage 1: NVDEC/D3D11 copy-back decode (not zero-copy rendering).
    // Keep a reliable CPU preset for incompatible hardware.
    VideoPreset video_preset=load_video_preset();
    // Changing the saved preset during playback affects the NEXT video only.
    const bool extra_detail_started=(video_preset==VideoPreset::ExtraDetail);
    std::string detail_graph;
    bool detail_active=false;
    option("hwdec",video_hwdec(video_preset));
    option("sws-scaler",video_scaler(video_preset));
    if(video_uses_gpu(video_preset)){
        // Resolve relative to the EXE, not the current working directory.
        char* base=SDL_GetBasePath();
        shader_path=std::string(base?base:"")+"shaders/FSRCNNX_x2_8-0-4-1.glsl";
        if(base)SDL_free(base);
        shader_ready=std::ifstream(shader_path,std::ios::binary).good();
        if(shader_ready){
            // The mpv OpenGL render API can execute this real GLSL shader.
            // No FFmpeg CAS pipeline is loaded in this GPU mode.
            option("glsl-shaders",shader_path.c_str());
            option("scale","ewa_lanczossharp");
            option("cscale","ewa_lanczos");
            option("fbo-format","rgba16f");
            shader_enabled=true;
        }else{
            gpu_diagnostic="Falta shaders/FSRCNNX_x2_8-0-4-1.glsl; usando CPU.";
        }
    }
#else
    option("hwdec","no");
#endif
    option("osd-level","0");option("sub-visibility","yes");
    option("sub-auto","no");option("audio-pitch-correction","yes");
    option("speed",std::to_string(options.speed).c_str());option("volume",std::to_string(options.volume).c_str());
    option("pause",options.paused?"yes":"no");
    option("network-timeout","15");option("tls-verify","yes");option("demuxer-max-bytes","33554432");
#ifdef __SWITCH__
    option("ao","hos");option("sub-fonts-dir","sdmc:/switch/xuperswitch");option("sub-font","DejaVu Sans");const char* ca="sdmc:/switch/xuperswitch/cacert.pem";
    if(std::ifstream(ca).good())option("tls-ca-file",ca);
#elif defined(XUPERSWITCH_TEST)
    option("ao","null");
#endif
    if(headers.size()>8){cleanup();return "Demasiadas cabeceras de video.";}
    std::vector<std::string> header_lines;std::vector<mpv_node> header_nodes;
    for(const auto& header:headers){
        if((header.first!="Content-Auth"&&header.first!="Content-License"&&header.first!="App"&&header.first!="App-Version"&&header.first!="Ranger-Id"&&header.first!="Pragma")||header.second.empty()||header.second.size()>16384||
           std::any_of(header.second.begin(),header.second.end(),[](unsigned char c){return c<32||c==127;})){cleanup();return "Cabecera de video no admitida.";}
        header_lines.push_back(header.first+": "+header.second);
    }
    if(!header_lines.empty()){
        header_nodes.resize(header_lines.size());
        for(size_t i=0;i<header_lines.size();++i){header_nodes[i].format=MPV_FORMAT_STRING;header_nodes[i].u.string=header_lines[i].data();}
        mpv_node_list list{static_cast<int>(header_nodes.size()),header_nodes.data(),nullptr};
        mpv_node node{};node.format=MPV_FORMAT_NODE_ARRAY;node.u.list=&list;
        const int result=mpv_set_option(mpv,"http-header-fields",MPV_FORMAT_NODE,&node);
        if(result<0){cleanup();return "No se pudieron configurar las cabeceras de video.";}
    }
    if(!error.empty()){cleanup();return error;}
    int rc=mpv_initialize(mpv);
        if(rc<0){error=mpv_error_string(rc);cleanup();return error;}
    trace("play:mpv_initialized");
    mpv_request_log_messages(mpv,"v");
    // Render the Windows video in 1080p instead of upscaling a 360p buffer.
    constexpr int width=kVideoRenderWidth,height=kVideoRenderHeight;size_t stride=width*4;
#ifdef _WIN32
    if(shader_ready&&video_uses_gpu(video_preset)){
        gpu=std::make_unique<GpuReadback>();
        if(gpu->open(mpv,width,height,gpu_diagnostic)){
            context=gpu->context();
            gpu_active=true;
            trace(("video:gpu_context="+gpu_diagnostic).c_str());
        }else{
            trace(("video:gpu_fallback="+gpu_diagnostic).c_str());
            gpu.reset();
            shader_enabled=false;
        }
    }
#endif
    if(!context){
        mpv_render_param init[]={{MPV_RENDER_PARAM_API_TYPE,const_cast<char*>(MPV_RENDER_API_TYPE_SW)},{MPV_RENDER_PARAM_INVALID,nullptr}};
        rc=mpv_render_context_create(&context,mpv,init);
        if(rc<0){error=std::string("Render software: ")+mpv_error_string(rc);cleanup();return error;}
    }
    mpv_render_context_set_update_callback(context,update,&dirty);
#if defined(_WIN32)
    pixels=_aligned_malloc(stride*height,64);
#else
    pixels=std::aligned_alloc(64,stride*height);
#endif
    texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STREAMING,width,height);
    if(!pixels||!texture){error="No se pudo crear el buffer de video.";cleanup();return error;}
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE);
    const char* load[]={"loadfile",source.c_str(),nullptr};rc=mpv_command(mpv,load);
    if(rc<0){error=mpv_error_string(rc);cleanup();return error;}
    trace("play:file_requested");
#ifdef __SWITCH__
    appletSetMediaPlaybackState(true);
    PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);padUpdate(&pad);
#endif
    bool running=true,paused=false,first_frame=true,menu=false,loaded=false;
    int page=0,selection=0,aspect=0;double pending_start=options.start;
    int64_t external_id=-1,external_index=-1;
    const std::vector<double> speeds{0.5,0.75,1,1.25,1.5,1.75,2};
    const std::vector<std::string> ratios{"Original","16:9","4:3","Rellenar"};
    PlaybackOutcome result;bool auto_next=options.auto_next;std::string notice,media_hint;
#ifdef _WIN32
    if(video_uses_gpu(video_preset)&&!gpu_active){
        notice="OpenGL no disponible; se usa el renderer compatible.";
        trace(("video:gpu_unsupported="+gpu_diagnostic).c_str());
    }
#endif
    Uint32 visible_until=SDL_GetTicks()+5000,last_draw=0;
    const bool live_channels=options.channels.size()>1&&options.channels.size()==options.items.size();
    ChannelRail rail(live_channels?options.channels.size():0,
                     live_channels?static_cast<size_t>(std::max(0,options.item)):0);
    bool strip_visible=live_channels;
    Uint32 strip_until=SDL_GetTicks()+6000;
    std::unique_ptr<Artwork> rail_artwork;
    if(live_channels)rail_artwork=std::make_unique<Artwork>(renderer);
#ifdef _WIN32
    // Source resolution can take network time. A background worker owns a
    // *copy* of its callback and shared result; it cannot outlive stack refs.
    struct PreviewWork {
        explicit PreviewWork(size_t i):index(i){}
        size_t index;
        std::string url,error;
        std::vector<std::pair<std::string,std::string>> headers;
        bool ok=false;
        std::atomic<bool> done{false};
    };
    std::shared_ptr<PreviewWork> preview_work;
    LivePreview miniature(renderer);
    int opened_preview=-1;
    std::string preview_error;
#endif
    auto show_channels=[&](){
        if(live_channels){strip_visible=true;strip_until=SDL_GetTicks()+6500;}
    };
    auto property=[&](const char* name){double value=0;mpv_get_property(mpv,name,MPV_FORMAT_DOUBLE,&value);return value;};
    auto flag=[&](const char* name){int value=0;mpv_get_property(mpv,name,MPV_FORMAT_FLAG,&value);return value!=0;};
    auto set_number=[&](const char* name,double value){if(mpv_set_property(mpv,name,MPV_FORMAT_DOUBLE,&value)<0)notice="Esta opcion no esta disponible en este video.";};
    auto command=[&](const char* name,const std::string& value,const char* mode=nullptr){const char* args[]={name,value.c_str(),mode,nullptr};
        if(mpv_command(mpv,args)<0)notice="No se pudo aplicar esa opcion al video.";};
    auto show=[&]{visible_until=SDL_GetTicks()+4000;};
#ifdef _WIN32
    auto set_gpu_shader=[&](bool enabled){
        if(!gpu_active||!shader_ready)return false;
        const int rc=mpv_set_property_string(mpv,"glsl-shaders",enabled?shader_path.c_str():"");
        if(rc<0){
            notice="No se pudo alternar el shader GPU.";
            trace(("video:gpu_shader_toggle_failed="+std::string(mpv_error_string(rc))).c_str());
            return false;
        }
        shader_enabled=enabled;
        trace(enabled?"video:fsrcnnx=on":"video:fsrcnnx=off");
        return true;
    };

    // Apply/undo a real FFmpeg filter graph on the SAME source frame for A/B.
    // On failure revert to unfiltered playback rather than breaking video.
    auto set_extra_detail=[&](bool enabled){
        if(detail_graph.empty())return false;
        const int apply=mpv_set_property_string(mpv,"vf",enabled?detail_graph.c_str():"");
        if(apply<0){
            mpv_set_property_string(mpv,"vf","");
            detail_active=false;
            notice="Filtro no disponible; reproduccion original.";
            trace(("video:detail_filter_failed="+std::string(mpv_error_string(apply))).c_str());
            return false;
        }
        detail_active=enabled;
        trace((std::string("video:extra_detail=")+(enabled?"on":"off")).c_str());
        return true;
    };
#endif
    auto seek_to=[&](double amount){if(!flag("seekable")){notice="Esta emision no permite adelantar o retroceder.";show();return;}
        command("seek",std::to_string(amount),"relative+exact");show();};
    auto toggle_pause=[&]{int value=!flag("pause");
        if(mpv_set_property(mpv,"pause",MPV_FORMAT_FLAG,&value)<0)notice="No se pudo pausar.";
        show();};
    struct Track {int64_t id;std::string type,label;bool selected;};
    auto tracks=[&]{std::vector<Track> rows;mpv_node node{};
        if(mpv_get_property(mpv,"track-list",MPV_FORMAT_NODE,&node)>=0){
            if(node.format==MPV_FORMAT_NODE_ARRAY)for(int i=0;i<node.u.list->num;++i){const auto& n=node.u.list->values[i];
        if(n.format!=MPV_FORMAT_NODE_MAP)continue;
                Track t{0,"","",false};std::string lang,title,codec;
                for(int j=0;j<n.u.list->num;++j){const std::string key=n.u.list->keys[j];const auto& v=n.u.list->values[j];
                    if(key=="id"&&v.format==MPV_FORMAT_INT64)t.id=v.u.int64;
                    if(key=="selected"&&v.format==MPV_FORMAT_FLAG)t.selected=v.u.flag;
                    if(v.format==MPV_FORMAT_STRING){const std::string value=v.u.string;
                        if(key=="type")t.type=value;
        if(key=="lang")lang=value;
        if(key=="title")title=value;
        if(key=="codec")codec=value;}
                }
                t.label=(lang.empty()?"Pista "+std::to_string(t.id):lang)+(title.empty()?"":" - "+title)+(codec.empty()?"":" ["+codec+"]");
                if(t.label.size()>120)t.label.resize(120);
                rows.push_back(std::move(t));
            }
            mpv_free_node_contents(&node);
        }return rows;};
    auto cycle_track=[&](const std::string& type){auto rows=tracks();std::vector<Track> choices;
        if(type=="sub")choices.push_back({-1,"sub","Desactivados",!flag("sub-visibility")});
        for(const auto& t:rows)if(t.type==type)choices.push_back(t);
        if(choices.empty()){notice="No hay pistas alternativas en este video.";show();return;}
        size_t selected=0;for(size_t i=0;i<choices.size();++i)if(choices[i].selected)selected=i;
        const auto& next=choices[(selected+1)%choices.size()];
        mpv_set_property_string(mpv,type=="audio"?"aid":"sid",next.id<0?"no":std::to_string(next.id).c_str());
        if(type=="sub"){int on=next.id>=0;mpv_set_property(mpv,"sub-visibility",MPV_FORMAT_FLAG,&on);}
        notice=next.label;show();};
    auto cycle_speed=[&](int direction){double speed=property("speed");size_t i=0;for(size_t k=0;k<speeds.size();++k)if(std::abs(speeds[k]-speed)<0.01)i=k;
        set_number("speed",speeds[(i+speeds.size()+direction)%speeds.size()]);show();};
    auto cycle_aspect=[&]{aspect=(aspect+1)%4;set_number("video-aspect-override",aspect==1?16.0/9:aspect==2?4.0/3:-1);
        set_number("panscan",aspect==3?1:0);notice="Pantalla: "+ratios[aspect];show();};
    auto request_item=[&](int index){if(index>=0&&static_cast<size_t>(index)<options.items.size()){result.next_item=index;running=false;}};
    auto draw_text=[&](int x,int y,const std::string& message,SDL_Color color,int width=1150){if(!options.font||message.empty())return;
        SDL_Surface* surface=TTF_RenderUTF8_Blended_Wrapped(options.font,message.c_str(),color,width*kFontRenderScale);
        if(!surface)return;
        SDL_Texture* label=SDL_CreateTextureFromSurface(renderer,surface);SDL_Rect dst{x,y,(surface->w+kFontRenderScale-1)/kFontRenderScale,(surface->h+kFontRenderScale-1)/kFontRenderScale};
        if(label){SDL_RenderCopy(renderer,label,nullptr,&dst);SDL_DestroyTexture(label);}SDL_FreeSurface(surface);};
    auto clock=[](double value){int n=std::max(0,static_cast<int>(value));char b[32];std::snprintf(b,sizeof(b),"%02d:%02d:%02d",n/3600,n/60%60,n%60);return std::string(b);};
    struct Row {std::string label;int action;int64_t value=0;};
    auto menu_rows=[&]{std::vector<Row> rows;
        if(page==0){rows={{flag("pause")?"Continuar":"Pausar",1},{"Audio / idioma",2},{"Subtitulos",3},{"Calidad / fuente",4},
            {"Episodios / canales",5},{"Velocidad: "+std::to_string(property("speed")).substr(0,4)+"x",6},
            {"Pantalla: "+ratios[aspect],7},{"Volumen: "+std::to_string(static_cast<int>(property("volume"))),8},
            {flag("mute")?"Activar sonido":"Silenciar",9},{"Sincronia audio: "+std::to_string(property("audio-delay")).substr(0,5)+" s",10},
            {"Sincronia subtitulos: "+std::to_string(property("sub-delay")).substr(0,5)+" s",11},
            {"Tamano subtitulos: "+std::to_string(property("sub-scale")).substr(0,4),12},{"Brillo: "+std::to_string(static_cast<int>(property("brightness"))),13},
            {"Retroceder 60 segundos",14},{"Adelantar 60 segundos",15},{"Reiniciar video",16},{"Informacion del video",17},{std::string("Siguiente episodio automatico: ")+(auto_next?"Si":"No"),24},{"Cerrar reproductor",18}};
#ifdef _WIN32
            rows.push_back({"Video mejorado: "+std::string(video_preset_label(video_preset))+" (proximo video)",25});
            if(extra_detail_started)
                rows.push_back({"Comparar imagen: "+std::string(detail_active?"CAS activo":"Original")+" (F8)",26});
            if(gpu_active&&shader_ready)
                rows.push_back({"Comparar GPU: "+std::string(shader_enabled?"FSRCNNX":"Original")+" (F8)",27});
#endif
        }
        if(page==1||page==2){if(page==2)rows.push_back({"Desactivar subtitulos",20,-1});for(const auto& t:tracks())if(t.type==(page==1?"audio":"sub"))rows.push_back({(t.selected?"* ":"  ")+t.label,page==1?19:20,t.id});}
        if(page==2)for(size_t i=0;i<options.subtitles.size();++i)rows.push_back({"Servicio: "+options.subtitles[i],23,static_cast<int64_t>(i)});
        if(page==3)for(size_t i=0;i<options.variants.size();++i)rows.push_back({(static_cast<int>(i)==options.variant?"* ":"  ")+options.variants[i],21,static_cast<int64_t>(i)});
        if(page==4)for(size_t i=0;i<options.items.size();++i)rows.push_back({(static_cast<int>(i)==options.item?"* ":"  ")+options.items[i],22,static_cast<int64_t>(i)});
        if(rows.empty())rows.push_back({"No hay opciones disponibles para este video",0});
        return rows;};
    while(running){
#ifdef __SWITCH__
        if(!appletMainLoop())break;
#endif
        bool activate=false,back=false,settings=false,audio=false,subs=false,slower=false,faster=false,ratio=false,mute=false;
        int horizontal=0,vertical=0,jump=0,item_delta=0;SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT)running=false;
#ifdef _WIN32
            if(event.type==SDL_USEREVENT&&event.user.code==REMOTE_OPEN_CATALOG){
                result.requested_remote_result=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                result.requested_remote_generation=static_cast<uint32_t>(
                    reinterpret_cast<uintptr_t>(event.user.data2));
                running=false;break;
            }
#endif
#ifdef _WIN32
            if(event.type==SDL_MOUSEMOTION&&event.motion.y<90){
                visible_until=SDL_GetTicks()+3500;
            }
            if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT&&
               event.button.x>=1083&&event.button.x<1255&&
               event.button.y>=11&&event.button.y<64){
                if(SDL_Window* window=SDL_RenderGetWindow(renderer)){
                    const bool active=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0;
                    SDL_SetWindowFullscreen(window,active?0:SDL_WINDOW_FULLSCREEN_DESKTOP);
                    visible_until=SDL_GetTicks()+3500;
                }
                continue;
            }
#endif
#ifdef _WIN32
            if(event.type==SDL_USEREVENT){
                if(event.user.code==REMOTE_YOUTUBE_START){
                    // Avoid two audio tracks when browser YouTube starts.
                    int yes=1;
                    mpv_set_property(mpv,"pause",MPV_FORMAT_FLAG,&yes);
                    notice="Xuper TV pausado mientras usas YouTube.";
                    show();
                }
                if(event.user.code==REMOTE_WHEEL){
                    const int ticks=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                    if(ticks!=0){
                        if(menu)vertical-=ticks>0?1:-1;
                        else if(strip_visible)rail.wheel(ticks);
                        else show_channels();
                    }
                }
                if(event.user.code==REMOTE_YOUTUBE_RETURN){
                    SDL_RaiseWindow(SDL_RenderGetWindow(renderer));
                    notice="De vuelta a Xuper TV. Pulsa reproducir para continuar.";
                    show();
                }
                if(event.user.code==REMOTE_VOLUME){
                    const int v=std::clamp(static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1)),0,100);
                    set_number("volume",v);notice="Volumen: "+std::to_string(v)+"%";show();
                }
                if(event.user.code==REMOTE_BRIGHTNESS){
                    const int v=std::clamp(static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1)),0,100);
                    set_number("brightness",v*2-100);notice="Brillo del video: "+std::to_string(v)+"%";show();
                }
            }
#endif
            if(live_channels&&!menu){
                if(event.type==SDL_MOUSEMOTION){
                    if(rail.pressed())rail.motion(event.motion.x);
                    if(event.motion.y>515)show_channels();
                }
                if(event.type==SDL_MOUSEWHEEL){
                    if(!strip_visible)show_channels();
                    else rail.wheel(event.wheel.y);
                    show_channels();
                }
                if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT){
                    if(strip_visible&&rail.contains(event.button.x,event.button.y))
                        rail.press(event.button.x,event.button.y,SDL_GetTicks());
                    else if(!strip_visible)show_channels();
                    else{strip_visible=false;rail=ChannelRail(options.channels.size(),rail.focus());}
                }
                if(event.type==SDL_MOUSEBUTTONUP&&event.button.button==SDL_BUTTON_LEFT&&rail.pressed()){
                    const int chosen=rail.release(event.button.x,event.button.y,SDL_GetTicks());
                    if(chosen>=0&&chosen!=options.item)request_item(chosen);
                    show_channels();
                }
            }
            if(event.type==SDL_KEYDOWN&&!event.key.repeat){
                if(live_channels&&!menu){
                    const auto key=event.key.keysym.sym;
                    if(key==SDLK_DOWN&&!strip_visible){show_channels();continue;}
                    if(strip_visible){
                        if(key==SDLK_LEFT){rail.move_focus(-1);show_channels();continue;}
                        if(key==SDLK_RIGHT){rail.move_focus(1);show_channels();continue;}
                        if(key==SDLK_RETURN){if(static_cast<int>(rail.focus())!=options.item)request_item(static_cast<int>(rail.focus()));continue;}
                        if(key==SDLK_ESCAPE||key==SDLK_UP){strip_visible=false;continue;}
                    }
                }
                if(event.key.keysym.sym==SDLK_F10){
                    xs::move_to_next_display(SDL_RenderGetWindow(renderer));
                    continue;
                }
                if(event.key.keysym.sym==SDLK_F11||(event.key.keysym.sym==SDLK_RETURN&&(event.key.keysym.mod&KMOD_ALT))){
                    if(SDL_Window* window=SDL_RenderGetWindow(renderer)){
                        const bool active=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0;
                        SDL_SetWindowFullscreen(window,active?0:SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                    continue;
                }
                switch(event.key.keysym.sym){
#ifdef _WIN32
                case SDLK_F8:
                    if(gpu_active&&shader_ready){
                        if(set_gpu_shader(!shader_enabled))
                            notice=shader_enabled?"GPU FSRCNNX ACTIVO | F8: comparar":
                                                  "Imagen original GPU | F8: comparar";
                    }else if(extra_detail_started&&!detail_graph.empty()){
                        if(set_extra_detail(!detail_active))
                            notice=detail_active?"CAS/Lanczos ACTIVO | F8: comparar":
                                                 "Imagen ORIGINAL | F8: comparar";
                    }else notice="Selecciona Detalle extra y vuelve a abrir el video.";
                    show();break;
                case SDLK_F4:result.requested_navigation=0;running=false;break;
                case SDLK_F5:result.requested_navigation=1;running=false;break;
                case SDLK_F6:result.requested_navigation=2;running=false;break;
                case SDLK_F7:result.requested_navigation=3;running=false;break;
#endif
                case SDLK_b:case SDLK_ESCAPE:back=true;break;case SDLK_SPACE:activate=true;break;case SDLK_RETURN:activate=true;break;
                case SDLK_x:case SDLK_TAB:settings=true;break;case SDLK_y:audio=true;break;case SDLK_s:subs=true;break;
                case SDLK_LEFT:horizontal-=1;break;case SDLK_RIGHT:horizontal+=1;break;case SDLK_UP:vertical-=1;break;case SDLK_DOWN:vertical+=1;break;
                case SDLK_PAGEUP:item_delta=-1;break;case SDLK_PAGEDOWN:item_delta=1;break;
                case SDLK_LEFTBRACKET:slower=true;break;case SDLK_RIGHTBRACKET:faster=true;break;case SDLK_f:ratio=true;break;case SDLK_m:mute=true;break;
                case SDLK_j:jump=-60;break;case SDLK_l:jump=60;break;
                }
            }
        }
#ifdef __SWITCH__
        padUpdate(&pad);u64 down=padGetButtonsDown(&pad);
        back|=down&HidNpadButton_B;activate|=down&HidNpadButton_A;settings|=down&(HidNpadButton_X|HidNpadButton_Plus);audio|=down&HidNpadButton_Y;
        if(down&HidNpadButton_Left)horizontal=-1;
        if(down&HidNpadButton_Right)horizontal=1;
        if(down&HidNpadButton_Up)vertical=-1;
        if(down&HidNpadButton_Down)vertical=1;
        if(down&HidNpadButton_ZL)jump=-60;
        if(down&HidNpadButton_ZR)jump=60;
        if(down&HidNpadButton_L)item_delta=-1;
        if(down&HidNpadButton_R)item_delta=1;
#endif
        if(back){if(menu){if(page){page=0;selection=0;}else menu=false;}else running=false;show();}
        if(settings){menu=!menu;page=0;selection=0;notice.clear();show();}
        if(audio)cycle_track("audio");
        if(subs)cycle_track("sub");
        if(slower)cycle_speed(-1);
        if(faster)cycle_speed(1);
        if(ratio)cycle_aspect();
        if(mute){int value=!flag("mute");mpv_set_property(mpv,"mute",MPV_FORMAT_FLAG,&value);show();}
        if(jump)seek_to(jump);
        if(item_delta)request_item(options.item+item_delta);
        if(menu){auto rows=menu_rows();selection=std::clamp(selection+vertical,0,static_cast<int>(rows.size())-1);
            const auto row=rows[selection];
            if(activate||horizontal){const int direction=horizontal?horizontal:1;
                switch(row.action){
                    case 1:toggle_pause();break;case 2:page=1;selection=0;break;case 3:page=2;selection=0;break;
                    case 4:page=3;selection=0;break;case 5:page=4;selection=0;break;case 6:cycle_speed(direction);break;case 7:cycle_aspect();break;
                    case 8:set_number("volume",std::clamp(property("volume")+direction*5,0.0,100.0));break;
                    case 9:{int value=!flag("mute");mpv_set_property(mpv,"mute",MPV_FORMAT_FLAG,&value);break;}
                    case 10:set_number("audio-delay",property("audio-delay")+direction*0.1);break;
                    case 11:set_number("sub-delay",property("sub-delay")+direction*0.25);break;
                    case 12:set_number("sub-scale",std::clamp(property("sub-scale")+direction*0.1,0.5,3.0));break;
                    case 13:set_number("brightness",std::clamp(property("brightness")+direction*5,-100.0,100.0));break;
                    case 14:seek_to(-60);break;case 15:seek_to(60);break;
                    case 16:if(flag("seekable"))command("seek","0","absolute+exact");else notice="Esta emision no permite reiniciar.";break;
                    case 17:{
                        char* format=mpv_get_property_string(mpv,"video-format");
                        notice=(format?std::string(format):"Video")+" | "+
                            std::to_string(static_cast<int>(property("width")))+"x"+
                            std::to_string(static_cast<int>(property("height")))+" | "+
                            std::to_string(property("container-fps")).substr(0,5)+" fps";
                        if(format)mpv_free(format);
#ifdef _WIN32
                        char* decoder=mpv_get_property_string(mpv,"hwdec-current");
                        notice+=" | Decodificador: "+std::string(decoder?decoder:"no");
                        if(decoder)mpv_free(decoder);
                        if(extra_detail_started)
                            notice+=std::string(" | CAS: ")+(detail_active?"ON":"OFF");
                        if(video_uses_gpu(video_preset))
                            notice+=std::string(" | GPU: ")+(gpu_active?"OpenGL readback":"CPU fallback")+
                                    " | FSRCNNX: "+(gpu_active&&shader_enabled?"ON":"OFF");
#endif
                        break;
                    }
                    case 18:running=false;break;
                    case 19:mpv_set_property_string(mpv,"aid",std::to_string(row.value).c_str());notice="Audio seleccionado";page=0;selection=1;break;
                    case 20:{mpv_set_property_string(mpv,"sid",row.value<0?"no":std::to_string(row.value).c_str());int value=row.value>=0;mpv_set_property(mpv,"sub-visibility",MPV_FORMAT_FLAG,&value);notice="Subtitulos actualizados";page=0;selection=2;break;}
                    case 21:if(row.value!=options.variant){result.next_variant=static_cast<int>(row.value);running=false;}break;
                    case 22:if(row.value!=options.item)request_item(static_cast<int>(row.value));break;
                    case 24:auto_next=!auto_next;break;
#ifdef _WIN32
                    case 27:{
                        if(set_gpu_shader(!shader_enabled))
                            notice=shader_enabled?"GPU FSRCNNX activo.":
                                                  "Imagen GPU sin FSRCNNX.";
                        break;
                    }
                    case 26:{
                        if(extra_detail_started&&!detail_graph.empty()){
                            if(set_extra_detail(!detail_active))
                                notice=detail_active?"Filtro CAS/Lanczos activo.":
                                                     "Imagen original (sin filtro).";
                        }else notice="Activa Detalle extra en el siguiente video.";
                        break;
                    }
                    case 25:{
                        video_preset=next_video_preset(video_preset,direction);
                        notice=save_video_preset(video_preset)?
                            "Video mejorado: "+std::string(video_preset_label(video_preset))+
                            " | Se aplica al siguiente video.":
                            "No se pudo guardar el ajuste de video.";
                        break;
                    }
#endif
                    case 23:{std::string data,subtitle_error;
                        if(external_index==row.value&&external_id>=0){mpv_set_property_string(mpv,"sid",std::to_string(external_id).c_str());int on=1;mpv_set_property(mpv,"sub-visibility",MPV_FORMAT_FLAG,&on);notice="Subtitulo seleccionado";page=0;selection=2;break;}
                        SDL_SetRenderDrawColor(renderer,10,17,28,255);SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,nullptr,nullptr);
                        SDL_SetRenderDrawColor(renderer,10,17,28,255);SDL_Rect loading{20,550,1240,140};SDL_RenderFillRect(renderer,&loading);
                        draw_text(48,580,"Cargando subtitulos...",{238,243,250,255});SDL_RenderPresent(renderer);
                        if(!options.subtitle_loader||!options.subtitle_loader(static_cast<size_t>(row.value),data,subtitle_error)){
                            notice=subtitle_error.empty()?"No se pudo descargar el subtitulo.":subtitle_error;break;}
                        if(external_id>=0)command("sub-remove",std::to_string(external_id));
                        std::ofstream file(subtitle_cache,std::ios::binary|std::ios::trunc);file.write(data.data(),static_cast<std::streamsize>(data.size()));file.close();
                        if(!file){notice="No se pudo guardar el subtitulo temporal.";break;}
                        subtitle_written=true;
                        const char* args[]={"sub-add",subtitle_cache,"select",options.subtitles[static_cast<size_t>(row.value)].c_str(),nullptr};
                        if(mpv_command(mpv,args)<0)notice="Formato de subtitulos no compatible.";
                        else{external_index=row.value;mpv_get_property(mpv,"sid",MPV_FORMAT_INT64,&external_id);int on=1;mpv_set_property(mpv,"sub-visibility",MPV_FORMAT_FLAG,&on);notice="Subtitulo cargado";page=0;selection=2;}break;}

                }show();
            }
        }else{if(activate)toggle_pause();
        if(horizontal)seek_to(horizontal*10);
        if(vertical){menu=true;page=0;selection=0;show();}}
        for(;;){
            mpv_event* e=mpv_wait_event(mpv,0);
        if(e->event_id==MPV_EVENT_NONE)break;
            if(e->event_id==MPV_EVENT_SHUTDOWN)running=false;
            if(e->event_id==MPV_EVENT_LOG_MESSAGE){
                const auto* log=static_cast<mpv_event_log_message*>(e->data);
                const std::string hint=log&&log->text?playback_error_hint(log->text):"";
                if(!hint.empty()&&hint!=media_hint&&(media_hint.empty()||hint.rfind("HTTP ",0)==0||media_hint.rfind("HTTP ",0)!=0)){
                    media_hint=hint;trace(("play:classified_error "+hint).c_str());
                }
            }
            if(e->event_id==MPV_EVENT_FILE_LOADED){loaded=true;
#ifdef _WIN32
                char* decoder=mpv_get_property_string(mpv,"hwdec-current");
                const std::string active=decoder?decoder:"no";
                trace(("video:decoder="+active+" preset="+video_preset_key(video_preset)).c_str());
                if(decoder)mpv_free(decoder);
                if(extra_detail_started){
                    const int vw=static_cast<int>(property("width"));
                    const int vh=static_cast<int>(property("height"));
                    detail_graph=extra_detail_filter(vw,vh);
                    if(!detail_graph.empty()){
                        if(set_extra_detail(true)){
                            trace(("video:filter="+detail_graph).c_str());
                        }else notice="Nitidez extra no disponible. Imagen original.";
                    }
                }
#endif
        if(pending_start>0&&flag("seekable"))command("seek",std::to_string(pending_start),"absolute+exact");
                pending_start=0;}
            if(e->event_id==MPV_EVENT_END_FILE){trace("play:end_file");auto* end=static_cast<mpv_event_end_file*>(e->data);
                if(end&&end->reason==MPV_END_FILE_REASON_ERROR){
                    error=media_hint.empty()?mpv_error_string(end->error):"No se pudo abrir el video: "+media_hint;
                    char failure[80];std::snprintf(failure,sizeof(failure),"play:end_error mpv=%d",end->error);trace(failure);
                }
                result.ended=end&&end->reason==MPV_END_FILE_REASON_EOF;running=false;}
        }
        if(!running)break;
        bool fresh=dirty.exchange(false);
        if(fresh){
#ifdef _WIN32
            if(gpu_active){
                std::string gpu_error;
                if(!gpu->render(pixels,gpu_error)){
                    trace(("video:gpu_readback_failed="+gpu_error).c_str());
                    // Restore video without replacing XuperTV or its SDL UI.
                    gpu->close();gpu.reset();gpu_active=false;shader_enabled=false;context=nullptr;
                    mpv_render_param reset[]={
                        {MPV_RENDER_PARAM_API_TYPE,const_cast<char*>(MPV_RENDER_API_TYPE_SW)},
                        {MPV_RENDER_PARAM_INVALID,nullptr}
                    };
                    rc=mpv_render_context_create(&context,mpv,reset);
                    if(rc<0){error="GPU y renderer compatible fallaron: "+std::string(mpv_error_string(rc));break;}
                    mpv_render_context_set_update_callback(context,update,&dirty);
                    notice="GPU no disponible. Continuando en modo CPU.";
                    show();
                }
            }
#endif
            if(
#ifdef _WIN32
                !gpu_active
#else
                true
#endif
            ){
                int size[2]={width,height};
                mpv_render_param frame[]={
                    {MPV_RENDER_PARAM_SW_SIZE,size},
                    {MPV_RENDER_PARAM_SW_FORMAT,const_cast<char*>("rgb0")},
                    {MPV_RENDER_PARAM_SW_STRIDE,&stride},
                    {MPV_RENDER_PARAM_SW_POINTER,pixels},
                    {MPV_RENDER_PARAM_INVALID,nullptr}
                };
                rc=mpv_render_context_render(context,frame);
            }else rc=0;
        if(rc<0){error=mpv_error_string(rc);break;}
            if(SDL_UpdateTexture(texture,nullptr,pixels,static_cast<int>(stride))<0){error=SDL_GetError();break;}
            if(first_frame&&loaded){trace("play:first_frame");first_frame=false;}
        }
        const Uint32 now=SDL_GetTicks();paused=flag("pause");
        if(strip_visible&&now>strip_until&&!rail.pressed())strip_visible=false;
#ifdef _WIN32
        const int held_channel=strip_visible?rail.preview(now):-1;
        if(held_channel<0||held_channel==options.item){
            if(opened_preview>=0){miniature.close();opened_preview=-1;}
        }else if(options.preview_resolve){
            if(opened_preview!=held_channel){
                if(preview_work&&preview_work->done.load(std::memory_order_acquire) &&
                   static_cast<int>(preview_work->index)!=held_channel){
                    preview_work.reset();
                }
                if(!preview_work){
                    auto work=std::make_shared<PreviewWork>(static_cast<size_t>(held_channel));
                    preview_work=work;
                    auto resolver=options.preview_resolve;
                    std::thread([work,resolver]{
                        try{
                            work->ok=resolver(work->index,work->url,work->headers,work->error);
                        }catch(...){work->error="No se pudo consultar vista previa.";work->ok=false;}
                        work->done.store(true,std::memory_order_release);
                    }).detach();
                }
                if(preview_work->done.load(std::memory_order_acquire)&&
                   static_cast<int>(preview_work->index)==held_channel){
                    opened_preview=held_channel;
                    if(opened_preview>=0)miniature.close();
                    preview_error.clear();
                    if(preview_work->ok){
                        if(!miniature.open(preview_work->url,preview_work->headers,preview_error)&&preview_error.empty())
                            preview_error="No se pudo abrir la señal de prueba.";
                    }else preview_error=preview_work->error.empty()?
                        "No se pudo obtener la señal.":preview_work->error;
                }
            }
            if(opened_preview==held_channel)miniature.poll();
        }
#endif
        const bool image_changed=rail_artwork&&rail_artwork->poll();
        if(fresh||image_changed||now-last_draw>=100){last_draw=now;SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,nullptr,nullptr);
            if(options.font&&(menu||paused||now<visible_until)&&(!strip_visible||menu)){
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer,10,17,28,230);
                SDL_Rect panel{20,menu?35:526,1240,menu?650:175};SDL_RenderFillRect(renderer,&panel);
                const SDL_Color white{238,243,250,255},cyan{73,219,223,255},muted{180,192,209,255};
                if(menu){draw_text(48,48,options.title.empty()?"Reproductor":options.title,white);
                    draw_text(48,84,page==0?"A: elegir | izq/der: ajustar | B: volver":"A: seleccionar | B: ajustes",muted);
                    auto rows=menu_rows();const int start=selection/10*10;
                    for(int i=start;i<static_cast<int>(rows.size())&&i<start+10;++i){const int y=130+(i-start)*41;
                        if(i==selection){SDL_SetRenderDrawColor(renderer,28,63,78,255);SDL_Rect highlight{35,y-2,1207,39};SDL_RenderFillRect(renderer,&highlight);}
                        draw_text(50,y,(i==selection?"> ":"  ")+rows[i].label,i==selection?cyan:white,1150);}
                    draw_text(48,650,"Opciones "+std::to_string(start+1)+"-"+std::to_string(std::min(start+10,static_cast<int>(rows.size())))+" / "+std::to_string(rows.size())+"  |  Arriba/abajo: navegar",muted);
                    draw_text(48,560,notice.empty()?"L/R: anterior/siguiente | Y: cambiar audio":notice,cyan,1150);
                }else{draw_text(44,539,(paused?"PAUSA  ":"")+options.title,white);draw_text(44,578,"A: pausa | X/+: opciones | Y: audio | izq/der: 10 s | ZL/ZR: 60 s | B: salir",muted);}
                const double pos=property("time-pos"),duration=property("duration");
                draw_text(48,menu?606:618,clock(pos)+(duration>0?" / "+clock(duration):" / EN VIVO"),white);
                SDL_SetRenderDrawColor(renderer,65,79,94,255);SDL_Rect bar{330,menu?619:631,870,8};SDL_RenderFillRect(renderer,&bar);
                if(duration>0){bar.w=static_cast<int>(870*std::clamp(pos/duration,0.0,1.0));SDL_SetRenderDrawColor(renderer,73,219,223,255);SDL_RenderFillRect(renderer,&bar);}
                if(!menu&&!notice.empty())draw_text(44,660,notice,cyan);
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
            }
            if(live_channels&&strip_visible&&!menu){
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,9,10,18,239);
                SDL_Rect shade{0,515,1280,205};SDL_RenderFillRect(renderer,&shade);
                SDL_SetRenderDrawColor(renderer,219,47,74,255);
                SDL_Rect line{0,514,1280,3};SDL_RenderFillRect(renderer,&line);
                draw_text(28,524,"TV EN VIVO   ·   Desliza para explorar  |  Clic: cambiar  |  Mantener: vista rápida",
                          {245,245,251,255},1220);
                SDL_Rect clip{ChannelRail::x,ChannelRail::y,ChannelRail::viewport,ChannelRail::height};
                SDL_RenderSetClipRect(renderer,&clip);
                for(size_t i=0;i<options.channels.size();++i){
                    const int xx=ChannelRail::x+static_cast<int>(i)*(ChannelRail::width+ChannelRail::gap)-rail.offset();
                    if(xx+ChannelRail::width<clip.x||xx>clip.x+clip.w)continue;
                    const auto& item=options.channels[i];
                    const bool focused=i==rail.focus(),playing=static_cast<int>(i)==options.item;
                    SDL_SetRenderDrawColor(renderer,focused?222:45,focused?47:49,focused?76:65,255);
                    SDL_Rect frame{xx-2,ChannelRail::y-2,ChannelRail::width+4,ChannelRail::height+4};
                    SDL_RenderFillRect(renderer,&frame);
                    SDL_SetRenderDrawColor(renderer,24,28,43,255);
                    SDL_Rect body{xx,ChannelRail::y,ChannelRail::width,ChannelRail::height};
                    SDL_RenderFillRect(renderer,&body);
                    if(rail_artwork&&!item.artwork_cache.empty()){
                        if(SDL_Texture* art=rail_artwork->get(item.artwork,item.artwork_cache)){
                            SDL_Rect thumb{xx+9,ChannelRail::y+7,ChannelRail::width-18,70};
                            SDL_RenderCopy(renderer,art,nullptr,&thumb);
                        }
                    }
                    if(playing){
                        SDL_SetRenderDrawColor(renderer,192,30,57,250);
                        SDL_Rect tag{xx+7,ChannelRail::y+7,101,23};SDL_RenderFillRect(renderer,&tag);
                        draw_text(xx+13,ChannelRail::y+9,"EN VIVO",{255,255,255,255},95);
                    }
                    draw_text(xx+10,ChannelRail::y+84,item.title,{248,248,251,255},ChannelRail::width-20);
                }
                SDL_RenderSetClipRect(renderer,nullptr);
                const int held=rail.preview(now);
                if(held>=0&&static_cast<size_t>(held)<options.channels.size()){
                    const auto& item=options.channels[static_cast<size_t>(held)];
                    SDL_SetRenderDrawColor(renderer,9,12,22,249);
                    SDL_Rect popup{735,274,517,229};SDL_RenderFillRect(renderer,&popup);
                    SDL_SetRenderDrawColor(renderer,231,56,84,255);
                    SDL_Rect edge{735,274,517,3};SDL_RenderFillRect(renderer,&edge);
                    draw_text(753,290,"VISTA PREVIA   ·   CANAL EN VIVO",{252,107,128,255},480);
                    SDL_Rect screen{917,320,317,179};
                    SDL_SetRenderDrawColor(renderer,17,22,33,255);
                    SDL_RenderFillRect(renderer,&screen);
                    bool video=false;
#ifdef _WIN32
                    if(opened_preview==held&&!miniature.failed())
                        video=miniature.draw(screen);
#endif
                    if(!video){
                        if(rail_artwork&&!item.artwork_cache.empty()){
                            if(SDL_Texture* art=rail_artwork->get(item.artwork,item.artwork_cache))
                                SDL_RenderCopy(renderer,art,nullptr,&screen);
                        }
                        SDL_SetRenderDrawColor(renderer,8,11,19,170);
                        SDL_Rect hint{917,452,317,47};SDL_RenderFillRect(renderer,&hint);
#ifdef _WIN32
                        const std::string message=opened_preview==held?
                            (preview_error.empty()?"Cargando señal...":preview_error):
                            (held==options.item?"Canal en reproducción":"Preparando vista previa...");
#else
                        const std::string message="Vista rápida del canal";
#endif
                        draw_text(925,459,message,{248,250,253,255},297);
                    }
                    draw_text(753,337,item.title,{255,255,255,255},154);
                    draw_text(753,392,item.description.empty()?"Canal en vivo":item.description,
                              {200,204,217,255},154);
                    draw_text(753,470,"Suelta para cerrar",{186,190,202,255},154);
                }
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
            }
#ifdef _WIN32
            if(menu||paused||now<visible_until){
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,14,17,32,222);
                SDL_Rect full{1083,11,171,52};
                SDL_RenderFillRect(renderer,&full);
                draw_text(1093,26,"⛶ Pantalla completa",
                          {252,244,250,255},149);
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
            }
            RemoteServer::shared().draw_cursor(renderer);
#endif
            SDL_RenderPresent(renderer);
        }else SDL_Delay(5);
    }
    result.auto_next=auto_next;
    // A classified network message can arrive after END_FILE in the same queue drain.
    if(!error.empty()&&!media_hint.empty())error="No se pudo abrir el video: "+media_hint;
    result.position=property("time-pos");result.speed=property("speed");result.volume=property("volume");result.paused=flag("pause");
    result.audio_delay=property("audio-delay");result.sub_delay=property("sub-delay");result.sub_scale=property("sub-scale");result.aspect=property("video-aspect-override");
    mpv_get_property(mpv,"aid",MPV_FORMAT_INT64,&result.audio);mpv_get_property(mpv,"sid",MPV_FORMAT_INT64,&result.subtitle);
    if(outcome)*outcome=result;
    cleanup();return error;
}
}
