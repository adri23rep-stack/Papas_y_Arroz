# Apple Controller LAN server (Windows)

This branch extends the **working Cinematic Windows build** with a network-controlled remote for the separate native iPhone app [Apple-Controller](https://github.com/adri23rep-stack/Apple-Controller). The media backend, source authorization, library and mpv decoder remain unchanged.

## Getting started
1. Install the Windows preview artifact **from this branch**.
2. Open `XuperTVDesktop.exe`; the bottom right of the cinematic home screen shows `iPhone: 192.168.x.x:47831 | PIN: 123456`.
3. On the same private Wi-Fi/LAN, open the Apple Controller iPhone app and enter the IP **without the port**, plus the PIN.
4. Approve the Windows Defender firewall prompt for private networks only.

The PIN changes at each desktop launch. After pairing once, the app saves an authorization token in the iOS Keychain and Windows `remote_secret.txt`, so reconnecting does not require repeating the PIN unless the token is rotated.

## API contract (v1)

TCP port `47831`, local/private IPv4 only, simple request-per-connection HTTP; no public internet exposure.

- `POST /pair` body `{"pin":"XXXXXX"}` returns `{"token":"<64 hex chars>","protocol":1}` (maximum 5 bad attempts before temporary lockout).
- `POST /input` uses `Authorization: Bearer <token>` and body `{"action":"up"}`; returns `{"ok":true}`.
- Commands: `up`, `down`, `left`, `right`, `ok`, `back`, `home`, `tv`, `series`, `movies`, `search`, `text`, `play_pause`, `menu`, `next`, `previous`, `click`, `pointer_move`, `pointer_down`, `pointer_up`, `volume`, `brightness`, `ping`.
- `pointer_move`: additional integer `dx`, `dy` (logical 1280×720 scene).
- `text`: additional field `text`, injected through SDL text-input events (only while the player displays a search field).
- `volume` and `brightness`: integer `value` 0–100; brightness maps to libmpv software video brightness, **not physical TV brightness**.
- `tv`, `movies`, `series`, `home` also exit video playback and open the requested section.

Security: only private LAN ranges are accepted; no arbitrary key events, shell commands, filesystem paths or arbitrary URLs; no external relay. Treat Wi-Fi as **trusted** because v1 uses unencrypted HTTP. Never port-forward TCP 47831; don't use public Wi-Fi. Random secret stays off GitHub.

## Status

GitHub Actions builds Windows desktop and runs existing CTest; the iPhone app is built separately in its own repo. An end-to-end test with a real iPhone, router and HDMI connected PC is still needed.

**No merge to main and no release until verified.** Attribution remains with JonaRuacho for the original Xuper TV homebrew.
