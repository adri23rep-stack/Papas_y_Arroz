#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace xs {
struct NativeRect {int x=0,y=0,w=0,h=0;};

inline NativeRect native_aspect_fit(int video_w,int video_h,int output_w,int output_h){
    if(video_w<=0||video_h<=0||output_w<=0||output_h<=0)return {};
    const double scale=std::min(double(output_w)/video_w,double(output_h)/video_h);
    const int w=std::clamp(static_cast<int>(std::lround(video_w*scale)),1,output_w);
    const int h=std::clamp(static_cast<int>(std::lround(video_h*scale)),1,output_h);
    return {(output_w-w)/2,(output_h-h)/2,w,h};
}
inline double native_clamp_seek(double destination,double duration){
    if(!std::isfinite(destination))return 0;
    if(std::isfinite(duration)&&duration>0)return std::clamp(destination,0.0,std::max(0.0,duration-0.05));
    return std::max(0.0,destination);
}
inline int native_clamp_volume(int vol){return std::clamp(vol,0,100);}
inline double native_pts_to_seconds(long long pts,int numerator,int denominator){
    if(denominator<=0||numerator<=0)return 0;
    return static_cast<double>(pts)*numerator/denominator;
}
inline bool native_is_local_media(const std::string& s){
    // Stream URLs must not be sent to the native pilot from the catalog,
    // especially those requiring private service authentication headers.
    if(s.empty())return false;
    if(s.find("://")!=std::string::npos)return false;
    if(s.rfind("\\\\",0)==0)return false; // network UNC shares
    return true;
}
}
