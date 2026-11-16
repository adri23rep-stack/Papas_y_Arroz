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

- New home screen with a full-width catalogue artwork backdrop, randomly chosen
  from a real movie or series for each Windows app launch.
- The image fades completely into a dark background at the FIRST row.
- Three uniform horizontal carousels, in order: TV, Series, Movies.
  Scroll with the wheel over a row, drag horizontally, or use left/right.
- The icon and heading of each row open the corresponding full catalogue.
- The old layout is still available through F8; other catalog views are
  intentionally unchanged until we design them together.
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
- Adapt the home layout for alternate logical resolutions after user feedback.
- Design dedicated settings and player controls to match the NOVA visual style.
- Improve transitions and polished interactive states without affecting
  playback responsiveness.

The old renderer remains accessible by pressing **F8** at any time.
