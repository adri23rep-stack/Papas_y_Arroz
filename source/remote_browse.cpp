#include "remote_browse.hpp"
#ifdef _WIN32
#include "remote_server.hpp"
#include "provider_catalog.hpp"
#include <algorithm>
#include <map>
#include <utility>
namespace xs {
namespace {
Catalog make_sections(const Catalog& root){
    Catalog sections;sections.title="Explorar XuperTV";
    Entry services;services.kind=EntryKind::ProviderDirectory;
    services.title="Plataformas";services.description="Catálogos detectados en XuperTV";
    sections.entries.push_back(std::move(services));
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
struct ProviderDiscovery{
    Catalog directory;
    std::map<std::string,std::vector<Entry>> groups;
    int sections_ok=0;
};
ProviderDiscovery discover_providers(const std::shared_ptr<ServiceSession>& session,
                                    const Catalog& root){
    ProviderDiscovery found;
    found.directory.title="Plataformas";
    found.directory.description="Solo aparecen secciones disponibles en XuperTV.";
    for(const auto& section:root.entries){
        if(section.kind!=EntryKind::Section)continue;
        BrowseResult section_page;
        try{section_page=service_browse_remote(session,section);}
        catch(...){continue;}
        if(!section_page.ok)continue;
        ++found.sections_ok;
        for(const auto& category:section_page.catalog.entries){
            if(category.kind!=EntryKind::VodCategory||category.column_id<0)continue;
            const std::string key=provider_for_catalog_label(category.title);
            if(key.empty())continue;
            auto entry=category;
            // Preserve the actual service-issued id and program type.
            entry.title=std::string(section.program_type=="1"?"Películas · ":"Series · ")+category.title;
            found.groups[key].push_back(std::move(entry));
        }
    }
    for(const auto& identity:kCatalogProviders){
        const auto it=found.groups.find(identity.key);
        if(it==found.groups.end()||it->second.empty())continue;
        Entry folder;
        folder.kind=EntryKind::Provider;
        folder.title=identity.title;
        folder.program_type=identity.key;
        folder.description=std::to_string(it->second.size())+" categorías disponibles";
        found.directory.entries.push_back(std::move(folder));
    }
    return found;
}
Catalog make_provider_page(const std::string& key,
                           const std::map<std::string,std::vector<Entry>>& categories){
    Catalog out;
    const char* name=provider_title(key);
    out.title=name?name:"Catálogo";
    const auto it=categories.find(key);
    if(it!=categories.end())out.entries=it->second;
    out.description="Contenido organizado según las categorías de XuperTV.";
    return out;
}
bool playable(EntryKind kind){return kind==EntryKind::Channel||kind==EntryKind::Episode||kind==EntryKind::Media;}
bool push_play(size_t index,uint32_t generation){
    SDL_Event ev{};ev.type=SDL_USEREVENT;ev.user.code=REMOTE_OPEN_BROWSE;
    ev.user.data1=reinterpret_cast<void*>(static_cast<intptr_t>(index));
    ev.user.data2=reinterpret_cast<void*>(static_cast<uintptr_t>(generation));
    return SDL_PushEvent(&ev)==1;
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
    page_={};history_.clear();provider_categories_.clear();autoplayed_=false;mobile_mode_=false;
}
bool RemoteBrowse::start(uint32_t& generation,std::string& error,
                         const std::string& provider,bool mobile){
    if(!provider.empty()&&!provider_title(provider)){
        error="Esta plataforma no está disponible en el control.";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_||busy_){error="Catálogo ocupado.";return false;}
        if(!session_||root_.entries.empty()){
            error="Conecta XuperTV en Windows primero.";return false;
        }
    }
    if(worker_.joinable())worker_.join();
    std::shared_ptr<ServiceSession> session;
    Catalog root;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();provider_categories_.clear();autoplayed_=false;mobile_mode_=mobile;
        generation=++generation_;
        if(provider.empty()){
            page_=make_sections(root_);
            state_="ready";message_="Explora los catálogos disponibles.";
            return true;
        }
        session=session_;root=root_;
        page_={};page_.title=provider_title(provider);
        busy_=true;state_="loading";message_="Detectando "+page_.title+"...";
    }
    const uint32_t request_generation=generation;
    worker_=std::thread([this,session,root,provider,request_generation]{
        const auto discovered=discover_providers(session,root);
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_||generation_!=request_generation)return;
        provider_categories_=discovered.groups;
        page_=make_provider_page(provider,provider_categories_);
        state_="ready";busy_=false;
        message_=page_.entries.empty()
            ?(discovered.sections_ok==0
                ?"No se pudo consultar el catálogo de la PC."
                :"XuperTV no tiene una sección "+page_.title+" disponible.")
            :"Elige películas o series de "+page_.title+".";
    });
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
        if(entry.kind==EntryKind::Provider){
            const auto it=provider_categories_.find(entry.program_type);
            if(it==provider_categories_.end()||it->second.empty()){
                error="Este catálogo ya no está disponible.";return BrowseSelection::Invalid;
            }
            history_.push_back(page_);
            page_=make_provider_page(entry.program_type,provider_categories_);
            next_generation=++generation_;
            state_="ready";message_="Elige una categoría.";autoplayed_=false;
            return BrowseSelection::Loading;
        }
        if(playable(entry.kind)){
            next_generation=generation_;
            if(mobile_mode_)return BrowseSelection::Play;
            if(!push_play(index,generation_)){
                error="Windows no pudo recibir la orden de reproduccion.";
                return BrowseSelection::Invalid;
            }
            return BrowseSelection::Play;
        }
        if(entry.kind!=EntryKind::ProviderDirectory&&
           entry.kind!=EntryKind::LiveSection&&entry.kind!=EntryKind::Section&&
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
        ProviderDiscovery discovered;
        bool found_platforms=false;
        if(entry.kind==EntryKind::ProviderDirectory){
            Catalog root;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                root=root_;
            }
            discovered=discover_providers(session,root);
            result.ok=true;
            result.catalog=discovered.directory;
            result.status=discovered.directory.entries.empty()
                ?(discovered.sections_ok==0
                    ?"No se pudieron consultar las plataformas del catálogo."
                    :"No se detectaron plataformas en las categorías disponibles.")
                :"Elige un catálogo.";
            found_platforms=true;
        }else if(entry.kind==EntryKind::LiveSection){
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
        if(result.ok&&!mobile_mode_&&entry.kind==EntryKind::Asset&&entry.program_type=="1"){
            for(size_t i=0;i<result.catalog.entries.size();++i)
                if(result.catalog.entries[i].kind==EntryKind::Episode){
                    autoplay=true;autoplay_index=i;break;
                }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopping_||generation_!=request_generation)return;
            if(result.ok){
                if(found_platforms)provider_categories_=std::move(discovered.groups);
                page_=std::move(result.catalog);
                // Enqueue autoplay BEFORE marking the page ready. Otherwise
                // the iPhone can read "playing" before Windows sees the event.
                const bool queued=!autoplay||push_play(autoplay_index,request_generation);
                state_=queued?"ready":"error";
                message_=!queued?"Windows no pudo recibir la pelicula.":
                          autoplay?"Reproduciendo pelicula en Windows.":result.status;
                autoplayed_=autoplay&&queued;
            }else{
                if(!history_.empty()){
                    page_=std::move(history_.back());history_.pop_back();
                }
                state_="error";message_=result.status.empty()?
                    "No se pudo cargar esta categoria.":result.status;
            }
            busy_=false;
        }
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
bool RemoteBrowse::mobile_selection(uint32_t generation,size_t index,Entry& result,
                                    std::shared_ptr<ServiceSession>& session) const{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!mobile_mode_||stopping_||busy_||state_!="ready"||
       generation!=generation_||index>=page_.entries.size()||
       !playable(page_.entries[index].kind))return false;
    result=page_.entries[index];
    session=session_;
    return session!=nullptr||result.kind==EntryKind::Media;
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
