# Live TV stability experiment
The Windows player now recognizes a live channel explicitly, avoids the experimental
OpenGL readback path and secondary video preview for live TV, applies extra
network buffering, and retries failed channels up to two times. It tries
alternate service-provided source variants when available and never retries
a user-initiated exit or channel change.

This is a defensive mitigation, **not yet a verified diagnosis** of the
reported automatic player closure. If it still closes, preserve
`diagnostico.log` from the XuperTV folder and the channel name (but do
not share private authentication headers or URLs).

No main merge and no release.
