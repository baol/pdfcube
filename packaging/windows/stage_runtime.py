#!/usr/bin/env python3
"""Stage PDFCube and its recursive MSYS2 runtime dependencies."""

import argparse
import pathlib
import re
import shutil
import subprocess


def dependencies(binary):
    output = subprocess.run(
        ["objdump", "-p", binary], check=True, capture_output=True, text=True
    ).stdout
    return re.findall(r"^\s*DLL Name:\s*(\S+)", output, re.MULTILINE)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=pathlib.Path)
parser.add_argument("prefix", type=pathlib.Path)
parser.add_argument("destination", type=pathlib.Path)
args = parser.parse_args()

args.destination.mkdir(parents=True, exist_ok=True)
prefix_bin = args.prefix / "bin"
available = {path.name.lower(): path for path in prefix_bin.glob("*.dll")}
queue = [args.binary.resolve()]
copied = set()
shutil.copy2(args.binary, args.destination / "pdfcube.exe")

loader_root = args.destination / "lib" / "gdk-pixbuf-2.0" / "2.10.0" / "loaders"
loader_root.mkdir(parents=True, exist_ok=True)
for loader in (args.prefix / "lib" / "gdk-pixbuf-2.0" / "2.10.0" / "loaders").glob("*.dll"):
    staged = loader_root / loader.name
    shutil.copy2(loader, staged)
    queue.append(loader)

while queue:
    current = queue.pop()
    for name in dependencies(current):
        key = name.lower()
        source = available.get(key)
        if source is None or key in copied:
            continue
        copied.add(key)
        shutil.copy2(source, args.destination / source.name)
        queue.append(source)

schema_source = args.prefix / "share" / "glib-2.0" / "schemas"
schema_destination = args.destination / "share" / "glib-2.0" / "schemas"
shutil.copytree(schema_source, schema_destination, dirs_exist_ok=True)
subprocess.run(["glib-compile-schemas", schema_destination], check=True)

for relative in (
    pathlib.Path("etc/fonts"),
    pathlib.Path("share/fontconfig"),
    pathlib.Path("share/gtk-4.0"),
    pathlib.Path("share/poppler"),
):
    source = args.prefix / relative
    if source.exists():
        shutil.copytree(source, args.destination / relative, dirs_exist_ok=True)

icon_destination = args.destination / "share" / "icons" / "hicolor" / "scalable" / "apps"
icon_destination.mkdir(parents=True, exist_ok=True)
shutil.copy2("data/org.pdfcube.pdfcube.svg", icon_destination)
shutil.copy2("LICENSE", args.destination)
shutil.copy2("README.md", args.destination)

if len(copied) < 10:
    raise SystemExit("too few runtime DLLs were staged")
