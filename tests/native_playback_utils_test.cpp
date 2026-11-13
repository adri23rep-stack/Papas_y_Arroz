#include "native_playback_utils.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
    using namespace xs;
    auto r=native_aspect_fit(1280,720,1920,1080);
    assert(r.x==0&&r.y==0&&r.w==1920&&r.h==1080);
    r=native_aspect_fit(1024,768,1920,1080);
    assert(r.w==1440&&r.h==1080&&r.x==240&&r.y==0);
    r=native_aspect_fit(1920,1080,800,600);
    assert(r.w==800&&r.h==450&&r.y==75);
    r=native_aspect_fit(0,1080,1920,1080);
    assert(r.w==0&&r.h==0);
    assert(native_clamp_seek(-15,150)==0);
    assert(std::abs(native_clamp_seek(200,150)-149.95)<1e-8);
    assert(native_clamp_seek(25,-1)==25);
    assert(native_clamp_seek(NAN,50)==0);
    assert(native_clamp_volume(-1)==0&&native_clamp_volume(900)==100);
    assert(native_pts_to_seconds(90000,1,90000)==1);
    assert(native_is_local_media("test.mp4"));
    assert(native_is_local_media("C:\\Users\\public\\Videos\\test.mp4"));
    assert(!native_is_local_media("https://example.org/video.mp4"));
    assert(!native_is_local_media("\\\\server\\video.mp4"));
    std::cout<<"Native playback geometry, seek and source policy passed.\\n";
    return 0;
}
