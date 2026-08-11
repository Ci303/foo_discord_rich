# Discord Rich Presence Integration v2.0.3-ci303.6

This release adds explicit album-artwork display policies, live artwork
resolution status, and focused Discord activity-conflict guidance in
Preferences, while preserving artwork-first behaviour as the default.

This is a maintained fork release of Discord Rich Presence Integration for
foobar2000.

Original project:
https://github.com/TheQwertiest/foo_discord_rich

Original author:
TheQwertiest

Fork maintainer:
Ci303

The original MIT license and third-party notices are preserved.

## What's Changed

- Added explicit album-artwork behaviour choices: prefer artwork with a
  configured large-image fallback, use the configured large image without
  artwork requests, or show artwork without a fallback.
- Added live artwork status and clear pending-versus-applied feedback in
  Preferences.
- Added in-component guidance for Discord recognised-application conflicts.
- Added a Discord asset-key manifest and aligned the included playback-state
  PNG filenames with their Portal keys.
- Changed the component default to the maintained `Foobar2000` Discord
  application; a persisted one-time migration replaces only the legacy default
  ID and preserves existing custom IDs.
- Isolated MusicBrainz and uploader cache entries, keyed valid MusicBrainz
  album IDs independently, and moved to a freshness-checked version 4 cache.
- Prevented cache clear/reload and superseded artwork requests from publishing
  stale results; successful cache load/clear actions now re-evaluate the active
  track where applicable and report failures accurately.
- Preserved Discord presence with its configured fallback if the artwork worker
  fails, and made MusicBrainz shutdown cancellation responsive.
- Fixed paused presence being restored when Discord reconnects.

## Validation

- Built and packaged `Release|x64` and `Release|Win32`.
- Passed the Python release and asset tests, focused C++ validation, package
  integrity checks, and release-metadata verification.
