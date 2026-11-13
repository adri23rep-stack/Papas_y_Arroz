# XuperTV Native Player v2 — Windows technical preview

This is a **real, independently built local-file video+audio player** using FFmpeg and SDL2. It is distributed in the same portable ZIP as the unchanged working `XuperTVDesktop.exe`, and does **not** use libmpv for native playback.

## What's working in the new executable

- `XuperNativePlayer.exe` directly opens local MP4/MKV/other FFmpeg-compatible video files.
- FFmpeg demuxing and video/audio codec decoding.
- Attempts **D3D11VA hardware video decoding**, with CPU startup fallback. Reports the **actual** decoder used after receiving frames.
- SDL2 GPU video renderer, requesting Windows **Direct3D 11**, with software fallback if unavailable. Video orientation uses CPU BGRA conversion and a normal SDL texture (no inverted OpenGL buffer).
- **Audio**: stereo 48 kHz float PCM, FFmpeg swresample, SDL queued playback, volume control, and switching between supported audio tracks.
- Sidecar **SRT subtitles**: `movie.srt` beside `movie.mp4`; use `C` to show/hide captions. These are not yet in-container subtitle streams.
- Double-click `XuperNativePlayer.exe` for a Windows file picker; dragging a local video onto it still works.
- Approximate video sync to the queued audio clock, with late-frame dropping and a monotonic fallback clock.
- Window resizing and fullscreen, 16:9/4:3 aspect fit, seek ±10/60 seconds, play/pause, click-to-seek timeline.
- Scale comparison with **F8** switches between nearest and linear texture sampling. **This is not FSRCNNX or RTX VSR.** No heavy filters are rebuilt on each toggle.
- On-screen status with codec, decoder, SDL renderer, progress and shortcuts (requires bundled `font.ttf`).
- CI tests its video pipeline with a bundled MP4, verifies its portable DLL dependencies without relying on runner PATH, generates a red-top/blue-bottom video to verify upright output, and exercises audio decoding with a synthesized tone.

## Controls

| Input | Action |
|---|---|
| Space | Pause / resume |
| Left / Right | Back / forward 10 seconds |
| Page Down / Page Up | Back / forward 60 seconds |
| Home | Seek to beginning |
| Up / Down | Volume ±5% |
| F / F11 | Fullscreen on / off |
| F8 | Nearest / bilinear GPU texture scaling (A/B only, not AI) |
| A | Switch audio track (when several are available) |
| C | Toggle external SRT subtitles |
| I | Toggle technical overlay |
| Mouse click on bottom timeline | Seek to point |
| Escape | Exit native player |

## How to test without modifying XuperTV

1. Extract the whole ZIP into a **new** folder. Keep original XuperTV and Stage 3 folders.
2. First open `XuperTVDesktop.exe` to ensure the existing program is unchanged.
3. Double-click `XuperNativePlayer.exe` to choose a file, drag a **local MP4** onto it, or use PowerShell:

```powershell
.\XuperNativePlayer.exe "C:\Videos\example.mp4"
.\XuperNativePlayer.exe "C:\Videos\example.mp4" --cpu
.\XuperNativePlayer.exe test.mp4 --cpu --smoke 24
```

4. Compare the same local scene against XuperTV's old renderer. Check `decoder=D3D11VA` in the console after decoding, and `Render: direct3d11` in the on-screen overlay when SDL selects D3D11.

## Important honest limitations

- **Local-file only.** The standalone player deliberately does not receive service authentication headers, cookies, private tokens, or arbitrary remote streams. Connecting TV/series/movie URLs and the companion iPhone control to the new engine will follow after it is stable.
- **Video is still transferred through CPU memory.** With D3D11VA, FFmpeg decodes using the GPU, transfers decoded frames to CPU, converts to BGRA, then SDL uploads a texture to its accelerated renderer. This is **not zero-copy** and can be slow at 4K.
- **Not yet included**: streaming HLS with credential-safe auth, remote control protocol, embedded subtitle tracks, volume normalization, RIFE interpolation, FSRCNNX GPU shader in the *new* engine, and seamless switching inside XuperTV.
- **Audio synchronization is an early implementation**: live streams and weird timestamp discontinuities need more work. Some unsupported audio formats may play silently rather than preventing video.
- **Renderer backend**: requests SDL's Direct3D 11 backend but may use another GPU renderer/software on some systems. The overlay shows what actually started.
- Current Windows CI tests on a runner, not on the RTX 3060. A passing test does not confirm playback quality or GPU acceleration on the user's hardware.
- **Licenses**: assess FFmpeg binary license and linked codecs before production distribution. Do not merge until validated.

## Planned next stage

After native local playback is confirmed: better A/V buffering, embedded subtitle selection, safe authorized streaming integration and finally a **true D3D11 zero-copy frame compositor** with GPU sharpening/FSRCNNX.

Branch: `feature/xuper-native-player-v2`. No merge to main and no release.
