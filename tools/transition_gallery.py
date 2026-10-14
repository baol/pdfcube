#!/usr/bin/env python3
"""Capture review assets from the actual renderer; needs Pillow and ffmpeg."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests"))
from transition_fixture import EXAMPLES, make_pdf

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--frames", type=int, default=33)
parser.add_argument("--update-docs", action="store_true",
                    help="Refresh the checked-in contact sheets from these captures")
args = parser.parse_args()
if args.frames < 5:
    parser.error("--frames must be at least 5")
args.output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, GDK_BACKEND="x11", LIBGL_ALWAYS_SOFTWARE="1")
for name, effect, orientation, motion in EXAMPLES:
    folder = args.output / name
    folder.mkdir(exist_ok=True)
    pdf = folder / "fixture.pdf"
    make_pdf(pdf, orientation, effect=effect, motion=motion)
    for i in range(args.frames):
        subprocess.run(["xvfb-run", "-a", str(args.binary.resolve()), "--no-fullscreen",
                        "--snapshot", str(folder / f"frame-{i:03}.png"),
                        "--snapshot-progress", str(i / (args.frames - 1)), str(pdf)],
                       env=env, check=True, stdout=subprocess.DEVNULL)
    sheet = Image.new("RGB", (480 * 5, 380), "#202020")
    for column, fraction in enumerate((0, .25, .5, .75, 1)):
        frame = round(fraction * (args.frames - 1))
        with Image.open(folder / f"frame-{frame:03}.png") as image:
            image.thumbnail((480, 350))
            sheet.paste(image, (column * 480, 25))
        ImageDraw.Draw(sheet).text((column * 480 + 8, 6), f"{frame / (args.frames - 1):.0%}", fill="white")
    sheet.save(folder / "contact-sheet.png")
    if args.update_docs:
        sheet.save(Path(__file__).resolve().parents[1] / "docs" / "transitions" / f"{name}.png")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate",
                    str((args.frames - 1) / 1.25), "-i", str(folder / "frame-%03d.png"),
                    "-vf", "tpad=start_duration=0.5:start_mode=clone:stop_duration=0.5:stop_mode=clone",
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", str(folder / "transition.mp4")],
                   check=True)
revision = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
(args.output / "manifest.json").write_text(json.dumps({
    "commit": revision,
    "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"])),
    "frames": args.frames, "backend": "X11",
    "renderer": "LIBGL_ALWAYS_SOFTWARE=1", "status": "candidate, not approved",
    "binary_version": subprocess.check_output([str(args.binary.resolve()), "--version"], text=True).strip(),
}, indent=2) + "\n")
(args.output / "index.html").write_text(
    "<!doctype html><meta charset=utf-8><title>Transition review</title>"
    "<h1>Transitions — candidates</h1>" + "".join(
        f"<h2>{name}</h2><video controls loop muted width=720 src='{name}/transition.mp4'></video>"
        f"<p><a href='{name}/fixture.pdf'>Fixture PDF</a></p>"
        f"<img width=1200 src='{name}/contact-sheet.png'>"
        for name, _, _, _ in EXAMPLES))
