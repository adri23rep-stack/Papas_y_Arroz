#pragma once
#include <array>
#include <cctype>
#include <string>
namespace xs {
// Only label catalog groups actually returned by the authenticated service.
// These names DO NOT imply official integration with any streaming platform.
struct ProviderName {const char* key;const char* title;};
inline constexpr std::array<ProviderName,13> kCatalogProviders{{
    {"netflix","Netflix"},
    {"hbo_max","HBO Max"},
    {"apple_tv","Apple TV"},
    {"youtube","YouTube"},
    {"disney","Disney+"},
    {"prime_video","Prime Video"},
    {"paramount","Paramount+"},
    {"vix","ViX"},
    {"crunchyroll","Crunchyroll"},
    {"pluto_tv","Pluto TV"},
    {"hulu","Hulu"},
    {"peacock","Peacock"},
    {"mubi","MUBI"}
}};
inline std::string normalize_provider_label(const std::string& raw){
    std::string normal;normal.reserve(raw.size()+2);
    bool blank=true;
    for(unsigned char c:raw){
        if(c<128&&std::isalnum(c)){
            normal.push_back(static_cast<char>(std::tolower(c)));
            blank=false;
        }else if(!blank){
            normal.push_back(' ');
            blank=true;
        }
    }
    while(!normal.empty()&&normal.back()==' ')normal.pop_back();
    return normal;
}
inline bool contains_provider_phrase(const std::string& normalized,const char* phrase){
    const std::string haystack=" "+normalized+" ";
    const std::string needle=" "+std::string(phrase)+" ";
    return haystack.find(needle)!=std::string::npos;
}
inline std::string provider_for_catalog_label(const std::string& label){
    const auto s=normalize_provider_label(label);
    if(contains_provider_phrase(s,"netflix"))return "netflix";
    if(contains_provider_phrase(s,"hbo max")||
       contains_provider_phrase(s,"hbo")||
       contains_provider_phrase(s,"max"))return "hbo_max";
    if(contains_provider_phrase(s,"apple tv")||contains_provider_phrase(s,"appletv"))return "apple_tv";
    if(contains_provider_phrase(s,"youtube")||contains_provider_phrase(s,"you tube"))return "youtube";
    if(contains_provider_phrase(s,"disney"))return "disney";
    if(contains_provider_phrase(s,"amazon prime")||
       contains_provider_phrase(s,"prime video")||
       contains_provider_phrase(s,"primevideo"))return "prime_video";
    if(contains_provider_phrase(s,"paramount"))return "paramount";
    if(contains_provider_phrase(s,"vix"))return "vix";
    if(contains_provider_phrase(s,"crunchyroll"))return "crunchyroll";
    if(contains_provider_phrase(s,"pluto tv")||contains_provider_phrase(s,"plutotv"))return "pluto_tv";
    if(contains_provider_phrase(s,"hulu"))return "hulu";
    if(contains_provider_phrase(s,"peacock"))return "peacock";
    if(contains_provider_phrase(s,"mubi"))return "mubi";
    return {};
}
inline const char* provider_title(const std::string& key){
    for(const auto& item:kCatalogProviders)if(key==item.key)return item.title;
    return nullptr;
}
}
