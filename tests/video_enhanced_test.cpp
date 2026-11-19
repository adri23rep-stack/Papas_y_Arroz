#include "video_enhanced.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
int main() {
    using namespace xs;
    assert(video_preset_from_string("compatible")==VideoPreset::Compatible);
    assert(video_preset_from_string("sharp")==VideoPreset::Sharp);
    assert(video_preset_from_string("extra_detail")==VideoPreset::ExtraDetail);
    assert(video_preset_from_string("gpu_fsrcnnx")==VideoPreset::GpuFsrcnnx);
    assert(video_preset_from_string("rtx_ai_super_resolution")==VideoPreset::RtxAiSuperResolution);
    assert(video_preset_from_string("invalid")==VideoPreset::Balanced);
    assert(next_video_preset(VideoPreset::Balanced,1)==VideoPreset::Sharp);
    assert(next_video_preset(VideoPreset::Balanced,-1)==VideoPreset::Compatible);
    assert(next_video_preset(VideoPreset::Sharp,1)==VideoPreset::ExtraDetail);
    assert(next_video_preset(VideoPreset::ExtraDetail,1)==VideoPreset::GpuFsrcnnx);
    assert(next_video_preset(VideoPreset::GpuFsrcnnx,1)==VideoPreset::RtxAiSuperResolution);
    assert(next_video_preset(VideoPreset::RtxAiSuperResolution,1)==VideoPreset::Compatible);
    assert(next_video_preset(VideoPreset::Compatible,-1)==VideoPreset::RtxAiSuperResolution);
    assert(std::strcmp(video_hwdec(VideoPreset::Compatible),"no")==0);
    assert(std::string(video_hwdec(VideoPreset::Balanced)).find("nvdec-copy")!=std::string::npos);
    assert(std::strcmp(video_scaler(VideoPreset::Sharp),"lanczos")==0);
    assert(std::strcmp(video_scaler(VideoPreset::ExtraDetail),"lanczos")==0);
    assert(video_uses_gpu(VideoPreset::GpuFsrcnnx));
    assert(video_uses_gpu(VideoPreset::RtxAiSuperResolution));
    assert(std::strcmp(video_scaler(VideoPreset::RtxAiSuperResolution),"lanczos")==0);
    assert(!video_uses_gpu(VideoPreset::Sharp));
    assert(extra_detail_filter(1280,720)=="lavfi=[scale=1920:1080:flags=lanczos,cas=strength=0.30]");
    assert(extra_detail_filter(640,360).find("scale=1920:1080")!=std::string::npos);
    assert(extra_detail_filter(1920,1080)=="lavfi=[cas=strength=0.24]");
    assert(extra_detail_filter(3840,2160)=="lavfi=[cas=strength=0.24]");
    assert(extra_detail_filter(1024,768)=="lavfi=[cas=strength=0.24]");
    assert(extra_detail_filter(0,0).empty());
    const char* path="video_enhanced_test.tmp";
    assert(save_video_preset(VideoPreset::GpuFsrcnnx,path));
    assert(load_video_preset(path)==VideoPreset::GpuFsrcnnx);
    assert(save_video_preset(VideoPreset::RtxAiSuperResolution,path));
    assert(load_video_preset(path)==VideoPreset::RtxAiSuperResolution);
    std::remove(path);
    assert(load_video_preset(path)==VideoPreset::Balanced);
    return 0;
}
