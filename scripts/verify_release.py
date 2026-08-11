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
    if not release_notes.splitlines() or release_notes.splitlines()[0] != expected_heading:
        raise ValueError("RELEASE_NOTES.md heading does not match VERSION")

    changelog = (root / "CHANGELOG.md").read_text(encoding="utf-8")
    escaped_version = re.escape(version)
    if not re.search(rf"^## \[{escaped_version}\]\[\] - \d{{4}}-\d{{2}}-\d{{2}}$", changelog, re.MULTILINE):
        raise ValueError("CHANGELOG.md has no dated heading for VERSION")
    if not re.search(rf"^\[unreleased\]: .+/compare/v{escaped_version}\.\.\.HEAD$", changelog, re.MULTILINE):
        raise ValueError("CHANGELOG.md Unreleased link does not start at VERSION")
    if not re.search(rf"^\[{escaped_version}\]: \S+v{escaped_version}$", changelog, re.MULTILINE):
        raise ValueError("CHANGELOG.md release link does not end at VERSION")

    if tag and tag != f"v{version}":
        raise ValueError(f"Tag {tag!r} does not match VERSION v{version}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--tag")
    args = parser.parse_args()
    verify(args.root.resolve(), args.tag)
