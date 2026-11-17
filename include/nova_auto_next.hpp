#pragma once
#include "catalog.hpp"
#include <optional>
#include <vector>

// Never guess the next episode from a title, poster, or search result.
// Use only adjacent episodes in the SAME provider-supplied series page.
// The provider's order is authoritative; ambiguous/mixed pages stop here.
namespace xs {
inline std::optional<Entry> nova_next_episode(
    const Entry& current, const std::vector<Entry>& ordered, bool autoNext){
    if(!autoNext || current.kind!=EntryKind::Episode ||
       current.program_type!="0" || current.content_id.empty() ||
       current.series_id.empty())return std::nullopt;
    std::size_t currentIndex=ordered.size();
    for(std::size_t i=0;i<ordered.size();++i){
        const Entry& item=ordered[i];
        if(item.kind==EntryKind::Episode && item.content_id==current.content_id &&
           item.series_id==current.series_id){
            if(currentIndex!=ordered.size())return std::nullopt; // ambiguous source
            currentIndex=i;
        }
    }
    if(currentIndex>=ordered.size()-1)return std::nullopt;
    const Entry& next=ordered[currentIndex+1];
    if(next.kind!=EntryKind::Episode || next.program_type!="0" ||
       next.series_id!=current.series_id || next.content_id.empty() ||
       next.content_id==current.content_id)return std::nullopt;
    return next;
}
}
