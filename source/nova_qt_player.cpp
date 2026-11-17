#include "nova_qt_player.hpp"
#include "catalog.hpp"
#include "remote_server.hpp"
#include "nova_quality.hpp"
#include "nova_motion.hpp"
#include "nova_gpu.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QByteArray>
#include <QTimer>
#include <QProcessEnvironment>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QUrl>
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
    veyraWatchdog_.stop();
    nativeVeyraPoll_.stop();
    // Ensure the original GPU engine releases its D3D12 swap chain before
    // Qt destroys the HWND used by its VideoPresenter.
    nativeVeyra_.reset();
    if(veyraProcess_)veyraProcess_->disconnect(this);
    if(veyraProcess_ && veyraProcess_->state()!=QProcess::NotRunning){
        veyraProcess_->terminate();
        if(!veyraProcess_->waitForFinished(1500)){
            veyraProcess_->kill();
            veyraProcess_->waitForFinished(1000);
        }
    }
    stop_.store(true);
    {
        QMutexLocker lock(&mutex_);
        if(handle_){ const char* args[]={"stop",nullptr};mpv_command_async(handle_,0,args); }
    }
    if(thread_.joinable())thread_.join();
}

void NovaQtPlayer::finish(const QString& message){
    QMetaObject::invokeMethod(this,[this,message]{
        if(videoWindow_)videoWindow_->hide();
        playing_.store(false);
        notice_=message;
        emit changed();
    },Qt::QueuedConnection);
}

void NovaQtPlayer::sendVeyraControl(const QJsonObject& command){
    if(!veyraBridgeReady_ || !veyraProcess_ ||
       veyraProcess_->state()!=QProcess::Running)return;
    QJsonObject msg=command;
    msg.insert(QStringLiteral("protocol"),1);
    veyraProcess_->write(QJsonDocument(msg).toJson(QJsonDocument::Compact)+"\n");
}

void NovaQtPlayer::scheduleVeyraFallback(){
    if(veyraFallbackPending_ || veyraStopRequested_ || !veyraFallback_)return;
    veyraFallbackPending_=true;
    const auto attempt=veyraAttemptId_;
    QTimer::singleShot(0,this,[this,attempt]{
        if(attempt!=veyraAttemptId_ || veyraStopRequested_)return;
        auto fallback=veyraFallback_;
        if(fallback)fallback();
    });
}

bool NovaQtPlayer::setVeyraSetting(const QString& key,int value){
    if(nativeVeyra_ && nativeVeyra_->ready() && veyraActive()){
        bool accepted=false;
        if(key=="veyra_multiplier" && value>=2 && value<=6)
            accepted=nativeVeyra_->multiplier(unsigned(value));
        else if(key=="veyra_sr_quality" && value>=0)
            accepted=nativeVeyra_->srQuality(unsigned(value));
        else if((value==0||value==1) && key=="veyra_fg")
            accepted=nativeVeyra_->change(NOVA_VEYRA_FEATURE_FG,value!=0);
        else if((value==0||value==1) && key=="veyra_sr")
            accepted=nativeVeyra_->change(NOVA_VEYRA_FEATURE_SR,value!=0);
        else if((value==0||value==1) && key=="veyra_nr")
            accepted=nativeVeyra_->change(NOVA_VEYRA_FEATURE_NR,value!=0);
        else if((value==0||value==1) && key=="veyra_hdr")
            accepted=nativeVeyra_->change(NOVA_VEYRA_FEATURE_HDR,value!=0);
        // Veyra reports whether RTX Video HDR is actually active; no fake flag.
        notice_=accepted?QStringLiteral("Veyra nativo: esperando confirmar el efecto en GPU."):
            QStringLiteral("Este efecto no está disponible en la sesión Veyra.");
        emit changed();
        return accepted;
    }
    if(!veyraActive() || veyraStopRequested_)return false;
    if(key==QStringLiteral("veyra_multiplier")){
        bool allowed=false;
        for(const auto& entry:veyraState_.value("fgMultiplierChoices").toList())
            if(entry.toMap().value("id").toInt()==value)allowed=true;
        if(!allowed)return false;
        sendVeyraControl({{"cmd","fg_multiplier"},{"value",value}});
    }else{
        const QString feature=key.mid(6);
        if(!key.startsWith("veyra_") || (feature!="fg" && feature!="sr" && feature!="nr" && feature!="hdr") ||
           (value!=0 && value!=1))return false;
        sendVeyraControl({{"cmd","gpu_feature"},{"name",feature},{"enabled",value!=0}});
    }
    notice_=QStringLiteral("Ajuste enviado a Veyra; esperando su estado.");
    emit changed();
    return true;
}

void NovaQtPlayer::play(const xs::SourceResult& result,const QString& title,bool live,double resumeSeconds){
    if(playing_.load()||!result.ok||!xs::valid_source(result.source))return;
    // NEW: link Veyra's real EngineController into this process. This is the
    // original D3D12/NGX/NVOF/shader pipeline, not MPV/RIFE or an IPC QML shim.
    // It is opt-in until an NVIDIA/MSVC GPU build is validated on the RTX 3060.
    // Never allow a network URL with arbitrary auth headers into this test.
    const QString nativeDll=qEnvironmentVariable("NOVA_VEYRA_NATIVE_DLL");
    const bool nativeStrict=!nativeDll.isEmpty() &&
        qEnvironmentVariableIntValue("NOVA_VEYRA_NATIVE_STRICT")==1;
    QString nativeLoadFailure;
    if(!skipVeyraOnce_ && !nativeDll.isEmpty() &&
       QFileInfo(nativeDll).isAbsolute() && QFileInfo(nativeDll).isFile() &&
       !live && result.headers.empty() && result.subtitles.empty()){
        const QString source=QString::fromStdString(result.source);
        const QUrl url(source);
        const bool safeHttp=url.isValid() &&
            (url.scheme()=="http"||url.scheme()=="https") &&
            qEnvironmentVariableIntValue("NOVA_VEYRA_NATIVE_ALLOW_NETWORK")==1 &&
            url.userInfo().isEmpty()&&!url.hasFragment()&&!url.hasQuery();
        const bool local=QFileInfo(source).isFile();
        if(local||safeHttp){
            auto candidate=std::make_unique<NovaVeyraNativeLoader>();
            if(candidate->load(nativeDll)){
                if(!videoWindow_){
                    videoWindow_=std::make_unique<QWindow>();
                    videoWindow_->installEventFilter(this);
                }
                auto* screen=QGuiApplication::focusWindow()
                    ?QGuiApplication::focusWindow()->screen():QGuiApplication::primaryScreen();
                videoWindow_->setScreen(screen);
                const QRect area=screen->availableGeometry();
                QRect bounds(QPoint(),QSize(960,540).boundedTo(area.size()));
                bounds.moveCenter(area.center());
                videoWindow_->setTitle(title);
                videoWindow_->setGeometry(bounds);
                videoWindow_->showNormal();
                videoWindow_->requestActivate();
                videoWindowId_=static_cast<std::int64_t>(videoWindow_->winId());
                NovaVeyraSettings options{};
                options.size=sizeof(options);
                options.fg_backend=NOVA_VEYRA_FG_DLSS;
                options.fg_multiplier=2;
                options.sr_quality=2;
                options.features=0; // First establish real decode before GPU features.
                if(candidate->open(reinterpret_cast<void*>(videoWindow_->winId()),source,options)){
                    nativeVeyra_=std::move(candidate);
                    nativeVeyraStarted_=false;
                    nativeVeyraPaused_=false;
                    nativeVeyraLastPosition_=-1.0;
                    veyraState_.clear();
                    veyraBridgeReady_=false;
                    veyraClock_.start();
                    title_=title;
                    notice_=QStringLiteral("Motor Veyra nativo · inicializando DirectX 12");
                    diagnostics_=QStringLiteral("Veyra EngineController original (sin MPV)");
                    playing_.store(true);
                    xs::RemoteServer::shared().begin_playback(title.toStdString());
                    emit changed();
                    nativeVeyraPoll_.disconnect(this);
                    connect(&nativeVeyraPoll_,&QTimer::timeout,this,
                            [this,result,title,live,resumeSeconds]{
                        if(!nativeVeyra_)return;
                        NovaVeyraSnapshot snap{};
                        const bool readable=nativeVeyra_->snapshot(snap);
                        const bool broken=!readable||snap.failed||
                            snap.transport==NOVA_VEYRA_FAILED||
                            (!nativeVeyraStarted_ && veyraClock_.elapsed()>25000);
                        if(broken){
                            const bool strict=qEnvironmentVariableIntValue("NOVA_VEYRA_NATIVE_STRICT")==1;
                            const QString reason=!readable
                                ?QStringLiteral("No responde la API de estado de Veyra.")
                                :snap.failed||snap.transport==NOVA_VEYRA_FAILED
                                    ?QStringLiteral("El motor Veyra informó un fallo GPU/decodificación.")
                                    :QStringLiteral("Veyra no mostró fotogramas en 25 segundos.");
                            nativeVeyraPoll_.stop();
                            nativeVeyra_.reset();
                            veyraState_.clear();
                            veyraBridgeReady_=false;
                            playing_.store(false);
                            xs::RemoteServer::shared().end_playback();
                            if(videoWindow_)videoWindow_->hide();
                            if(strict){
                                notice_=reason;
                                diagnostics_=QStringLiteral("Fallo real Veyra D3D12; MPV desactivado durante esta prueba.");
                                emit changed();
                                return;
                            }
                            skipVeyraOnce_=true; // the failed native path is never retried
                            play(result,title,live,resumeSeconds);
                            notice_=QStringLiteral("Veyra nativo falló; recuperado con MPV.");
                            emit changed();
                            return;
                        }
                        if(snap.transport==NOVA_VEYRA_ENDED){
                            const double pos=snap.duration_seconds>0?snap.duration_seconds:
                                nativeVeyraLastPosition_;
                            nativeVeyraPoll_.stop();
                            nativeVeyra_.reset();
                            veyraState_.clear();
                            veyraBridgeReady_=false;
                            playing_.store(false);
                            xs::RemoteServer::shared().end_playback();
                            if(videoWindow_)videoWindow_->hide();
                            emit changed();
                            emit finishedPlayback(pos,true);
                            return;
                        }
                        if(!snap.running)return;
                        nativeVeyraStarted_=true;
                        veyraBridgeReady_=true;
                        nativeVeyraPaused_=snap.transport==NOVA_VEYRA_PAUSED;
                        if(std::isfinite(snap.position_seconds)&&snap.position_seconds>=0)
                            nativeVeyraLastPosition_=snap.position_seconds;
                        QVariantMap state;
                        state.insert("nativeEngine",true);
                        state.insert("srQuality",int(snap.applied_sr_quality));
                        state.insert("requestedSrQuality",int(snap.desired_sr_quality));
                        state.insert("fgBackend",QStringLiteral("DLSS"));
                        state.insert("fgEnabled",bool(snap.fg_active));
                        state.insert("srEnabled",bool(snap.sr_active));
                        state.insert("nrEnabled",bool(snap.nr_active));
                        state.insert("hdrEnabled",bool(snap.hdr_active));
                        state.insert("fgMultiplier",int(snap.applied_fg_multiplier));
                        state.insert("requestedFgMultiplier",int(snap.desired_fg_multiplier));
                        state.insert("sourceFps",snap.source_fps);
                        state.insert("submitFps",snap.submit_fps);
                        state.insert("submitFpsKnown",bool(snap.submit_fps_known));
                        QVariantList multipliers;
                        if(snap.fg_max_generated>0){
                            for(unsigned n:{2u,3u,4u,6u})
                                if(n<=snap.fg_max_generated+1)
                                    multipliers.push_back(QVariantMap{
                                        {"id",QString::number(n)},{"label",QString::number(n)+"x"}});
                        }
                        state.insert("fgMultiplierChoices",multipliers);
                        veyraState_=state;
                        diagnostics_=QStringLiteral("Veyra D3D12 · fuente %1 FPS · salida %2")
                            .arg(snap.source_fps,0,'f',1)
                            .arg(snap.submit_fps_known?QString::number(snap.submit_fps,'f',1):
                                 QStringLiteral("sin medir"));
                        xs::RemotePlaybackSnapshot remoteState;
                        remoteState.title=title_.toStdString();
                        remoteState.engine="veyra";
                        remoteState.veyraSettings={{int(snap.fg_active),int(snap.sr_active),
                            int(snap.nr_active),int(snap.hdr_active),int(snap.applied_fg_multiplier)}};
                        // The original EngineController supplies stream indices,
                        // languages and selected track. These feed the iPhone.
                        for(const auto& track:nativeVeyra_->audioTracks()){
                            xs::RemotePlaybackChoice choice;
                            choice.kind="audio";
                            choice.id=track.stream_index;
                            choice.label=QString::fromUtf8(track.label_utf8).toStdString();
                            choice.selected=track.selected!=0;
                            remoteState.audio.push_back(std::move(choice));
                        }
                        for(const auto& item:multipliers){
                            bool ok=false;
                            const int n=item.toMap().value("id").toString().toInt(&ok);
                            if(ok)remoteState.veyraMultipliers.push_back(n);
                        }
                        xs::RemoteServer::shared().publish_playback_options(std::move(remoteState));
                        emit changed();
                    });
                    nativeVeyraPoll_.start(250);
                    return;
                }
                videoWindow_->hide();
                candidate->close();
                nativeLoadFailure=QStringLiteral("Veyra no aceptó abrir el video en la ventana D3D12 de Nova.");
            }else{
                nativeLoadFailure=candidate->lastError();
            }
        }
    }
    if(nativeStrict){
        if(nativeLoadFailure.isEmpty()){
            nativeLoadFailure=QStringLiteral("La DLL nativa no existe, o este video requiere cabeceras/DRM/red en vivo no admitidas.");
        }
        title_=title;
        playing_.store(false);
        notice_=nativeLoadFailure;
        diagnostics_=QStringLiteral("Veyra nativo no inició. MPV desactivado solo para la prueba estricta.");
        emit changed();
        return;
    }
    // Legacy experimental IPC remains opt-in, but isn't the integration plan.
    // Only patched Veyra receives the media source, via a private pipe.
    // A failed handshake falls back to mpv once (no retry loop).
    const bool skipVeyra=skipVeyraOnce_;
    skipVeyraOnce_=false;
    if(!skipVeyra && qEnvironmentVariableIntValue("NOVA_VEYRA_PILOT")==1){
        const QString executable=qEnvironmentVariable("NOVA_VEYRA_EXE");
        const QString proxyScript=qEnvironmentVariable("NOVA_VEYRA_PROXY_SCRIPT");
        const bool proxyScriptValid=proxyScript.isEmpty()||QFileInfo(proxyScript).isFile();
        const bool stockWin32=!qEnvironmentVariable("NOVA_VEYRA_WIN32_EXE").isEmpty();
        const QString source=QString::fromStdString(result.source);
        const QUrl url(source);
        const bool network=url.isValid() &&
            (url.scheme()==QStringLiteral("http")||url.scheme()==QStringLiteral("https"));
        const bool allowedNetwork=network &&
            qEnvironmentVariableIntValue("NOVA_VEYRA_ALLOW_NETWORK")==1 &&
            url.userInfo().isEmpty() && !url.hasFragment() &&
            (!url.hasQuery() || qEnvironmentVariableIntValue("NOVA_VEYRA_ALLOW_SIGNED_NETWORK")==1);
        const bool local=QFileInfo(source).isFile();
        // Sources with HTTP auth headers or external subtitle URLs still require
        // the mpv backend. URLs are NEVER passed on Veyra's process command line.
        // Parameterised URLs need explicit opt-in (Veyra may log them itself).
        if(!executable.isEmpty() && QFileInfo(executable).isFile() &&
           proxyScriptValid &&
           // Stock Veyra.exe supports ONLY local files here. No remote auth,
           // live streams, seeking to prior history, or extra media headers.
           (!stockWin32 || (!live && local && resumeSeconds<=0.0 && !proxyScript.isEmpty())) &&
           result.headers.empty() && result.subtitles.empty() &&
           (local || allowedNetwork)){
            if(veyraProcess_ && veyraProcess_->state()==QProcess::NotRunning)
                veyraProcess_.reset();
            if(!veyraProcess_){
                veyraProcess_=std::make_unique<QProcess>();
                const auto attempt=++veyraAttemptId_;
                veyraOutput_.clear();
                veyraSource_=local?QFileInfo(source).absoluteFilePath():source;
                veyraBridgeReady_=false;
                veyraState_.clear();
                veyraStarted_=false;
                veyraEnded_=false;
                veyraFallbackPending_=false;
                veyraClock_.start();
                veyraLastStatus_=0;
                veyraLastPosition_=-1.0;
                veyraResumeSeconds_=live?0.0:resumeSeconds;
                veyraStopRequested_=false;
                veyraLive_=live;
                veyraProcess_->setProgram(executable);
                // In QML-only pilot mode the executable is python.exe and the
                // only arguments are -u and the LOCAL proxy script path.
                // Media URLs are still sent through Nova's protected stdin.
                if(!proxyScript.isEmpty())
                    veyraProcess_->setArguments({QStringLiteral("-u"),proxyScript});
                else veyraProcess_->setArguments({});
                veyraProcess_->setWorkingDirectory(QFileInfo(executable).absolutePath());
                // Veyra owns a separate Qt/MSVC runtime. Do not inherit Nova plugin paths.
                veyraProcess_->setStandardErrorFile(QProcess::nullDevice());
                QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
                for(const auto* name:{"QT_PLUGIN_PATH","QT_QPA_PLATFORM_PLUGIN_PATH","QML2_IMPORT_PATH","QML_IMPORT_PATH"})env.remove(QString::fromLatin1(name));
                env.insert(QStringLiteral("NOVA_VEYRA_STDIO"),QStringLiteral("1"));
                veyraProcess_->setProcessEnvironment(env);
                veyraFallback_=[this,result,title,live,resumeSeconds,attempt]{
                    if(attempt!=veyraAttemptId_ || veyraStopRequested_)return;
                    const double resume=live?0.0:(veyraLastPosition_>=0?veyraLastPosition_:resumeSeconds);
                    veyraWatchdog_.stop();
                    if(veyraProcess_){
                        veyraProcess_->disconnect(this);
                        veyraProcess_->kill();
                        veyraProcess_->waitForFinished(1000);
                        veyraProcess_.reset();
                    }
                    ++veyraAttemptId_; // invalidate callbacks from the old session
                    veyraBridgeReady_=false;
                    veyraState_.clear();
                    veyraSource_.clear();
                    veyraOutput_.clear();
                    xs::RemoteServer::shared().end_playback();
                    playing_.store(false);
                    skipVeyraOnce_=true;
                    play(result,title,live,resume);
                    notice_=QStringLiteral("Veyra no pudo continuar; video recuperado con MPV.");
                    emit changed();
                };
                QObject::connect(veyraProcess_.get(), &QProcess::readyReadStandardOutput,
                    this, [this,stockWin32] {
                    if(!veyraProcess_)return;
                    veyraOutput_.append(veyraProcess_->readAllStandardOutput());

                    for(;;){
                        const auto newline=veyraOutput_.indexOf('\n');
                        if(newline<0){
                            if(veyraOutput_.size()>65536)scheduleVeyraFallback();
                            break;
                        }
                        if(newline>65536){scheduleVeyraFallback();return;}
                        const QByteArray line=veyraOutput_.left(newline);
                        veyraOutput_.remove(0,newline+1);
                        const QJsonDocument doc=QJsonDocument::fromJson(line);
                        if(!doc.isObject())continue;
                        const QJsonObject msg=doc.object();
                        if(msg.value(QStringLiteral("protocol")).toInt()!=1)continue;
                        const QString type=msg.value(QStringLiteral("type")).toString();
                        if(type==QStringLiteral("nova-ready") && !veyraBridgeReady_){
                            veyraBridgeReady_=true;
                            notice_=stockWin32
                                ?QStringLiteral("Veyra Win32: prueba de video local y controles básicos")
                                :QStringLiteral("Veyra conectado: controles activos");
                            sendVeyraControl({{"cmd","open_source"},{"source",veyraSource_},{"live",veyraLive_}});
                            veyraSource_.clear();
                            if(veyraResumeSeconds_>0)
                                sendVeyraControl({{"cmd","resume"},{"seconds",veyraResumeSeconds_}});
                            emit changed();
                        }else if(type==QStringLiteral("nova-error")){
                            scheduleVeyraFallback();
                        }else if(type==QStringLiteral("nova-status") && veyraBridgeReady_){
                            veyraLastStatus_=veyraClock_.elapsed();
                            if(msg.value("failed").toBool()){scheduleVeyraFallback();return;}
                            if(!veyraLive_ && msg.value("ended").toBool() && !veyraStopRequested_){
                                veyraEnded_=true;
                                const double duration=msg.value("duration").toDouble(-1.0);
                                if(std::isfinite(duration) && duration>0)veyraLastPosition_=duration;
                                stop();
                                return;
                            }
                            veyraStarted_|=msg.value("running").toBool();
                            if(msg.value("experimentalWin32").toBool()){
                                // Stock Win32 does not expose decoder state,
                                // stream duration, FG/SR/NR or remote track APIs.
                                // Do NOT advertise synthetic enhancement switches.
                                veyraState_.clear();
                                diagnostics_=QStringLiteral(
                                    "Veyra Win32 experimental · GPU/FPS sin medición");
                                emit changed();
                                continue;
                            }
                            veyraState_=msg.toVariantMap();
                            diagnostics_=QStringLiteral("Veyra · %1 · origen %2 FPS · salida %3 FPS")
                                .arg(msg.value("fgBackend").toString())
                                .arg(msg.value("sourceFps").toDouble(),0,'f',1)
                                .arg(msg.value("submitFpsKnown").toBool()?QString::number(msg.value("submitFps").toDouble(),'f',1):QStringLiteral("sin medir"));
                            const double pos=msg.value(QStringLiteral("position")).toDouble(-1);
                            if(!veyraLive_ && msg.value(QStringLiteral("hasSource")).toBool() &&
                               std::isfinite(pos) && pos>3.0)
                                veyraLastPosition_=pos;
                            const QJsonArray audio=msg.value(QStringLiteral("audioTracks")).toArray();
                            xs::RemotePlaybackSnapshot state;
                            state.title=title_.toStdString();
                            state.engine="veyra";
                            state.veyraSettings={{msg.value("fgEnabled").toBool()?1:0,
                                msg.value("srEnabled").toBool()?1:0,msg.value("nrEnabled").toBool()?1:0,
                                msg.value("hdrEnabled").toBool()?1:0,msg.value("fgMultiplier").toInt(2)}};
                            for(const auto& choice:msg.value("fgMultiplierChoices").toArray()){
                                bool ok=false;
                                const int n=choice.toObject().value("id").toString().toInt(&ok);
                                if(ok && n>=2 && n<=64)state.veyraMultipliers.push_back(n);
                            }
                            const int selected=msg.value(QStringLiteral("selectedAudioTrack")).toInt(-1);
                            for(const QJsonValue& value:audio){
                                const QJsonObject track=value.toObject();
                                const int index=track.value(QStringLiteral("index")).toInt(-1);
                                if(index<0)continue;
                                xs::RemotePlaybackChoice choice;
                                choice.kind="audio";
                                choice.id=index;
                                choice.label=track.value(QStringLiteral("label")).toString().toStdString();
                                if(choice.label.empty())choice.label="Audio "+std::to_string(index);
                                choice.selected=index==selected;
                                state.audio.push_back(std::move(choice));
                            }
                            const int selectedSub=msg.value(QStringLiteral("selectedSubtitle")).toInt(-1);
                            state.subtitles.push_back({-1,"subtitle","Desactivados",selectedSub<0});
                            const QJsonArray subtitles=msg.value(QStringLiteral("subtitleTracks")).toArray();
                            for(const QJsonValue& value:subtitles){
                                const QJsonObject track=value.toObject();
                                const int index=track.value(QStringLiteral("index")).toInt(-1);
                                if(index<0 || !track.value(QStringLiteral("usable")).toBool())continue;
                                xs::RemotePlaybackChoice choice;
                                choice.kind="subtitle";
                                choice.id=index;
                                choice.label=track.value(QStringLiteral("label")).toString().toStdString();
                                if(choice.label.empty())choice.label="Subtítulo "+std::to_string(index);
                                choice.selected=index==selectedSub;
                                state.subtitles.push_back(std::move(choice));
                            }
                            xs::RemoteServer::shared().publish_playback_options(std::move(state));
                            emit changed();
                        }
                    }
                });
                QObject::connect(veyraProcess_.get(), &QProcess::finished, this,
                    [this,attempt](int exitCode, QProcess::ExitStatus exitStatus){
                        if(attempt!=veyraAttemptId_)return;
                        const bool handshake=veyraBridgeReady_;
                        const bool intentional=veyraStopRequested_;
                        const double position=veyraLive_?-1.0:veyraLastPosition_;
                        if(!intentional && (!handshake || exitCode!=0 || exitStatus==QProcess::CrashExit)){
                            scheduleVeyraFallback();
                            return;
                        }
                        const bool ended=veyraEnded_;
                        QTimer::singleShot(0,this,[this,attempt,position,ended]{
                            if(attempt!=veyraAttemptId_)return;
                            veyraWatchdog_.stop();
                            veyraState_.clear();
                            // End old session BEFORE notifying home; the callback may
                            // immediately activate the next episode/channel.
                            xs::RemoteServer::shared().end_playback();
                            playing_.store(false);
                            veyraBridgeReady_=false;
                            veyraSource_.clear();
                            notice_=QStringLiteral("Veyra terminó la reproducción");
                            emit changed();
                            emit finishedPlayback(position,ended);
                        });
                    });
                veyraProcess_->start();
                if(veyraProcess_->waitForStarted(3000)){
                    title_=title;
                    xs::RemoteServer::shared().begin_playback(title.toStdString());
                    notice_=QStringLiteral("Reproduciendo en Veyra (esperando conexión de controles)");
                    playing_.store(true);
                    emit changed();
                    veyraWatchdog_.disconnect(this);
                    connect(&veyraWatchdog_,&QTimer::timeout,this,[this,attempt]{
                        if(attempt!=veyraAttemptId_ || veyraStopRequested_)return;
                        const auto elapsed=veyraClock_.elapsed();
                        if((!veyraBridgeReady_ && elapsed>10000) ||
                           (veyraBridgeReady_ && elapsed-veyraLastStatus_>10000) ||
                           (!veyraStarted_ && elapsed>30000))scheduleVeyraFallback();
                    });
                    veyraWatchdog_.start(1000);
                    return;
                }
                veyraProcess_.reset(); // Failed to launch: mpv still works.
            }
        }
    }
    veyraState_.clear();
    diagnostics_.clear();
    if(thread_.joinable())thread_.join();
    stop_.store(false);
    if(!videoWindow_){
        videoWindow_=std::make_unique<QWindow>();
        videoWindow_->installEventFilter(this);
    }
    auto* screen=QGuiApplication::focusWindow()?QGuiApplication::focusWindow()->screen():QGuiApplication::primaryScreen();
    videoWindow_->setScreen(screen);
    const QRect area=screen->availableGeometry();
    QRect bounds(QPoint(),QSize(960,540).boundedTo(area.size()));
    bounds.moveCenter(area.center());
    videoWindow_->setTitle(title);
    videoWindow_->showNormal();
    videoWindow_->setGeometry(bounds);
    videoWindow_->requestActivate();
    videoWindowId_=static_cast<std::int64_t>(videoWindow_->winId());
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
    if(nativeVeyra_){
        NovaVeyraSnapshot state{};
        const bool valid=nativeVeyra_->snapshot(state);
        const double position=valid && state.position_seconds>=0?
            state.position_seconds:nativeVeyraLastPosition_;
        nativeVeyraPoll_.stop();
        nativeVeyra_->stop();
        nativeVeyra_.reset();
        playing_.store(false);
        veyraBridgeReady_=false;
        veyraState_.clear();
        xs::RemoteServer::shared().end_playback();
        if(videoWindow_)videoWindow_->hide();
        notice_=QStringLiteral("Veyra nativo detenido");
        emit changed();
        emit finishedPlayback(position,false);
        return;
    }
    if(veyraProcess_ && veyraProcess_->state()!=QProcess::NotRunning){
        veyraStopRequested_=true;
        veyraWatchdog_.stop();
        sendVeyraControl({{"cmd","quit"}});
        const auto attempt=veyraAttemptId_;
        QTimer::singleShot(1500,this,[this,attempt]{
            if(attempt==veyraAttemptId_ && veyraStopRequested_ && veyraProcess_ &&
               veyraProcess_->state()!=QProcess::NotRunning)veyraProcess_->kill();
        });
        return;
    }
    stop_.store(true);
    command("stop",nullptr);
}
void NovaQtPlayer::togglePause(){
    if(nativeVeyra_){
        nativeVeyraPaused_=!nativeVeyraPaused_;
        nativeVeyra_->pause(nativeVeyraPaused_);
        return;
    }
    if(veyraProcess_ && veyraProcess_->state()==QProcess::Running){
        sendVeyraControl({{"cmd","pause"}});
        return;
    }
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    int paused=0;
    if(mpv_get_property(handle_,"pause",MPV_FORMAT_FLAG,&paused)>=0){
        paused=!paused;
        mpv_set_property_async(handle_,0,"pause",MPV_FORMAT_FLAG,&paused);
    }
}
void NovaQtPlayer::setVolume(int value){
    if(nativeVeyra_){
        nativeVeyra_->volume(std::clamp(value,0,100)/100.0);
        return;
    }
    if(veyraProcess_ && veyraProcess_->state()==QProcess::Running){
        sendVeyraControl({{"cmd","volume"},{"value",std::clamp(value,0,100)/100.0}});
        return;
    }
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    double volume=static_cast<double>(std::clamp(value,0,100));
    mpv_set_property_async(handle_,0,"volume",MPV_FORMAT_DOUBLE,&volume);
}
void NovaQtPlayer::setSpeed(int percent){
    if(nativeVeyra_){
        const auto speed=std::clamp(percent,50,200)/100.0;
        if(!nativeVeyra_->rate(speed)){
            notice_=QStringLiteral("Veyra nativo no aceptó el cambio de velocidad.");
            emit changed();
        }
        return;
    }
    if(veyraProcess_ && veyraProcess_->state()==QProcess::Running){
        sendVeyraControl({{"cmd","speed"},{"value",std::clamp(percent,50,200)/100.0}});
        return;
    }
    QMutexLocker lock(&mutex_);
    if(!handle_)return;
    double speed=std::clamp(percent,50,200)/100.0;
    mpv_set_property_async(handle_,0,"speed",MPV_FORMAT_DOUBLE,&speed);
}
void NovaQtPlayer::toggleFullscreen(){
    if(veyraProcess_ && veyraProcess_->state()==QProcess::Running){
        sendVeyraControl({{"cmd","fullscreen"}});
        return;
    }
    if(!videoWindow_ || !videoWindow_->isVisible())return;
    if(videoWindow_->visibility()==QWindow::FullScreen){
        if(restoreMaximized_)videoWindow_->showMaximized();
        else videoWindow_->showNormal();
    }else{
        restoreMaximized_=videoWindow_->visibility()==QWindow::Maximized;
        videoWindow_->showFullScreen();
    }
}

void NovaQtPlayer::cycleMonitor(){
    if(veyraProcess_ && veyraProcess_->state()==QProcess::Running){
        sendVeyraControl({{"cmd","next_monitor"}});
        return;
    }
    if(nativeVeyra_ && videoWindow_){
        const auto screens=QGuiApplication::screens();
        if(screens.size()<2)return;
        auto* current=videoWindow_->screen();
        const int index=screens.indexOf(current);
        auto* next=screens[(index+1)%screens.size()];
        const QRect area=next->availableGeometry();
        const bool full=videoWindow_->visibility()==QWindow::FullScreen;
        if(full)videoWindow_->showNormal();
        videoWindow_->setScreen(next);
        QRect bounds(QPoint(),QSize(960,540).boundedTo(area.size()));
        bounds.moveCenter(area.center());
        videoWindow_->setGeometry(bounds);
        if(full)videoWindow_->showFullScreen();
    }
}

bool NovaQtPlayer::eventFilter(QObject* object,QEvent* event){
    if(object!=videoWindow_.get())return QObject::eventFilter(object,event);
    if(event->type()==QEvent::Close){
        stop();
    }else if(event->type()==QEvent::MouseButtonDblClick){
        toggleFullscreen();
        return true;
    }else if(event->type()==QEvent::KeyPress){
        auto* key=static_cast<QKeyEvent*>(event);
        switch(key->key()){
        case Qt::Key_F: case Qt::Key_F11:
            if(!key->isAutoRepeat())toggleFullscreen();
            return true;
        case Qt::Key_Escape:
            if(videoWindow_->visibility()==QWindow::FullScreen)toggleFullscreen();
            else stop();
            return true;
        case Qt::Key_Space: togglePause();return true;
        case Qt::Key_Left: seek(-5);return true;
        case Qt::Key_Right: seek(5);return true;
        }
    }
    return QObject::eventFilter(object,event);
}
void NovaQtPlayer::selectTrack(int kind,int id,uint32_t generation){
    if(nativeVeyra_){
        const auto current=xs::RemoteServer::shared().playback_options();
        if(kind==1 && current.active && current.generation==generation && id>=0)
            nativeVeyra_->selectAudio(id);
        return;
    }
    if(veyraProcess_ && veyraProcess_->state()==QProcess::Running){
        const auto snapshot=xs::RemoteServer::shared().playback_options();
        if(snapshot.active && snapshot.generation==generation){
            if(kind==1 && id>=0)
                sendVeyraControl({{"cmd","audio_track"},{"index",id}});
            else if(kind==2 && id>=-1)
                sendVeyraControl({{"cmd","subtitle_track"},{"index",id}});
        }
        return;
    }
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
    if(nativeVeyra_){
        // Lanczos, CAS and FSRCNNX belong to the separate MPV renderer.
        // Never trigger a libmpv restart during a native D3D12 session.
        xs::RemoteServer::shared().set_setting_value(
            xs::remote_setting_index("video_preset"),requested_quality_.load());
        notice_=QStringLiteral("Los perfiles Lanczos/CAS/FSRCNNX son de MPV; Veyra tiene sus controles SR, NR y FPS.");
        emit changed();
        return;
    }
    // No restart, and no misleading status, when selecting the same mode.
    if(requested_quality_.exchange(value)==value)return;
    restart_revision_.fetch_add(1);
}
bool NovaQtPlayer::gpuRifeReady(){
    const auto exeDir=QCoreApplication::applicationDirPath();
    return xs::nova_rife_files_present(
        std::filesystem::path(QDir::toNativeSeparators(exeDir).toStdWString()));
}
bool NovaQtPlayer::requestMotion(int value){
    if(value<0||value>2)return false;
    if(nativeVeyra_){
        // The old options mean MPV SmoothDisplay and Vulkan RIFE, not DLSS FG.
        // Do not advertise a different algorithm under their old labels.
        xs::RemoteServer::shared().set_setting_value(
            xs::remote_setting_index("motion_mode"),requested_motion_.load());
        notice_=QStringLiteral("RIFE/Vulkan son de MPV; usa Generación de cuadros en ajustes Veyra.");
        emit changed();
        return false;
    }
    if(value==2&&!gpuRifeReady()){
        // The server may have saved the iPhone's selection before this
        // request reaches the UI thread. Undo it without restarting playback.
        xs::RemoteServer::shared().set_setting_value(
            xs::remote_setting_index("motion_mode"),requested_motion_.load());
        notice_=QStringLiteral("RIFE GPU no está instalado. Añade el complemento Vulkan.");
        emit changed();
        return false;
    }
    if(requested_motion_.exchange(value)!=value)
        restart_revision_.fetch_add(1);
    return true;
}

void NovaQtPlayer::applySetting(int id,int value){
    if(id>=0 && static_cast<size_t>(id)<xs::kRemoteSettings.size()){
        const auto key=QString::fromLatin1(xs::kRemoteSettings[id].key);
        if(key.startsWith("veyra_")){setVeyraSetting(key,value);return;}
    }
    if(veyraActive() && id==xs::remote_setting_index("mute")){
        setVolume(value?0:xs::RemoteServer::shared().setting_value(xs::remote_setting_index("volume")));
        return;
    }
    if(id==xs::remote_setting_index("video_preset")){
        requestQuality(value);
        return;
    }
    if(id==xs::remote_setting_index("motion_mode")){
        requestMotion(value);
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
    if(nativeVeyra_){
        if(std::isfinite(seconds)){
            NovaVeyraSnapshot state{};
            if(nativeVeyra_->snapshot(state))
                nativeVeyra_->seek(std::max(0.0,state.position_seconds+seconds));
        }
        return;
    }
    if(veyraProcess_ && veyraProcess_->state()==QProcess::Running){
        if(std::isfinite(seconds))
            sendVeyraControl({{"cmd","seek_by"},{"seconds",seconds}});
        return;
    }
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
    const int motionId=xs::remote_setting_index("motion_mode");
    int activeMotion=remote.setting_value(motionId);
    if(activeMotion<0||activeMotion>2)activeMotion=0;
    requested_motion_.store(activeMotion);
    int previousPreset=activePreset,previousMotion=activeMotion;
    QString motionFallbackReason;
    bool fastRife=false;
    bool switching=false, registered=false, reachedEnd=false;
    bool recoveryAttempted=false;
    double lastPosition=resumeSeconds;
    QString endMessage=QStringLiteral("Volviste al catálogo.");
    xs::NovaPlaybackRestartGate restartGate;

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
        if(!live){
            const double current=number("time-pos",lastPosition);
            if(xs::nova_can_resume(false,current))restore.position=current;
        }else restore.position=0.0;
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
        const auto motion=xs::nova_motion_mode(activeMotion);
        const bool gpuMode=xs::nova_needs_shader(preset);
        const bool gpuRife=xs::nova_motion_needs_gpu(motion);
        // Evaluate before touching libmpv: the previous CPU interpolator
        // must NEVER be used as an implicit fallback for a missing GPU.
        const bool missingRife=gpuRife&&!gpuRifeReady();
        const QString shader=QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("shaders/FSRCNNX_x2_8-0-4-1.glsl"));
        bool failed=missingRife||(gpuMode&&(!QFileInfo(shader).isFile()));
        std::unique_ptr<QTemporaryDir> rifeSession;
        QString rifeStatusPath;
        bool loaded=false, restart=false, streamError=false;
        auto gpuReadyAt=std::chrono::steady_clock::now();
        int badSyncSamples=0;
        QString baseDiagnostics;
        auto lastFpsCheck=std::chrono::steady_clock::now();
        QString failure=missingRife?QStringLiteral("Falta el complemento GPU RIFE Vulkan."):
            (failed?QStringLiteral("Falta el shader GPU FSRCNNX."):QString());
        mpv_handle* mpv=failed?nullptr:mpv_create();
        if(!failed&&!mpv){
            failed=true;
            failure=QStringLiteral("No se pudo iniciar el motor MPV.");
        }
        if(!failed && gpuRife){
            const QDir app(QCoreApplication::applicationDirPath());
            const QString home=app.filePath(QStringLiteral("nova-gpu/python"));
            QDir lib(home+QStringLiteral("/lib"));
            const QStringList versions=lib.entryList(
                QStringList{QStringLiteral("python3.*")},
                QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name);
            if(versions.isEmpty() || !QFileInfo(lib.filePath(versions.last()+
                    QStringLiteral("/encodings/__init__.py"))).isFile()){
                failed=true;
                failure=QStringLiteral("Falta el runtime Python de VapourSynth.");
            }else{
                const QByteArray folder=QDir::toNativeSeparators(home).toUtf8();
                const QByteArray pythonPath=QDir::toNativeSeparators(
                    lib.filePath(versions.last())).toUtf8();
                qputenv("PYTHONHOME",folder);
                qputenv("PYTHONPATH",pythonPath+";"+pythonPath+"/lib-dynload;"+pythonPath+"/site-packages");
            }
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
                     opt("wid",std::to_string(videoWindowId_).c_str()) &&
#ifdef Q_OS_WIN
                     // Keep presentation composed by Windows during monitor-sized resizes.
                     opt("d3d11-exclusive-fs","no") &&
                     opt("d3d11-flip","no") &&
#endif
                     opt("network-timeout",live?"25":"15") &&
                     opt("cache","yes") &&
                     opt("sub-auto","no") &&
                     opt("tls-verify","yes"));
            if(!failed){
                // mpv's hardware decoder chooses D3D11VA/NVDEC on supported
                // devices; compatible mode deliberately stays on CPU.
                const char* decoder=preset==xs::VideoPreset::Compatible?"no":
                    (gpuRife?"auto-copy":"auto-safe");
                failed=!opt("hwdec",decoder);
                if(preset==xs::VideoPreset::Sharp ||
                   preset==xs::VideoPreset::ExtraDetail)
                    failed=failed||!opt("scale","lanczos");
                else if(gpuMode){
                    const QByteArray shaderPath=QDir::toNativeSeparators(shader).toUtf8();
                    failed=failed||!opt("gpu-api",gpuRife?"d3d11":"opengl") ||
                           !opt("glsl-shaders",shaderPath.constData()) ||
                           !opt("scale","ewa_lanczossharp") ||
                           !opt("cscale","ewa_lanczos");
                }else failed=failed||!opt("scale","bicubic");
            }
            if(!failed){
                if(motion==xs::MotionMode::SmoothDisplay){
                    failed=!opt("video-sync","display-resample") ||
                           !opt("interpolation","yes") ||
                           !opt("tscale","linear");
                }else if(gpuRife){
                    // Use genuine GPU RIFE through VapourSynth/Vulkan. No
                    // CPU minterpolate in this pipeline. MPV interprets %N%
                    // as a length-prefixed filename, so drive colons, commas
                    // and spaces cannot be mistaken for vf delimiters.
                    const QString script=QDir(QCoreApplication::applicationDirPath())
                        .filePath(QStringLiteral("nova-gpu/rife.vpy"));
                    const QByteArray bytes=QDir::toNativeSeparators(script).toUtf8();
                    rifeSession=std::make_unique<QTemporaryDir>();
                    if(rifeSession->isValid()){
                        rifeStatusPath=rifeSession->filePath(fastRife?QStringLiteral("rife-fast"):
                            gpuMode?QStringLiteral("rife-balanced"):QStringLiteral("rife-status"));
                        const QByteArray statusPath=QDir::toNativeSeparators(rifeStatusPath).toUtf8();
                        const std::string spec="vapoursynth=file=%"+
                            std::to_string(bytes.size())+"%"+bytes.toStdString()+
                            ":concurrent-frames=2:user-data=%"+
                            std::to_string(statusPath.size())+"%"+statusPath.toStdString();
                        failed=!opt("vf",spec.c_str());
                    }else{
                        failed=true;
                        failure=QStringLiteral("No se pudo preparar el estado de RIFE.");
                    }
                }
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
                    if(id!=presetId&&id!=motionId)applySetting(id,remote.setting_value(id));
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
                mpv_request_log_messages(mpv,"warn");
                const char* load[]={"loadfile",result.source.c_str(),nullptr};
                if(mpv_command(mpv,load)<0){
                    failed=true;failure=QStringLiteral("No pudo abrirse la transmisión.");
                }else{
                    if(!registered){
                        remote.begin_playback(title.toStdString());
                        registered=true;
                    }
                    mpv_observe_property(mpv,41,"track-list",MPV_FORMAT_NONE);
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
                        gpuReadyAt=std::chrono::steady_clock::now();
                        badSyncSamples=0;
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
                            std::string graph=xs::extra_detail_filter(int(w),int(h));
                            // "vf add" preserves the already-installed RIFE
                            // VapourSynth filter; replacing vf would silently
                            // disable GPU interpolation whenever CAS is chosen.
                            const char* add[]={"vf","add",graph.c_str(),nullptr};
                            if(graph.empty()||mpv_command(mpv,add)<0){
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
                        double originalFps=0.0;
                        mpv_get_property(mpv,"container-fps",MPV_FORMAT_DOUBLE,&originalFps);
                        const QString motionStatus=QString::fromUtf8(xs::nova_motion_label(motion));
                        const QString details=QStringLiteral(
                            "Origen %1×%2 · Salida %3×%4 · Decodificador %5 · %6\n"
                            "FPS originales: %7 · MotionBoost: %8 · Procesamiento: %9")
                            .arg(sourceW).arg(sourceH).arg(outputW).arg(outputH)
                            .arg(decoding,QString::fromStdString(filter))
                            .arg(originalFps>0?QString::number(originalFps,'f',2):
                                 QStringLiteral("sin datos"),motionStatus,
                                 gpuRife?QStringLiteral("GPU Vulkan RIFE (verificar FPS reales)"):
                                         QStringLiteral("MPV"));
                        baseDiagnostics=details;
                        if(gpuRife && (gpuMode||fastRife))baseDiagnostics+=QStringLiteral("\nResolución de cálculo RIFE: hasta %1 px de ancho").arg(fastRife?384:640);
                        if(!motionFallbackReason.isEmpty())baseDiagnostics+="\n"+motionFallbackReason;
                        publish(QStringLiteral("Reproduciendo · %1").arg(
                            QString::fromUtf8(xs::video_preset_label(preset))),details);
                        lastFpsCheck=std::chrono::steady_clock::now();
                    }
                    if(event->event_id==MPV_EVENT_PROPERTY_CHANGE&&event->reply_userdata==41)
                        publishTracks(mpv,title);
                    if(event->event_id==MPV_EVENT_FILE_LOADED)
                        publishTracks(mpv,title);
                    if(event->event_id==MPV_EVENT_LOG_MESSAGE&&
                       (gpuMode||gpuRife)){
                        const auto* log=static_cast<const mpv_event_log_message*>(event->data);
                        if(log&&log->text){
                            std::string message=std::string(log->prefix?log->prefix:"")+" "+log->text;
                            std::transform(message.begin(),message.end(),message.begin(),
                                [](unsigned char c){return static_cast<char>(std::tolower(c));});
                            if(gpuRife && message.find("vapoursynth")!=std::string::npos &&
                               (message.find("script evaluation failed")!=std::string::npos ||
                                message.find("disabling filter")!=std::string::npos ||
                                message.find("filter error at frame")!=std::string::npos)){
                                failed=true;
                                failure=QStringLiteral("RIFE GPU no pudo cargar el filtro.");
                                break;
                            }
                            if(gpuMode && (message.find("shader")!=std::string::npos||
                                message.find("glsl")!=std::string::npos)&&
                               (message.find("failed")!=std::string::npos||
                                message.find("error")!=std::string::npos)){
                                failed=true;
                                failure=QStringLiteral("El shader FSRCNNX falló al cargarse.");
                                break;
                            }
                        }
                    }
                    if(event->event_id==MPV_EVENT_PLAYBACK_RESTART || event->event_id==MPV_EVENT_VIDEO_RECONFIG){
                        gpuReadyAt=std::chrono::steady_clock::now();
                        badSyncSamples=0;
                    }
                    if(event->event_id==MPV_EVENT_END_FILE){
                        const auto* end=static_cast<const mpv_event_end_file*>(event->data);
                        reachedEnd=end&&end->reason==MPV_END_FILE_REASON_EOF;
                        if(end&&end->reason==MPV_END_FILE_REASON_ERROR){
                            failed=true;
                            streamError=true;
                            failure=QStringLiteral("Se cortó la reproducción.");
                        }
                        break;
                    }
                    if(event->event_id==MPV_EVENT_SHUTDOWN)break;
                }
                if(loaded && gpuRife && std::chrono::steady_clock::now()-lastFpsCheck>=
                   std::chrono::seconds(2) && QFileInfo(rifeStatusPath).size()>0){
                    failed=true;
                    failure=QStringLiteral("RIFE no pudo iniciar Vulkan. Se reanuda el video normal.");
                    break;
                }
                if(loaded&&gpuRife&&std::chrono::steady_clock::now()-gpuReadyAt>=
                   std::chrono::seconds(8) &&
                   std::chrono::steady_clock::now()-lastFpsCheck>=std::chrono::seconds(2)){
                    // Stop RIFE if the video persistently falls behind audio.
                    double avsync=0.0;
                    if(mpv_get_property(mpv,"avsync",MPV_FORMAT_DOUBLE,&avsync)>=0 &&
                       std::isfinite(avsync)&&std::abs(avsync)>0.75)
                        ++badSyncSamples;
                    else badSyncSamples=0;
                    if(badSyncSamples>=3){
                        if(!fastRife){
                            capture(mpv);
                            fastRife=true;
                            restart=true;
                            publish(QStringLiteral("Ajustando RIFE para recuperar la sincronización..."));
                            break;
                        }
                        failed=true;
                        failure=QStringLiteral(
                            "RIFE GPU no mantiene audio/video sincronizados. Volviendo a reproducción normal.");
                        break;
                    }
                }
                // The filter's requested output (60) is NOT proof of 60
                // rendered FPS. Expose the estimated post-filter output,
                // leaving unknown measurements explicitly unknown.
                if(loaded&&std::chrono::steady_clock::now()-lastFpsCheck>=
                   std::chrono::seconds(2)){
                    lastFpsCheck=std::chrono::steady_clock::now();
                    double estimated=0.0;
                    QString info=baseDiagnostics;
                    if(mpv_get_property(mpv,"estimated-vf-fps",MPV_FORMAT_DOUBLE,
                                        &estimated)>=0 && estimated>0.0 &&
                       std::isfinite(estimated))
                        info+=QStringLiteral("\nFPS estimados tras filtro: %1")
                            .arg(estimated,0,'f',1);
                    else
                        info+=QStringLiteral("\nFPS tras filtro: sin datos todavía");
                    publish(QStringLiteral("Reproduciendo · %1").arg(
                        QString::fromUtf8(xs::video_preset_label(preset))),info);
                }
                const auto revision=restart_revision_.load();
                restartGate.observe(revision,xs::NovaPlaybackRestartGate::Clock::now());
                const int wanted=requested_quality_.load();
                const int wantedMotion=requested_motion_.load();
                if(restartGate.ready(wanted,wantedMotion,activePreset,activeMotion,
                                     xs::NovaPlaybackRestartGate::Clock::now())){
                    capture(mpv);
                    previousPreset=activePreset;
                    previousMotion=activeMotion;
                    activePreset=wanted;
                    activeMotion=wantedMotion;
                    motionFallbackReason.clear();
                    fastRife=false;
                    switching=true;
                    recoveryAttempted=false;
                    restart=true;
                    publish(QStringLiteral("Aplicando imagen y movimiento..."),
                            QStringLiteral("Reiniciando MPV sin cerrar Nova"));
                    const char* osd[]={"show-text","Aplicando imagen y movimiento...","1200",nullptr};
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
            if(recoveryAttempted || (loaded && streamError && !gpuRife)){
                endMessage=failure;
                break;
            }
            // Revert only once. Keep the same source and saved position.
            int fallback=switching?previousPreset:static_cast<int>(xs::VideoPreset::Balanced);
            int fallbackMotion=switching?previousMotion:0;
            if(gpuRife){
                // Keep the picture preset and retry the same source without RIFE.
                fallback=activePreset;
                fallbackMotion=0;
            }else if(!switching && fallback==activePreset){
                fallback=static_cast<int>(xs::VideoPreset::Compatible);
            }
            if(fallback!=activePreset||fallbackMotion!=activeMotion){
                recoveryAttempted=true;
                activePreset=fallback;
                activeMotion=fallbackMotion;
                previousPreset=fallback;
                previousMotion=fallbackMotion;
                switching=false;
                remote.set_setting_value(presetId,fallback);
                if(gpuRife)motionFallbackReason=failure;
                else remote.set_setting_value(motionId,fallbackMotion);
                requested_quality_.store(fallback);
                requested_motion_.store(fallbackMotion);
                restart_revision_.fetch_add(1);
                publish(QStringLiteral("Filtro no disponible. Restaurando modo anterior."),
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
