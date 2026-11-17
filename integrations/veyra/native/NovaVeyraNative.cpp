// SPDX-License-Identifier: GPL-3.0-only
// Direct native adapter to Likely7/Veyra-NRVideo 2.1.9, NOT a rewritten GPU filter.
// Compiled with MSVC alongside Veyra's original veyra_engine CMake target.
// Veyra owns FFmpeg, D3D12, NVOF, NGX, audio clock, shader history and Present.
#define NOVA_VEYRA_NATIVE_EXPORTS
#include "NovaVeyraApi.h"
#include "veyra/engine/EngineController.h"
#include "veyra/engine/EnhancementSettings.h"
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <memory>
#include <new>
#include <algorithm>

namespace {
using veyra::engine::EnhancementSettings;
using veyra::engine::EngineController;
using veyra::engine::FrameGenerationBackend;
using veyra::engine::PlayerOptions;

struct Session {
    EngineController engine;
};

bool checked(const NovaVeyraSettings* options){
    if(!options || options->size!=sizeof(NovaVeyraSettings))return false;
    if(options->features & ~(NOVA_VEYRA_FEATURE_NR|NOVA_VEYRA_FEATURE_SR|NOVA_VEYRA_FEATURE_FG))return false;
    if(options->fg_backend>NOVA_VEYRA_FG_AMD_FRC || options->sr_quality>4)return false;
    if(options->software_decode>1)return false;
    if((options->features&NOVA_VEYRA_FEATURE_FG)!=0){
        if(options->fg_multiplier<2||options->fg_multiplier>8)return false;
        const auto backend=static_cast<FrameGenerationBackend>(options->fg_backend);
        if(veyra::engine::twoXFrameGeneration(backend) && options->fg_multiplier>2)return false;
        if(backend==FrameGenerationBackend::Dlss &&
           options->fg_multiplier!=2&&options->fg_multiplier!=3&&
           options->fg_multiplier!=4&&options->fg_multiplier!=6)return false;
    }
    return true;
}
EnhancementSettings settingsFrom(const NovaVeyraSettings& in,EnhancementSettings base={}){
    base.nr=(in.features&NOVA_VEYRA_FEATURE_NR)!=0;
    base.sr=(in.features&NOVA_VEYRA_FEATURE_SR)!=0;
    base.multiplier=(in.features&NOVA_VEYRA_FEATURE_FG)?in.fg_multiplier:1u;
    base.frameGenerationBackend=static_cast<FrameGenerationBackend>(in.fg_backend);
    base.videoSrQuality=in.sr_quality;
    return base;
}
}

extern "C" {
uint32_t NOVA_VEYRA_CALL nova_veyra_abi(void){return NOVA_VEYRA_NATIVE_ABI;}

void* NOVA_VEYRA_CALL nova_veyra_create(void){
    try {return new Session{};} catch (...) {return nullptr;}
}

void NOVA_VEYRA_CALL nova_veyra_destroy(void* ptr){
    if(!ptr)return;
    // Always close the real Veyra engine while its HWND is still alive.
    // This call may wait for the engine's own bounded shutdown.
    try {delete static_cast<Session*>(ptr);} catch (...) {}
}

int NOVA_VEYRA_CALL nova_veyra_open(
    void* ptr,void* target,const wchar_t* source,const NovaVeyraSettings* options){
    if(!ptr||!target||!source||!source[0]||!checked(options))return 0;
    if(!IsWindow(static_cast<HWND>(target)))return 0;
    try {
        EnhancementSettings enhancement=settingsFrom(*options);
        if(!enhancement.validate().empty())return 0;
        PlayerOptions playback=PlayerOptions::from(enhancement);
        playback.softwareDecode=options->software_decode!=0;
        static_cast<Session*>(ptr)->engine.open(static_cast<HWND>(target),source,playback);
        return 1; // queued; do NOT misreport success before snapshot reports Playing
    } catch (...) {return 0;}
}
void NOVA_VEYRA_CALL nova_veyra_stop(void* ptr){
    if(ptr)try{static_cast<Session*>(ptr)->engine.stop();}catch(...){}
}
void NOVA_VEYRA_CALL nova_veyra_pause(void* ptr,uint32_t paused){
    if(ptr)try{static_cast<Session*>(ptr)->engine.pause(paused!=0);}catch(...){}
}
void NOVA_VEYRA_CALL nova_veyra_seek(void* ptr,double seconds){
    if(ptr&&std::isfinite(seconds)&&seconds>=0)
        try{static_cast<Session*>(ptr)->engine.seek(seconds);}catch(...){}
}
void NOVA_VEYRA_CALL nova_veyra_volume(void* ptr,float volume){
    if(ptr&&std::isfinite(volume))
        try{static_cast<Session*>(ptr)->engine.setVolume(std::clamp(volume,0.0f,1.0f),false);}catch(...){}
}
int NOVA_VEYRA_CALL nova_veyra_settings(void* ptr,const NovaVeyraSettings* settings){
    if(!ptr||!checked(settings))return 0;
    try {
        auto& engine=static_cast<Session*>(ptr)->engine;
        EnhancementSettings next=settingsFrom(*settings,engine.snapshot().desired);
        return next.validate().empty()&&engine.requestSettings(next) ? 1 : 0;
    } catch (...) {return 0;}
}
int NOVA_VEYRA_CALL nova_veyra_snapshot(void* ptr,NovaVeyraSnapshot* out){
    if(!ptr||!out||out->size!=sizeof(NovaVeyraSnapshot))return 0;
    try {
        const auto s=static_cast<Session*>(ptr)->engine.snapshot();
        const uint32_t size=out->size;
        *out={};
        out->size=size;
        out->transport=static_cast<uint32_t>(s.transport);
        out->failed=s.failed?1u:0u;
        out->running=s.running?1u:0u;
        out->nr_active=s.nrActive?1u:0u;
        out->sr_active=s.srActive?1u:0u;
        out->fg_active=s.fgActive?1u:0u;
        out->hdr_active=s.videoHdrActive?1u:0u;
        out->source_width=s.sourceWidth;
        out->source_height=s.sourceHeight;
        out->fg_max_generated=s.fgMultiFrameMax>0?uint32_t(s.fgMultiFrameMax):0u;
        out->submit_fps_known=s.submissionFps.has_value()?1u:0u;
        out->position_seconds=s.position;
        out->duration_seconds=s.duration;
        out->source_fps=s.nominalSourceFps;
        out->submit_fps=s.submissionFps.value_or(0.0);
        out->source_frames=s.frames;
        out->generated_frames=s.generated;
        out->session_id=s.sessionId;
        return 1;
    } catch (...) {return 0;}
}
} // extern "C"
