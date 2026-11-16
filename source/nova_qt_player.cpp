#include "nova_qt_player.hpp"
#include "catalog.hpp"
#include "remote_server.hpp"
#include <mpv/client.h>
#include <QMetaObject>
#include <QMutexLocker>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

NovaQtPlayer::NovaQtPlayer(QObject* parent):QObject(parent){}
NovaQtPlayer::~NovaQtPlayer(){
    stop_.store(true);
    {
        QMutexLocker lock(&mutex_);
        if(handle_){ const char* args[]={"stop",nullptr};mpv_command_async(handle_,0,args); }
    }
    if(thread_.joinable())thread_.join();
}

void NovaQtPlayer::finish(const QString& message){
    QMetaObject::invokeMethod(this,[this,message]{
        playing_.store(false);
        notice_=message;
        emit changed();
    },Qt::QueuedConnection);
}

void NovaQtPlayer::play(const xs::SourceResult& result,const QString& title,bool live){
    if(playing_.load()||!result.ok||!xs::valid_source(result.source))return;
    if(thread_.joinable())thread_.join();
    stop_.store(false);
    title_=title;
    notice_=QStringLiteral("Abriendo reproductor...");
    playing_.store(true);
    emit changed();
    thread_=std::thread([this,result,title,live](){playWorker(result,title,live);});
}

void NovaQtPlayer::command(const char* name,const char* argument){
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    const char* args[]={name,argument,nullptr};
    mpv_command_async(handle_,0,args);
}

void NovaQtPlayer::stop(){
    stop_.store(true);
    command("stop",nullptr);
}
void NovaQtPlayer::togglePause(){
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    int paused=0;
    if(mpv_get_property(handle_,"pause",MPV_FORMAT_FLAG,&paused)>=0){
        paused=!paused;
        mpv_set_property_async(handle_,0,"pause",MPV_FORMAT_FLAG,&paused);
    }
}
void NovaQtPlayer::setVolume(int value){
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    const double volume=static_cast<double>(std::clamp(value,0,100));
    mpv_set_property_async(handle_,0,"volume",MPV_FORMAT_DOUBLE,&volume);
}
void NovaQtPlayer::setSpeed(int percent){
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    const double speed=std::clamp(percent,50,200)/100.0;
    mpv_set_property_async(handle_,0,"speed",MPV_FORMAT_DOUBLE,&speed);
}
void NovaQtPlayer::toggleFullscreen(){
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    int fullscreen=0;
    if(mpv_get_property(handle_,"fullscreen",MPV_FORMAT_FLAG,&fullscreen)>=0){
        fullscreen=!fullscreen;
        mpv_set_property_async(handle_,0,"fullscreen",MPV_FORMAT_FLAG,&fullscreen);
    }
}
void NovaQtPlayer::applySetting(int id,int value){
    if(id==xs::remote_setting_index("volume")){setVolume(value);return;}
    if(id==xs::remote_setting_index("speed")){setSpeed(value);return;}
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    if(id==xs::remote_setting_index("brightness")){
        const double brightness=std::clamp(value,0,100)*2.0-100.0;
        mpv_set_property_async(handle_,0,"brightness",MPV_FORMAT_DOUBLE,&brightness);
    }else if(id==xs::remote_setting_index("mute")){
        const int flag=value!=0;
        mpv_set_property_async(handle_,0,"mute",MPV_FORMAT_FLAG,&flag);
    }else if(id==xs::remote_setting_index("audio_delay")||
             id==xs::remote_setting_index("sub_delay")){
        const double delay=value/1000.0;
        mpv_set_property_async(handle_,0,
            id==xs::remote_setting_index("audio_delay")?"audio-delay":"sub-delay",
            MPV_FORMAT_DOUBLE,&delay);
    }else if(id==xs::remote_setting_index("sub_scale")){
        const double scale=value/100.0;
        mpv_set_property_async(handle_,0,"sub-scale",MPV_FORMAT_DOUBLE,&scale);
    }else if(id==xs::remote_setting_index("aspect")){
        const char* aspect=value==1?"16:9":value==2?"4:3":"-1";
        mpv_set_property_string(handle_,"video-aspect-override",aspect);
        const double panscan=value==3?1.0:0.0;
        mpv_set_property_async(handle_,0,"panscan",MPV_FORMAT_DOUBLE,&panscan);
    }
}
void NovaQtPlayer::seek(double seconds){
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    const std::string amount=std::to_string(seconds);
    const char* args[]={"seek",amount.c_str(),"relative",nullptr};
    mpv_command_async(handle_,0,args);
}

void NovaQtPlayer::playWorker(xs::SourceResult result,QString title,bool live){
    mpv_handle* mpv=mpv_create();
    if(!mpv){finish(QStringLiteral("No se pudo iniciar el motor de video."));return;}
    auto opt=[&](const char* key,const char* val){return mpv_set_option_string(mpv,key,val)>=0;};
    opt("config","no");
    opt("load-scripts","no");
    opt("terminal","no");
    opt("force-window","yes");
    opt("keep-open","no");
    opt("title",title.toUtf8().constData());
    // mpv's built-in Windows video output creates its own native video
    // window. Qt remains in charge of the catalog and phone pairing.
    opt("vo","gpu");
    opt("hwdec","auto-safe");
    opt("network-timeout",live?"25":"15");
    opt("cache","yes");
    opt("sub-auto","no");
    opt("tls-verify","yes");
    if(live)opt("demuxer-readahead-secs","3");

    // Preserve the exact protocol's authenticated header allowlist. Never
    // print or expose the source URL, header values, or session tokens.
    std::vector<std::string> header_lines;
    std::vector<mpv_node> nodes;
    if(result.headers.size()>8){
        mpv_terminate_destroy(mpv);
        finish(QStringLiteral("Demasiadas cabeceras de reproducción."));
        return;
    }
    for(const auto& h:result.headers){
        const std::string& name=h.first;
        if((name!="Content-Auth"&&name!="Content-License"&&name!="App"&&
            name!="App-Version"&&name!="Ranger-Id"&&name!="Pragma")||
           h.second.empty()||h.second.size()>16384||
           std::any_of(h.second.begin(),h.second.end(),[](unsigned char c){return c<32||c==127;})){
            mpv_terminate_destroy(mpv);
            finish(QStringLiteral("Cabeceras de reproducción no admitidas."));
            return;
        }
        header_lines.push_back(name+": "+h.second);
    }
    if(!header_lines.empty()){
        nodes.resize(header_lines.size());
        for(size_t i=0;i<nodes.size();++i){
            nodes[i].format=MPV_FORMAT_STRING;
            nodes[i].u.string=header_lines[i].data();
        }
        mpv_node_list list{static_cast<int>(nodes.size()),nodes.data(),nullptr};
        mpv_node node{};node.format=MPV_FORMAT_NODE_ARRAY;node.u.list=&list;
        if(mpv_set_option(mpv,"http-header-fields",MPV_FORMAT_NODE,&node)<0){
            mpv_terminate_destroy(mpv);
            finish(QStringLiteral("No se pudieron aplicar las cabeceras del video."));
            return;
        }
    }

    if(mpv_initialize(mpv)<0){
        mpv_terminate_destroy(mpv);
        finish(QStringLiteral("No se pudo iniciar la salida de video."));
        return;
    }
    {
        QMutexLocker lock(&mutex_);
        handle_=mpv;
    }
    // Apply the same stored iPhone settings to the Qt player's mpv engine.
    const auto& remote=xs::RemoteServer::shared();
    for(size_t i=0;i<xs::kRemoteSettings.size();++i){
        const int id=static_cast<int>(i);
        applySetting(id,remote.setting_value(id));
    }
    const char* load[]={"loadfile",result.source.c_str(),nullptr};
    if(mpv_command(mpv,load)<0){
        QMutexLocker lock(&mutex_);
        handle_=nullptr;
        lock.unlock();
        mpv_terminate_destroy(mpv);
        finish(QStringLiteral("No se pudo abrir la transmisión."));
        return;
    }
    QMetaObject::invokeMethod(this,[this]{
        notice_=QStringLiteral("Reproduciendo. Esc o Atrás para volver.");
        emit changed();
    },Qt::QueuedConnection);
    QString endMessage=QStringLiteral("Volviste al catálogo.");
    double lastPosition=0.0;
    bool reachedEnd=false;
    while(!stop_.load()){
        const mpv_event* event=mpv_wait_event(mpv,0.2);
        double now=0.0;
        if(mpv_get_property(mpv,"time-pos",MPV_FORMAT_DOUBLE,&now)>=0&&
           now>=0.0&&now<86400.0)lastPosition=now;
        if(!event)continue;
        if(event->event_id==MPV_EVENT_END_FILE){
            const auto* end=static_cast<const mpv_event_end_file*>(event->data);
            reachedEnd=end&&end->reason==MPV_END_FILE_REASON_EOF;
            if(end&&end->reason==MPV_END_FILE_REASON_ERROR)
                endMessage=QStringLiteral("Se cortó la reproducción.");
            break;
        }
        if(event->event_id==MPV_EVENT_SHUTDOWN)break;
    }
    {
        QMutexLocker lock(&mutex_);
        handle_=nullptr;
    }
    mpv_terminate_destroy(mpv);
    QMetaObject::invokeMethod(this,[this,lastPosition,reachedEnd](){
        emit finishedPlayback(lastPosition,reachedEnd);
    },Qt::QueuedConnection);
    finish(endMessage);
}
