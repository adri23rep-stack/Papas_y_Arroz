# Xuper TV Cinematic — TV UI experiment (Windows)

Experimental front-end based on the first cinematic UI concept. This branch
preserves the working Windows player and device bootstrap from
`windows-desktop-preview`, and only replaces the on-screen interface and
extends playback interaction.

## Main menu
- Dark cinematic theme, red accents and full-width featured-content panel.
- Top navigation: **Inicio / TV en vivo / Películas / Series / Favoritos / Apoyar**.
- Artwork and selected item appear in the hero; the **Abrir ahora** button works.
- Ten visible, mouse-clickable/focusable cards per page. Keyboard arrows and
  PgUp/PgDn retain pagination.
- F10 moves between displays, F11 toggles HDMI full screen.

## Live TV controls
- When playing a channel, **click the video** (or move the pointer near the
  bottom) to reveal a horizontal channel rail.
- **Drag** the rail with the left mouse button, or **use the mouse wheel**, to
  browse more channels.
- A **short click** on a channel starts it. The regular source resolver and
  player are reused: protected content is never sideloaded.
- **Press and hold a tile (~550 ms)** for a picture-in-picture preview card.
  Windows tries to resolve that channel through the existing service and plays
  a separate **muted 320×180 live video preview** while the main program
  continues playing. Network/server permissions may block secondary video;
  the card then shows the channel artwork and status instead.
- With the rail visible, left/right chooses a channel, **Enter** tunes, and
  **Esc / Up** closes it. Down opens it while hidden.
- The rail hides after a few seconds without interaction.
- Existing libmpv playback controls, audio, subtitles and TV/HDMI shortcuts
  remain available.

## Safety and status
- The preview runs only on demand and is muted. The URL and headers come from
  the same authorized live-channel resolver used by the existing player.
- A lightweight background request prevents freezing the main picture while
  resolving the selected channel.
- Background live previews can consume additional bandwidth and CPU.
- Source availability, regional restrictions, and actual HDMI playback must
  still be tested on a real PC. Automated C++ tests cannot prove streaming.

**No release; no merge into main until validated.**
