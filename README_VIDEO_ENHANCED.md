# XuperTV Enhanced Stage 3 — Experimental GPU FSRCNNX

## What this build actually does

The fifth playback preset **GPU FSRCNNX (experimental)** attempts an OpenGL 3.3 libmpv renderer with the original **FSRCNNX x2 8-0-4-1** shader by igv. This is a **real mpv GLSL neural-network upscaling shader** (not NVIDIA RTX VSR). It is used when the output-to-source upscale factor is suitable, such as 720p to 1080p; 1080p already displayed at 1080p may look unchanged.

### Why this is experimental

XuperTV still uses SDL2 to draw its menus, tiles, subtitles and TV guide. Instead of replacing that rendering system in one risky step, Stage 3 renders the video offscreen using mpv's OpenGL render API, then **reads that frame back from GPU memory to an SDL video texture**. Consequently:

- FSRCNNX itself can run on the GPU, but **this is not a zero-copy GPU compositor** and video upload/readback may still be expensive. Expect possible frame pacing and CPU/GPU transfer overhead.
- If the GPU context cannot be initialized, the app falls back to the existing software rendering and reports **GPU: CPU fallback** in Video Information. No artificial GPU success indication.
- A real device test is required to verify shader compilation, image quality and timing. Windows GitHub-hosted runners usually cannot test an RTX GPU.
- FSRCNNX is separate from **RTX Video Super Resolution**, **RIFE**, **HDR** and **libplacebo gpu-next**. This build does **not** claim any of those features.
- Windows NVIDIA hardware decoder NVDEC remains available as `nvdec-copy` as in Stage 1. It does not imply the rendering shader is active.

## How to test

1. Keep your existing Original, Enhanced Stage 1 and Stage 2 folders unchanged. Extract Stage 3 as a completely **new folder** with the `shaders` subfolder next to `XuperTVDesktop.exe`.
2. Play a 720p (1280x720) video and open **Opciones → Video mejorado**. Select **GPU FSRCNNX (experimental)** and start the video again.
3. In **Informacion del video**, check **GPU: OpenGL readback / CPU fallback** and **FSRCNNX: ON/OFF**. The former proves the OpenGL render backend started, but not that the image looks better.
4. Press **F8** while on the same scene to toggle the shader (or use **Comparar GPU** in Options). Initial shader compilation can still cause a short stutter. If performance is poor, compare frame pacing and use the previous folder/preset.
5. Expect increased video-latency in this experimental readback pipeline. This is the core issue a proper **zero-copy** integration must later solve.

## Credits and license

The file `shaders/FSRCNNX_x2_8-0-4-1.glsl` is copyright **2017–2021 igv**. It is redistributed without modifications under **LGPL-3.0-or-later**, with its original copyright/license header preserved. Its full license is at `licenses/LGPL-3.0-only.txt`, with the optional later-version clause preserved in the source header.

Source mirror used: https://github.com/iwalton3/default-shader-pack/blob/master/shaders/FSRCNNX_x2_8-0-4-1.glsl

Upstream shader author: https://github.com/igv/FSRCNN-TensorFlow/releases

This development branch is **not** merged to main and is **not** a release. Verify on Windows + NVIDIA hardware before any merge.
