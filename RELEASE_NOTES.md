# Discord Rich Presence Integration v2.0.3-ci303.1

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

- Added maintained fork documentation and configuration guidance.
- Added title-formatting examples for common Discord presence layouts.
- Made setup safer by requiring an explicit flag before resetting submodules.
- Added MusicBrainz/Cover Art Archive request timeouts and switched Cover Art
  Archive art URLs to HTTPS.
- Hardened uploader subprocess handling so stdout/stderr are drained while the
  process is running.
- Made playback time parsing defensive.
- Restored Release stack cookie checks.
- Improved build compatibility with newer Visual Studio/MSVC and installed
  Windows SDK versions.

## Validation

- Built `Release|x64` with Visual Studio 2022.
- Packaged `foo_discord_rich.fb2k-component`.
- Installed and smoke-tested the component in a local foobar2000 v2 x64
  installation.
