#include "nova_qt_home.hpp"
#include "nova_catalog_cache.hpp"
#include "remote_server.hpp"
#include "remote_pairing_qr.hpp"
#include <SDL.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cwchar>
#include <qrencode.h>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <curl/curl.h>
#include <cstdio>
#include <sstream>
#include <QCoreApplication>
#include <QCursor>
#include <QFutureWatcher>
#include <QGuiApplication>
#include "nova_quality.hpp"
#include "nova_motion.hpp"
#include "nova_auto_next.hpp"
#include <QImage>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <unordered_set>
#include <vector>

namespace {
BOOL CALLBACK collectMonitor(HMONITOR monitor,HDC,LPRECT,LPARAM output){
    reinterpret_cast<std::vector<HMONITOR>*>(output)->push_back(monitor);
    return TRUE;
}
void moveToMonitor(HWND window,HMONITOR destination){
    MONITORINFO from{sizeof(MONITORINFO)},to{sizeof(MONITORINFO)};
    RECT r{};
    if(!GetWindowRect(window,&r)||!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&from)||
       !GetMonitorInfoW(destination,&to))return;
    const bool fullscreen=EqualRect(&r,&from.rcMonitor);
    const bool maximized=IsZoomed(window);
    if(maximized){ShowWindow(window,SW_RESTORE);GetWindowRect(window,&r);}
    const RECT area=fullscreen?to.rcMonitor:to.rcWork;
    const LONG w=fullscreen?area.right-area.left:std::min(r.right-r.left,area.right-area.left);
    const LONG h=fullscreen?area.bottom-area.top:std::min(r.bottom-r.top,area.bottom-area.top);
    SetWindowPos(window,nullptr,area.left+(area.right-area.left-w)/2,area.top+(area.bottom-area.top-h)/2,
                 w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    if(maximized)ShowWindow(window,SW_MAXIMIZE);
}
void nextMonitor(QQuickWindow* home,QWindow* player){
    if(!home)return;
    HWND video=player&&player->isVisible()?reinterpret_cast<HWND>(player->winId()):nullptr;
    HWND catalog=reinterpret_cast<HWND>(home->winId());
    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(nullptr,nullptr,collectMonitor,reinterpret_cast<LPARAM>(&monitors));
    if(monitors.size()<2)return;
    auto current=std::find(monitors.begin(),monitors.end(),MonitorFromWindow(video?video:catalog,MONITOR_DEFAULTTONEAREST));
    const auto target=(current==monitors.end()||++current==monitors.end())?monitors.front():*current;
    moveToMonitor(catalog,target);
    if(video)moveToMonitor(video,target);
}
struct HomeSnapshot {
    QVariantList channels,series,movies,demo_items;
    std::vector<xs::Entry> tv_entries,series_entries,movie_entries,demo_entries;
    std::shared_ptr<xs::ServiceSession> session;
    xs::Catalog root;
    QString status;
    QString service_status;
    bool service_connected=false;
    bool previews_complete=false;
};
constexpr const char* kPublicSpanishPlaylist =
    "https://iptv-org.github.io/iptv/languages/spa.m3u";
constexpr const char* kPublicPlaylistCache = "iptv_es_cache.m3u";
constexpr size_t kPublicPlaylistMaxBytes=8u*1024u*1024u;

size_t appendPublicPlaylist(char* p,size_t size,size_t count,void* context){
    if(size && count>kPublicPlaylistMaxBytes/size)return 0;
    const size_t bytes=size*count;
    auto& buffer=*static_cast<std::string*>(context);
    if(bytes>kPublicPlaylistMaxBytes-buffer.size())return 0;
    buffer.append(p,bytes);
    return bytes;
}
xs::Catalog parsePublicPlaylist(const std::string& contents){
    if(contents.size()>kPublicPlaylistMaxBytes)return {};
    std::istringstream input(contents);
    xs::Catalog found=xs::read_m3u(input);
    // A failed CDN response is not a playlist, even if the server returns 200.
    if(found.entries.empty()||found.entries.size()>5000)return {};
    for(auto& item:found.entries){
        item.description="Canal público de IPTV-org · Lista en español";
        item.program_type="public-live";
    }
    found.title="TV en vivo · IPTV-org";
    return found;
}
struct PublicPlaylistResult {xs::Catalog catalog;bool from_cache=false;};
PublicPlaylistResult publicSpanishPlaylist(){
    PublicPlaylistResult result;
    // This runs in the background home-loading worker, never on Qt's UI thread.
    CURL* curl=curl_easy_init();
    if(curl){
        std::string data;
        curl_easy_setopt(curl,CURLOPT_URL,kPublicSpanishPlaylist);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,appendPublicPlaylist);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&data);
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,6L);
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,16L);
        curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
        curl_easy_setopt(curl,CURLOPT_MAXREDIRS,3L);
        curl_easy_setopt(curl,CURLOPT_USERAGENT,"NovaQt/1.0 (public IPTV playlist)");
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
        curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"https");
#else
        curl_easy_setopt(curl,CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);
        curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS,CURLPROTO_HTTPS);
#endif
        std::ifstream ca("cacert.pem",std::ios::binary);
        if(ca.good())curl_easy_setopt(curl,CURLOPT_CAINFO,"cacert.pem");
        const CURLcode rc=curl_easy_perform(curl);
        long code=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
        curl_easy_cleanup(curl);
        if(rc==CURLE_OK&&code==200){
            result.catalog=parsePublicPlaylist(data);
            if(!result.catalog.entries.empty()){
                std::ofstream cache(kPublicPlaylistCache,std::ios::binary|std::ios::trunc);
                if(cache)cache.write(data.data(),static_cast<std::streamsize>(data.size()));
                return result;
            }
        }
    }
    // No network? Preserve the last successfully parsed playlist.
    std::ifstream cache(kPublicPlaylistCache,std::ios::binary|std::ios::ate);
    if(cache&&cache.tellg()>0&&cache.tellg()<=static_cast<std::streamoff>(kPublicPlaylistMaxBytes)){
        std::string content(static_cast<size_t>(cache.tellg()),'\0');
        cache.seekg(0);
        if(cache.read(content.data(),static_cast<std::streamsize>(content.size()))){
            result.catalog=parsePublicPlaylist(content);
            result.from_cache=!result.catalog.entries.empty();
        }
    }
    return result;
}
void addEntry(std::vector<xs::Entry>& out,const xs::Entry& e,int maxItems);
QVariantList listFor(const std::vector<xs::Entry>& entries);
void appendPublicChannels(HomeSnapshot& out,const xs::Catalog& playlist){
    std::unordered_set<std::string> seen;
    for(const auto& item:out.tv_entries)
        if(!item.source.empty())seen.insert(item.source);
    for(const auto& item:playlist.entries){
        if(out.tv_entries.size()>=5000)break;
        if(item.kind==xs::EntryKind::Media && xs::valid_source(item.source) &&
           seen.insert(item.source).second)
            out.tv_entries.push_back(item);
    }
}
QVariantList tvPreview(const std::vector<xs::Entry>& entries){
    const size_t count=std::min(entries.size(),size_t{80});
    return listFor(std::vector<xs::Entry>(entries.begin(),entries.begin()+count));
}
QVariantMap itemFor(const xs::Entry& e){
    const bool cover=(e.artwork.rfind("https://",0)==0||
                      e.artwork.rfind("http://",0)==0)&&e.artwork.size()<1800;
    QVariantList schedule;
    for(const auto& program:e.schedule)
        schedule.append(QVariantMap{{"title",QString::fromStdString(program.title)},
                                    {"start",QString::fromStdString(program.start)},
                                    {"end",QString::fromStdString(program.end)}});
    return {{QStringLiteral("schedule"),schedule},
            {QStringLiteral("title"),QString::fromStdString(e.title)},
            {QStringLiteral("artwork"),cover?QString::fromStdString(e.artwork):QString()},
            {QStringLiteral("kind"),static_cast<int>(e.kind)},
            {QStringLiteral("description"),QString::fromStdString(e.description)}};
}
QVariantList listFor(const std::vector<xs::Entry>& entries){
    QVariantList items;
    for(const auto& e:entries)items.push_back(itemFor(e));
    return items;
}
void addEntry(std::vector<xs::Entry>& out,const xs::Entry& e,int maxItems){
    if(static_cast<int>(out.size())>=maxItems||e.title.empty())return;
    if(std::any_of(out.begin(),out.end(),[&](const xs::Entry& current){
        return current.title==e.title&&current.content_id==e.content_id&&
               current.channel_code==e.channel_code;
    }))return;
    out.push_back(e);
}
void addAssets(const xs::Catalog& page,const std::string& type,
               std::vector<xs::Entry>& entries,int maxItems){
    for(const auto& e:page.entries)
        if(e.kind==xs::EntryKind::Asset&&e.program_type==type)
            addEntry(entries,e,maxItems);
}
HomeSnapshot loadHome(){
    HomeSnapshot out;
    // Always provide a separate OFFLINE test player. It must never be
    // presented as a live TV channel from the authenticated service.
    {
        std::ifstream local_file("catalog.m3u");
        if(local_file){
            const auto local=xs::read_m3u(local_file);
            for(const auto& e:local.entries)
                if(e.kind==xs::EntryKind::Media)addEntry(out.demo_entries,e,80);
        }
        out.demo_items=listFor(out.demo_entries);
    }

    auto login=xs::service_bootstrap("service.json");
    out.session=login.session;
    out.service_connected=(out.session!=nullptr);
    // Preserve the real service error even when public TV is available.
    out.service_status=QString::fromStdString(login.status);
    if(!out.service_connected&&out.service_status.isEmpty())
        out.service_status=QStringLiteral("Sin sesión autorizada de XuperTV.");
    out.root=std::move(login.catalog);
    if(!out.session){out.status=out.service_status;return out;}
    // Publish the authorized catalog promptly; heavy rails are populated
    // independently after the user can already navigate all sections.
    for(const auto& e:out.root.entries)
        if(e.kind==xs::EntryKind::Media)addEntry(out.tv_entries,e,80);

    out.channels=tvPreview(out.tv_entries);
    out.status=out.root.entries.empty()
        ?out.service_status
        :QStringLiteral("XuperTV conectado. Cargando portadas y avances sin bloquear el catálogo...");
    return out;
}

// Previews are optional: never hold the entire home screen hostage to
// six TV categories plus VOD requests (each can take 25 seconds).
// The entry points to the FULL provider catalog are already ready in root.
HomeSnapshot loadHomePreviews(const std::shared_ptr<xs::ServiceSession>& session,
                               const xs::Catalog& root,
                               std::vector<xs::Entry> initial_tv,
                               const std::shared_ptr<xs::ServiceRequest>& request){
    HomeSnapshot out;
    out.session=session;
    out.root=root;
    out.service_connected=true;
    // Fetch provider channels first; a 1000-channel public fallback must not
    // exhaust the provider rail's limit before we inspect its own categories.
    xs::Catalog public_channels;
    for(auto& e:initial_tv)
        if(e.program_type=="public-live")
            public_channels.entries.push_back(std::move(e));
    // Authorized M3U playlists can be returned directly as Media entries;
    // don't mistake them for an empty live service just because there are no
    // column/category entries.
    for(const auto& e:out.root.entries)
        if(e.kind==xs::EntryKind::Media)addEntry(out.tv_entries,e,80);
    // A first empty category must not hide channels in the next category.
    int checkedLive=0;
    for(const auto& category:out.root.entries){
        // service_root puts VOD sections first, before live categories.
        // Skip those sections rather than ending the live-channel scan.
        if(category.kind!=xs::EntryKind::Category)continue;
        if(request->stopped()||checkedLive++>=6||out.tv_entries.size()>=72)break;
        const auto response=xs::service_browse_remote(out.session,category,request);
        if(response.ok){
            for(const auto& e:response.catalog.entries)
                if(e.kind==xs::EntryKind::Channel)addEntry(out.tv_entries,e,72);
        }else if(out.status.isEmpty())out.status=QString::fromStdString(response.status);
    }
    for(const char* type:{"0","1"}){
        if(request->stopped())break;
        auto& dest=std::string(type)=="0"?out.series_entries:out.movie_entries;
        const auto section=std::find_if(out.root.entries.begin(),out.root.entries.end(),
            [&](const xs::Entry& e){
                return e.kind==xs::EntryKind::Section&&e.program_type==type;
            });
        if(section==out.root.entries.end())continue;
        const auto response=xs::service_browse_remote(out.session,*section,request);
        if(!response.ok){
            if(out.status.isEmpty())out.status=QString::fromStdString(response.status);
            continue;
        }
        addAssets(response.catalog,type,dest,60);
        int visited=0;
        for(const auto& e:response.catalog.entries){
            if(request->stopped()||dest.size()>=60||visited>=5)break;
            if(e.kind!=xs::EntryKind::VodCategory)continue;
            ++visited;
            const auto page=xs::service_browse_remote(out.session,e,request);
            if(page.ok)addAssets(page.catalog,type,dest,60);
            else if(out.status.isEmpty())out.status=QString::fromStdString(page.status);
        }
    }
    if(request->stopped()&&out.status.isEmpty())
        out.status=QStringLiteral("Carga de avances interrumpida por tiempo de espera; abre las categorías completas o actualiza.");
    appendPublicChannels(out,public_channels);
    out.channels=tvPreview(out.tv_entries);
    out.series=listFor(out.series_entries);
    out.movies=listFor(out.movie_entries);
    out.previews_complete=out.status.isEmpty()&&!request->stopped();
    if(!out.status.isEmpty()){
        out.service_status=out.status;
    }else if(out.series.isEmpty()&&out.movies.isEmpty()){
        out.service_status=QStringLiteral(
                "XuperTV autorizó la sesión, pero los avances VOD están vacíos. "
                "Abre Series o Películas > Ver todo para consultar las categorías completas.");
        out.status=out.service_status;
    }else if(!public_channels.entries.empty())
        out.status=QStringLiteral("Catálogo de XuperTV y TV pública disponibles.");
    else out.status=QStringLiteral("Catálogo de XuperTV conectado.");
    return out;
}

QString generatePairingQr(const std::string& address,const std::string& pin){
    const std::string payload=xs::pairing_qr_payload(address,pin);
    if(payload.empty())return {};
    QRcode* code=QRcode_encodeString(payload.c_str(),0,QR_ECLEVEL_M,QR_MODE_8,1);
    if(!code)return {};
    constexpr int quiet=4,cell=7;
    const int width=(code->width+quiet*2)*cell;
    QImage image(width,width,QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for(int y=0;y<code->width;++y)
        for(int x=0;x<code->width;++x)
            if(code->data[y*code->width+x]&1)
                painter.drawRect((x+quiet)*cell,(y+quiet)*cell,cell,cell);
    painter.end();
    QRcode_free(code);
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    if(!image.save(&buffer,"PNG"))return {};
    return QStringLiteral("data:image/png;base64,")+
           QString::fromLatin1(data.toBase64());
}
}

NovaQtHome::NovaQtHome(QObject* parent):QObject(parent),player_(this){
    connect(&player_,&NovaQtPlayer::changed,this,&NovaQtHome::changed);
    connect(&player_,&NovaQtPlayer::finishedPlayback,this,[this](double seconds,bool ended){
        // Save only known positions; the Veyra bridge reports -1 if unavailable.
        if(seconds>=0.0 && !active_entry_.content_id.empty()){
            playback_history_.save(active_entry_.content_id,seconds,
                playback_history_.speed,playback_history_.volume,
                playback_history_.auto_next,ended);
        }
        refreshContinue();
        if(queued_remote_entry_){
            const auto entry=*queued_remote_entry_;
            queued_remote_entry_.reset();
            next_episode_.reset();
            activate(entry);
        }else if(ended && playback_history_.auto_next && next_episode_ &&
                 active_entry_.kind==xs::EntryKind::Episode &&
                 active_entry_.program_type=="0" &&
                 !active_entry_.content_id.empty()){
            // Only a real EOF (not manual stop or error) advances. The next
            // provider episode was captured when playback started, so browsing
            // another category while watching cannot change the target.
            const auto next=*next_episode_;
            next_episode_.reset();
            activate(next);
        }else next_episode_.reset();
    });
    refreshContinue();
    // SDL is used ONLY as the existing iPhone remote's event queue; Qt draws
    // every screen and mpv owns the actual playback window.
    sdl_started_=SDL_Init(SDL_INIT_EVENTS|SDL_INIT_TIMER)==0;
    if(sdl_started_){
        xs::RemoteServer::shared().start();
        connect(&remote_timer_,&QTimer::timeout,this,&NovaQtHome::pollRemote);
        remote_timer_.start(45);
        brightness_=xs::RemoteServer::shared().setting_value(xs::remote_setting_index("brightness"));
        updateRemote();
    }else status_=QStringLiteral("Control remoto no disponible.");
}
int NovaQtHome::videoPreset()const{
    return xs::RemoteServer::shared().setting_value(xs::remote_setting_index("video_preset"));
}
QString NovaQtHome::videoPresetLabel()const{
    return QString::fromUtf8(xs::video_preset_label(xs::nova_video_preset(videoPreset())));
}
void NovaQtHome::setVideoPreset(int value){
    const int id=xs::remote_setting_index("video_preset");
    if(!xs::valid_remote_setting(id,value))return;
    if(!xs::RemoteServer::shared().set_setting_value(id,value))return;
    player_.requestQuality(value);
    emit changed();
}
int NovaQtHome::motionMode()const{
    return xs::RemoteServer::shared().setting_value(xs::remote_setting_index("motion_mode"));
}
QString NovaQtHome::motionLabel()const{
    return QString::fromUtf8(xs::nova_motion_label(xs::nova_motion_mode(motionMode())));
}
void NovaQtHome::setMotionMode(int value){
    const int id=xs::remote_setting_index("motion_mode");
    if(!xs::valid_remote_setting(id,value))return;
    if(value==2 && !player_.gpuRifeReady()){
        status_=QStringLiteral("RIFE GPU no instalado. Extrae el complemento RIFE de Nova.");
        emit changed();
        return;
    }
    if(!xs::RemoteServer::shared().set_setting_value(id,value))return;
    player_.requestMotion(value);
    emit changed();
}
NovaQtHome::~NovaQtHome(){
    if(source_request_)source_request_->cancel();
    if(previews_request_)previews_request_->cancel();
    remote_timer_.stop();
    player_.stop();
    if(sdl_started_){
        xs::RemoteServer::shared().stop();
        SDL_Quit();
    }
}

void NovaQtHome::updateRemote(){
    if(!sdl_started_)return;
    const auto info=xs::RemoteServer::shared().info();
    const QString address=QString::fromStdString(info.address);
    const QString pin=QString::fromStdString(info.pin);
    const bool changedInfo=remote_ready_!=info.running||address_!=address||
                          pin_!=pin||remote_connected_!=info.connected;
    if(address_!=address||pin_!=pin)
        qr_=generatePairingQr(info.address,info.pin);
    address_=address;pin_=pin;
    remote_ready_=info.running;
    remote_connected_=info.connected;
    if(changedInfo)emit changed();
}


void NovaQtHome::refreshContinue(){
    continue_entries_.clear();
    for(const auto& recent:saved_.recent){
        if(recent.kind!=xs::EntryKind::Episode||
           playback_history_.position(recent.content_id)<=5.0)continue;
        xs::Entry entry=recent;
        // The saved library deliberately stores metadata, not authenticated
        // streaming URLs. Reattach matching artwork from the active catalog.
        for(const auto& candidate:series_entries_){
            if((candidate.content_id==entry.content_id||candidate.content_id==entry.series_id)&&!candidate.artwork.empty()){
                entry.artwork=candidate.artwork;break;
            }
        }
        for(const auto& candidate:movie_entries_){
            if((candidate.content_id==entry.content_id||candidate.content_id==entry.series_id)&&!candidate.artwork.empty()){
                entry.artwork=candidate.artwork;break;
            }
        }
        continue_entries_.push_back(entry);
        if(continue_entries_.size()>=18)break;
    }
    continue_watching_=listFor(continue_entries_);
    for(int i=0;i<continue_watching_.size();++i){
        auto meta=continue_watching_[i].toMap();
        meta.insert(QStringLiteral("resumeSeconds"),
                    playback_history_.position(continue_entries_[static_cast<size_t>(i)].content_id));
        continue_watching_[i]=meta;
    }
    emit changed();
}

void NovaQtHome::updateFeatured(){
    std::vector<xs::Entry> choices;
    int bestScore=-1;
    for(const auto* group:{&movie_entries_,&series_entries_}){
        for(const auto& item:*group){
            const int score=(item.backdrop.empty()?0:4)+(item.description.empty()?0:2)
                          +(item.artwork.empty()?0:1);
            if(score<bestScore)continue;
            if(score>bestScore){choices.clear();bestScore=score;}
            choices.push_back(item);
        }
    }
    if(choices.empty()&&!channel_entries_.empty())choices.push_back(channel_entries_.front());
    featured_entry_=xs::Entry{};
    featured_backdrop_.clear();
    if(choices.empty()){emit changed();return;}
    const int index=QRandomGenerator::global()->bounded(static_cast<int>(choices.size()));
    featured_entry_=choices[static_cast<size_t>(index)];
    featured_backdrop_=QString::fromStdString(featured_entry_.backdrop.empty()?
        featured_entry_.artwork:featured_entry_.backdrop);
    // Request the original image on TMDb only when this provider URL
    // explicitly supports size variants. Never upscale a low-res thumbnail.
    if(featured_backdrop_.contains(QStringLiteral("image.tmdb.org/t/p/"))){
        featured_backdrop_.replace(
            QRegularExpression(QStringLiteral("/t/p/(w[0-9]+|original)/")),
            QStringLiteral("/t/p/original/"));
    }
    featured_info_visible_=false;
    emit changed();
}

void NovaQtHome::playFeatured(){
    if(featured_entry_.title.empty()||loading_)return;
    const auto chosen=featured_entry_;
    if(chosen.kind!=xs::EntryKind::Asset||!session_){
        activate(chosen);
        return;
    }
    // "Reproducir" should actually play the first available episode/part,
    // instead of merely opening the series directory.
    loading_=true;
    status_=QStringLiteral("Preparando reproducción...");
    emit changed();
    const auto session=session_;
    auto* watcher=new QFutureWatcher<xs::BrowseResult>(this);
    connect(watcher,&QFutureWatcher<xs::BrowseResult>::finished,this,
            [this,watcher,chosen](){
        const auto result=watcher->result();
        loading_=false;
        if(result.ok){
            nova_cache::save(cache_scope_,nova_cache::pageKey(chosen),result.catalog);
            const auto it=std::find_if(result.catalog.entries.begin(),
                result.catalog.entries.end(),[](const xs::Entry& e){
                    return e.kind==xs::EntryKind::Episode||
                           e.kind==xs::EntryKind::Media||
                           e.kind==xs::EntryKind::Channel;
                });
            if(it!=result.catalog.entries.end())playEntry(*it);
            else showCatalog(result.catalog,true);
        }else{
            status_=QString::fromStdString(result.status);
            emit changed();
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([session,chosen](){
        return xs::service_browse_remote(session,chosen);
    }));
}
bool NovaQtHome::playLocalPilotSample(){
    if(!veyraPilotTestAvailable() || player_.playing() || loading_)return false;
    // Existing catalog safety deliberately allows only the fixed test.mp4
    // sample, never arbitrary local paths from a remote playlist.
    const QString local=QDir::current().absoluteFilePath("test.mp4");
    if(!QFileInfo(local).isFile()){
        status_=QStringLiteral("Coloca un video llamado test.mp4 junto al EXE de Nova.");
        emit changed();
        return false;
    }
    xs::SourceResult source;
    source.ok=true;
    source.source="test.mp4";
    active_entry_=xs::Entry{};
    next_episode_.reset();
    player_.play(source,QStringLiteral("Prueba local Veyra"),false,0.0);
    return player_.playing();
}
void NovaQtHome::openDemo(int index){
    if(index<0||static_cast<size_t>(index)>=demo_entries_.size())return;
    activate(demo_entries_[static_cast<size_t>(index)]);
}
void NovaQtHome::openContinue(int index){
    if(index<0||static_cast<size_t>(index)>=continue_entries_.size())return;
    activate(continue_entries_[static_cast<size_t>(index)]);
}

void NovaQtHome::mergePublicChannels(){
    HomeSnapshot merged;
    for(const auto& e:channel_entries_)if(e.program_type!="public-live")merged.tv_entries.push_back(e);
    appendPublicChannels(merged,public_channels_);
    channel_entries_=std::move(merged.tv_entries);
    channels_=tvPreview(channel_entries_);
    catalog_connected_=!channel_entries_.empty()||!root_.entries.empty();
}
void NovaQtHome::refresh(){
    if(refreshing_||loading_)return;
    refreshing_=true;
    const quint64 refresh_epoch=++refresh_epoch_;
    if(previews_request_)previews_request_->cancel();
    previews_request_.reset();
    const auto new_scope=nova_cache::scope();
    if(new_scope!=cache_scope_){
        session_.reset();root_={};page_={};items_.clear();history_.clear();browsing_=false;
        channel_entries_.clear();series_entries_.clear();movie_entries_.clear();series_.clear();movies_.clear();
        mergePublicChannels();updateFeatured();
        xs::RemoteServer::shared().set_session({},root_);
    }
    cache_scope_=new_scope;
    QString stamp;
    if(auto cached=nova_cache::read(cache_scope_,"root",&stamp)){
        root_=*cached;
        auto restore=[this](const QString& key,std::vector<xs::Entry>& entries){
            if(auto page=nova_cache::read(cache_scope_,key))entries=std::move(page->entries);
        };
        restore("tv",channel_entries_);restore("movies",movie_entries_);restore("series",series_entries_);
        series_=listFor(series_entries_);movies_=listFor(movie_entries_);
        status_=QStringLiteral("Catálogo guardado el %1. Actualizando…").arg(stamp);
        service_status_=status_;
        mergePublicChannels();
    }
    emit changed();
    auto* iptv=new QFutureWatcher<PublicPlaylistResult>(this);
    connect(iptv,&QFutureWatcher<PublicPlaylistResult>::finished,this,[this,iptv,refresh_epoch]{
        auto result=iptv->result();iptv->deleteLater();
        if(refresh_epoch!=refresh_epoch_)return;
        public_channels_=std::move(result.catalog);mergePublicChannels();updateFeatured();emit changed();
    });
    iptv->setFuture(QtConcurrent::run(&publicSpanishPlaylist));
    auto* watcher=new QFutureWatcher<HomeSnapshot>(this);
    connect(watcher,&QFutureWatcher<HomeSnapshot>::finished,this,[this,watcher,refresh_epoch](){
        const auto snap=watcher->result();
        service_connected_=snap.service_connected;
        service_status_=snap.service_status;
        demo_items_=snap.demo_items;demo_entries_=snap.demo_entries;
        if(!snap.tv_entries.empty())channel_entries_=snap.tv_entries;
        refreshing_=false;
        session_=snap.session;
        cache_scope_=nova_cache::scope();
        if(!snap.root.entries.empty()){
            root_=snap.root;
            nova_cache::save(cache_scope_,"root",root_);
        }else if(!root_.entries.empty()){
            service_status_+=QStringLiteral(" · Mostrando catálogo guardado.");
        }
        mergePublicChannels();
        updateFeatured();
        refreshContinue();
        xs::RemoteServer::shared().set_session(session_,root_);
        xs::RemoteServer::shared().index_assets(series_entries_);
        xs::RemoteServer::shared().index_assets(movie_entries_);
        if(!loading_&&!browsing_&&!player_.playing())status_=service_status_;
        previews_loading_=bool(session_)&&!snap.root.entries.empty();
        emit changed();
        watcher->deleteLater();
        if(!previews_loading_)return;
        // The full section hierarchy is usable immediately. Only optional
        // home-page tile previews are populated in this second worker.
        const auto expected_session=session_;
        const auto root=root_;
        const auto initial_channels=channel_entries_;
        const auto request=std::make_shared<xs::ServiceRequest>(std::chrono::seconds(90));
        previews_request_=request;
        auto* preview=new QFutureWatcher<HomeSnapshot>(this);
        connect(preview,&QFutureWatcher<HomeSnapshot>::finished,this,
                [this,preview,expected_session,refresh_epoch](){
            const auto next=preview->result();
            preview->deleteLater();
            if(refresh_epoch!=refresh_epoch_ || session_!=expected_session)return; // refresh superseded this result
            previews_loading_=false;
            previews_request_.reset();
            if(!next.service_status.isEmpty())service_status_=next.service_status;
            if(next.previews_complete){
                auto save=[this](const QString& key,const std::vector<xs::Entry>& entries){
                    xs::Catalog page;page.entries=entries;nova_cache::save(cache_scope_,key,page);
                };
                save("tv",next.tv_entries);save("series",next.series_entries);save("movies",next.movie_entries);
            }
            if(next.previews_complete||!next.channels.isEmpty())
                channels_=next.channels;
            if(next.previews_complete||!next.series.isEmpty()){series_=next.series;series_entries_=next.series_entries;}
            if(next.previews_complete||!next.movies.isEmpty()){movies_=next.movies;movie_entries_=next.movie_entries;}
            if(next.previews_complete||!next.tv_entries.empty())channel_entries_=next.tv_entries;
            mergePublicChannels();

            updateFeatured();
            refreshContinue();
            xs::RemoteServer::shared().index_assets(series_entries_);
            xs::RemoteServer::shared().index_assets(movie_entries_);
            if(!browsing_ && !player_.playing() && !loading_)
                status_=next.status;
            emit changed();
        });
        preview->setFuture(QtConcurrent::run(
            [expected_session,root,initial_channels,request](){
                return loadHomePreviews(expected_session,root,initial_channels,request);
            }));
    });
    watcher->setFuture(QtConcurrent::run(&loadHome));
}

void NovaQtHome::showCatalog(xs::Catalog catalog,bool pushHistory){
    if(pushHistory&&browsing_)history_.push_back({page_,selected_item_});
    ++page_epoch_;
    page_=std::move(catalog);
    xs::RemoteServer::shared().index_assets(page_.entries);
    items_=listFor(page_.entries);
    page_title_=QString::fromStdString(page_.title);
    browsing_=true;
    selected_item_=0;
    status_=page_.entries.empty()?QStringLiteral("Este catálogo está vacío."):
                                 QStringLiteral("Selecciona un título.");
    emit changed();
}
void NovaQtHome::openSection(int row){
    if(row<0||row>2||loading_)return;
    selected_section_=row;
    xs::Catalog page;
    page.title=row==0?"TV en vivo":row==1?"Series":"Películas";
    const auto& previews=row==0?channel_entries_:
                         row==1?series_entries_:movie_entries_;
    page.entries=previews;
    // Every existing service folder remains available: TV categories and
    // complete VOD categories are browsable instead of stopping at 18 tiles.
    for(const auto& e:root_.entries){
        if(row==0&&e.kind==xs::EntryKind::Category)
            page.entries.push_back(e);
    }
    if(row==0){history_.clear();showCatalog(std::move(page),false);return;}
    const std::string type=row==1?"0":"1";
    const auto section=std::find_if(root_.entries.begin(),root_.entries.end(),
        [&](const xs::Entry& e){
            return e.kind==xs::EntryKind::Section&&e.program_type==type;
        });
    if(section==root_.entries.end()){
        history_.clear();showCatalog(std::move(page),false);return;
    }
    if(!session_||nova_cache::read(cache_scope_,nova_cache::pageKey(*section))){
        if(!session_&&!nova_cache::read(cache_scope_,nova_cache::pageKey(*section))){
            history_.clear();showCatalog(std::move(page),false);return;
        }
        history_.clear();browsing_=false;browseEntry(*section);return;
    }
    loading_=true;status_=QStringLiteral("Abriendo catálogo completo...");emit changed();
    const auto session=session_;const auto target=*section;
    auto* watcher=new QFutureWatcher<xs::BrowseResult>(this);
    connect(watcher,&QFutureWatcher<xs::BrowseResult>::finished,this,
            [this,watcher,target,page=std::move(page)]()mutable{
        const auto response=watcher->result();
        if(response.ok){
            nova_cache::save(cache_scope_,nova_cache::pageKey(target),response.catalog);
            // Keep actual category folders first; posters remain available.
            xs::Catalog merged=std::move(page);
            for(const auto& e:response.catalog.entries)merged.entries.push_back(e);
            history_.clear();showCatalog(std::move(merged),false);
        }else{
            auto cached=nova_cache::read(cache_scope_,nova_cache::pageKey(target));
            history_.clear();showCatalog(cached?*cached:std::move(page),false);
            status_=QString::fromStdString(response.status);
        }
        loading_=false;emit changed();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([session,target](){
        return xs::service_browse_remote(session,target);
    }));
}

void NovaQtHome::openHomeCard(int row,int index){
    if((loading_&&!source_request_)||index<0)return;
    const auto& entries=row==0?channel_entries_:
                        row==1?series_entries_:movie_entries_;
    if(static_cast<size_t>(index)>=entries.size())return;
    selected_section_=row;
    selected_home_card_=index;
    activate(entries[static_cast<size_t>(index)]);
}
void NovaQtHome::selectGridItem(int index){
    if(!browsing_||page_.entries.empty())return;
    selected_item_=std::clamp(index,0,static_cast<int>(page_.entries.size())-1);
    emit changed();
}
void NovaQtHome::openItem(int index){
    if((loading_&&!source_request_)||!browsing_||index<0||
       static_cast<size_t>(index)>=page_.entries.size())return;
    selected_item_=index;
    emit changed();
    activate(page_.entries[static_cast<size_t>(index)]);
}
void NovaQtHome::activate(const xs::Entry& e){
    if(source_request_){
        if(e.kind==active_entry_.kind&&e.content_id==active_entry_.content_id&&
           e.channel_code==active_entry_.channel_code&&e.series_id==active_entry_.series_id)return;
        source_request_->cancel();source_request_.reset();
        ++source_request_epoch_;loading_=false;
    }
    if(e.kind==xs::EntryKind::Media||e.kind==xs::EntryKind::Channel||
       e.kind==xs::EntryKind::Episode){playEntry(e);return;}
    if(e.kind==xs::EntryKind::Section||e.kind==xs::EntryKind::Asset||
       e.kind==xs::EntryKind::Category||e.kind==xs::EntryKind::VodCategory||
       e.kind==xs::EntryKind::VodPage||e.kind==xs::EntryKind::SearchPage)
        browseEntry(e);
    else{status_=QStringLiteral("Este elemento no tiene reproducción directa.");emit changed();}
}
void NovaQtHome::browseEntry(const xs::Entry& e){
    if(loading_)return;
    const auto key=nova_cache::pageKey(e);
    const auto scope=cache_scope_;
    auto cached=nova_cache::read(scope,key);
    if(cached)showCatalog(*cached,true);
    if(!session_){
        status_=cached?QStringLiteral("Mostrando la copia guardada."):QStringLiteral("Esta sección aún no tiene copia guardada.");
        emit changed();return;
    }
    loading_=!cached.has_value();
    status_=cached?QStringLiteral("Mostrando copia guardada. Actualizando…"):QStringLiteral("Cargando contenido…");
    const auto view=++page_epoch_;
    emit changed();
    const auto session=session_;
    auto* watcher=new QFutureWatcher<xs::BrowseResult>(this);
    connect(watcher,&QFutureWatcher<xs::BrowseResult>::finished,this,
            [this,watcher,e,scope,key,view,was_cached=cached.has_value()](){
        const auto response=watcher->result();
        watcher->deleteLater();
        if(scope!=cache_scope_)return;
        if(response.ok)nova_cache::save(scope,key,response.catalog);
        if(view!=page_epoch_||(was_cached&&loading_))return;
        loading_=false;
        if(response.ok){

            xs::Catalog catalog=response.catalog;
            if(catalog.title.empty())catalog.title=e.title;
            // Keep movie behavior from the existing desktop: a movie page
            // containing a single playable part starts that part.
            if(!was_cached&&e.kind==xs::EntryKind::Asset&&e.program_type=="1"&&
               catalog.entries.size()==1&&catalog.entries[0].kind==xs::EntryKind::Episode){
                playEntry(catalog.entries[0]);
            }else{
                const auto index=selected_item_;
                showCatalog(std::move(catalog),!was_cached);
                if(was_cached&&!page_.entries.empty())selectGridItem(index);
            }
        }else{

            status_=QString::fromStdString(response.status);
        }
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([session,e](){
        return xs::service_browse_remote(session,e);
    }));
}
void NovaQtHome::playEntry(const xs::Entry& e){
    if(player_.playing()||loading_)return;
    ++page_epoch_;
    next_episode_=xs::nova_next_episode(e,page_.entries,playback_history_.auto_next);
    active_entry_=e;
    saved_.remember(e);
    refreshContinue();
    if(e.kind==xs::EntryKind::Media){
        xs::SourceResult source;
        source.ok=xs::valid_source(e.source);
        source.source=e.source;
        if(source.ok){
            player_.play(source,QString::fromStdString(e.title),
                         e.program_type=="public-live",0.0,
                         e.program_type=="public-live");
            status_=player_.notice();
            if(status_.isEmpty())
                status_=player_.playing()
                    ?QStringLiteral("Abriendo canal público...")
                    :QStringLiteral("El reproductor no pudo abrir este canal.");
            emit changed();
        }else{status_=QStringLiteral("Esta lista local no contiene una fuente válida.");emit changed();}
        return;
    }
    if(!session_){status_=QStringLiteral("Para reproducir este título, actualiza la conexión con XuperTV.");emit changed();return;}
    loading_=true;
    status_=QStringLiteral("Pidiendo a XuperTV la fuente de video…");
    emit changed();
    const auto session=session_;
    const quint64 request_epoch=++source_request_epoch_;
    const auto request=std::make_shared<xs::ServiceRequest>();
    source_request_=request;
    auto* progress=new QTimer(this);
    connect(progress,&QTimer::timeout,this,[this,request,progress]{
        if(source_request_!=request){progress->deleteLater();return;}
        if(request->at_video_server()){
            status_=QStringLiteral("Consultando el servidor de video…");
            progress->deleteLater();emit changed();
        }
    });
    progress->start(150);
    auto* watcher=new QFutureWatcher<xs::SourceResult>(this);
    QPointer<QFutureWatcher<xs::SourceResult>> pending=watcher;
    connect(watcher,&QFutureWatcher<xs::SourceResult>::finished,this,
            [this,watcher,e,request_epoch](){
        const auto source=watcher->result();
        watcher->deleteLater();
        // An expired request cannot unexpectedly open a video after
        // the user has already returned to the catalog or retried.
        if(request_epoch!=source_request_epoch_)return;
        source_request_.reset();
        loading_=false;
        if(source.ok&&xs::valid_source(source.source)){
            status_=QStringLiteral("Fuente recibida. Abriendo reproductor…");emit changed();
            player_.play(source,QString::fromStdString(e.title),
                e.kind==xs::EntryKind::Channel,
                e.kind==xs::EntryKind::Episode
                    ?playback_history_.position(e.content_id):0.0);
            // Previously we kept displaying "Solicitando fuente..." even when
            // the request had already returned successfully or MPV failed.
            status_=player_.notice();
            if(status_.isEmpty())
                status_=player_.playing()
                    ?QStringLiteral("Reproductor inicializando...")
                    :QStringLiteral("La fuente llegó, pero el reproductor no inició.");
        }else{
            status_=QString::fromStdString(source.status.empty()
                ?"XuperTV no entregó una fuente reproducible.":source.status);
        }
        emit changed();
    });
    // A provider source may require several chained HTTP requests. Do not
    // freeze the home screen indefinitely; late answers are safely ignored.
    QTimer::singleShot(45000,this,[this,pending,request_epoch](){
        if(request_epoch!=source_request_epoch_ || !pending ||
           pending->isFinished())return;
        ++source_request_epoch_;
        if(source_request_)source_request_->cancel();
        source_request_.reset();
        loading_=false;
        status_=QStringLiteral(
            "XuperTV no entregó la fuente en 45 s. Puedes reintentar; "
            "esto ocurre ANTES de que MPV o Veyra reciban el video.");
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([session,e,request](){
        return xs::service_source_remote(session,e,0,request);
    }));
}

void NovaQtHome::back(){
    if(source_request_){
        source_request_->cancel();source_request_.reset();
        ++source_request_epoch_;loading_=false;
        status_=QStringLiteral("Solicitud de reproducción cancelada.");
        emit changed();return;
    }
    if(player_.playing()){player_.stop();return;}
    if(loading_)return;
    if(!history_.empty()){
        auto prev=std::move(history_.back());
        history_.pop_back();
        showCatalog(std::move(prev.catalog),false);
        selected_item_=prev.selected;
        emit changed();
    }else home();
}
void NovaQtHome::home(){
    if(!loading_||source_request_)++page_epoch_;
    if(source_request_){
        source_request_->cancel();source_request_.reset();
        ++source_request_epoch_;loading_=false;
    }
    if(player_.playing())player_.stop();
    browsing_=false;history_.clear();selected_item_=0;selected_section_=0;
    status_=QStringLiteral("Inicio");
    emit changed();
}
void NovaQtHome::search(const QString& query){
    const QString text=query.trimmed();
    if(text.size()<2||loading_)return;
    if(!session_){
        xs::Catalog results;
        results.title="Resultados: "+text.toStdString();
        for(const auto& e:channel_entries_)
            if(QString::fromStdString(e.title).contains(text,Qt::CaseInsensitive))
                results.entries.push_back(e);
        history_.clear();
        showCatalog(std::move(results),false);
        return;
    }
    loading_=true;status_=QStringLiteral("Buscando en el catálogo...");
    emit changed();
    auto* watcher=new QFutureWatcher<xs::BrowseResult>(this);
    const auto session=session_;
    connect(watcher,&QFutureWatcher<xs::BrowseResult>::finished,this,[this,watcher,text](){
        const auto result=watcher->result();
        loading_=false;
        if(result.ok){
            xs::Catalog page=result.catalog;
            page.title="Resultados: "+text.toStdString();
            history_.clear();
            showCatalog(std::move(page),false);
        }else status_=QString::fromStdString(result.status);
        emit changed();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([session,text](){
        return xs::service_search_remote(session,text.toStdString());
    }));
}
void NovaQtHome::navigate(int dx,int dy){
    if(player_.playing()){if(dx)player_.seek(dx*15);return;}
    emit directionPressed(dx,dy);
}
void NovaQtHome::clickPointer(bool down){
    if(!window_)return;
    const auto& r=xs::RemoteServer::shared();
    const QPointF local(r.cursor_x()*window_->width()/1280.0,
                        r.cursor_y()*window_->height()/720.0);
    const QPointF global=QPointF(window_->mapToGlobal(local.toPoint()));
    QMouseEvent event(down?QEvent::MouseButtonPress:QEvent::MouseButtonRelease,
                      local,global,Qt::LeftButton,
                      down?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(window_,&event);
}
void NovaQtHome::pollRemote(){
    updateRemote();
    const std::string typed=xs::RemoteServer::shared().take_text();
    if(!typed.empty())search(QString::fromUtf8(typed.c_str()));
    SDL_Event event{};
    int count=0;
    while(count++<80&&SDL_PollEvent(&event)){
        if(event.type==SDL_MOUSEMOTION&&window_){
            // The iPhone's trackpad maps its old 1280x720 coordinates into
            // Nova's actual window size and triggers normal QML hover effects.
            const QPoint local(event.motion.x*window_->width()/1280,
                               event.motion.y*window_->height()/720);
            QCursor::setPos(window_->mapToGlobal(local));
        }
        if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP){
            clickPointer(event.type==SDL_MOUSEBUTTONDOWN);continue;
        }
        if(event.type==SDL_USEREVENT){
            if(event.user.code==xs::REMOTE_PLAYBACK_SELECT)
                player_.selectTrack(int(reinterpret_cast<intptr_t>(event.user.data1)),int(reinterpret_cast<intptr_t>(event.user.data2)),event.user.windowID);
            if(event.user.code==xs::REMOTE_VOLUME)
                player_.setVolume(static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1)));
            if(event.user.code==xs::REMOTE_BRIGHTNESS){
                brightness_=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                player_.applySetting(xs::remote_setting_index("brightness"),brightness_);
                emit changed();
            }
            if(event.user.code==xs::REMOTE_SETTING){
                const int id=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                const int value=static_cast<int>(reinterpret_cast<intptr_t>(event.user.data2));
                if(id==xs::remote_setting_index("brightness")){brightness_=value;emit changed();}
                player_.applySetting(id,value);
                if(id==xs::remote_setting_index("video_preset")||
                   id==xs::remote_setting_index("motion_mode"))emit changed();
            }
            if(event.user.code==xs::REMOTE_WHEEL)
                navigate(0,-static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1)));
            if(event.user.code==xs::REMOTE_OPEN_BROWSE||
               event.user.code==xs::REMOTE_OPEN_CATALOG){
                const uint32_t generation=static_cast<uint32_t>(
                    reinterpret_cast<uintptr_t>(event.user.data2));
                const size_t index=static_cast<size_t>(
                    reinterpret_cast<uintptr_t>(event.user.data1));
                xs::Catalog picked;
                auto& remote=xs::RemoteServer::shared();
                const bool valid=event.user.code==xs::REMOTE_OPEN_BROWSE
                    ?remote.selected_browse_result(generation,index,picked)
                    :remote.selected_result(generation,index,picked);
                if(valid&&index<picked.entries.size()){
                    if(player_.playing()){
                        queued_remote_entry_=picked.entries[index];
                        player_.stop();
                    }else activate(picked.entries[index]);
                }
            }
            continue;
        }
        if(event.type!=SDL_KEYDOWN)continue;
        switch(event.key.keysym.sym){
            case SDLK_UP:navigate(0,-1);break;
            case SDLK_DOWN:navigate(0,1);break;
            case SDLK_LEFT:navigate(-1,0);break;
            case SDLK_RIGHT:navigate(1,0);break;
            case SDLK_RETURN:
                if(player_.playing())player_.togglePause();
                else emit selectPressed();
                break;
            case SDLK_b:case SDLK_ESCAPE:
                if(player_.playing())player_.stop();else emit backPressed();break;
            case SDLK_F4:home();break;
            case SDLK_F5:openSection(0);break;
            case SDLK_F6:openSection(2);break;
            case SDLK_F7:openSection(1);break;
            case SDLK_SPACE:player_.togglePause();break;
            case SDLK_F10:
                if(player_.playing())player_.cycleMonitor();
                else nextMonitor(window_,player_.videoWindow());
                break;
            case SDLK_F11:
                if(player_.playing())player_.toggleFullscreen();
                else if(window_)window_->setVisibility(window_->visibility()==QWindow::FullScreen?
                    QWindow::Windowed:QWindow::FullScreen);
                break;
            case SDLK_PAGEDOWN:navigate(1,0);break;
            case SDLK_PAGEUP:navigate(-1,0);break;
            default:break;
        }
    }
}
