# XuperTV Enhanced — Stage 2: Contrast-Adaptive Detail (experimental)

This branch preserves Stage 1 NVIDIA decode, the 1080p render buffer, iPhone remote, and original XuperTV menus.

## New features

- A fourth **Video mejorado** preset: **Detalle extra (CAS)**. The setting persists in `video_enhanced.txt` and takes effect when you start the **next** video.
- Real FFmpeg **Contrast Adaptive Sharpen** filter (`cas`), not just a decoder or a name-only setting.
- For native 16:9 SD/HD video below 1080p, a software Lanczos scale to 1920x1080 happens *before* CAS (`scale=1920:1080:flags=lanczos,cas=strength=0.30`). Other aspect ratios and HD/UHD sources preserve their resolution and use gentler sharpening (`cas=strength=0.24`).
- A/B comparison on the **same video** with **F8**, or the **Comparar imagen** row in Options. Toggle between the CAS output and the original filter-free output without switching XuperTV versions. The original video/audio source, subtitle settings, frame timing, and renderer remain unchanged.
- The in-player **Informacion del video** reports the current decode mode and whether CAS is enabled.

## Important limitations

- **NOT NVIDIA RTX Video Super Resolution, GPU shaders, libplacebo, FSRCNNX, or RIFE.** CAS and Lanczos here are performed by FFmpeg/libmpv on the CPU, despite hardware-assisted `nvdec-copy` decoding.
- Software sharpening can increase CPU usage, exaggerate compression artifacts, or produce halos. 720p to 1080p scaling cannot reconstruct details absent from the original source.
- Existing `MPV_RENDER_API_TYPE_SW` remains in place to preserve complex SDL2 overlays, live-TV previews, and menus. A separate GPU compositor is required for shader-based AI upscaling.
- Real video and iPhone testing is still needed. CI tests the filter, but cannot evaluate perceptual quality or performance on RTX 3060.

## How to A/B compare

1. Extract this build to a **new folder**, leaving your old XuperTV and Stage 1 versions untouched.
2. Play any 720p video, open **Opciones → Video mejorado**, choose **Detalle extra (CAS)**. Reopen the video once.
3. While the same scene plays, press **F8** to alternate CAS and Original. Alternatively, toggle **Comparar imagen** in Options with the remote's D-pad.
4. Pause on a detailed frame. Observe text, faces, hair, hard edges, blockiness, and halos. If worse or CPU usage spikes, use the **Nitidez alta** or **Equilibrado** preset.
5. Open **Información del video** to inspect `Decodificador` and `CAS: ON/OFF` separately. `nvdec-copy` means GPU decoding, **not GPU sharpening**.

## Rollback

Stop this build and launch your old folder, or change the preset to `Compatible`, `Equilibrado`, or `Nitidez alta`. No merge to main and no release without on-device validation.
