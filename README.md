# Discord Rich Presence Integration
[![version][version_badge]][releases]

![foo_discord_rich](https://i.imgur.com/OPLvsku.png)

This is a component for the [foobar2000](https://www.foobar2000.org) audio player, which displays currently played track data via Discord Rich Presence.

This repository is a maintained fork of the original
[TheQwertiest/foo_discord_rich](https://github.com/TheQwertiest/foo_discord_rich)
component.

For setup examples, title-formatting fields, and album art behavior, see the
[configuration guide](docs/CONFIGURATION.md).

## Attribution

Original project and component implementation by
[TheQwertiest](https://github.com/TheQwertiest) and contributors.

This fork is maintained by [Ci303](https://github.com/Ci303) because the
original project is no longer actively maintained. The original MIT license and
third-party notices are preserved.

## Building

### Prerequisites

- Visual Studio 2022 with the MSVC v143 C++ toolset.
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
msbuild workspaces\foo_discord_rich.sln /m /p:Configuration=Release /p:Platform=x64
py scripts\pack_component.py --configuration Release --platform x64
```

Replace `x64` with `Win32` for the 32-bit component build.

[changelog]: CHANGELOG.md
[version_badge]: https://img.shields.io/github/release/Ci303/foo_discord_rich.svg
[releases]: https://github.com/Ci303/foo_discord_rich/releases
