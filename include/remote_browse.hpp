#pragma once
#ifdef _WIN32
#include "catalog.hpp"
#include "service.hpp"
#include <cstdint>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
namespace xs {
struct RemoteBrowseSnapshot {
    uint32_t generation=0;
    std::string state="idle",message,title,description;
    std::string image;
    size_t total=0;
    bool auto_played=false;
    std::vector<std::pair<size_t,Entry>> items;
};
enum class BrowseSelection { Invalid, Loading, Play };
class RemoteBrowse {
public:
    void set_session(std::shared_ptr<ServiceSession> session,const Catalog& root);
    bool start(uint32_t& generation,std::string& error,const std::string& provider={},bool mobile=false);
    BrowseSelection select(uint32_t generation,size_t index,uint32_t& next_generation,std::string& error);
    bool back(uint32_t generation,uint32_t& next_generation,std::string& error);
    RemoteBrowseSnapshot snapshot(uint32_t generation,size_t offset=0,size_t limit=48) const;
    bool selected_result(uint32_t generation,size_t index,Catalog& result) const;
    bool mobile_selection(uint32_t generation,size_t index,Entry& result,
                          std::shared_ptr<ServiceSession>& session) const;
    void stop();
    ~RemoteBrowse(){stop();}
private:
    mutable std::mutex mutex_;
    std::shared_ptr<ServiceSession> session_;
    Catalog root_,page_;
    std::vector<Catalog> history_;
    std::map<std::string,std::vector<Entry>> provider_categories_;
    std::thread worker_;
    uint32_t generation_=0;
    bool busy_=false,stopping_=false,autoplayed_=false,mobile_mode_=false;
    std::string state_="idle",message_;
};
}
#endif
