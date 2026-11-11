#pragma once

namespace xs {
// Keep the Switch build unchanged. On Windows, rasterize glyphs with extra
// detail before SDL scales the 1280x720 UI to the display's actual resolution.
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
