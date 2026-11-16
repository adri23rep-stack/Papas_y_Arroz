# Xuper TV NOVA — Windows UI preview

A completely new visual frontend, implemented independently of the existing
Cinematic UI. This is a **work-in-progress** and must not replace the stable
Windows experience before real device and remote-control testing.

## Try it

- Open `XuperTVDesktop.exe` from a Windows build of this branch.
- Press **F8** to switch between **NOVA** and the original interface.
- Your last choice is stored as `ui_mode.txt` beside the executable.
- **F9** keeps the existing iPhone QR pairing dialog.
- **F10** moves the window to the next monitor; **F11** toggles fullscreen.
- Search, directional keys, Back, Enter, favorites, and the iPhone remote still
  use the original app/controller logic.

## What NOVA has now

- New vertical navigation with six familiar destinations.
- New featured-content area with real catalogue title, description and art.
- Home rails: live TV, movies and series, using the existing artwork cache.
- Browse grids and posters with a strong selection focus.
- The existing pairing overlay and secure iPhone remote state.
- Full separation between UI layout (`include/nova_layout.hpp`) and
  rendering (`source/view_nova.cpp`); layout navigation and click hit-tests
  have their own automated unit test.

## What does NOT change

NOVA is only a frontend. It does not change authentication, service requests,
stream extraction, subtitles, playback, live TV recovery, saved favorites,
Wi-Fi pairing, HTTP endpoints, or the iPhone app. The Switch code remains intact.

No new dependencies, proprietary assets, subscription, logos from third-party
platforms, or fake programme data have been added.

## Remaining work before replacing Cinematic

- Validate new Windows binaries and remote/mouse focus on an actual PC.
- Add fluid scrolling and responsive layouts for alternate display sizes.
- Design dedicated settings and player controls to match the NOVA visual style.
- Improve transitions and polished interactive states without affecting
  playback responsiveness.

The old renderer remains accessible by pressing **F8** at any time.
