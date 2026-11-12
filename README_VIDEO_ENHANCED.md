# XuperTV Enhanced — Stage 1 (experimental)

This preview branch builds on the complete Windows TV + iPhone remote integration.

## What works in stage 1

- **Optional NVIDIA NVDEC decoding (copy-back mode).** With the current SDL2 + libmpv software render architecture, GPU-decoded frames must be copied back to system RAM. The decoder tries `nvdec-copy`, then `d3d11va-copy`, and safe software fallback. Hardware decoding depends on the installed drivers and the content codec, so it is **not guaranteed to be active**. Incompatible GPUs still work.
- **Software scaling quality presets.** Balanced uses bicubic. Sharp uses Lanczos through mpv's software scaler. Neither is AI upscaling.
- **Compatible / Balanced / Sharp presets**, available at the bottom of the in-player options menu. The setting persists in `video_enhanced.txt` (beside the EXE) and takes effect on the **next** video playback. Compatible disables hardware decoding.
- The player diagnostics show the **actual** `hwdec-current` reported by mpv rather than claiming NVIDIA is active without verification.
- Remote wheel/scroll now uses a reliable SDL user event and keeps the original iPhone control and 1080p output preview.

## Not yet implemented

- **libplacebo GPU renderer** and **FSRCNNX shader**: these require a native GPU rendering path. The existing app currently uses `MPV_RENDER_API_TYPE_SW`, so writing shader settings alone would have no effect. Stage 2 needs to move from software compositing to a compatible GPU renderer without breaking cinematic menus, mouse events, secure playback headers, or the TV preview.
- **RTX Video Super Resolution**, **RTX Video HDR**, **RIFE / TensorRT interpolation**: not shipped or activated. They require separate validation and/or SDK integration.
- Current video output is 1920x1080 on Windows, but UI coordinates still use a 1280x720 logical layout.

## How to test

1. Download the Windows ZIP artifact for branch `feature/video-enhanced-stage1` once CI succeeds.
2. Replace the portable folder as a unit. Keep your local configuration/private session files off GitHub.
3. Play a video; open Options; choose `Video mejorado` to cycle preset; reopen a video.
4. Open `Informacion del video` and inspect the decoder: `nvdec-copy`, `d3d11va-copy`, or `no` means actual fallback.
5. Compare CPU/GPU usage and visible quality on real 720p/1080p content before merging.

Windows build/test and a real device test are required before any merge to main or release.
