#include "nova_catalog_sources.hpp"
#include <cassert>
#include <string>
#include <iostream>

int main(){
    using namespace xs::nova_sources;
    assert(safe_https("https://example.com/playlist.m3u"));
    assert(!safe_https("http://example.com/playlist.m3u"));
    assert(!safe_https("https://127.0.0.1/private.m3u"));
    assert(!safe_https("https://localhost/private.m3u"));

    Feed live{"test-live","Test channels","https://example.com/a.m3u","live"};
    Feed vod{"test-vod","Test VOD","https://example.com/b.m3u","vod"};
    const std::string body=
        "#EXTM3U\n"
        "#EXTINF:-1 tvg-logo=\"https://example.com/logo.png\" tvg-type=\"live\", Canal Uno\n"
        "https://example.com/live.m3u8\n"
        "#EXTINF:4000 tvg-type=\"movie\" tvg-id=\"ia.movie\" group-title=\"Movies: Classic\",Test Movie\n"
        "https://example.com/movie.mp4\n"
        "#EXTINF:1300 tvg-type=\"series\" series-name=\"Test Show\" season=\"1\" episode=\"2\",Test Show S01E02\n"
        "https://example.com/episode.mp4\n"
        "#EXTINF:1200 tvg-type=\"series\",Without Series Name\n"
        "https://example.com/unknown.mp4\n"
        "#EXTINF:-1,Needs custom headers\n"
        "#EXTVLCOPT:http-user-agent=secret\n"
        "https://example.com/restricted.m3u8\n";
    assert(playlist_shape(body));
    Result a;append_m3u(body,vod,a);
    assert(a.channels.size()==2);
    assert(a.movies.size()==1);
    assert(a.series.size()==1);
    assert(a.channels[0].title=="Canal Uno");
    assert(a.channels[0].program_type=="public-live");
    assert(a.channels[0].artwork=="https://example.com/logo.png");
    assert(a.movies[0].program_type=="public-movie");
    assert(a.series[0].program_type=="public-series");

    Result b;append_m3u(body,live,b);
    assert(b.channels.size()==4);
    assert(b.movies.empty());
    assert(b.series.empty());
    assert(!playlist_shape("<html><body>403</body></html>"));
    assert(!playlist_shape(""));

    b.channels.push_back(b.channels[0]);
    dedupe(b.channels,100);
    assert(b.channels.size()==4);
    std::cout<<"NOVA_CATALOG_SOURCES_TEST_OK\n";
}
