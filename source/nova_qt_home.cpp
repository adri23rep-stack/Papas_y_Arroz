#include "nova_qt_home.hpp"
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
#include <QCoreApplication>
#include <QCursor>
#include <QFutureWatcher>
#include <QGuiApplication>
#include "nova_quality.hpp"
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
#include <vector>

namespace {
BOOL CALLBACK findVideoWindow(HWND window,LPARAM output){
    DWORD pid=0;
    GetWindowThreadProcessId(window,&pid);
    wchar_t name[64]{};
    if(pid==GetCurrentProcessId()&&IsWindowVisible(window)&&
       GetClassNameW(window,name,64)&&std::wcscmp(name,L"mpv")==0){
        *reinterpret_cast<HWND*>(output)=window;
        return FALSE;
    }
    return TRUE;
}
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
void nextMonitor(QQuickWindow* home){
    if(!home)return;
    HWND video=nullptr;
    EnumWindows(findVideoWindow,reinterpret_cast<LPARAM>(&video));
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
};
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
    out.root=std::move(login.catalog);
    if(out.session&&out.root.entries.empty()){
        const auto response=xs::service_root(out.session);
        if(response.ok)out.root=response.catalog;
        else out.status=QString::fromStdString(response.status);
    }
    if(!out.session){
        out.status=QString::fromStdString(login.status);
        if(out.status.isEmpty())out.status=QStringLiteral("Sin conexión al servicio.");
        return out;
    }
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
        if(checkedLive++>=6||out.tv_entries.size()>=72)break;
        const auto response=xs::service_channels(out.session,category.column_id);
        if(response.ok){
            for(const auto& e:response.catalog.entries)
                if(e.kind==xs::EntryKind::Channel)addEntry(out.tv_entries,e,72);
        }else if(out.status.isEmpty())out.status=QString::fromStdString(response.status);
    }
    for(const char* type:{"0","1"}){
        auto& dest=std::string(type)=="0"?out.series_entries:out.movie_entries;
        const auto section=std::find_if(out.root.entries.begin(),out.root.entries.end(),
            [&](const xs::Entry& e){
                return e.kind==xs::EntryKind::Section&&e.program_type==type;
            });
        if(section==out.root.entries.end())continue;
        const auto response=xs::service_browse(out.session,*section);
        if(!response.ok)continue;
        addAssets(response.catalog,type,dest,60);
        int visited=0;
        for(const auto& e:response.catalog.entries){
            if(dest.size()>=60||visited>=5)break;
            if(e.kind!=xs::EntryKind::VodCategory)continue;
            ++visited;
            const auto page=xs::service_browse(out.session,e);
            if(page.ok)addAssets(page.catalog,type,dest,60);
        }
    }
    out.channels=listFor(out.tv_entries);
    out.series=listFor(out.series_entries);
    out.movies=listFor(out.movie_entries);
    if(out.channels.empty()&&out.series.empty()&&out.movies.empty()){
        if(out.status.isEmpty())out.status=QString::fromStdString(login.status);
        if(out.status.isEmpty())out.status=QStringLiteral("Conectado, pero Inicio todavía no tiene contenido. Prueba las categorías de TV, Series o Películas.");
    }else out.status=QStringLiteral("Catálogo conectado.");
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
        if(!active_entry_.content_id.empty()){
            playback_history_.save(active_entry_.content_id,seconds,
                playback_history_.speed,playback_history_.volume,
                playback_history_.auto_next,ended);
        }
        refreshContinue();
        if(queued_remote_entry_){
            const auto entry=*queued_remote_entry_;
            queued_remote_entry_.reset();
            activate(entry);
        }
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
NovaQtHome::~NovaQtHome(){
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
void NovaQtHome::openDemo(int index){
    if(index<0||static_cast<size_t>(index)>=demo_entries_.size())return;
    activate(demo_entries_[static_cast<size_t>(index)]);
}
void NovaQtHome::openContinue(int index){
    if(index<0||static_cast<size_t>(index)>=continue_entries_.size())return;
    activate(continue_entries_[static_cast<size_t>(index)]);
}

void NovaQtHome::refresh(){
    if(loading_)return;
    loading_=true;
    status_=QStringLiteral("Cargando canales, series y películas...");
    emit changed();
    auto* watcher=new QFutureWatcher<HomeSnapshot>(this);
    connect(watcher,&QFutureWatcher<HomeSnapshot>::finished,this,[this,watcher](){
        const auto snap=watcher->result();
        channels_=snap.channels;series_=snap.series;movies_=snap.movies;
        demo_items_=snap.demo_items;demo_entries_=snap.demo_entries;
        catalog_connected_=snap.session!=nullptr && (
            !snap.root.entries.empty() || !snap.channels.isEmpty() ||
            !snap.series.isEmpty() || !snap.movies.isEmpty());
        channel_entries_=snap.tv_entries;
        series_entries_=snap.series_entries;
        movie_entries_=snap.movie_entries;
        session_=snap.session;root_=snap.root;
        updateFeatured();
        refreshContinue();
        xs::RemoteServer::shared().set_session(session_,root_);
        xs::RemoteServer::shared().index_assets(series_entries_);
        xs::RemoteServer::shared().index_assets(movie_entries_);
        status_=snap.status;
        loading_=false;
        emit changed();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&loadHome));
}

void NovaQtHome::showCatalog(xs::Catalog catalog,bool pushHistory){
    if(pushHistory&&browsing_)history_.push_back({page_,selected_item_});
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
    if(row==0||!session_){history_.clear();showCatalog(std::move(page),false);return;}
    const std::string type=row==1?"0":"1";
    const auto section=std::find_if(root_.entries.begin(),root_.entries.end(),
        [&](const xs::Entry& e){
            return e.kind==xs::EntryKind::Section&&e.program_type==type;
        });
    if(section==root_.entries.end()){
        history_.clear();showCatalog(std::move(page),false);return;
    }
    loading_=true;status_=QStringLiteral("Abriendo catálogo completo...");emit changed();
    const auto session=session_;const auto target=*section;
    auto* watcher=new QFutureWatcher<xs::BrowseResult>(this);
    connect(watcher,&QFutureWatcher<xs::BrowseResult>::finished,this,
            [this,watcher,page=std::move(page)]()mutable{
        const auto response=watcher->result();
        if(response.ok){
            // Keep actual category folders first; posters remain available.
            xs::Catalog merged=std::move(page);
            for(const auto& e:response.catalog.entries)merged.entries.push_back(e);
            history_.clear();showCatalog(std::move(merged),false);
        }else{
            history_.clear();showCatalog(std::move(page),false);
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
    if(loading_||index<0)return;
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
    if(loading_||!browsing_||index<0||
       static_cast<size_t>(index)>=page_.entries.size())return;
    selected_item_=index;
    emit changed();
    activate(page_.entries[static_cast<size_t>(index)]);
}
void NovaQtHome::activate(const xs::Entry& e){
    if(e.kind==xs::EntryKind::Media||e.kind==xs::EntryKind::Channel||
       e.kind==xs::EntryKind::Episode){playEntry(e);return;}
    if(e.kind==xs::EntryKind::Section||e.kind==xs::EntryKind::Asset||
       e.kind==xs::EntryKind::Category||e.kind==xs::EntryKind::VodCategory||
       e.kind==xs::EntryKind::VodPage||e.kind==xs::EntryKind::SearchPage)
        browseEntry(e);
    else{status_=QStringLiteral("Este elemento no tiene reproducción directa.");emit changed();}
}
void NovaQtHome::browseEntry(const xs::Entry& e){
    if(!session_){status_=QStringLiteral("No hay sesión para abrir esta sección.");emit changed();return;}
    if(loading_)return;
    loading_=true;status_=QStringLiteral("Cargando contenido...");
    emit changed();
    const auto session=session_;
    auto* watcher=new QFutureWatcher<xs::BrowseResult>(this);
    connect(watcher,&QFutureWatcher<xs::BrowseResult>::finished,this,
            [this,watcher,e](){
        const auto response=watcher->result();
        loading_=false;
        if(response.ok){
            xs::Catalog catalog=response.catalog;
            if(catalog.title.empty())catalog.title=e.title;
            // Keep movie behavior from the existing desktop: a movie page
            // containing a single playable part starts that part.
            if(e.kind==xs::EntryKind::Asset&&e.program_type=="1"&&
               catalog.entries.size()==1&&catalog.entries[0].kind==xs::EntryKind::Episode){
                playEntry(catalog.entries[0]);
            }else showCatalog(std::move(catalog),true);
        }else{
            status_=QString::fromStdString(response.status);
        }
        emit changed();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([session,e](){
        return e.kind==xs::EntryKind::Category
            ?xs::service_channels(session,e.column_id)
            :xs::service_browse_remote(session,e);
    }));
}
void NovaQtHome::playEntry(const xs::Entry& e){
    if(player_.playing()||loading_)return;
    active_entry_=e;
    saved_.remember(e);
    refreshContinue();
    if(e.kind==xs::EntryKind::Media){
        xs::SourceResult source;
        source.ok=xs::valid_source(e.source);
        source.source=e.source;
        if(source.ok)player_.play(source,QString::fromStdString(e.title),false);
        else{status_=QStringLiteral("Esta lista local no contiene una fuente válida.");emit changed();}
        return;
    }
    if(!session_){status_=QStringLiteral("No existe una sesión de video.");emit changed();return;}
    loading_=true;
    status_=QStringLiteral("Solicitando fuente de reproducción...");
    emit changed();
    const auto session=session_;
    auto* watcher=new QFutureWatcher<xs::SourceResult>(this);
    connect(watcher,&QFutureWatcher<xs::SourceResult>::finished,this,[this,watcher,e](){
        const auto source=watcher->result();
        loading_=false;
        if(source.ok&&xs::valid_source(source.source))
            player_.play(source,QString::fromStdString(e.title),
                e.kind==xs::EntryKind::Channel,
                e.kind==xs::EntryKind::Episode
                    ?playback_history_.position(e.content_id):0.0);
        else
            status_=QString::fromStdString(source.status.empty()?
                "El servicio no devolvió un video reproducible.":source.status);
        emit changed();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([session,e](){
        return xs::service_source_remote(session,e,0);
    }));
}

void NovaQtHome::back(){
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
    if(player_.playing())player_.stop();
    browsing_=false;history_.clear();selected_item_=0;selected_section_=0;
    status_=QStringLiteral("Inicio");
    emit changed();
}
void NovaQtHome::search(const QString& query){
    const QString text=query.trimmed();
    if(text.size()<2||!session_||loading_)return;
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
                if(id==xs::remote_setting_index("video_preset"))emit changed();
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
            case SDLK_F10:nextMonitor(window_);break;
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
