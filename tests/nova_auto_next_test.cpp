#include "nova_auto_next.hpp"
#include <cassert>
#include <vector>

static xs::Entry episode(const char* id,const char* series){
    xs::Entry e;
    e.kind=xs::EntryKind::Episode;
    e.content_id=id;e.series_id=series;e.program_type="0";
    return e;
}
int main(){
    const auto first=episode("ep1","s1");
    const auto second=episode("ep2","s1");
    const auto third=episode("ep3","s1");
    const auto other=episode("ep1","s2");
    const std::vector<xs::Entry> page{first,second,third};
    assert(xs::nova_next_episode(first,page,true).has_value());
    assert(xs::nova_next_episode(first,page,true)->content_id=="ep2");
    assert(xs::nova_next_episode(second,page,true)->content_id=="ep3");
    assert(!xs::nova_next_episode(third,page,true)); // last episode
    assert(!xs::nova_next_episode(first,page,false)); // auto-next disabled
    assert(!xs::nova_next_episode(first,{},true)); // empty catalog
    assert(!xs::nova_next_episode(first,{first},true)); // single item
    assert(!xs::nova_next_episode(first,{second,third},true)); // missing source
    assert(!xs::nova_next_episode(first,{first,first,second},true)); // ambiguous duplicate
    assert(!xs::nova_next_episode(first,{first,other},true)); // different series
    assert(xs::nova_next_episode(first,{first,third,second},true)->content_id=="ep3");
    auto movie=first;
    movie.program_type="1";
    assert(!xs::nova_next_episode(movie,{movie,second},true));
    auto missingSeries=first;
    missingSeries.series_id="";
    assert(!xs::nova_next_episode(missingSeries,{missingSeries,second},true));
    auto differentKind=first;
    differentKind.kind=xs::EntryKind::Channel;
    assert(!xs::nova_next_episode(differentKind,{differentKind,second},true));
    xs::Entry gap;
    gap.kind=xs::EntryKind::Asset;
    assert(!xs::nova_next_episode(first,{first,gap,second},true)); // no skip across folders
    auto withoutId=second;withoutId.content_id="";
    assert(!xs::nova_next_episode(first,{first,withoutId},true));
    return 0;
}
