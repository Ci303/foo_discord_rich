# Discord Rich Presence Integration
[![version][version_badge]][changelog] [![Build status][appveyor_badge]](https://ci.appveyor.com/project/TheQwertiest/foo-discord-rich/branch/master) [![CodeFactor][codefactor_badge]](https://www.codefactor.io/repository/github/theqwertiest/foo_discord_rich/overview/master) [![Codacy Badge][codacy_badge]](https://app.codacy.com/gh/TheQwertiest/foo_discord_rich/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) 

![foo_discord_rich](https://i.imgur.com/OPLvsku.png)

This is a component for the [foobar2000](https://www.foobar2000.org) audio player, which displays currently played track data via Discord Rich Presence.

Visit [component homepage](https://theqwertiest.github.io/foo_discord_rich) for more info.

For setup examples, title-formatting fields, and album art behavior, see the
[configuration guide](docs/CONFIGURATION.md).

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
[version_badge]: https://img.shields.io/github/release/theqwertiest/foo_discord_rich.svg
[appveyor_badge]: https://ci.appveyor.com/api/projects/status/t5bhoxmfgavhq81m/branch/master?svg=true
[codacy_badge]: https://api.codacy.com/project/badge/Grade/319298ca5bd64a739d1e70e3e27d59ab
[codefactor_badge]: https://www.codefactor.io/repository/github/theqwertiest/foo_discord_rich/badge/master
