#pragma once
#include <string>
#include <fstream>

namespace xs {
// Stage 3 optionally uses libmpv OpenGL with a real FSRCNNX GPU shader.
// Unsupported systems automatically retain the software renderer.
enum class VideoPreset { Compatible, Balanced, Sharp, ExtraDetail, GpuFsrcnnx, RtxAiSuperResolution };

inline VideoPreset video_preset_from_string(const std::string& value) {
    if(value=="compatible")return VideoPreset::Compatible;
    if(value=="sharp")return VideoPreset::Sharp;
    if(value=="extra_detail")return VideoPreset::ExtraDetail;
    if(value=="gpu_fsrcnnx")return VideoPreset::GpuFsrcnnx;
    if(value=="rtx_ai_super_resolution")return VideoPreset::RtxAiSuperResolution;
    return VideoPreset::Balanced;
}
inline const char* video_preset_key(VideoPreset preset) {
    switch(preset) {
        case VideoPreset::Compatible:return "compatible";
        case VideoPreset::Balanced:return "balanced";
        case VideoPreset::Sharp:return "sharp";
        case VideoPreset::ExtraDetail:return "extra_detail";
        case VideoPreset::GpuFsrcnnx:return "gpu_fsrcnnx";
        case VideoPreset::RtxAiSuperResolution:return "rtx_ai_super_resolution";
    }
    return "balanced";
}
inline const char* video_preset_label(VideoPreset preset) {
    switch(preset) {
        case VideoPreset::Compatible:return "Compatible (CPU)";
        case VideoPreset::Balanced:return "Equilibrado";
        case VideoPreset::Sharp:return "Nitidez alta";
        case VideoPreset::ExtraDetail:return "Detalle extra (CAS)";
        case VideoPreset::GpuFsrcnnx:return "GPU FSRCNNX (experimental)";
        case VideoPreset::RtxAiSuperResolution:return "RTX AI Super Resolution";
    }
    return "Equilibrado";
}
inline VideoPreset next_video_preset(VideoPreset preset,int direction) {
    int index=static_cast<int>(preset);
    return static_cast<VideoPreset>((index+6+(direction<0?-1:1))%6);
}
inline const char* video_hwdec(VideoPreset preset) {
    // Copy-back modes work with software-rendered libmpv and can gracefully
    // fallback on unsupported codecs or devices.
    return preset==VideoPreset::Compatible?"no":"nvdec-copy,d3d11va-copy,auto-copy";
}
inline const char* video_scaler(VideoPreset preset) {
    return (preset==VideoPreset::Sharp||preset==VideoPreset::ExtraDetail||preset==VideoPreset::GpuFsrcnnx||
            preset==VideoPreset::RtxAiSuperResolution)?"lanczos":"bicubic";
}
// On exactly 16:9 SD/HD sources, upscale to 1080p with Lanczos before
// adaptive sharpening. For other aspect ratios or >=1080p sources, leave
// dimensions untouched and only sharpen gently. Never pretend to add
// detail missing from the recording.
inline bool video_uses_gpu(VideoPreset preset){return preset==VideoPreset::GpuFsrcnnx ||
    preset==VideoPreset::RtxAiSuperResolution;}
inline std::string extra_detail_filter(int width,int height) {
    if(width<=0||height<=0)return "";
    const bool sd_hd=width<1920&&height<1080&&width>=480&&height>=270;
    const bool widescreen=(width*9==height*16); // exact standard 16:9
    if(sd_hd&&widescreen)
        return "lavfi=[scale=1920:1080:flags=lanczos,cas=strength=0.30]";
    return "lavfi=[cas=strength=0.24]";
}
inline VideoPreset load_video_preset(const char* path="video_enhanced.txt") {
    std::ifstream file(path);
    std::string value;
    if(file>>value)return video_preset_from_string(value);
    return VideoPreset::Balanced;
}
inline bool save_video_preset(VideoPreset preset,const char* path="video_enhanced.txt") {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if(!file)return false;
    file<<video_preset_key(preset)<<"\n";
    file.flush();
    return file.good();
}
}
