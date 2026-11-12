#include "youtube_remote.hpp"
#include <cassert>
#include <string>
int main(){
    using xs::youtube_results_url;
    using xs::streaming_url;
    assert(streaming_url("netflix_open")=="https://www.netflix.com/");
    assert(streaming_url("hbo_max_open")=="https://www.hbomax.com/");
    assert(streaming_url("apple_tv_open")=="https://tv.apple.com/");
    assert(streaming_url("shell_command").empty());
    assert(youtube_results_url("gatos divertidos")==
        "https://www.youtube.com/results?search_query=gatos%20divertidos");
    assert(youtube_results_url("musica & cine")==
        "https://www.youtube.com/results?search_query=musica%20%26%20cine");
    assert(youtube_results_url("C++")==
        "https://www.youtube.com/results?search_query=C%2B%2B");
    assert(youtube_results_url("niños")==
        "https://www.youtube.com/results?search_query=ni%C3%B1os");
    assert(youtube_results_url("").empty());
    assert(youtube_results_url(" ").empty());
    assert(youtube_results_url("x").empty());
    assert(youtube_results_url("canal\n").empty());
    assert(youtube_results_url(std::string(121,'a')).empty());
    return 0;
}
