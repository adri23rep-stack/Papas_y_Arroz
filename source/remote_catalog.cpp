#include "remote_catalog.hpp"
#ifdef _WIN32
#include <algorithm>
#include <utility>
namespace xs {
void RemoteCatalog::set_session(std::shared_ptr<ServiceSession> session){
    std::lock_guard<std::mutex> lock(mutex_);
    session_=std::move(session);
}
bool RemoteCatalog::begin(const std::string& query,uint32_t& generation,std::string& error){
    if(query.empty()||query.size()>120||search_key(query).empty()){
        error="Introduce un nombre de 1 a 120 caracteres.";return false;
    }
    std::shared_ptr<ServiceSession> local;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_){error="La aplicación se está cerrando.";return false;}
        if(busy_){error="Hay otra búsqueda en curso.";return false;}
        if(!session_){error="Primero espera a que Xuper TV se conecte.";return false;}
        local=session_;
        ++generation_;
        generation=generation_;
        busy_=true;state_="loading";message_="Buscando en TV, películas y series...";
        results_.clear();
    }
    if(worker_.joinable())worker_.join();
    worker_=std::thread([this,local,query,generation]{
        BrowseResult result;
        try{result=service_search_remote(local,query);}
        catch(...){result.status="No se pudo consultar el catálogo.";result.ok=false;}
        std::vector<Entry> items;
        // Keep a mix of channels, movies and series; no stream URLs or tokens
        // are ever returned to the iPhone.
        size_t tv=0,movies=0,series=0;
        for(const auto& entry:result.catalog.entries){
            if(entry.kind==EntryKind::Channel&&tv<20){items.push_back(entry);++tv;}
            else if(entry.kind==EntryKind::Asset&&entry.program_type=="1"&&movies<20){
                items.push_back(entry);++movies;
            }else if(entry.kind==EntryKind::Asset&&entry.program_type=="0"&&series<20){
                items.push_back(entry);++series;
            }
            if(items.size()>=60)break;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if(generation!=generation_||stopping_)return;
        results_=std::move(items);
        message_=result.status;
        state_=result.ok?"ready":"error";
        busy_=false;
    });
    return true;
}
RemoteSearchSnapshot RemoteCatalog::snapshot(uint32_t gen)const{
    std::lock_guard<std::mutex> lock(mutex_);
    RemoteSearchSnapshot out;out.generation=generation_;
    if(gen!=generation_){out.state="expired";out.message="Esa búsqueda ya no está disponible.";return out;}
    out.state=state_;out.message=message_;
    if(state_=="ready")out.items=results_;
    return out;
}
bool RemoteCatalog::select(uint32_t gen,size_t index,Catalog& catalog)const{
    std::lock_guard<std::mutex> lock(mutex_);
    if(gen!=generation_||state_!="ready"||index>=results_.size())return false;
    catalog.title="Resultados del iPhone";
    catalog.entries=results_;
    return true;
}
void RemoteCatalog::stop(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_=true;
    }
    if(worker_.joinable())worker_.join();
}
}
#endif
