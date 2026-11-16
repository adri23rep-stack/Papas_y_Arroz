#include "nova_qt_home.hpp"
#include "catalog.hpp"
#include "service.hpp"
#include <QFutureWatcher>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
struct HomeSnapshot {
    QVariantList channels,series,movies;
    QString status;
};

QVariantMap itemFor(const xs::Entry& e) {
    // Display metadata only: never expose session, tokens or stream URLs.
    const bool usable=e.artwork.rfind("https://",0)==0||
                      e.artwork.rfind("http://",0)==0;
    return {
        {QStringLiteral("title"),QString::fromStdString(e.title)},
        {QStringLiteral("artwork"),usable?QString::fromStdString(e.artwork):QString()},
    };
}

void addEntry(QVariantList& out,const xs::Entry& e,int limit){
    if(out.size()>=limit||e.title.empty())return;
    for(const auto& item:out){
        const auto existing=item.toMap();
        if(existing.value(QStringLiteral("title")).toString()==QString::fromStdString(e.title))
            return;
    }
    out.append(itemFor(e));
}

void addAssets(const xs::Catalog& result,const std::string& type,
               QVariantList& items,int maxItems){
    for(const auto& e:result.entries){
        if(e.kind==xs::EntryKind::Asset&&e.program_type==type)
            addEntry(items,e,maxItems);
    }
}

HomeSnapshot loadHome(){
    HomeSnapshot out;
    auto login=xs::service_bootstrap("service.json");
    auto session=login.session;
    auto root=std::move(login.catalog);
    if(session&&root.entries.empty()){
        const auto browse=xs::service_root(session);
        if(browse.ok)root=browse.catalog;
    }
    if(!session){
        // Use the user's real local M3U as an offline fallback.
        // Never invent movie names, covers, or channel logos.
        std::ifstream file("catalog.m3u");
        if(file){
            const auto local=xs::read_m3u(file);
            for(const auto& e:local.entries)if(e.kind==xs::EntryKind::Media)
                addEntry(out.channels,e,18);
        }
        out.status=QString::fromStdString(login.status);
        if(out.status.isEmpty())out.status=QStringLiteral("No se pudo conectar al catálogo.");
        return out;
    }

    auto live=std::find_if(root.entries.begin(),root.entries.end(),[](const xs::Entry& e){
        return e.kind==xs::EntryKind::Category;
    });
    if(live!=root.entries.end()){
        const auto channels=xs::service_channels(session,live->column_id);
        if(channels.ok)
            for(const auto& e:channels.catalog.entries)
                if(e.kind==xs::EntryKind::Channel)addEntry(out.channels,e,18);
    }

    for(const char* type:{"0","1"}){
        QVariantList& dest=std::string(type)=="0"?out.series:out.movies;
        const auto section=std::find_if(root.entries.begin(),root.entries.end(),
            [&](const xs::Entry& e){
                return e.kind==xs::EntryKind::Section&&e.program_type==type;
            });
        if(section==root.entries.end())continue;
        const auto categories=xs::service_browse(session,*section);
        if(!categories.ok)continue;
        addAssets(categories.catalog,type,dest,18);
        int visited=0;
        for(const auto& e:categories.catalog.entries){
            if(dest.size()>=18||visited>=3)break;
            if(e.kind!=xs::EntryKind::VodCategory)continue;
            ++visited;
            const auto page=xs::service_browse(session,e);
            if(page.ok)addAssets(page.catalog,type,dest,18);
        }
        if(dest.empty())addEntry(dest,*section,18);
    }

    if(out.channels.empty()&&out.series.empty()&&out.movies.empty())
        out.status=QStringLiteral("El catálogo no devolvió contenido para Inicio.");
    else out.status=QStringLiteral("Catálogo cargado");
    return out;
}
} // namespace

NovaQtHome::NovaQtHome(QObject* parent):QObject(parent){}

void NovaQtHome::refresh(){
    if(loading_)return;
    loading_=true;
    status_=QStringLiteral("Cargando canales, series y películas...");
    emit changed();

    auto* watcher=new QFutureWatcher<HomeSnapshot>(this);
    connect(watcher,&QFutureWatcher<HomeSnapshot>::finished,this,[this,watcher](){
        const auto data=watcher->result();
        channels_=data.channels;
        series_=data.series;
        movies_=data.movies;
        status_=data.status;
        loading_=false;
        emit changed();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&loadHome));
}
