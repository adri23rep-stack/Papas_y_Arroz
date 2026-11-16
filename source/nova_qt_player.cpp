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

void NovaQtPlayer::play(const xs::SourceResult& result,const QString& title,bool live,double resumeSeconds){
    if(playing_.load()||!result.ok||!xs::valid_source(result.source))return;
    if(thread_.joinable())thread_.join();
    stop_.store(false);
    title_=title;
    notice_=QStringLiteral("Abriendo reproductor...");
    playing_.store(true);
    emit changed();
    thread_=std::thread([this,result,title,live,resumeSeconds](){
        playWorker(result,title,live,resumeSeconds);
    });
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
    double volume=static_cast<double>(std::clamp(value,0,100));
    mpv_set_property_async(handle_,0,"volume",MPV_FORMAT_DOUBLE,&volume);
}
void NovaQtPlayer::setSpeed(int percent){
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    double speed=std::clamp(percent,50,200)/100.0;
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
void NovaQtPlayer::selectTrack(int kind,int id,uint32_t generation){
    QMutexLocker lock(&mutex_);
    const auto snapshot=xs::RemoteServer::shared().playback_options();
    if(!handle_||!snapshot.active||snapshot.generation!=generation||(kind!=1&&kind!=2))return;
    const auto track=id<0?std::string("no"):std::to_string(id);
    mpv_set_property_string(handle_,kind==1?"aid":"sid",track.c_str());
    if(kind==2&&id>=0)mpv_set_property_string(handle_,"sub-visibility","yes");
}

static void publishTracks(mpv_handle* mpv,const QString& title){
    xs::RemotePlaybackSnapshot state;
    state.title=title.toStdString();
    mpv_node tracks{};
    if(mpv_get_property(mpv,"track-list",MPV_FORMAT_NODE,&tracks)<0)return;
    bool hasSub=false;
    if(tracks.format==MPV_FORMAT_NODE_ARRAY){
        for(int i=0;i<tracks.u.list->num;++i){
            const auto& row=tracks.u.list->values[i];
            if(row.format!=MPV_FORMAT_NODE_MAP)continue;
            xs::RemotePlaybackChoice choice;
            std::string type,lang,name;
            for(int j=0;j<row.u.list->num;++j){
                const std::string key=row.u.list->keys[j];
                const auto& value=row.u.list->values[j];
                if(key=="id"&&value.format==MPV_FORMAT_INT64)choice.id=int(value.u.int64);
                if(key=="selected"&&value.format==MPV_FORMAT_FLAG)choice.selected=value.u.flag;
                if(value.format!=MPV_FORMAT_STRING)continue;
                if(key=="type")type=value.u.string;
                if(key=="lang")lang=value.u.string;
                if(key=="title")name=value.u.string;
            }
            if(type!="audio"&&type!="sub")continue;
            choice.kind=type=="audio"?"audio":"subtitle";
            choice.label=lang;
            if(!name.empty())choice.label+=(lang.empty()?"":" - ")+name;
            if(choice.label.empty())choice.label="Pista "+std::to_string(choice.id);
            if(type=="sub")hasSub|=choice.selected;
            (type=="audio"?state.audio:state.subtitles).push_back(std::move(choice));
        }
    }
    mpv_free_node_contents(&tracks);
    state.subtitles.insert(state.subtitles.begin(),{-1,"subtitle","Desactivados",!hasSub});
    xs::RemoteServer::shared().publish_playback_options(std::move(state));
}

void NovaQtPlayer::applySetting(int id,int value){
    if(id==xs::remote_setting_index("volume")){setVolume(value);return;}
    if(id==xs::remote_setting_index("speed")){setSpeed(value);return;}
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    if(id==xs::remote_setting_index("brightness")){
        const int alpha=255-(100-std::clamp(value,0,100))*217/100;
        const auto shade=QStringLiteral("{\\an7\\pos(0,0)\\bord0\\shad0\\1c&H000000&\\1a&H%1&\\p1}m 0 0 l 100 0 100 100 0 100")
            .arg(alpha,2,16,QChar('0')).toUtf8();
        const char* args[]={"osd-overlay","42","ass-events",shade.constData(),"100","100",nullptr};
        mpv_command_async(handle_,0,args);
    }else if(id==xs::remote_setting_index("mute")){
        int flag=value!=0;
        mpv_set_property_async(handle_,0,"mute",MPV_FORMAT_FLAG,&flag);
    }else if(id==xs::remote_setting_index("audio_delay")||
             id==xs::remote_setting_index("sub_delay")){
        double delay=value/1000.0;
        mpv_set_property_async(handle_,0,
            id==xs::remote_setting_index("audio_delay")?"audio-delay":"sub-delay",
            MPV_FORMAT_DOUBLE,&delay);
    }else if(id==xs::remote_setting_index("sub_scale")){
        double scale=value/100.0;
        mpv_set_property_async(handle_,0,"sub-scale",MPV_FORMAT_DOUBLE,&scale);
    }else if(id==xs::remote_setting_index("aspect")){
        const char* aspect=value==1?"16:9":value==2?"4:3":"-1";
        mpv_set_property_string(handle_,"video-aspect-override",aspect);
        double panscan=value==3?1.0:0.0;
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

void NovaQtPlayer::playWorker(xs::SourceResult result,QString title,bool live,double resumeSeconds){
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
    if(!live&&resumeSeconds>5&&resumeSeconds<86400){
        const std::string start=std::to_string(resumeSeconds);
        opt("start",start.c_str());
    }

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
    xs::RemoteServer::shared().begin_playback(title.toStdString());
    mpv_observe_property(mpv,41,"track-list",MPV_FORMAT_NONE);
    QString endMessage=QStringLiteral("Volviste al catálogo.");
    double lastPosition=0.0;
    bool reachedEnd=false;
    while(!stop_.load()){
        const mpv_event* event=mpv_wait_event(mpv,0.2);
        double now=0.0;
        if(mpv_get_property(mpv,"time-pos",MPV_FORMAT_DOUBLE,&now)>=0&&
           now>=0.0&&now<86400.0)lastPosition=now;
        if(!event)continue;
        if(event->event_id==MPV_EVENT_FILE_LOADED||(event->event_id==MPV_EVENT_PROPERTY_CHANGE&&event->reply_userdata==41))publishTracks(mpv,title);
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
    xs::RemoteServer::shared().end_playback();
    mpv_terminate_destroy(mpv);
    // Clear the active-player state BEFORE notifying the catalog so a new
    // selection from iPhone can start immediately after this stream ends.
    finish(endMessage);
    QMetaObject::invokeMethod(this,[this,lastPosition,reachedEnd](){
        emit finishedPlayback(lastPosition,reachedEnd);
    },Qt::QueuedConnection);
}
