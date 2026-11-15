# XuperTV — Detected platform catalogues in iPhone remote

## How this works

The PC's authenticated service already returns actual video-on-demand categories inside its **Películas** and **Series** sections. Each category has a real ID, its name and an item type. When the phone opens the **Plataformas** folder, Windows inspects those real category labels and groups matching categories by platform name.

Recognized labels include Netflix, HBO Max, Apple TV, YouTube, Disney+, Prime Video, Paramount+, ViX, Crunchyroll, Pluto TV, Hulu, Peacock and MUBI. These are **name matches**, not official integrations or guarantees that the platform has content in this service. A provider appears only if a corresponding category exists in the currently available service response.

The iPhone tiles for YouTube, Netflix, HBO Max and Apple TV now open the internal catalog, not the platform's external website. If the platform has no category in XuperTV, the phone shows an explicit unavailable/empty state rather than redirecting to the official site.

## Usage

- From the iPhone control, tap a brand tile to open that brand's catalog immediately.
- From the full **Biblioteca**, open **Plataformas** to explore detected brands.
- Tap a brand, then its real **Películas** or **Series** category, and choose a title or episode to play on Windows.
- Returning to the parent page and the original TV/Movies/Series sections is preserved.

## Protocol changes

The existing paired LAN-only `POST /catalog/browse/start` accepts an optional `{"provider":"netflix"}` (also `hbo_max`, `apple_tv`, `youtube`, and other known keys). The default request without `provider` retains the normal library view, with a new **Plataformas** folder.

The backend scans authenticated VOD category listings on a worker thread. Results are paginated through the existing `POST /catalog/browse/results` endpoint and still contain only display metadata; no stream URLs, credentials or cookies are sent to iOS. Selecting a title continues to use the already existing Windows playback command.

## Limitations

- Detection is based on platform **names in first-level movie/series categories**. It cannot infer that a movie is licensed by Netflix or HBO, and it doesn't scan every nested category.
- A YouTube tile without a corresponding YouTube-labeled category shows an unavailable message; official YouTube playback is deliberately **not** launched by that tile.
- Real platform content depends on the authenticated XuperTV service at that moment, and labels can change over time.
- There is no official streaming-provider login, scraping, bypass of subscriptions, or DRM modification.
- The PC and iPhone builds must be updated together. QR pairing and saved phone credentials are preserved from the previous feature branch, but real-device testing is still necessary.

Experimental branches: `feature/provider-catalog-tiles` (both repositories). No main merge, no release.
