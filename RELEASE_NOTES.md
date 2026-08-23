# Discord Rich Presence Integration v2.0.3-ci303.7

This release makes album-artwork sources configurable and adds TheAudioDB as
an optional final fallback, while preserving the existing MusicBrainz / Cover
Art Archive and custom-uploader workflows.

This is a maintained fork release of Discord Rich Presence Integration for
foobar2000.

Original project:
https://github.com/TheQwertiest/foo_discord_rich

Original author:
TheQwertiest

Fork maintainer:
Ci303

The original MIT licence and third-party notices are preserved.

## What's Changed

- Added a Providers preference tab which owns the artwork-source settings;
  provider-specific live status remains visible on the Main tab.
- Added TheAudioDB as an optional fallback using the user's own supporter key.
  The key is masked in Preferences, stored for the current Windows user in
  Windows Credential Manager, excluded from the artwork cache, and redacted
  from request logging.
- Kept local or embedded foobar2000 artwork first when a trusted external
  uploader is configured, followed by MusicBrainz / Cover Art Archive and then
  TheAudioDB. Each source now has separate cache identity and status.
- Prevented Credential Manager failures from disrupting playback presence and
  stopped repeated TheAudioDB requests after a rejected key until that key is
  replaced or explicitly tested again.
- Made clearing the stored TheAudioDB key transactional with Preferences Apply
  and Reset, and preserved pending key edits and test state across tab switches.
- Made custom-uploader tests respond to cancellation and clean up their child
  process.
- Made the local build script discover and validate Python 3.10 or newer,
  avoiding a hard dependency on the Windows `py` launcher.

## Provider scope

The built-in online metadata sources are MusicBrainz / Cover Art Archive and
TheAudioDB. Local or embedded foobar2000 artwork can be passed to a configured
external uploader. This release does not add an arbitrary-provider plug-in
framework, a general local-directory browser, or bundled artwork hosting.

Do not put API keys or access tokens in the custom-uploader command: process
command lines and this setting are not secret storage. Use the provider's
supported credential field or the uploader's own secure configuration.

## Validation

- Python release, asset, provider, and build-tooling tests.
- Focused strict C++ validation.
- `Release|x64` and `Release|Win32` builds and package-integrity checks.
