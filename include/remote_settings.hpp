#pragma once
#ifdef _WIN32
#include <array>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>
namespace xs {
struct RemoteSettingDef {
    const char* key;
    const char* category;
    const char* label;
    int low,high,step,default_value;
};
// Only explicitly supported options can be changed over the LAN.
// Signed integer values are validated, not used as filesystem paths or mpv property names.
inline constexpr std::array<RemoteSettingDef,12> kRemoteSettings{{
    {"volume","Sonido","Volumen",0,100,1,100},
    {"brightness","Imagen","Brillo",0,100,1,50},
    {"speed","Reproduccion","Velocidad",50,200,25,100},
    {"aspect","Imagen","Formato",0,3,1,0},
    {"auto_next","Reproduccion","Siguiente episodio",0,1,1,1},
    {"mute","Sonido","Silencio",0,1,1,0},
    {"audio_delay","Sonido","Sincronia audio (ms)",-1000,1000,100,0},
    {"sub_delay","Subtitulos","Sincronia subtitulos (ms)",-5000,5000,250,0},
    {"sub_scale","Subtitulos","Tamano subtitulos (%)",50,300,10,100},
    {"video_preset","Imagen","Calidad de video",0,5,1,1},
    {"motion_mode","Imagen","Movimiento / FPS",0,2,1,0},
    {"rtx_scale_mode","Imagen","RTX AI - nivel de escalado",0,4,1,0}
}};
inline int remote_setting_index(const std::string& key){
    for(size_t i=0;i<kRemoteSettings.size();++i)if(key==kRemoteSettings[i].key)
        return static_cast<int>(i);
    return -1;
}
inline bool valid_remote_setting(int index,int value){
    if(index<0||static_cast<size_t>(index)>=kRemoteSettings.size())return false;
    const auto& s=kRemoteSettings[index];
    return value>=s.low&&value<=s.high&&(value-s.low)%s.step==0;
}
class RemoteSettingsStore {
public:
    RemoteSettingsStore(){
        for(size_t i=0;i<kRemoteSettings.size();++i)values_[i]=kRemoteSettings[i].default_value;
    }
    int get(int id)const{
        std::lock_guard<std::mutex> lock(mutex_);
        return id>=0&&static_cast<size_t>(id)<values_.size()?values_[id]:0;
    }
    bool put(int id,int value,bool persist=true){
        if(!valid_remote_setting(id,value))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        values_[id]=value;
        if(persist)save_locked();
        return true;
    }
    void load(){
        std::lock_guard<std::mutex> lock(mutex_);
        std::ifstream file("remote_playback_settings.txt");
        std::string key;int value=0;
        while(file>>key>>value){
            const int id=remote_setting_index(key);
            if(valid_remote_setting(id,value))values_[id]=value;
        }
    }
private:
    void save_locked()const{
        std::ofstream file("remote_playback_settings.txt",std::ios::trunc);
        if(!file)return;
        for(size_t i=0;i<values_.size();++i)
            file<<kRemoteSettings[i].key<<" "<<values_[i]<<"\n";
    }
    mutable std::mutex mutex_;
    std::array<int,kRemoteSettings.size()> values_{};
};
}
#endif
