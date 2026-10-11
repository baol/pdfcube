#!/usr/bin/env python3
"""Derive PDFCube's version from its Git tag or release archive name."""

import os
import pathlib
import re
import subprocess


ROOT = pathlib.Path(__file__).resolve().parent.parent
RELEASE_VERSION = r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?"
VERSION = rf"{RELEASE_VERSION}(?:\+[0-9A-Za-z.-]+)?"


def git(*arguments):
    try:
        return subprocess.run(
            ["git", "-C", ROOT, *arguments],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        ).stdout.strip()
    except (FileNotFoundError, subprocess.CalledProcessError):
        return ""


configured = os.environ.get("PDFCUBE_VERSION", "")
if re.fullmatch(VERSION, configured):
    print(configured)
    raise SystemExit

dirty = bool(git("status", "--porcelain", "--untracked-files=no"))
exact_tag = git("describe", "--tags", "--exact-match", "--match", "v[0-9]*", "HEAD")
match = re.fullmatch(rf"v({RELEASE_VERSION})", exact_tag)
if match and not dirty:
    print(match.group(1))
    raise SystemExit

description = git("describe", "--tags", "--match", "v[0-9]*", "--long", "HEAD")
match = re.fullmatch(
    rf"v({RELEASE_VERSION})-([0-9]+)-g([0-9a-f]+)", description
)
if match:
    suffix = f"{match.group(2)}.g{match.group(3)}"
    if dirty:
        suffix += ".dirty"
    print(f"{match.group(1)}+{suffix}")
    raise SystemExit

archive = re.fullmatch(rf"pdfcube-({VERSION})", ROOT.name)
if archive:
    print(archive.group(1))
    raise SystemExit

revision = git("rev-parse", "--short=12", "HEAD") or "unknown"
dirty_suffix = ".dirty" if dirty else ""
print(f"0.0.0+g{revision}{dirty_suffix}")
