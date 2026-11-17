#pragma once
// The Nova side of the native (in-process) Veyra 2.1.9 EngineController.
// A DLL boundary lets MSYS2 Nova use the actual MSVC/D3D12 engine without
// passing Qt/STL objects across different C++ runtimes.
//
// This file does NOT contain GPU algorithms. They remain in original GPLv3
// Veyra source, compiled in nova_veyra_native.dll as a separate module.
#include "../integrations/veyra/native/NovaVeyraApi.h"
#include <QLibrary>
#include <QFileInfo>
#include <QString>
#include <cstdint>
#include <utility>
#include <vector>
#include <algorithm>

class NovaVeyraNativeLoader {
public:
    NovaVeyraNativeLoader()=default;
    NovaVeyraNativeLoader(const NovaVeyraNativeLoader&)=delete;
    NovaVeyraNativeLoader& operator=(const NovaVeyraNativeLoader&)=delete;
    ~NovaVeyraNativeLoader(){close();}

    bool load(const QString& path) {
        if(engine_ || !QFileInfo(path).isAbsolute() || !QFileInfo(path).isFile())return false;
        lib_.setFileName(path);
        if(!lib_.load())return false;
        abi_=reinterpret_cast<decltype(&nova_veyra_abi)>(lib_.resolve("nova_veyra_abi"));
        create_=reinterpret_cast<decltype(&nova_veyra_create)>(lib_.resolve("nova_veyra_create"));
        destroy_=reinterpret_cast<decltype(&nova_veyra_destroy)>(lib_.resolve("nova_veyra_destroy"));
        open_=reinterpret_cast<decltype(&nova_veyra_open)>(lib_.resolve("nova_veyra_open"));
        stop_=reinterpret_cast<decltype(&nova_veyra_stop)>(lib_.resolve("nova_veyra_stop"));
        pause_=reinterpret_cast<decltype(&nova_veyra_pause)>(lib_.resolve("nova_veyra_pause"));
        seek_=reinterpret_cast<decltype(&nova_veyra_seek)>(lib_.resolve("nova_veyra_seek"));
        volume_=reinterpret_cast<decltype(&nova_veyra_volume)>(lib_.resolve("nova_veyra_volume"));
        rate_=reinterpret_cast<decltype(&nova_veyra_rate)>(lib_.resolve("nova_veyra_rate"));
        settings_=reinterpret_cast<decltype(&nova_veyra_settings)>(lib_.resolve("nova_veyra_settings"));
        snapshot_=reinterpret_cast<decltype(&nova_veyra_snapshot)>(lib_.resolve("nova_veyra_snapshot"));
        audioCount_=reinterpret_cast<decltype(&nova_veyra_audio_count)>(lib_.resolve("nova_veyra_audio_count"));
        audioAt_=reinterpret_cast<decltype(&nova_veyra_audio_at)>(lib_.resolve("nova_veyra_audio_at"));
        selectAudio_=reinterpret_cast<decltype(&nova_veyra_select_audio)>(lib_.resolve("nova_veyra_select_audio"));
        if(!abi_||!create_||!destroy_||!open_||!stop_||!pause_||!seek_||
           !volume_||!rate_||!settings_||!snapshot_||!audioCount_||!audioAt_||!selectAudio_||abi_()!=NOVA_VEYRA_NATIVE_ABI){
            close();return false;
        }
        engine_=create_();
        if(!engine_){close();return false;}
        return true;
    }

    bool ready()const{return engine_!=nullptr;}
    bool open(void* hwnd,const QString& source,const NovaVeyraSettings& options){
        if(!ready())return false;
        requested_=options;
        const auto wide=source.toStdWString();
        return open_(engine_,hwnd,wide.c_str(),&options)!=0;
    }
    void stop(){if(ready())stop_(engine_);}
    void pause(bool p){if(ready())pause_(engine_,p?1u:0u);}
    void seek(double seconds){if(ready())seek_(engine_,seconds);}
    void volume(double volume){if(ready())volume_(engine_,static_cast<float>(volume));}
    bool rate(double value){return ready() && rate_(engine_,value)!=0;}
    bool change(uint32_t flag,bool on) {
        if(!ready())return false;
        auto next=requested_;
        if(on)next.features|=flag;else next.features&=~flag;
        if(settings_(engine_,&next)==0)return false;
        requested_=next;return true;
    }
    bool multiplier(uint32_t value) {
        if(!ready())return false;
        auto next=requested_;
        next.fg_multiplier=value;
        next.features|=NOVA_VEYRA_FEATURE_FG;
        if(settings_(engine_,&next)==0)return false;
        requested_=next;return true;
    }
    bool snapshot(NovaVeyraSnapshot& state)const{
        if(!ready())return false;
        state={};state.size=sizeof(state);
        return snapshot_(engine_,&state)!=0;
    }
    std::vector<NovaVeyraAudioTrack> audioTracks()const {
        std::vector<NovaVeyraAudioTrack> tracks;
        if(!ready())return tracks;
        const auto count=std::min<uint32_t>(audioCount_(engine_),64u);
        for(uint32_t i=0;i<count;++i){
            NovaVeyraAudioTrack track{};
            track.size=sizeof(track);
            if(audioAt_(engine_,i,&track))tracks.push_back(track);
        }
        return tracks;
    }
    bool selectAudio(int index){return ready()&&index>=0&&selectAudio_(engine_,index)!=0;}
    void close() {
        if(engine_ && destroy_)destroy_(engine_);
        engine_=nullptr;
        if(lib_.isLoaded())lib_.unload();
        abi_=nullptr;create_=nullptr;destroy_=nullptr;open_=nullptr;
        stop_=nullptr;pause_=nullptr;seek_=nullptr;volume_=nullptr;
        settings_=nullptr;snapshot_=nullptr;rate_=nullptr;
        audioCount_=nullptr;audioAt_=nullptr;selectAudio_=nullptr;
    }
private:
    QLibrary lib_;
    void* engine_=nullptr;
    NovaVeyraSettings requested_{};
    decltype(&nova_veyra_abi) abi_=nullptr;
    decltype(&nova_veyra_create) create_=nullptr;
    decltype(&nova_veyra_destroy) destroy_=nullptr;
    decltype(&nova_veyra_open) open_=nullptr;
    decltype(&nova_veyra_stop) stop_=nullptr;
    decltype(&nova_veyra_pause) pause_=nullptr;
    decltype(&nova_veyra_seek) seek_=nullptr;
    decltype(&nova_veyra_volume) volume_=nullptr;
    decltype(&nova_veyra_rate) rate_=nullptr;
    decltype(&nova_veyra_settings) settings_=nullptr;
    decltype(&nova_veyra_snapshot) snapshot_=nullptr;
    decltype(&nova_veyra_audio_count) audioCount_=nullptr;
    decltype(&nova_veyra_audio_at) audioAt_=nullptr;
    decltype(&nova_veyra_select_audio) selectAudio_=nullptr;
};
