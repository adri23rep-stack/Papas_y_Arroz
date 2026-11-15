#include "remote_browse.hpp"
#ifdef _WIN32
#include "remote_server.hpp"
#include <algorithm>
#include <utility>
namespace xs {
namespace {
Catalog make_sections(const Catalog& root){
    Catalog sections;sections.title="Explorar XuperTV";
    const auto live=std::find_if(root.entries.begin(),root.entries.end(),
        [](const Entry& item){return item.kind==EntryKind::Category;});
    if(live!=root.entries.end()){
        Entry tv;tv.kind=EntryKind::LiveSection;tv.title="TV en vivo";
        sections.entries.push_back(std::move(tv));
    }
    for(const char* type:{"1","0"}){
        auto entry=std::find_if(root.entries.begin(),root.entries.end(),
            [type](const Entry& item){return item.kind==EntryKind::Section&&item.program_type==type;});
        if(entry!=root.entries.end())sections.entries.push_back(*entry);
    }
    return sections;
}
bool playable(EntryKind kind){return kind==EntryKind::Channel||kind==EntryKind::Episode||kind==EntryKind::Media;}
void push_play(size_t index,uint32_t generation){
    SDL_Event ev{};ev.type=SDL_USEREVENT;ev.user.code=REMOTE_OPEN_BROWSE;
    ev.user.data1=reinterpret_cast<void*>(static_cast<intptr_t>(index));
    ev.user.data2=reinterpret_cast<void*>(static_cast<uintptr_t>(generation));
    SDL_PushEvent(&ev);
}
}
void RemoteBrowse::set_session(std::shared_ptr<ServiceSession> session,const Catalog& root){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;busy_=false;state_="idle";
    }
    if(worker_.joinable())worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    session_=std::move(session);root_=root;
    page_={};history_.clear();autoplayed_=false;
}
bool RemoteBrowse::start(uint32_t& generation,std::string& error){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_||busy_){error="Catalogo ocupado.";return false;}
        if(!session_||root_.entries.empty()){error="Conecta XuperTV en Windows primero.";return false;}
    }
    if(worker_.joinable())worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    page_=make_sections(root_);history_.clear();autoplayed_=false;
    generation=++generation_;state_="ready";message_="Elige una categoria.";
    return true;
}
BrowseSelection RemoteBrowse::select(uint32_t generation,size_t index,
                                     uint32_t& next_generation,std::string& error){
    Entry entry;
    std::shared_ptr<ServiceSession> session;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_||busy_||state_!="ready"||generation!=generation_||index>=page_.entries.size()){
            error="Esta pagina ya no esta disponible.";return BrowseSelection::Invalid;
        }
        entry=page_.entries[index];session=session_;
        if(playable(entry.kind)){
            next_generation=generation_;
            push_play(index,generation_);
            return BrowseSelection::Play;
        }
        if(entry.kind!=EntryKind::LiveSection&&entry.kind!=EntryKind::Section&&
           entry.kind!=EntryKind::Category&&entry.kind!=EntryKind::VodCategory&&
           entry.kind!=EntryKind::VodPage&&entry.kind!=EntryKind::Asset&&
           entry.kind!=EntryKind::SearchPage){
            error="Este elemento no se puede abrir.";return BrowseSelection::Invalid;
        }
        if(!session){error="La sesion del catalogo se cerro.";return BrowseSelection::Invalid;}
        if(history_.size()>=16){error="Demasiadas carpetas abiertas.";return BrowseSelection::Invalid;}
        history_.push_back(page_);
        busy_=true;autoplayed_=false;state_="loading";message_="Cargando "+entry.title+"...";
        next_generation=++generation_;
    }
    if(worker_.joinable())worker_.join();
    const uint32_t request_generation=next_generation;
    worker_=std::thread([this,session,entry,request_generation]{
        BrowseResult result;
        if(entry.kind==EntryKind::LiveSection){
            result.ok=true;result.catalog.title="TV en vivo";
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for(const auto& item:root_.entries)
                    if(item.kind==EntryKind::Category)result.catalog.entries.push_back(item);
            }
        }else{
            try {result=service_browse_remote(session,entry);}
            catch(...){result.ok=false;result.status="No se pudo cargar esta categoria.";}
        }
        if(result.catalog.title.empty())result.catalog.title=entry.title;
        bool autoplay=false;
        size_t autoplay_index=0;
        if(result.ok&&entry.kind==EntryKind::Asset&&entry.program_type=="1"){
            for(size_t i=0;i<result.catalog.entries.size();++i)
                if(result.catalog.entries[i].kind==EntryKind::Episode){
                    autoplay=true;autoplay_index=i;break;
                }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopping_||generation_!=request_generation)return;
            if(result.ok){
                page_=std::move(result.catalog);
                state_="ready";
                message_=autoplay?"Reproduciendo pelicula en Windows.":result.status;
                autoplayed_=autoplay;
            }else{
                if(!history_.empty()){
                    page_=std::move(history_.back());history_.pop_back();
                }
                state_="error";message_=result.status.empty()?
                    "No se pudo cargar esta categoria.":result.status;
            }
            busy_=false;
        }
        if(autoplay)push_play(autoplay_index,request_generation);
    });
    return BrowseSelection::Loading;
}
bool RemoteBrowse::back(uint32_t generation,uint32_t& next_generation,std::string& error){
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_||busy_||generation!=generation_||history_.empty()){
        error="No hay una pagina anterior disponible.";return false;
    }
    page_=std::move(history_.back());history_.pop_back();
    next_generation=++generation_;state_="ready";autoplayed_=false;
    message_="Pagina anterior.";
    return true;
}
RemoteBrowseSnapshot RemoteBrowse::snapshot(uint32_t generation,size_t offset,size_t limit)const{
    std::lock_guard<std::mutex> lock(mutex_);
    RemoteBrowseSnapshot out;out.generation=generation_;
    if(generation!=generation_){
        out.state="expired";out.message="El catalogo cambio. Abrelo otra vez.";
        return out;
    }
    out.state=state_;out.message=message_;out.title=page_.title;
    out.description=page_.description;out.image=page_.artwork;
    out.total=page_.entries.size();out.auto_played=autoplayed_;
    if(state_=="ready"){
        const size_t end=std::min(out.total,offset+std::min<size_t>(limit,60));
        for(size_t i=offset;i<end;++i)out.items.emplace_back(i,page_.entries[i]);
    }
    return out;
}
bool RemoteBrowse::selected_result(uint32_t generation,size_t index,Catalog& out)const{
    std::lock_guard<std::mutex> lock(mutex_);
    if(generation!=generation_||state_!="ready"||index>=page_.entries.size()||
       !playable(page_.entries[index].kind))return false;
    out=page_;
    return true;
}
void RemoteBrowse::stop(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_=true;++generation_;
    }
    if(worker_.joinable())worker_.join();
}
}
#endif
