#pragma once
#ifdef _WIN32
#include "catalog.hpp"
#include "service.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace xs {
struct RemoteSearchSnapshot {
    uint32_t generation=0;
    std::string state="idle";
    std::string message;
    std::vector<Entry> items;
};
class RemoteCatalog {
public:
    void set_session(std::shared_ptr<ServiceSession> session);
    bool begin(const std::string& query,uint32_t& generation,std::string& error);
    RemoteSearchSnapshot snapshot(uint32_t generation)const;
    bool select(uint32_t generation,size_t index,Catalog& catalog)const;
    void stop();
    ~RemoteCatalog(){stop();}
    RemoteCatalog()=default;
    RemoteCatalog(const RemoteCatalog&)=delete;
    RemoteCatalog& operator=(const RemoteCatalog&)=delete;
private:
    mutable std::mutex mutex_;
    std::shared_ptr<ServiceSession> session_;
    std::thread worker_;
    uint32_t generation_=0;
    bool busy_=false,stopping_=false;
    std::string state_="idle",message_;
    std::vector<Entry> results_;
};
}
#endif
