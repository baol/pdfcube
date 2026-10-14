#!/usr/bin/env python3
"""Capture review assets from the actual renderer; needs Pillow and ffmpeg."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests"))
from transition_fixture import EXAMPLES, make_pdf

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--frames", type=int, default=33)
parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1),
                    help="Number of snapshots to capture concurrently (default: up to 4)")
parser.add_argument("--update-docs", action="store_true",
                    help="Refresh the checked-in contact sheets from these captures")
parser.add_argument("--effects", nargs="+", choices=("Blinds", "Split", "Cover", "Uncover", "Fly"),
                    help="Capture only selected effects (default: all)")
parser.add_argument("--inside-xvfb", action="store_true", help=argparse.SUPPRESS)
args = parser.parse_args()
if args.frames < 5:
    parser.error("--frames must be at least 5")
if args.jobs < 1:
    parser.error("--jobs must be at least 1")

# Starting a fresh X server for every frame dominates the gallery runtime. Keep
# xvfb-run as the portable display allocator, but use it once for the complete
# gallery and let the child processes share that display.
if not args.inside_xvfb:
    xvfb_run = shutil.which("xvfb-run")
    if not xvfb_run:
        parser.error("xvfb-run is required")
    command = [xvfb_run, "-a", sys.executable, str(Path(__file__).resolve()),
               *sys.argv[1:], "--inside-xvfb"]
    raise SystemExit(subprocess.run(command).returncode)

args.output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, GDK_BACKEND="x11", LIBGL_ALWAYS_SOFTWARE="1")
examples = [entry for entry in EXAMPLES if not args.effects or entry[1] in args.effects]
for name, effect, orientation, motion, angle in examples:
    folder = args.output / name
    folder.mkdir(exist_ok=True)
    pdf = folder / "fixture.pdf"
    make_pdf(pdf, orientation, effect=effect, motion=motion, angle=angle)
    for old_frame in folder.glob("frame-*.png"):
        old_frame.unlink()

    def capture(i):
        subprocess.run([str(args.binary.resolve()), "--no-fullscreen",
                        "--snapshot", str(folder / f"frame-{i:03}.png"),
                        "--snapshot-progress", str(i / (args.frames - 1)), str(pdf)],
                       env=env, check=True, stdout=subprocess.DEVNULL, timeout=30)

    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        list(executor.map(capture, range(args.frames)))
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
    "examples": [entry[0] for entry in examples],
    "renderer": "LIBGL_ALWAYS_SOFTWARE=1", "status": "candidate, not approved",
    "binary_version": subprocess.check_output([str(args.binary.resolve()), "--version"], text=True).strip(),
}, indent=2) + "\n")
(args.output / "index.html").write_text(
    "<!doctype html><meta charset=utf-8><title>Transition review</title>"
    "<h1>Transitions — candidates</h1>" + "".join(
        f"<h2>{name}</h2><video controls loop muted width=720 src='{name}/transition.mp4'></video>"
        f"<p><a href='{name}/fixture.pdf'>Fixture PDF</a></p>"
        f"<img width=1200 src='{name}/contact-sheet.png'>"
        for name, _, _, _, _ in examples))
