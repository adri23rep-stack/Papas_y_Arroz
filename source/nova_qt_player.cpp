#include "nova_qt_player.hpp"
#include "catalog.hpp"
#include "remote_server.hpp"
#include "nova_quality.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <mpv/client.h>
#include <QMetaObject>
#include <QMutexLocker>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
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

void NovaQtPlayer::requestQuality(int value){
    if(value<0||value>4)return;
    // No restart, and no misleading status, when selecting the same mode.
    if(requested_quality_.exchange(value)==value)return;
    quality_revision_.fetch_add(1);
}

void NovaQtPlayer::applySetting(int id,int value){
    if(id==xs::remote_setting_index("video_preset")){
        requestQuality(value);
        return;
    }
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
    // Restart only libmpv, never the catalog/Qt application or iPhone pairing.
    struct PlaybackRestore {
        double position=0.0, volume=100.0, speed=1.0;
        double audioDelay=0.0, subDelay=0.0, subScale=1.0;
        int paused=0, muted=0, subVisible=1;
        std::string audioTrack, subtitleTrack, aspect="-1";
        double panscan=0.0;
        bool captured=false;
    } restore;
    restore.position=resumeSeconds;
    auto& remote=xs::RemoteServer::shared();
    const int presetId=xs::remote_setting_index("video_preset");
    int activePreset=remote.setting_value(presetId);
    if(activePreset<0||activePreset>4)activePreset=1;
    requested_quality_.store(activePreset);
    int previousPreset=activePreset;
    bool switching=false, registered=false, reachedEnd=false;
    double lastPosition=0.0;
    QString endMessage=QStringLiteral("Volviste al catálogo.");
    xs::NovaQualityRestartGate qualityGate;

    auto publish=[this](const QString& message,const QString& details=QString()){
        QMetaObject::invokeMethod(this,[this,message,details]{
            notice_=message;
            if(!details.isNull())diagnostics_=details;
            emit changed();
        },Qt::QueuedConnection);
    };
    auto capture=[&](mpv_handle* mpv){
        auto number=[mpv](const char* name,double old){
            double value=old;
            return mpv_get_property(mpv,name,MPV_FORMAT_DOUBLE,&value)>=0?value:old;
        };
        auto flag=[mpv](const char* name,int old){
            int value=old;
            return mpv_get_property(mpv,name,MPV_FORMAT_FLAG,&value)>=0?value:old;
        };
        auto string=[mpv](const char* name,const std::string& old){
            char* value=mpv_get_property_string(mpv,name);
            if(!value)return old;
            std::string copy(value);
            mpv_free(value);
            return copy;
        };
        if(!live)restore.position=number("time-pos",restore.position);
        else restore.position=0.0;
        restore.volume=number("volume",restore.volume);
        restore.speed=number("speed",restore.speed);
        restore.audioDelay=number("audio-delay",restore.audioDelay);
        restore.subDelay=number("sub-delay",restore.subDelay);
        restore.subScale=number("sub-scale",restore.subScale);
        restore.panscan=number("panscan",restore.panscan);
        restore.paused=flag("pause",restore.paused);
        restore.muted=flag("mute",restore.muted);
        restore.subVisible=flag("sub-visibility",restore.subVisible);
        restore.audioTrack=string("aid",restore.audioTrack);
        restore.subtitleTrack=string("sid",restore.subtitleTrack);
        restore.aspect=string("video-aspect-override",restore.aspect);
        restore.captured=true;
    };
    // A quality failure is retried once with the previously working preset.
    // Never persist a preset that failed to load this playback.
    while(!stop_.load()){
        const auto preset=xs::nova_video_preset(activePreset);
        const bool gpuMode=xs::nova_needs_shader(preset);
        const QString shader=QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("shaders/FSRCNNX_x2_8-0-4-1.glsl"));
        bool failed=gpuMode&&(!QFileInfo(shader).isFile());
        bool loaded=false, restart=false;
        QString failure=failed?QStringLiteral("Falta el shader GPU FSRCNNX."):QString();
        mpv_handle* mpv=failed?nullptr:mpv_create();
        if(!failed&&!mpv){
            failed=true;
            failure=QStringLiteral("No se pudo iniciar el motor MPV.");
        }
        if(!failed){
            auto opt=[&](const char* key,const char* val){
                if(mpv_set_option_string(mpv,key,val)>=0)return true;
                failure=QStringLiteral("MPV rechazó la opción: ")+QString::fromLatin1(key);
                return false;
            };
            failed=!(opt("config","no") &&
                     opt("load-scripts","no") &&
                     opt("terminal","no") &&
                     opt("force-window","yes") &&
                     opt("keep-open","no") &&
                     opt("title",title.toUtf8().constData()) &&
                     opt("vo","gpu") &&
                     opt("network-timeout",live?"25":"15") &&
                     opt("cache","yes") &&
                     opt("sub-auto","no") &&
                     opt("tls-verify","yes"));
            if(!failed){
                // mpv's hardware decoder chooses D3D11VA/NVDEC on supported
                // devices; compatible mode deliberately stays on CPU.
                const char* decoder=preset==xs::VideoPreset::Compatible?"no":"auto-safe";
                failed=!opt("hwdec",decoder);
                if(preset==xs::VideoPreset::Sharp ||
                   preset==xs::VideoPreset::ExtraDetail)
                    failed=failed||!opt("scale","lanczos");
                else if(gpuMode){
                    const QByteArray shaderPath=QDir::toNativeSeparators(shader).toUtf8();
                    failed=failed||!opt("gpu-api","opengl") ||
                           !opt("glsl-shaders",shaderPath.constData()) ||
                           !opt("scale","ewa_lanczossharp") ||
                           !opt("cscale","ewa_lanczos");
                }else failed=failed||!opt("scale","bicubic");
            }
            if(!failed&&live)failed=!opt("demuxer-readahead-secs","3");
            if(!failed&&xs::nova_can_resume(live,restore.position)){
                const std::string start=std::to_string(restore.position);
                failed=!opt("hr-seek","yes") || !opt("start",start.c_str());
            }
            if(!failed&&restore.captured){
                // Keep the previous playback paused while the stream seeks.
                failed=!opt("pause",restore.paused?"yes":"no");
            }
            if(!failed){
                // Keep authorized headers, never emit URLs or tokens to logs/UI.
                std::vector<std::string> headers;
                std::vector<mpv_node> nodes;
                if(result.headers.size()>8){
                    failed=true;failure=QStringLiteral("Demasiadas cabeceras de reproducción.");
                }
                for(const auto& h:result.headers){
                    const auto& key=h.first;
                    if(failed)break;
                    if((key!="Content-Auth"&&key!="Content-License"&&key!="App"&&
                        key!="App-Version"&&key!="Ranger-Id"&&key!="Pragma")||
                       h.second.empty()||h.second.size()>16384||
                       std::any_of(h.second.begin(),h.second.end(),[](unsigned char c){
                           return c<32||c==127;
                       })){
                        failed=true;failure=QStringLiteral("Cabeceras no admitidas.");
                        break;
                    }
                    headers.push_back(key+": "+h.second);
                }
                if(!failed&&!headers.empty()){
                    nodes.resize(headers.size());
                    for(size_t i=0;i<nodes.size();++i){
                        nodes[i].format=MPV_FORMAT_STRING;
                        nodes[i].u.string=headers[i].data();
                    }
                    mpv_node_list list{static_cast<int>(nodes.size()),nodes.data(),nullptr};
                    mpv_node node{};node.format=MPV_FORMAT_NODE_ARRAY;node.u.list=&list;
                    if(mpv_set_option(mpv,"http-header-fields",MPV_FORMAT_NODE,&node)<0){
                        failed=true;failure=QStringLiteral("Falló la autenticación del video.");
                    }
                }
                if(!failed&&mpv_initialize(mpv)<0){
                    failed=true;failure=QStringLiteral("No pudo iniciarse la salida MPV.");
                }
            }
            if(!failed){
                {
                    QMutexLocker lock(&mutex_);
                    handle_=mpv;
                }
                for(size_t i=0;i<xs::kRemoteSettings.size();++i){
                    const int id=static_cast<int>(i);
                    if(id!=presetId)applySetting(id,remote.setting_value(id));
                }
                // Restore values changed during playback, not just saved settings.
                if(restore.captured){
                    mpv_set_property(mpv,"volume",MPV_FORMAT_DOUBLE,&restore.volume);
                    mpv_set_property(mpv,"speed",MPV_FORMAT_DOUBLE,&restore.speed);
                    mpv_set_property(mpv,"mute",MPV_FORMAT_FLAG,&restore.muted);
                    mpv_set_property(mpv,"audio-delay",MPV_FORMAT_DOUBLE,&restore.audioDelay);
                    mpv_set_property(mpv,"sub-delay",MPV_FORMAT_DOUBLE,&restore.subDelay);
                    mpv_set_property(mpv,"sub-scale",MPV_FORMAT_DOUBLE,&restore.subScale);
                    mpv_set_property(mpv,"panscan",MPV_FORMAT_DOUBLE,&restore.panscan);
                    mpv_set_property_string(mpv,"video-aspect-override",restore.aspect.c_str());
                    mpv_set_property(mpv,"sub-visibility",MPV_FORMAT_FLAG,&restore.subVisible);
                }
                const char* load[]={"loadfile",result.source.c_str(),nullptr};
                if(mpv_command(mpv,load)<0){
                    failed=true;failure=QStringLiteral("No pudo abrirse la transmisión.");
                }else{
                    if(!registered){
                        remote.begin_playback(title.toStdString());
                        registered=true;
                    }
                    mpv_observe_property(mpv,41,"track-list",MPV_FORMAT_NONE);
                    mpv_request_log_messages(mpv,"warn");
                }
            }
        }
        if(!failed&&mpv){
            while(!stop_.load()){
                const mpv_event* event=mpv_wait_event(mpv,0.15);
                double position=0.0;
                if(!live&&mpv_get_property(mpv,"time-pos",MPV_FORMAT_DOUBLE,&position)>=0 &&
                   xs::nova_can_resume(false,position))lastPosition=position;
                if(event){
                    if(event->event_id==MPV_EVENT_FILE_LOADED){
                        loaded=true;
                        if(restore.captured){
                            if(!restore.audioTrack.empty())
                                mpv_set_property_string(mpv,"aid",restore.audioTrack.c_str());
                            if(!restore.subtitleTrack.empty())
                                mpv_set_property_string(mpv,"sid",restore.subtitleTrack.c_str());
                            mpv_set_property(mpv,"sub-visibility",MPV_FORMAT_FLAG,&restore.subVisible);
                        }
                        std::string filter="Sin filtro";
                        if(preset==xs::VideoPreset::Sharp)filter="Lanczos";
                        if(preset==xs::VideoPreset::ExtraDetail){
                            // MPV_FORMAT_INT64 must be stored in int64_t.
                            int64_t w=0,h=0;
                            mpv_get_property(mpv,"video-params/w",MPV_FORMAT_INT64,&w);
                            mpv_get_property(mpv,"video-params/h",MPV_FORMAT_INT64,&h);
                            const std::string graph=xs::extra_detail_filter(int(w),int(h));
                            if(graph.empty()||mpv_set_property_string(mpv,"vf",graph.c_str())<0){
                                failed=true;
                                failure=QStringLiteral("Filtro CAS no disponible.");
                                break;
                            }
                            filter="CAS/Lanczos";
                        }
                        if(gpuMode)
                            filter="FSRCNNX configurado (verificar GPU)";
                        int64_t sourceW=0,sourceH=0,outputW=0,outputH=0;
                        mpv_get_property(mpv,"video-params/w",MPV_FORMAT_INT64,&sourceW);
                        mpv_get_property(mpv,"video-params/h",MPV_FORMAT_INT64,&sourceH);
                        mpv_get_property(mpv,"video-out-params/w",MPV_FORMAT_INT64,&outputW);
                        mpv_get_property(mpv,"video-out-params/h",MPV_FORMAT_INT64,&outputH);
                        char* decoder=mpv_get_property_string(mpv,"hwdec-current");
                        QString decoding=decoder?QString::fromUtf8(decoder):QStringLiteral("sin datos");
                        if(decoder)mpv_free(decoder);
                        const QString details=QStringLiteral("Origen %1×%2 · Salida %3×%4 · Decodificador %5 · %6")
                            .arg(sourceW).arg(sourceH).arg(outputW).arg(outputH)
                            .arg(decoding,QString::fromStdString(filter));
                        publish(QStringLiteral("Reproduciendo · %1").arg(
                            QString::fromUtf8(xs::video_preset_label(preset))),details);
                    }
                    if(event->event_id==MPV_EVENT_PROPERTY_CHANGE&&event->reply_userdata==41)
                        publishTracks(mpv,title);
                    if(event->event_id==MPV_EVENT_FILE_LOADED)
                        publishTracks(mpv,title);
                    if(event->event_id==MPV_EVENT_LOG_MESSAGE&&gpuMode){
                        const auto* log=static_cast<const mpv_event_log_message*>(event->data);
                        if(log&&log->text){
                            std::string message(log->text);
                            std::transform(message.begin(),message.end(),message.begin(),
                                [](unsigned char c){return static_cast<char>(std::tolower(c));});
                            if((message.find("shader")!=std::string::npos||
                                message.find("glsl")!=std::string::npos)&&
                               (message.find("failed")!=std::string::npos||
                                message.find("error")!=std::string::npos)){
                                failed=true;
                                failure=QStringLiteral("El shader FSRCNNX falló al cargarse.");
                                break;
                            }
                        }
                    }
                    if(event->event_id==MPV_EVENT_END_FILE){
                        const auto* end=static_cast<const mpv_event_end_file*>(event->data);
                        reachedEnd=end&&end->reason==MPV_END_FILE_REASON_EOF;
                        if(end&&end->reason==MPV_END_FILE_REASON_ERROR){
                            failed=true;
                            failure=QStringLiteral("Se cortó la reproducción.");
                        }
                        break;
                    }
                    if(event->event_id==MPV_EVENT_SHUTDOWN)break;
                }
                const auto revision=quality_revision_.load();
                qualityGate.observe(revision,xs::NovaQualityRestartGate::Clock::now());
                const int wanted=requested_quality_.load();
                if(qualityGate.ready(wanted,activePreset,
                                      xs::NovaQualityRestartGate::Clock::now())){
                    capture(mpv);
                    previousPreset=activePreset;
                    activePreset=wanted;
                    switching=true;
                    restart=true;
                    publish(QStringLiteral("Aplicando calidad..."),
                            QStringLiteral("Reiniciando MPV sin cerrar Nova"));
                    const char* osd[]={"show-text","Aplicando calidad...","1200",nullptr};
                    mpv_command_async(mpv,0,osd);
                    break;
                }
            }
            // A failed startup has no valid media position/tracks to capture.
            if(failed&&loaded&&!stop_.load())capture(mpv);
        }
        {
            QMutexLocker lock(&mutex_);
            if(handle_==mpv)handle_=nullptr;
        }
        if(mpv)mpv_terminate_destroy(mpv);
        if(stop_.load())break;
        if(restart)continue;
        if(failed){
            // A network failure after a successfully loaded stream is not
            // evidence that its quality mode is unsupported.
            if(loaded&&failure==QStringLiteral("Se cortó la reproducción.")){
                endMessage=failure;
                break;
            }
            // Revert only once. Keep the same source and saved position.
            const int fallback=switching?previousPreset:
                               (activePreset!=1?1:0);
            if(fallback!=activePreset){
                activePreset=fallback;
                previousPreset=fallback;
                switching=false;
                remote.set_setting_value(presetId,fallback);
                requested_quality_.store(fallback);
                quality_revision_.fetch_add(1);
                publish(QStringLiteral("No se pudo aplicar la calidad. Restaurando modo anterior."),
                        failure);
                continue;
            }
            endMessage=failure.isEmpty()?QStringLiteral("No se pudo reproducir."):failure;
            break;
        }
        if(reachedEnd)endMessage=QStringLiteral("Reproducción terminada.");
        break;
    }
    if(registered)remote.end_playback();
    finish(endMessage);
    QMetaObject::invokeMethod(this,[this,lastPosition,reachedEnd](){
        emit finishedPlayback(lastPosition,reachedEnd);
    },Qt::QueuedConnection);
}
