#pragma once
#include "catalog.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace xs {
struct CinematicHomeContent {
    std::vector<Entry> channels,movies,series;
    // Most recent stored live channel, even when it was in another category.
    Entry last_channel;
    bool has_last=false;
};
inline void remember_last_channel(CinematicHomeContent& home,const std::vector<Entry>& recent){
    home.has_last=false;
    for(const auto& e:recent){
        if(e.kind!=EntryKind::Channel||e.channel_code.empty())continue;
        home.last_channel=e;home.has_last=true;
        // Reuse rich metadata when this channel was fetched in the current
        // session; persistent state deliberately stores no stream credentials.
        for(const auto& live:home.channels)if(live.channel_code==e.channel_code){
            home.last_channel=live;break;
        }
        return;
    }
}
inline Catalog make_cinematic_home(const CinematicHomeContent& home,
                                   const std::vector<Entry>& root,
                                   int& last_index){
    Catalog catalog;catalog.title="Inicio";last_index=-1;
    if(home.has_last&&home.last_channel.kind==EntryKind::Channel){
        last_index=static_cast<int>(catalog.entries.size());
        catalog.entries.push_back(home.last_channel);
    }
    size_t live=0;
    for(const auto& e:home.channels){
        if(e.kind!=EntryKind::Channel)continue;
        if(home.has_last&&e.channel_code==home.last_channel.channel_code)continue;
        catalog.entries.push_back(e);if(++live>=6)break;
    }
    if(live==0){
        Entry tv;tv.title="Ver todos los canales";tv.kind=EntryKind::LiveSection;
        catalog.entries.push_back(std::move(tv));
    }
    auto append=[&](const std::vector<Entry>& source,const char* type,const char* fallback){
        size_t count=0;
        for(const auto& e:source){
            if(e.kind!=EntryKind::Asset||e.program_type!=type)continue;
            catalog.entries.push_back(e);if(++count>=4)break;
        }
        if(count==0){
            const auto found=std::find_if(root.begin(),root.end(),[&](const Entry& e){
                return e.kind==EntryKind::Section&&e.program_type==type;
            });
            if(found!=root.end())catalog.entries.push_back(*found);
            else{Entry e;e.title=fallback;e.kind=EntryKind::Section;e.program_type=type;
                 catalog.entries.push_back(std::move(e));}
        }
    };
    append(home.movies,"1","Explorar películas");
    append(home.series,"0","Explorar series");
    return catalog;
}
}
