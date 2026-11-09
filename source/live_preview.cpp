#include "live_preview.hpp"
#ifdef _WIN32
#include "catalog.hpp"
#include <malloc.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace xs {
static constexpr int kWidth=320,kHeight=180;
void LivePreview::changed(void* opaque){
    static_cast<std::atomic<bool>*>(opaque)->store(true);
}
void LivePreview::close(){
    if(context_){mpv_render_context_set_update_callback(context_,nullptr,nullptr);mpv_render_context_free(context_);context_=nullptr;}
    if(handle_){mpv_terminate_destroy(handle_);handle_=nullptr;}
    if(texture_){SDL_DestroyTexture(texture_);texture_=nullptr;}
    if(pixels_){_aligned_free(pixels_);pixels_=nullptr;}
    has_frame_=false;failed_=false;dirty_=true;
}
bool LivePreview::open(const std::string& url,const std::vector<std::pair<std::string,std::string>>& headers,std::string& error){
    close();
    if(!renderer_||!valid_source(url)){error="Fuente de vista previa no admitida.";return false;}
    handle_=mpv_create();
    if(!handle_){error="No se pudo iniciar el visor pequeño.";return false;}
    auto opt=[&](const char* name,const char* value){return mpv_set_option_string(handle_,name,value)>=0;};
    if(!opt("config","no")||!opt("load-scripts","no")||!opt("vo","libmpv")||
       !opt("ao","null")||!opt("mute","yes")||!opt("hwdec","no")||
       !opt("terminal","no")||!opt("tls-verify","yes")){
        error="No se pudo preparar el visor.";close();return false;
    }
    opt("osd-level","0");opt("sub-visibility","no");opt("network-timeout","8");
    opt("demuxer-max-bytes","4194304");opt("demuxer-readahead-secs","1");
    if(headers.size()>8){error="Demasiadas cabeceras para vista previa.";close();return false;}
    std::vector<std::string> values;
    std::vector<mpv_node> nodes;
    for(const auto& h:headers){
        if((h.first!="Content-Auth"&&h.first!="Content-License"&&h.first!="App"&&
            h.first!="App-Version"&&h.first!="Ranger-Id"&&h.first!="Pragma")||
           h.second.empty()||h.second.size()>16384||
           std::any_of(h.second.begin(),h.second.end(),[](unsigned char c){return c<32||c==127;})){
            error="Cabecera no admitida.";close();return false;
        }
        values.push_back(h.first+": "+h.second);
    }
    if(!values.empty()){
        nodes.resize(values.size());
        for(size_t i=0;i<values.size();++i){nodes[i].format=MPV_FORMAT_STRING;nodes[i].u.string=values[i].data();}
        mpv_node_list list{static_cast<int>(nodes.size()),nodes.data(),nullptr};
        mpv_node value{};value.format=MPV_FORMAT_NODE_ARRAY;value.u.list=&list;
        if(mpv_set_option(handle_,"http-header-fields",MPV_FORMAT_NODE,&value)<0){
            error="No se pudo autorizar la vista previa.";close();return false;
        }
    }
    int rc=mpv_initialize(handle_);
    if(rc<0){error="No se pudo inicializar vista previa.";close();return false;}
    mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,const_cast<char*>(MPV_RENDER_API_TYPE_SW)},
                               {MPV_RENDER_PARAM_INVALID,nullptr}};
    rc=mpv_render_context_create(&context_,handle_,params);
    if(rc<0){error="No se pudo preparar video pequeño.";close();return false;}
    mpv_render_context_set_update_callback(context_,changed,&dirty_);
    pixels_=_aligned_malloc(kWidth*kHeight*4,64);
    texture_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ABGR8888,
                               SDL_TEXTUREACCESS_STREAMING,kWidth,kHeight);
    if(!pixels_||!texture_){error="No hay memoria para vista previa.";close();return false;}
    SDL_SetTextureBlendMode(texture_,SDL_BLENDMODE_NONE);
    const char* cmd[]={"loadfile",url.c_str(),nullptr};
    if(mpv_command(handle_,cmd)<0){error="No se pudo solicitar el video pequeño.";close();return false;}
    return true;
}
void LivePreview::poll(){
    if(!handle_||!context_||failed_)return;
    for(;;){
        mpv_event* event=mpv_wait_event(handle_,0);
        if(!event||event->event_id==MPV_EVENT_NONE)break;
        if(event->event_id==MPV_EVENT_END_FILE){
            auto* end=static_cast<mpv_event_end_file*>(event->data);
            if(end&&end->reason==MPV_END_FILE_REASON_ERROR)failed_=true;
        }
        if(event->event_id==MPV_EVENT_SHUTDOWN)failed_=true;
    }
    if(failed_||!dirty_.exchange(false))return;
    int size[2]={kWidth,kHeight};
    size_t stride=kWidth*4;
    mpv_render_param frame[]={{MPV_RENDER_PARAM_SW_SIZE,size},
                              {MPV_RENDER_PARAM_SW_FORMAT,const_cast<char*>("rgb0")},
                              {MPV_RENDER_PARAM_SW_STRIDE,&stride},
                              {MPV_RENDER_PARAM_SW_POINTER,pixels_},
                              {MPV_RENDER_PARAM_INVALID,nullptr}};
    if(mpv_render_context_render(context_,frame)<0||
       SDL_UpdateTexture(texture_,nullptr,pixels_,static_cast<int>(stride))<0){
        failed_=true;
        return;
    }
    has_frame_=true;
}
bool LivePreview::draw(const SDL_Rect& rect)const{
    if(!has_frame_||!texture_)return false;
    SDL_RenderCopy(renderer_,texture_,nullptr,&rect);
    return true;
}
}
#endif
