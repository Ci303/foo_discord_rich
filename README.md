# Discord Rich Presence Integration
[![version][version_badge]][releases]

![foo_discord_rich](https://camo.githubusercontent.com/cb7e637992a7f62bc7ffb9aef0082de03875249f9e39cac8bca724b6d4ce56b6/68747470733a2f2f63646e2e696d6763686573742e636f6d2f66696c65732f346a6463763639766462342e706e67)

This is a component for the [foobar2000](https://www.foobar2000.org) audio player, which displays currently played track data via Discord Rich Presence.

This repository is a personal fork of [Ci303/foo_discord_rich](https://github.com/Ci303/foo_discord_rich), which is itself based on the original [TheQwertiest/foo_discord_rich](https://github.com/TheQwertiest/foo_discord_rich) component.

> This is a personal fork maintained for the addition of Discord Rich Presence progress bar/activity support. It may not receive ongoing updates.

For setup examples, title-formatting fields, and album art behavior, see the
[configuration guide](docs/CONFIGURATION.md).

The maintained Discord application's Portal asset mapping is documented in the
[Discord asset manifest](images/README.md).

## Credits

The Discord Rich Presence progress bar/activity functionality in the `progress-bar` branch was ported from [supern64/foo_discord_rich](https://github.com/supern64/foo_discord_rich) and adapted to this codebase.

Credit for the original project, its contributors, and the ported functionality belongs to their respective authors. The original MIT license and third-party notices are preserved.

## Installation

Download the latest `.fb2k-component` package from
[Releases](https://github.com/noxia-xyz/foo_discord_rich/releases).

- Use the `x64` package for foobar2000 v2 64-bit.
- Use the `Win32` package for foobar2000 32-bit.

Open the downloaded component package with foobar2000 or install it from
`Preferences > Components > Install...`.

## Attribution

Original project:

[TheQwertiest/foo_discord_rich](https://github.com/TheQwertiest/foo_discord_rich).

Original component implementation by TheQwertiest and contributors.

This fork is maintained by [noxia-xyz](https://github.com/noxia-xyz), based on the maintained
[Ci303/foo_discord_rich](https://github.com/Ci303/foo_discord_rich) codebase.

The original MIT license and third-party notices are preserved.

## Building

### Prerequisites

- Visual Studio 2022 with the MSVC v145 C++ toolset, or Visual Studio 2026 with the MSVC v145 C++ toolset.
- Windows 10 SDK.
- Python 3.10 or newer.
- NuGet package restore support for Visual Studio/MSBuild.

### Setup

```powershell
python -m pip install semver
python -u scripts\setup.py
nuget restore workspaces\foo_discord_rich.sln
```

`scripts\setup.py` downloads/configures submodules, applies required patches, and generates build metadata under `_result`. It does not discard local changes inside submodules by default. Use `--reset_submodules` only when you intentionally want a clean submodule checkout.

### Build and Package

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```
`build.ps1` runs MSBuild with the explicit `v145` toolset and `14.51.36231`
tool binaries, then discovers a working Python 3.10 or newer interpreter and
packages the artifact. If Python is not on `PATH`, pass its full path with
`-PythonExecutable`.

To deploy directly into a local foobar2000 x64 user-components directory after build:

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64 -Deploy
```

Replace `x64` with `Win32` for the 32-bit component build. Tagged releases are
built by GitHub Actions for both platforms.

[changelog]: CHANGELOG.md
[version_badge]: https://img.shields.io/github/release/noxia-xyz/foo_discord_rich.svg
[releases]: https://github.com/noxia-xyz/foo_discord_rich/releases

