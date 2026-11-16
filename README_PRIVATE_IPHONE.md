# Experimental private iPhone player

The Windows-side authenticated mobile catalogue/source endpoints are integrated
in `windows-desktop-preview`. The private SwiftUI/PIN/AVPlayer interface lives
in the separate `adri23rep-stack/Apple-Controller` repository on
`feature/remote-no-scroll-layout`. The PC also includes bounded live TV recovery.

## Access

- While using the iOS remote, **hold the XuperTV header** or tap the tiny
  lock icon by the manual pairing PIN.
- Create your own six-to-twelve-digit private code on first entry.
- The code is salted and PBKDF2-hashed in iOS Keychain, never hard-coded or
  transmitted to Windows. Five incorrect tries temporarily lock the screen.
- The private screen still requires normal Windows pairing authorization.
- Normal remote tiles remain unchanged.

## Content

The PC implements independent `/mobile/catalog/*` endpoints so selecting
a title in the phone **does not launch or change the PC video**.

The private player requests `/mobile/source` for a selected **channel or
episode**, and uses iOS AVPlayer for compatible HTTPS MP4/HLS sources.
**Only direct HTTPS sources with NO provider-specific media headers** are
provided. Streams requiring Content-License, Content-Auth, encryption,
unsupported protocols, or DRM are blocked with a clear message.

This feature does NOT promise all TV channels/movies on the iPhone;
many live service streams require special playback headers and cannot
currently be opened in iOS AVPlayer. No DRM decryption, protected CDN
header export, web scraping, or external unrestricted proxy is implemented.

Both devices need to be on an accessible LAN for catalogue and video
selection; direct HTTPS playback from the CDN needs Internet access.

## Security

Source URLs are returned only to the previously paired iPhone bearer token
over the existing local network endpoint (current protocol uses LAN HTTP).
The optional private code is an **app-side privacy gate**, not a new
authorization level on the Windows server. The bearer token and service
account identity must never be placed in media URLs.

Keep `main` unchanged. Test paired iPhone + Windows together before any
release, especially mobile source restrictions, authorization, and LAN privacy.
