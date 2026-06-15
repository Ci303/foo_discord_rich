#!/usr/bin/env python3

import subprocess
from pathlib import Path

import call_wrapper


def patch():
    cur_dir = Path(__file__).parent.absolute()
    root_dir = cur_dir.parent
    patches = [Path(p) for p in (cur_dir/"patches").glob('*.patch')]
    for p in patches:
        if not p.is_file():
            raise FileNotFoundError(f"Patch file was not found: {p}")

    if not patches:
        return

    subprocess.check_call(["git", "apply", "--ignore-whitespace", *[str(p) for p in patches]], cwd=root_dir)

if __name__ == '__main__':
    call_wrapper.final_call_decorator(
        "Patching submodules",
        "Patching submodules: success",
        "Patching submodules: failure!"
    )(patch)()
