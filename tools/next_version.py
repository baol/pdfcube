#!/usr/bin/env python3
"""Calculate the next stable release version from a minimum and Git tags."""

import pathlib
import re
import subprocess


ROOT = pathlib.Path(__file__).resolve().parent.parent
STABLE_VERSION = re.compile(r"(?:v)?([0-9]+)\.([0-9]+)\.([0-9]+)")


def parse_version(value):
    match = STABLE_VERSION.fullmatch(value.strip())
    if not match:
        raise ValueError(f"invalid stable version: {value.strip()}")
    return tuple(int(part) for part in match.groups())


def format_version(version):
    return ".".join(str(part) for part in version)


def next_version(minimum, tags):
    floor = parse_version(minimum)
    releases = []
    for tag in tags:
        try:
            releases.append(parse_version(tag))
        except ValueError:
            continue
    if not releases:
        return floor
    latest = max(releases)
    automatic = (latest[0], latest[1], latest[2] + 1)
    return max(floor, automatic)


def main():
    minimum = (ROOT / "MIN_VERSION").read_text(encoding="utf-8").strip()
    tags = subprocess.run(
        ["git", "-C", ROOT, "tag", "--list", "v*"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    print(format_version(next_version(minimum, tags)))


if __name__ == "__main__":
    main()
