"""Check fixed-step timing, complete cycle, pixel orientation and web encoding."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from transition_fixture import make_pdf

from render_client import Frames
from PIL import Image, ImageChops, ImageStat
from unittest.mock import patch
import importlib.util

driver, recorder = map(lambda s: Path(s).resolve(), sys.argv[1:])
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    pdf, output = root / "fixture.pdf", root / "video.mp4"
    make_pdf(pdf)
    frames = Frames(driver,root)
    raw = root / "frames.rgb"
    frames.render("--record-raw",raw,"--record-width","320","--record-height","180",
                  "--record-fps","4",pdf)
    subprocess.run(["ffmpeg","-v","error","-f","rawvideo","-pixel_format","rgb24",
                    "-video_size","320x180","-framerate","4","-i",str(raw),
                    "-c:v","libx264","-threads","1","-preset","fast","-crf","23",
                    "-pix_fmt","yuv420p","-movflags","+faststart",str(output)], check=True)
    data = json.loads(subprocess.check_output([
        "ffprobe", "-v", "error", "-show_streams", "-of", "json", str(output)]))
    stream = data["streams"][0]
    assert len(data["streams"]) == 1
    assert (stream["width"], stream["height"]) == (320, 180)
    assert stream["codec_name"] == "h264"
    assert stream["pix_fmt"] == "yuv420p"
    assert stream["r_frame_rate"] == "4/1"
    # First slide fallback dwell=5s, animation=1.25s, second dwell=4s,
    # instant wrap to first plus one final frame.
    assert int(stream["nb_frames"]) == 42, stream
    mp4 = output.read_bytes()
    assert mp4.index(b"moov") < mp4.index(b"mdat"), "faststart metadata missing"
    pixels = subprocess.check_output([
        "ffmpeg", "-v", "error", "-threads", "1", "-i", str(output), "-f", "rawvideo",
        "-pix_fmt", "rgb24", "-threads", "1", "pipe:1"])
    size = 320 * 180 * 3
    first, incoming, last = pixels[:size], pixels[30*size:31*size], pixels[-size:]
    def difference(a,b):
        left = Image.frombytes("RGB",(320,180),a)
        right = Image.frombytes("RGB",(320,180),b)
        return sum(ImageStat.Stat(ImageChops.difference(left,right)).mean)/3
    assert difference(first,last) < 5
    assert difference(first,incoming) > 30
    # Compare against an independent PNG snapshot
    # to catch framebuffer readback being vertically flipped.
    snapshot = root / "cover.png"
    frames.render("--snapshot",snapshot,pdf)
    with Image.open(snapshot) as image:
        reference = image.convert("RGB").crop((0,30,320,210)).tobytes()
    frames.close()
    error = difference(first,reference)
    # Low-resolution grid lines snap to whole pixels; the reference is downsampled.
    assert error < 20, f"snapshot mismatch: {error}"
    flipped = b"".join(first[y*960:(y+1)*960] for y in reversed(range(180)))
    assert difference(first,reference) < difference(flipped,reference)
    # Pixel content checks above also catch empty/black renders.
    spec = importlib.util.spec_from_file_location("recorder",recorder)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # Parser guards execute in-process, without starting a GUI or encoder.
    command = ["recorder",str(driver),str(pdf),str(output),"--width","320","--height","180"]
    with patch.object(sys,"argv",command), patch("sys.stderr"):
        try:
            module.main()
            raise AssertionError("existing output was overwritten")
        except SystemExit as error:
            assert error.code != 0
    with patch.object(sys,"argv",command + ["--width","321"]), patch("sys.stderr"):
        try:
            module.main()
            raise AssertionError("odd dimension was accepted")
        except SystemExit as error:
            assert error.code != 0
