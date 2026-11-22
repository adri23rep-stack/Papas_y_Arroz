#pragma once
// Nova's independent public catalog providers. Header-only intentionally:
// no changes to the legacy Switch/SDL targets or their build dependencies.
#include "catalog.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cctype>

namespace xs {
namespace nova_sources {

struct Feed {
    std::string id, name, url, kind;
};
struct Result {
    std::vector<Entry> channels, movies, series;
    size_t fresh=0, cached=0, failed=0;
};
inline std::string trim(std::string s) {
    const auto b=s.find_first_not_of(" \t\r\n");
    return b==std::string::npos?std::string{}:
        s.substr(b,s.find_last_not_of(" \t\r\n")-b+1);
}
inline std::string attribute(const std::string& line,const std::string& name) {
    // Exact attribute boundary; don't match foo-tvg-type by accident.
    const auto needle=name+"=\"";
    size_t at=line.find(needle);
    while(at!=std::string::npos && at && line[at-1]!=' ' && line[at-1]!='\t')
        at=line.find(needle,at+1);
    if(at==std::string::npos)return {};
    const size_t begin=at+needle.size(),end=line.find('"',begin);
    return end==std::string::npos?std::string{}:line.substr(begin,end-begin);
}
inline std::string title_of(const std::string& ext) {
    bool quoted=false;
    for(size_t i=8;i<ext.size();++i){
        if(ext[i]=='"')quoted=!quoted;
        else if(ext[i]==','&&!quoted)return trim(ext.substr(i+1));
    }
    return attribute(ext,"tvg-name");
}
inline bool safe_https(const std::string& url) {
    // Configuration cannot point at localhost or private networks.
    if(url.rfind("https://",0)!=0 || !valid_source(url))return false;
    const auto begin=size_t{8};
    const auto end=url.find_first_of("/?#",begin);
    const auto host=url.substr(begin,end==std::string::npos?end:end-begin);
    if(host=="localhost"||host.size()>240||host.find(':')!=std::string::npos)return false;
    if(host=="127.0.0.1"||host=="0.0.0.0"||host=="169.254.169.254")return false;
    // Only DNS names; no IP literals or embedded credentials.
    return host.find('.')!=std::string::npos &&
        std::any_of(host.begin(),host.end(),[](unsigned char c){return std::isalpha(c)!=0;});
}
inline std::vector<Feed> feeds() {
    std::vector<Feed> out{
        {"iptv-es","IPTV-org español","https://iptv-org.github.io/iptv/languages/spa.m3u","live"},
        {"iptv-mx","IPTV-org México","https://iptv-org.github.io/iptv/countries/mx.m3u","live"},
        {"nexus-es","IPTV Nexus español","https://dearbulut.github.io/iptv/playlists/language/spa.m3u","live"},
        {"nexus-mx","IPTV Nexus México","https://dearbulut.github.io/iptv/playlists/country/mx.m3u","live"},
        {"legal-live","LegalFreeIPTV","https://biskyt.github.io/legalFreeIPTVplaylist/live.m3u","live"},
        {"legal-vod","LegalFreeIPTV VOD","https://biskyt.github.io/legalFreeIPTVplaylist/vod.m3u","vod"},
        {"nexus-movies","IPTV Nexus películas libres","https://dearbulut.github.io/iptv/playlists/vod/movies.m3u","movie"},
        {"nexus-series","IPTV Nexus series libres","https://dearbulut.github.io/iptv/playlists/vod/series.m3u","series"}
    };
    // Additional compatible feeds can be installed without releasing another
    // Nova binary. Format: unique-id|Display name|https://host/playlist.m3u|live/vod/mixed/movie/series
    std::ifstream extra("nova_sources.txt");
    std::string line;
    while(out.size()<16 && std::getline(extra,line)){
        line=trim(line);
        if(line.empty()||line[0]=='#'||line.size()>1200)continue;
        std::array<std::string,4> fields;
        std::istringstream in(line);
        bool ok=true;
        for(auto& value:fields)
            if(!std::getline(in,value,'|')){ok=false;break;}
            else value=trim(value);
        if(!ok||fields[0].empty()||fields[0].size()>60||fields[1].empty()||
           fields[1].size()>80||!safe_https(fields[2])||
           (fields[3]!="live"&&fields[3]!="vod"&&fields[3]!="mixed"&&fields[3]!="movie"&&fields[3]!="series"))continue;
        if(std::any_of(out.begin(),out.end(),[&](const Feed& f){return f.id==fields[0];}))continue;
        out.push_back({fields[0],fields[1],fields[2],fields[3]});
    }
    return out;
}
inline bool playlist_shape(const std::string& data) {
    const auto s=trim(data.substr(0,std::min<size_t>(data.size(),256)));
    return s.rfind("#EXTM3U",0)==0 && data.find("#EXTINF:")!=std::string::npos;
}
inline void append_m3u(const std::string& data,const Feed& feed,Result& result) {
    if(data.size()>8*1024*1024||!playlist_shape(data))return;
    std::istringstream in(data);
    std::string line,ext;bool unsupported=false;
    size_t parsed=0;
    while(std::getline(in,line)&&parsed<6000){
        line=trim(line);
        if(line.rfind("#EXTINF:",0)==0){ext=line;unsupported=false;continue;}
        if(line.empty())continue;
        if(line[0]=='#'){
            if(line.rfind("#EXTVLCOPT:",0)==0||line.rfind("#KODIPROP:",0)==0)
                unsupported=true; // Headers would be required to play this.
            continue;
        }
        if(ext.empty()||unsupported||line.find('|')!=std::string::npos||
           !valid_source(line)){ext.clear();continue;}
        Entry entry;
        entry.title=title_of(ext);
        if(entry.title.empty())entry.title=attribute(ext,"tvg-name");
        if(entry.title.empty()||entry.title.size()>180){ext.clear();continue;}
        entry.source=line;
        entry.kind=EntryKind::Media;
        entry.artwork=attribute(ext,"tvg-logo");
        if(entry.artwork.size()>1800||!valid_source(entry.artwork))entry.artwork.clear();
        auto type=attribute(ext,"tvg-type");
        if(feed.kind=="movie")type="movie";
        if(feed.kind=="series")type="series";
        auto& target=type=="movie" && feed.kind!="live"?result.movies:
                     type=="series" && feed.kind!="live" &&
                     (feed.kind=="series"||!attribute(ext,"series-name").empty())
                     ?result.series:result.channels;
        if(type=="movie" && feed.kind!="live")
            entry.program_type="public-movie";
        else if(type=="series" && feed.kind!="live" &&
                     (feed.kind=="series"||!attribute(ext,"series-name").empty()))
            entry.program_type="public-series";
        else entry.program_type="public-live";
        entry.description="Fuente: "+feed.name;
        const auto group=attribute(ext,"group-title");
        if(!group.empty())entry.description+=" · "+group.substr(0,100);
        const auto imdb=attribute(ext,"imdb-id");
        const auto id=attribute(ext,"tvg-id");
        entry.content_id=!imdb.empty()?imdb:id;
        target.push_back(std::move(entry));
        ++parsed;ext.clear();
    }
}
struct Download {
    Feed feed;
    std::string data;
    bool cached=false;
};
inline size_t write_data(char* p,size_t s,size_t n,void* ctx) {
    auto& data=*static_cast<std::string*>(ctx);
    if(s && n>(8*1024*1024)/s)return 0;
    const auto count=s*n;
    if(count>8*1024*1024-data.size())return 0;
    data.append(p,count);
    return count;
}
inline Download download(const Feed& feed) {
    Download out;out.feed=feed;
    const auto cache="nova_cache_"+feed.id+".m3u";
    CURL* curl=curl_easy_init();
    if(curl){
        curl_easy_setopt(curl,CURLOPT_URL,feed.url.c_str());
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_data);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&out.data);
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,12L);
        curl_easy_setopt(curl,CURLOPT_ACCEPT_ENCODING,"");
        curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
        curl_easy_setopt(curl,CURLOPT_MAXREDIRS,2L);
        curl_easy_setopt(curl,CURLOPT_USERAGENT,"NovaQt/1.0 public catalog");
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
        curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
        curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"https");
#else
        curl_easy_setopt(curl,CURLOPT_PROTOCOLS,CURLPROTO_HTTPS);
        curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS,CURLPROTO_HTTPS);
#endif
        std::ifstream ca("cacert.pem",std::ios::binary);
        if(ca.good())curl_easy_setopt(curl,CURLOPT_CAINFO,"cacert.pem");
        const auto code=curl_easy_perform(curl);
        long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
        curl_easy_cleanup(curl);
        if(code==CURLE_OK&&status==200&&playlist_shape(out.data)){
            std::ofstream file(cache,std::ios::binary|std::ios::trunc);
            if(file)file.write(out.data.data(),static_cast<std::streamsize>(out.data.size()));
            return out;
        }
    }
    out.data.clear();
    std::ifstream file(cache,std::ios::binary|std::ios::ate);
    if(!file||file.tellg()<=0||file.tellg()>8*1024*1024)return out;
    out.data.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    if(!file.read(&out.data[0],static_cast<std::streamsize>(out.data.size()))||
       !playlist_shape(out.data)){out.data.clear();return out;}
    out.cached=true;
    return out;
}
inline void dedupe(std::vector<Entry>& group,size_t limit) {
    std::unordered_set<std::string> seen;
    std::vector<Entry> kept;kept.reserve(std::min(limit,group.size()));
    for(auto& e:group){
        // A channel with a different URL can be a useful alternative even
        // if the same broadcaster appears in multiple sources.
        const auto key=search_key(e.title);
        if(key.empty()||!seen.insert(key).second)continue;
        kept.push_back(std::move(e));
        if(kept.size()>=limit)break;
    }
    group=std::move(kept);
}
inline Result load() {
    Result result;
    std::vector<std::future<Download>> jobs;
    for(const auto& feed:feeds())
        jobs.emplace_back(std::async(std::launch::async,[feed]{return download(feed);}));
    for(auto& job:jobs){
        auto source=job.get();
        if(source.data.empty()){++result.failed;continue;}
        const auto before=result.channels.size()+result.movies.size()+result.series.size();
        append_m3u(source.data,source.feed,result);
        const auto after=result.channels.size()+result.movies.size()+result.series.size();
        if(after==before){++result.failed;continue;}
        if(source.cached)++result.cached;else ++result.fresh;
    }
    dedupe(result.channels,5000);
    dedupe(result.movies,1800);
    dedupe(result.series,2400);
    return result;
}
} // namespace nova_sources
} // namespace xs
