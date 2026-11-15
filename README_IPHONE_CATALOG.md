# XuperTV iPhone Full Catalogue + Desktop Settings (Experimental)

## In the iPhone app

- Open **Biblioteca** from the new fourth top shortcut next to fullscreen, search and settings.
- Navigate real authenticated **TV en vivo, Películas, Series, Categorías, Títulos, Episodios**, including additional pages of up to 48 entries.
- TV channels and episodes start playing **on the Windows PC** when tapped. Opening a movie automatically resolves the permitted movie detail and plays its first available video; opening a series shows its episodes first.
- The iPhone displays titles, descriptions and HTTPS artwork metadata. It never receives private stream URLs, media licences, session credentials or account cookies.
- The gear opens a desktop-settings panel: volume, brightness, playback speed, screen aspect, auto-next, mute, audio delay, subtitle delay, subtitle size and video enhancement preset; fullscreen and monitor-switch actions are also available.

## Windows protocol (paired local network only)

All requests use the existing LAN-only server on port 47831 with a verified pairing bearer token. Requests are JSON POST.

- `/catalog/browse/start` → `{generation,state}`
- `/catalog/browse/results` ← `{generation,offset,limit}` → title, description, state, total, paginated entries
- `/catalog/browse/select` ← `{generation,index}` → loading next page or launching Windows playback
- `/catalog/browse/back` ← `{generation}` → parent folder
- `/settings/get` → ten available keys, value/range/step/category and labels
- `/settings/set` ← `{key,value}` → validated preference and SDL UI event

The browsing worker uses a separate service session copy to avoid writing channel caches from HTTP worker threads. Playback routes back to the main SDL thread and existing service-authorized `service_source`. No new external media source parser or DRM bypass was introduced.

## Caveats

- This feature targets the **existing XuperTVDesktop.exe** with the working libmpv service playback, **not** `XuperNativePlayer.exe` yet. The independent native engine stays available for local test videos only.
- Changing video quality affects the **next** video. Other settings update live when the old mpv player is active and act as defaults otherwise. Some old menu changes may not be reflected in the iPhone until refreshed.
- Only settings the PC currently supports are exposed. No fictional general Windows system settings, service account management or remote installation.
- iOS and Windows build validation is not the same as real-world testing with a PC + iPhone. Verify authorization, mixed VOD categories, episode launch, artwork, screen layout and performance on the actual hardware.
- Keep the current Windows app and iPhone IPA in separate folders/backups until verified.
- No merge to `main` and no release.

Branches: PC `feature/iphone-catalog-settings`, iOS `feature/iphone-catalog-settings`.
