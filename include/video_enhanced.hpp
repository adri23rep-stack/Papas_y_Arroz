#pragma once
#include <string>
#include <fstream>

namespace xs {
// Stage 1 retains libmpv's existing software-rendered SDL integration.
// A GPU decoder can be used through "copy" interop without claiming that
// the renderer already supports GPU shaders / libplacebo / FSRCNNX.
enum class VideoPreset { Compatible, Balanced, Sharp };

inline VideoPreset video_preset_from_string(const std::string& value) {
    if(value=="compatible")return VideoPreset::Compatible;
    if(value=="sharp")return VideoPreset::Sharp;
    return VideoPreset::Balanced;
}
inline const char* video_preset_key(VideoPreset preset) {
    switch(preset) {
        case VideoPreset::Compatible:return "compatible";
        case VideoPreset::Balanced:return "balanced";
        case VideoPreset::Sharp:return "sharp";
    }
    return "balanced";
}
inline const char* video_preset_label(VideoPreset preset) {
    switch(preset) {
        case VideoPreset::Compatible:return "Compatible (CPU)";
        case VideoPreset::Balanced:return "Equilibrado";
        case VideoPreset::Sharp:return "Nitidez alta";
    }
    return "Equilibrado";
}
inline VideoPreset next_video_preset(VideoPreset preset,int direction) {
    int index=static_cast<int>(preset);
    return static_cast<VideoPreset>((index+3+(direction<0?-1:1))%3);
}
inline const char* video_hwdec(VideoPreset preset) {
    // Copy-back modes work with software-rendered libmpv and can gracefully
    // fallback on unsupported codecs or devices.
    return preset==VideoPreset::Compatible?"no":"nvdec-copy,d3d11va-copy,auto-copy";
}
inline const char* video_scaler(VideoPreset preset) {
    return preset==VideoPreset::Sharp?"lanczos":"bicubic";
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
