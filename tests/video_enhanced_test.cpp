#include "video_enhanced.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
int main() {
    using namespace xs;
    assert(video_preset_from_string("compatible")==VideoPreset::Compatible);
    assert(video_preset_from_string("sharp")==VideoPreset::Sharp);
    assert(video_preset_from_string("invalid")==VideoPreset::Balanced);
    assert(next_video_preset(VideoPreset::Balanced,1)==VideoPreset::Sharp);
    assert(next_video_preset(VideoPreset::Balanced,-1)==VideoPreset::Compatible);
    assert(next_video_preset(VideoPreset::Sharp,1)==VideoPreset::Compatible);
    assert(std::strcmp(video_hwdec(VideoPreset::Compatible),"no")==0);
    assert(std::string(video_hwdec(VideoPreset::Balanced)).find("nvdec-copy")!=std::string::npos);
    assert(std::strcmp(video_scaler(VideoPreset::Sharp),"lanczos")==0);
    const char* path="video_enhanced_test.tmp";
    assert(save_video_preset(VideoPreset::Sharp,path));
    assert(load_video_preset(path)==VideoPreset::Sharp);
    std::remove(path);
    assert(load_video_preset(path)==VideoPreset::Balanced);
    return 0;
}
