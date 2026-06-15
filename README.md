# Discord Rich Presence Integration
[![version][version_badge]][releases]

![foo_discord_rich](https://i.imgur.com/OPLvsku.png)

This is a component for the [foobar2000](https://www.foobar2000.org) audio player, which displays currently played track data via Discord Rich Presence.

This repository is a maintained fork of the original
[TheQwertiest/foo_discord_rich](https://github.com/TheQwertiest/foo_discord_rich)
component.

For setup examples, title-formatting fields, and album art behavior, see the
[configuration guide](docs/CONFIGURATION.md).

## Installation

Download the latest `.fb2k-component` package from
[Releases](https://github.com/Ci303/foo_discord_rich/releases).

- Use the `x64` package for foobar2000 v2 64-bit.
- Use the `Win32` package for foobar2000 32-bit.

Open the downloaded component package with foobar2000 or install it from
`Preferences > Components > Install...`.

## Attribution

Original project:
[TheQwertiest/foo_discord_rich](https://github.com/TheQwertiest/foo_discord_rich).
Original component implementation by TheQwertiest and contributors.

This fork is maintained by [Ci303](https://github.com/Ci303) because the
original project is no longer actively maintained. The original MIT license and
third-party notices are preserved.

## Building

### Prerequisites

- Visual Studio 2022 with the MSVC v145 C++ toolset, or Visual Studio 2026 with the MSVC v145 C++ toolset.
- Windows 10 SDK.
- Python 3.
- NuGet package restore support for Visual Studio/MSBuild.

### Setup

```powershell
py -3 -m pip install semver
py -u scripts\setup.py
nuget restore workspaces\foo_discord_rich.sln
```

`scripts\setup.py` downloads/configures submodules, applies required patches, and generates build metadata under `_result`. It does not discard local changes inside submodules by default. Use `--reset_submodules` only when you intentionally want a clean submodule checkout.

### Build and Package

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```
`build.ps1` runs MSBuild with the explicit `v145` toolset and `14.51.36231` tool binaries, then packages the artifact.

To deploy directly into a local foobar2000 x64 user-components directory after build:

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64 -Deploy
```

Replace `x64` with `Win32` for the 32-bit component build. Tagged releases are
built by GitHub Actions for both platforms.

[changelog]: CHANGELOG.md
[version_badge]: https://img.shields.io/github/release/Ci303/foo_discord_rich.svg
[releases]: https://github.com/Ci303/foo_discord_rich/releases

