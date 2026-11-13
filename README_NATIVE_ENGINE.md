# XuperTV Native Engine — Pilot v1 (Windows)

We are replacing **the playback engine** incrementally, not rewriting codecs or the entire catalog.

## Why

The legacy video path uses libmpv's OpenGL render API and reads every 1080p frame back into CPU memory to pass it to SDL2. Switching FSRCNNX on/off can still stutter; OpenGL-to-SDL transfers introduced an upside-down video bug. A native Direct3D 11 renderer can remove these unnecessary transfers after the decoder and render paths have been joined safely.

## What this branch actually implements

- **Independent FFmpeg demux and video decoding probe:** `XuperNativeProbe.exe`.
- Attempts **D3D11VA hardware video decode** using FFmpeg. Falls back to CPU decoding when unsupported.
- Reports **actual decoded frames, codec, resolution, FPS, hardware decoder**, and how many audio tracks the media has.
- Exercises the native decoder with a bundled, local `test.mp4` and dedicated Windows CI tests.
- Lives **alongside the current XuperTVDesktop.exe**, with no changes to production playback, authenticated media requests or UI.
- Does **not** yet show frames on screen or play sound. A successful probe only proves decoding works; it does **not** prove end-to-end playback or upscaling is working.

## How to test

Unzip the entire portable folder. Open a terminal in that folder:

```powershell
.\XuperNativeProbe.exe test.mp4 24
.\XuperNativeProbe.exe test.mp4 24 cpu
```

Compare `decoder=d3d11va` versus `decoder=cpu`. **On an actual NVIDIA-equipped Windows PC**, `d3d11va` may be selected when the codec and driver support it. The CI runner is not a substitute for testing on the RTX 3060.

The old `XuperTVDesktop.exe` remains intact and functional. If the native probe fails, continue using XuperTV normally.

## Architecture: native Windows playback engine

```
XuperTV catalog / existing service auth / subtitles / iPhone controller
                               |
                   PlaybackController (future)
                         /             \
               Legacy libmpv      Native Windows engine
                                  |  FFmpeg demux (HLS, files, streams)
                                  |  FFmpeg video decode (D3D11VA/CPU)
                                  |  FFmpeg audio decode + SDL WASAPI audio
                                  |  Frame scheduler / audio clock
                                  |  Direct3D 11 surface and shader chain
                                  |  SDL2 UI composited on the GPU
                                  |  render diagnostics and A/B comparison
```

**What stays:** catalog, authentication, navigation, Apple Controller protocol, subtitles and user profiles, existing DRM/access restrictions and platform rules. Never copy private service tokens into logs or test artifacts.

**Why FFmpeg:** video formats, HLS/DASH, subtitles and audio are enormous projects on their own. Keeping FFmpeg for codecs lets us own the GPU presentation, frame timing, caching and filters without reinventing incompatible decoders.

## Next milestones

1. **v1 (this branch):** FFmpeg native decoder, D3D11VA detection, software fallback, CI.
2. **v2:** Actual D3D11 video swapchain with proper texture/orientation and a locally playable test file, still separate from XuperTV.
3. **v3:** Audio playback and A/V sync, seeking, streams and HLS buffering, subtitling, error recovery. Existing player remains available.
4. **v4:** Migrate XuperTV live TV and movie integration behind a backend selector; preserve authenticated headers, playlist and remote control.
5. **v5:** True GPU postprocessing (FSRCNNX, libplacebo-compatible shaders or custom HLSL), GPU timing and instant A/B without rebuilding the video pipeline. Add safe fallback and performance measurement.

**No merge to main, no release, no change to the currently working app** until native playback is stable and verified on the target PC.

## Credits

FFmpeg is a third-party multimedia project; its license(s), any enabled external libraries and redistribution obligations must be reviewed before distributing a production version. The probe is an engineering preview.
