#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <istream>
#include <string>
#include <vector>

namespace xs {
struct NativeSubtitleCue {
    int64_t start_ms=0;
    int64_t end_ms=0;
    std::string text;
};
inline bool native_subtitle_time(const std::string& s,int64_t& milliseconds){
    int h=-1,m=-1,seconds=-1,millis=-1;
    char separator=0;
    int chars=0;
    if(std::sscanf(s.c_str(),"%d:%d:%d%c%d%n",
                   &h,&m,&seconds,&separator,&millis,&chars)!=5
       ||chars!=static_cast<int>(s.size())
       ||h<0||h>99||m<0||m>59||seconds<0||seconds>59
       ||millis<0||millis>999||(separator!=','&&separator!='.'))return false;
    milliseconds=((h*60LL+m)*60+seconds)*1000+millis;
    return true;
}
inline std::string native_subtitle_trim(std::string s){
    while(!s.empty()&&std::isspace(static_cast<unsigned char>(s.front())))s.erase(s.begin());
    while(!s.empty()&&std::isspace(static_cast<unsigned char>(s.back())))s.pop_back();
    return s;
}
inline std::vector<NativeSubtitleCue> native_parse_srt(std::istream& input,size_t limit=20000){
    std::vector<NativeSubtitleCue> out;
    std::string line;
    bool found_timing=false;
    NativeSubtitleCue current;
    auto commit=[&](){
        if(found_timing&&current.end_ms>current.start_ms&&!current.text.empty())
            out.push_back(current);
        current={};
        found_timing=false;
    };
    size_t lines=0;
    while(std::getline(input,line)&&out.size()<limit&&++lines<200000){
        if(line.size()>4096)continue;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(lines==1&&line.rfind("\xef\xbb\xbf",0)==0)line.erase(0,3);
        const std::string trimmed=native_subtitle_trim(line);
        if(trimmed.empty()){commit();continue;}
        const auto arrow=trimmed.find("-->");
        if(arrow!=std::string::npos){
            if(found_timing)commit();
            const std::string start=native_subtitle_trim(trimmed.substr(0,arrow));
            std::string end=native_subtitle_trim(trimmed.substr(arrow+3));
            const auto trailing=end.find_first_of(" \t");
            if(trailing!=std::string::npos)end.resize(trailing);
            int64_t a=0,b=0;
            if(native_subtitle_time(start,a)&&native_subtitle_time(end,b)&&b>a){
                current.start_ms=a;
                current.end_ms=b;
                found_timing=true;
            }
        }else if(found_timing&&current.text.size()+trimmed.size()<800){
            if(!current.text.empty())current.text+="\n";
            current.text+=trimmed;
        }
    }
    if(out.size()<limit)commit();
    std::stable_sort(out.begin(),out.end(),[](const auto& a,const auto& b){
        return a.start_ms<b.start_ms;
    });
    return out;
}
inline std::vector<NativeSubtitleCue> native_read_srt(const std::string& path){
    std::ifstream file(path,std::ios::binary);
    if(!file)return {};
    return native_parse_srt(file);
}
inline std::string native_subtitle_at(const std::vector<NativeSubtitleCue>& cues,int64_t position_ms){
    if(cues.empty())return {};
    auto it=std::upper_bound(cues.begin(),cues.end(),position_ms,
        [](int64_t t,const NativeSubtitleCue& c){return t<c.start_ms;});
    if(it==cues.begin())return {};
    --it;
    return (position_ms>=it->start_ms&&position_ms<it->end_ms)?it->text:std::string{};
}
}
