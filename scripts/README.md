### Main scripts
- setup.py - Set up everything, so that project can be built.
- pack_component.py - Pack project binaries to .fb2k-component archive.

### Auxiliary scripts
- download_submodules.py - Download or update project submodules.
- patch_submodules.py - Apply project patches to third-party submodules.

By default, setup does not discard local changes inside submodules. Pass
`--reset_submodules` to `setup.py` or `--reset-submodules` to
`download_submodules.py` when a clean submodule checkout is required.
