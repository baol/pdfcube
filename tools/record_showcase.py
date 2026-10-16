#!/usr/bin/env python3
"""Render a PDFCube presentation at fixed frame steps and encode a web MP4."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("pdf", type=Path)
    parser.add_argument("output", type=Path, help="Output .mp4 (must not exist)")
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--crf", type=int, default=23,
                        help="H.264 quality: lower is larger/sharper (default: 23)")
    parser.add_argument("--threads", type=int, default=2, help="FFmpeg encoder threads")
    parser.add_argument("--preset", choices=("fast", "medium", "slow"), default="slow")
    parser.add_argument("--inside-xvfb", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not 1 <= args.fps <= 120:
        parser.error("--fps must be in 1..120")
    if any(n < 2 or n > 8192 or n % 2 for n in (args.width, args.height)):
        parser.error("dimensions must be even numbers in 2..8192")
    if not 1 <= args.threads <= 64:
        parser.error("--threads must be in 1..64")
    if not 0 <= args.crf <= 51:
        parser.error("--crf must be in 0..51")
    if args.output.suffix.lower() != ".mp4":
        parser.error("output must end in .mp4")
    if args.output.exists():
        parser.error("output already exists; choose another path")
    for path in (args.binary, args.pdf):
        if not path.is_file():
            parser.error(f"missing input: {path}")
    for name in ("ffmpeg", "xvfb-run"):
        if not shutil.which(name):
            parser.error(f"{name} is required")
    if not args.inside_xvfb:
        return subprocess.call([
            "xvfb-run", "-a", f"--server-args=-screen 0 {args.width}x{args.height}x24 -noreset",
            sys.executable, str(Path(__file__).resolve()),
            *sys.argv[1:], "--inside-xvfb"])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, GDK_BACKEND="x11", GDK_SCALE="1", GDK_DPI_SCALE="1",
               LIBGL_ALWAYS_SOFTWARE="1", GTK_A11Y="none",
               DBUS_SESSION_BUS_ADDRESS="disabled:", GIO_USE_VFS="local")
    # Encode into the destination filesystem, publishing only after both programs
    # succeed. No multi-gigabyte raw recording or PNG sequence is kept on disk.
    with tempfile.TemporaryDirectory(prefix=".pdfcube-record-", dir=args.output.parent) as tmp:
        temporary = Path(tmp) / "video.mp4"
        encoder = subprocess.Popen([
            "ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "warning", "-n",
            "-f", "rawvideo", "-pixel_format", "rgb24", "-video_size",
            f"{args.width}x{args.height}", "-framerate", str(args.fps), "-i", "pipe:0",
            "-an", "-c:v", "libx264", "-threads", str(args.threads), "-preset", args.preset, "-crf", str(args.crf),
            "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(temporary)],
            stdin=subprocess.PIPE)
        try:
            capture = subprocess.run([
                str(args.binary.resolve()), "--record-raw", "/dev/stdout",
                "--record-fps", str(args.fps), "--record-width", str(args.width),
                "--record-height", str(args.height), str(args.pdf.resolve())],
                env=env, stdout=encoder.stdin)
            encoder.stdin.close()
            encoded = encoder.wait()
            if capture.returncode or encoded:
                raise RuntimeError(f"recording failed (renderer={capture.returncode}, encoder={encoded})")
            # Hard-link creation refuses to replace an output created meanwhile.
            os.link(temporary, args.output)
        finally:
            if encoder.stdin and not encoder.stdin.closed:
                encoder.stdin.close()
            if encoder.poll() is None:
                encoder.terminate()
                encoder.wait()
    print(f"Saved {args.output} ({args.output.stat().st_size / 1024 / 1024:.1f} MiB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
