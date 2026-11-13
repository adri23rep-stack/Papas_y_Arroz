#pragma once
#include <string>

namespace xs {

// First independent part of the new Windows playback engine.
// Source and decoder diagnostics do NOT mean audio/video playback is complete.
enum class NativeDecoderKind { Cpu, D3D11VA };
struct NativeProbeResult {
    bool ok = false;
    bool hardware_requested = false;
    bool hardware_available = false;
    NativeDecoderKind active_decoder = NativeDecoderKind::Cpu;
    int width = 0;
    int height = 0;
    int video_frames = 0;
    int audio_streams = 0;
    double nominal_fps = 0;
    std::string codec;
    std::string diagnostic;
};

// Only local files are permitted for this diagnostic; normal XuperTV's
// authenticated service playback is intentionally still on the old engine.
// The probe validates genuine decode output, not just driver availability.
NativeProbeResult native_probe_local_video(const std::string& file_path,
                                            int max_frames = 24,
                                            bool prefer_hardware = true);
}
