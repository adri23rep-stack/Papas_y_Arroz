// SPDX-License-Identifier: GPL-3.0-only
// NovaVeyra native GPU engine ABI.
// Reuses Veyra-NRVideo 2.1.9's actual EngineController via a separate MSVC DLL.
// No C++ objects, Qt objects, STL strings or runtime-owned memory cross the ABI.
#pragma once
#include <stdint.h>

#define NOVA_VEYRA_NATIVE_ABI 1u
#define NOVA_VEYRA_FEATURE_NR 0x01u
#define NOVA_VEYRA_FEATURE_SR 0x02u
#define NOVA_VEYRA_FEATURE_FG 0x04u

// Values match Veyra's FrameGenerationBackend (EnhancementSettings.h).
#define NOVA_VEYRA_FG_DLSS 0u
#define NOVA_VEYRA_FG_XESS 1u
#define NOVA_VEYRA_FG_FSR 2u
#define NOVA_VEYRA_FG_FSR4 3u
#define NOVA_VEYRA_FG_VFG 4u
#define NOVA_VEYRA_FG_ANR 5u
#define NOVA_VEYRA_FG_AMD_FRC 6u

// Transport values match Veyra's TransportState enum, not guessed.
#define NOVA_VEYRA_EMPTY 0u
#define NOVA_VEYRA_OPENING 1u
#define NOVA_VEYRA_PLAYING 2u
#define NOVA_VEYRA_PAUSED 3u
#define NOVA_VEYRA_ENDED 4u
#define NOVA_VEYRA_STOPPING 5u
#define NOVA_VEYRA_FAILED 6u

typedef struct NovaVeyraSettings {
    uint32_t size; // sizeof(NovaVeyraSettings) required
    uint32_t features; // NR / SR / FG bits; 0 = unprocessed
    uint32_t fg_multiplier; // 1 means off, 2/3/4/6 for DLSS
    uint32_t fg_backend; // NOVA_VEYRA_FG_*; use actual provider availability
    uint32_t sr_quality; // 0 DLSS SR, 1..4 RTX Video SR quality levels
    uint32_t software_decode; // 0 uses hardware if possible
} NovaVeyraSettings;

typedef struct NovaVeyraSnapshot {
    uint32_t size; // sizeof(NovaVeyraSnapshot) required
    uint32_t transport; // NOVA_VEYRA_*; from real Veyra snapshot
    uint32_t failed;
    uint32_t running;
    uint32_t nr_active; // actually applied, not requested
    uint32_t sr_active;
    uint32_t fg_active;
    uint32_t hdr_active;
    uint32_t source_width;
    uint32_t source_height;
    uint32_t fg_max_generated; // 0 when unqueried/unknown
    uint32_t submit_fps_known;
    double position_seconds;
    double duration_seconds;
    double source_fps;
    double submit_fps;
    uint64_t source_frames;
    uint64_t generated_frames;
    uint64_t session_id;
} NovaVeyraSnapshot;

#if defined(_WIN32)
  #define NOVA_VEYRA_CALL __cdecl
  #if defined(NOVA_VEYRA_NATIVE_EXPORTS)
    #define NOVA_VEYRA_EXPORT __declspec(dllexport)
  #else
    #define NOVA_VEYRA_EXPORT
  #endif
#else
  #define NOVA_VEYRA_CALL
  #define NOVA_VEYRA_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// NULL on failure. Engine lifecycle is asynchronous; open() only queues work.
// HWND is the *video child window supplied by Nova*, not a Veyra UI window.
// All exception handling stays within the DLL's MSVC runtime.
NOVA_VEYRA_EXPORT uint32_t NOVA_VEYRA_CALL nova_veyra_abi(void);
NOVA_VEYRA_EXPORT void* NOVA_VEYRA_CALL nova_veyra_create(void);
NOVA_VEYRA_EXPORT void NOVA_VEYRA_CALL nova_veyra_destroy(void* engine);
NOVA_VEYRA_EXPORT int NOVA_VEYRA_CALL nova_veyra_open(
    void* engine, void* hwnd, const wchar_t* source, const NovaVeyraSettings* settings);
NOVA_VEYRA_EXPORT void NOVA_VEYRA_CALL nova_veyra_stop(void* engine);
NOVA_VEYRA_EXPORT void NOVA_VEYRA_CALL nova_veyra_pause(void* engine, uint32_t paused);
NOVA_VEYRA_EXPORT void NOVA_VEYRA_CALL nova_veyra_seek(void* engine, double seconds);
NOVA_VEYRA_EXPORT void NOVA_VEYRA_CALL nova_veyra_volume(void* engine, float volume);
NOVA_VEYRA_EXPORT int NOVA_VEYRA_CALL nova_veyra_rate(void* engine, double rate);
NOVA_VEYRA_EXPORT int NOVA_VEYRA_CALL nova_veyra_settings(
    void* engine, const NovaVeyraSettings* settings);
NOVA_VEYRA_EXPORT int NOVA_VEYRA_CALL nova_veyra_snapshot(
    void* engine, NovaVeyraSnapshot* out);
#ifdef __cplusplus
} // extern "C"
#endif
