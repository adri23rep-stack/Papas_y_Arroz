# Apple Controller — YouTube browser mode (Windows)

Feature experimental added on top of `feature/search-remote-catalog`.
The iPhone app itself remains in the separate
[Apple-Controller repository](https://github.com/adri23rep-stack/Apple-Controller),
branch `feature/youtube-mode`.

## User flow

1. Connect iPhone and PC using the existing local pairing PIN.
2. On **Apple Controller**, tap the dedicated **YouTube** card.
3. The PC opens **https://www.youtube.com/** with your normal Windows browser,
   shown over Xuper TV. Xuper TV's libmpv playback is paused to avoid double
   audio.
4. Search a video on your iPhone; the PC opens YouTube's **normal search page**
   with your query. The iPhone does not scrape or copy YouTube result thumbnails.
5. Use the **same iPhone trackpad** for the actual Windows pointer, with tap
   to click and press/drag. The arrows and OK drive normal browser keys.
6. From the dedicated remote, use pause/play, full screen, mute, seek ±10
   seconds, next video, browser back, and Windows volume up/down.
7. Tap **Regresar a Xuper TV** to raise the Xuper TV window again. Xuper TV
   remains paused; resume manually when ready.

## Constraints

- This is **not** an unofficial YouTube decoder or a downloader: the user
  watches the real YouTube site with their own browser and account.
- **No YouTube API key or login token is needed**. Search is a URL to
  `www.youtube.com/results?search_query=...`, with strict UTF-8 percent
  encoding and a length limit.
- Input keys and mouse movement work only if the foreground Windows window
  identifies itself as **YouTube** in its window title. If you use another
  app or the YouTube tab is not active, commands are rejected for safety.
- The browser opens on its usual display. Move it to your HDMI TV if Windows
  didn't remember your previous placement; the iPhone cannot force a browser
  tab to appear on a particular monitor yet.
- YouTube's original video player determines which keyboard shortcuts work;
  some controls require the player/video to be focused. We use the shortcuts
  documented by YouTube, not a hidden playback API.
- Volume +/- controls the **Windows master volume**, not just Xuper TV.
- This v1 mode shows search results on the TV/browser. For **YouTube
  thumbnails inside the iPhone app**, a later integration with the official
  YouTube Data API or a separately authorized approach would be needed.
- No public internet remote/control, arbitrary URLs, arbitrary shell commands,
  extraction of videos, ad blocking or proxy traffic.
- LAN control uses the existing pairing token over plain HTTP on a **trusted
  private network**. Never forward port 47831 or connect through public Wi-Fi.

## Validation

GitHub Actions compiles the Windows desktop, iOS client and tests URL encoding
and authorization. It cannot fully emulate the target PC's default browser,
YouTube login, fullscreen behavior, or pointer across multiple monitors.
Validate these on the actual iPhone and Windows/HDMI setup.

**No merge to main and no release until user validation.**
