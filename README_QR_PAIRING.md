# XuperTV: QR pairing + remembered iPhone (experimental)

## Pair once, keep access across updates

1. On Windows, launch XuperTVDesktop.exe, click **QR iPhone** in the upper-right corner or press **F9**.
2. On Apple Controller, tap **Escanear QR**, allow the camera, and scan the code. Manual IP + PIN is still available.
3. iOS sends a PIN pairing request and stores the resulting authentication token securely in Keychain.
4. Future launches reuse the token automatically. A transient Wi-Fi outage does not erase it.
5. Keep the same iOS bundle ID and signing Keychain access group, and the same Windows user account, when updating.

## Windows credential persistence

The old remote_secret.txt lived beside the EXE. Updating into a clean folder could generate a different key and disconnect previously linked phones.

The new protected file lives at %LOCALAPPDATA%\XuperTV\pairing.dat, separately from portable program folders.
Windows DPAPI encrypts it for the Windows account. A valid old remote_secret.txt is migrated automatically on first launch, then its plaintext copy is deleted.
An unreadable protected file will not be silently overwritten and rotate tokens.

The QR includes only the local IPv4 address and a fresh six-digit PIN for pairing. It never includes the permanent bearer token. A different PIN on next launch does not invalidate remembered phones.

## Security / limits

- Existing LAN-only request validation, PIN attempts limit and bearer authorization remain in place.
- Close the pairing overlay after scanning. It is accessible via F9.
- The iPhone action **Olvidar esta PC** deletes only its own stored token; it does not revoke other paired phones remotely.
- A different app signing identity or iOS Keychain access group can prevent access to previously saved tokens. Device wipes or changed Windows accounts require reconnecting.
- If the PC IP changes, enter the new IP or scan again. LAN host discovery is not included.
- PC + iPhone real-hardware verification remains necessary. No main merge or release.