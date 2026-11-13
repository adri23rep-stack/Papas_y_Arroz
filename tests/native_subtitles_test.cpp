#include "native_subtitles.hpp"
#include <cassert>
#include <sstream>
#include <string>
int main(){
    int64_t ms=0;
    assert(xs::native_subtitle_time("00:01:02,345",ms)&&ms==62345);
    assert(xs::native_subtitle_time("01:00:00.123",ms)&&ms==3600123);
    assert(!xs::native_subtitle_time("00:60:04,900",ms));
    assert(!xs::native_subtitle_time("not a time",ms));
    std::istringstream sample(
      "1\r\n00:00:00,400 --> 00:00:01,800\r\nHola mundo\r\n\r\n"
      "2\r\n00:00:01,900 --> 00:00:04,300\r\nSegunda linea\r\nOtra linea\r\n");
    const auto cues=xs::native_parse_srt(sample);
    assert(cues.size()==2);
    assert(xs::native_subtitle_at(cues,0).empty());
    assert(xs::native_subtitle_at(cues,450)=="Hola mundo");
    assert(xs::native_subtitle_at(cues,1850).empty());
    assert(xs::native_subtitle_at(cues,2100)=="Segunda linea\nOtra linea");
    assert(xs::native_subtitle_at(cues,4400).empty());
    return 0;
}
