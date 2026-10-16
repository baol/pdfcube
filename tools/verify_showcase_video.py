#!/usr/bin/env python3
"""Validate a generated showcase before publishing its attachment."""
import json
from pathlib import Path
import subprocess
import sys

video = Path(sys.argv[1])
assert 0 < video.stat().st_size < 10_000_000, "video exceeds attachment size limit"
data = json.loads(subprocess.check_output([
    "ffprobe", "-v", "error", "-show_streams", "-of", "json", str(video)]))
assert len(data["streams"]) == 1, "expected a single silent video stream"
stream = data["streams"][0]
assert stream["codec_name"] == "h264"
assert stream["pix_fmt"] == "yuv420p"
assert (stream["width"], stream["height"]) == (1280, 720)
assert stream["avg_frame_rate"] == "30/1"
assert float(stream["duration"]) > 0
content = video.read_bytes()
assert content.index(b"moov") < content.index(b"mdat"), "faststart missing"
# Decode samples from across the video and reject an empty framebuffer capture.
for fraction in (0, .25, .5, .75, .99):
    pixels = subprocess.check_output([
        "ffmpeg", "-nostdin", "-v", "error", "-ss",
        str(float(stream["duration"]) * fraction), "-i", str(video),
        "-frames:v", "1", "-vf", "scale=64:36", "-f", "rawvideo",
        "-pix_fmt", "rgb24", "pipe:1"])
    assert len(pixels) == 64 * 36 * 3 and max(pixels) - min(pixels) > 30, "blank frame"
print(f"Verified {video}: 720p, 30 fps, {video.stat().st_size} bytes")
