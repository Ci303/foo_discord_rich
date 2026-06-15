#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path

import call_wrapper


def run_git(root_dir, *args):
    subprocess.check_call(["git", *args], cwd=root_dir)


def download_submodule(root_dir, submodule_name):
    print(f"Downloading {submodule_name}...")
    try:
        run_git(root_dir, "submodule", "update", "--init", "--depth=10", "--", f"submodules/{submodule_name}")
    except subprocess.CalledProcessError:
        try:
            run_git(root_dir, "submodule", "update", "--init", "--depth=50", "--", f"submodules/{submodule_name}")
        except subprocess.CalledProcessError:
            # Shallow copy does not honour default branch config
            run_git(root_dir/"submodules"/submodule_name, "config", "--add", "remote.origin.fetch", "+refs/heads/*:refs/remotes/origin/*")
            run_git(root_dir, "submodule", "deinit", "--force", "--", f"submodules/{submodule_name}")
            run_git(root_dir, "submodule", "update", "--init", "--force", "--", f"submodules/{submodule_name}")

def download(reset_submodules=False):
    cur_dir = Path(__file__).parent.absolute()
    root_dir = cur_dir.parent

    run_git(root_dir, "submodule", "sync")
    if reset_submodules:
        run_git(root_dir, "submodule", "foreach", "git", "reset", "--hard")
    for subdir in [f for f in (root_dir/"submodules").iterdir() if f.is_dir()]:
        download_submodule(root_dir, subdir.name)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Download project submodules")
    parser.add_argument("--reset-submodules", default=False, action="store_true",
                        help="Discard local changes inside submodules before updating")
    args = parser.parse_args()

    call_wrapper.final_call_decorator(
        "Downloading submodules",
        "Downloading submodules: success",
        "Downloading submodules: failure!"
    )(download)(args.reset_submodules)
