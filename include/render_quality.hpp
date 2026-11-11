#pragma once
namespace xs {
// Keep the Nintendo Switch memory and rendering profile unchanged.
#if defined(_WIN32)
inline constexpr int kFontRenderScale=2;
inline constexpr int kVideoRenderWidth=1920;
inline constexpr int kVideoRenderHeight=1080;
#else
inline constexpr int kFontRenderScale=1;
inline constexpr int kVideoRenderWidth=640;
inline constexpr int kVideoRenderHeight=360;
#endif
}
