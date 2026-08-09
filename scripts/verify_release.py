#!/usr/bin/env python3

import argparse
import re
from pathlib import Path


def read_version(root: Path) -> str:
    version = (root / "VERSION").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?", version):
        raise ValueError(f"Invalid VERSION value: {version!r}")
    return version


def verify(root: Path, tag: str | None = None) -> None:
    version = read_version(root)
    expected_heading = f"# Discord Rich Presence Integration v{version}"
    release_notes = (root / "RELEASE_NOTES.md").read_text(encoding="utf-8")
    if not release_notes.startswith(expected_heading):
        raise ValueError("RELEASE_NOTES.md heading does not match VERSION")
    if tag and tag != f"v{version}":
        raise ValueError(f"Tag {tag!r} does not match VERSION v{version}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--tag")
    args = parser.parse_args()
    verify(args.root.resolve(), args.tag)
