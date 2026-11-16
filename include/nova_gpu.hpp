#pragma once
#include <filesystem>
#include <string>
#include <system_error>

// RIFE is OPTIONAL: never let a missing Vulkan plugin/model break regular MPV.
// The plugin is loaded by VapourSynth, which is already supported by MSYS2 mpv.
// A real GPU test is required; existence of these files is only a preflight.
namespace xs {
inline const char* nova_gpu_script_rel() { return "nova-gpu/rife.vpy"; }
inline bool nova_rife_files_present(const std::filesystem::path& root) {
    const auto dir=root/"nova-gpu";
    std::error_code ec;
    for(const auto& f : {
        root/"libvapoursynth.dll",
        root/"libvapoursynth-script-0.dll",
        dir/"rife.vpy",
        dir/"librife_windows_x86-64.dll",
        dir/"models"/"rife-v4.6_ensembleFalse"/"flownet.bin",
        dir/"models"/"rife-v4.6_ensembleFalse"/"flownet.param"
    }) {
        if(!std::filesystem::is_regular_file(f,ec) || ec) return false;
        ec.clear();
    }
    return true;
}
}
