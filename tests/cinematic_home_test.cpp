#include "cinematic_home.hpp"
#include <cassert>
int main(){
    xs::CinematicHomeContent data;
    xs::Entry news;news.kind=xs::EntryKind::Channel;news.channel_code="news01";news.title="Noticias";news.artwork="https://img.test/logo.png";data.channels.push_back(news);
    xs::Entry film;film.kind=xs::EntryKind::Asset;film.program_type="1";film.title="Pelicula 1";data.movies.push_back(film);
    xs::Entry series;series.kind=xs::EntryKind::Asset;series.program_type="0";series.title="Serie 1";data.series.push_back(series);
    xs::Entry prior=news;prior.artwork.clear();
    xs::remember_last_channel(data,{prior});
    assert(data.has_last&&data.last_channel.title=="Noticias");
    assert(data.last_channel.artwork=="https://img.test/logo.png");
    int last=-1;auto home=xs::make_cinematic_home(data,{},last);
    assert(last==0&&home.entries.size()==4); // last, TV entry, movie, series
    assert(home.entries[0].kind==xs::EntryKind::Channel);
    assert(home.entries[1].kind==xs::EntryKind::LiveSection);
    assert(home.entries[2].title=="Pelicula 1"&&home.entries[3].title=="Serie 1");
    xs::CinematicHomeContent empty;auto fallback=xs::make_cinematic_home(empty,{},last);
    assert(last==-1&&fallback.entries.size()==3);
    assert(fallback.entries[0].kind==xs::EntryKind::LiveSection);
    return 0;
}
