### Main scripts
- setup.py - Set up everything, so that project can be built.
- build.ps1 - Build, package, and optionally deploy the component with the explicit v145 toolset defaults and validated Python discovery.
- python_command.ps1 - Find Python 3.10 or newer without depending on one Windows launcher.
- pack_component.py - Pack project binaries to .fb2k-component archive.

### Auxiliary scripts
- download_submodules.py - Download or update project submodules.
- patch_submodules.py - Apply project patches to third-party submodules.

By default, setup does not discard local changes inside submodules. Pass
`--reset_submodules` to `setup.py` or `--reset-submodules` to
`download_submodules.py` when a clean submodule checkout is required.
