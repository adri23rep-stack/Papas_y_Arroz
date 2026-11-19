#pragma once
#include "catalog.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <optional>

namespace nova_cache {
inline QString digest(const QByteArray& bytes){
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
inline QString scope(){
    QFile config("service.json");
    if(!config.open(QIODevice::ReadOnly)||config.size()>65536)return {};
    const auto data=config.readAll();
    if(QJsonDocument::fromJson(data).object().value("installation_serial").toString().isEmpty())return {};
    return QStringLiteral("nova-catalog-v1/")+digest(data+'\0'+qgetenv("NOVA_DEVICE_PROFILE"));
}
inline QString pageKey(const xs::Entry& e){
    QJsonArray key{int(e.kind),e.column_id,QString::fromStdString(e.content_id),
        QString::fromStdString(e.program_type),e.page,QString::fromStdString(e.query)};
    return digest(QJsonDocument(key).toJson(QJsonDocument::Compact));
}
inline bool allowed(int k){
    using K=xs::EntryKind;
    for(auto kind:{K::Category,K::Channel,K::Section,K::VodCategory,K::Asset,K::Episode,K::VodPage})
        if(k==int(kind))return true;
    return false;
}
inline void save(const QString& dir,const QString& key,const xs::Catalog& page){
    if(dir.isEmpty()||page.entries.size()>5000)return;
    QJsonArray rows;
    for(const auto& e:page.entries){
        if(!allowed(int(e.kind)))continue;
        rows.append(QJsonArray{int(e.kind),QString::fromStdString(e.title),e.column_id,
            QString::fromStdString(e.content_id),QString::fromStdString(e.channel_code),
            QString::fromStdString(e.series_id),QString::fromStdString(e.program_type),e.page});
    }
    auto bytes=QJsonDocument(QJsonObject{{"items",rows},{"title",QString::fromStdString(page.title)},
        {"saved",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}}).toJson(QJsonDocument::Compact);
    if(bytes.size()>4*1024*1024||!QDir().mkpath(dir))return;
    QSaveFile file(dir+"/"+key+".json");
    if(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size())file.commit();
}
inline std::optional<xs::Catalog> read(const QString& dir,const QString& key,QString* stamp=nullptr){
    QFile file(dir+"/"+key+".json");
    if(dir.isEmpty()||!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024)return {};
    auto obj=QJsonDocument::fromJson(file.readAll()).object();
    if(!obj.value("items").isArray())return {};
    xs::Catalog page;
    page.title=obj.value("title").toString().toStdString();
    for(const auto& value:obj.value("items").toArray()){
        const auto row=value.toArray();
        if(row.size()!=8||!allowed(row.at(0).toInt(-1)))return {};
        xs::Entry e;e.kind=xs::EntryKind(row.at(0).toInt());e.title=row.at(1).toString().toStdString();
        e.column_id=row.at(2).toInt(-1);e.content_id=row.at(3).toString().toStdString();
        e.channel_code=row.at(4).toString().toStdString();e.series_id=row.at(5).toString().toStdString();
        e.program_type=row.at(6).toString().toStdString();e.page=row.at(7).toInt(1);
        page.entries.push_back(std::move(e));
    }
    if(stamp)*stamp=obj.value("saved").toString();
    return page;
}
}
