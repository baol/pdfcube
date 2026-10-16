#!/usr/bin/env python3
import hashlib
import pathlib
import sys
import tempfile
from render_client import Frames
from PIL import Image

driver, fixture, wide_fixture = map(pathlib.Path, sys.argv[1:4])

def snapshot(path, *extra, document=fixture):
    pathlib.Path(path).unlink(missing_ok=True)
    frames.render("--no-fullscreen","--snapshot",path,*extra,document)
    assert pathlib.Path(path).is_file(), (
        "native renderer completed but produced no PNG"
    )
    data = pathlib.Path(path).read_bytes()
    assert data.startswith(b"\x89PNG\r\n\x1a\n")
    return hashlib.sha256(data).hexdigest()

def png_pixels(path):
    with Image.open(path) as image:
        rgba = image.convert("RGBA")
        width,height = rgba.size
        data = rgba.tobytes()
        return width,height,[data[i*width*4:(i+1)*width*4] for i in range(height)]

def pixel(row, column):
    offset = column * 4
    return tuple(row[offset:offset + 4])

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    frames = Frames(driver,root)
    first = snapshot(root / "first.png")
    performance_log = root / "timings.csv"
    repeated = snapshot(
        root / "repeated.png", "--performance-log", performance_log
    )
    assert first == repeated, "software-rendered snapshots must be deterministic"
    assert performance_log.read_text() == (
        "frame_time_us,frame_interval_us,cpu_submit_us,transition,progress,preparing\n"
    )
    width, height, rows = png_pixels(root / "first.png")
    assert all(
        pixel(rows[y], x)[:3] == (255, 255, 255)
        for x, y in ((0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1))
    ), "a page matching the window ratio must fill all four edges"

    colored = snapshot(
        root / "colored.png", "--bgcolor", "0:0:1", document=wide_fixture
    )
    wide_width, wide_height, wide_rows = png_pixels(root / "colored.png")
    white_rows = [
        y for y, row in enumerate(wide_rows) if pixel(row, wide_width // 2)[:3] == (255, 255, 255)
    ]
    assert pixel(wide_rows[white_rows[0]], 0)[:3] == (255, 255, 255)
    assert pixel(wide_rows[white_rows[-1]], wide_width - 1)[:3] == (255, 255, 255)
    assert abs((wide_width / len(white_rows)) - (16 / 9)) < 0.01
    assert abs(white_rows[0] - (wide_height - 1 - white_rows[-1])) <= 1
    (root / ".pdfcuberc").write_text("bgcolor=0:1:0\n")
    overridden = snapshot(
        root / "overridden.png", "--bgcolor", "0:0:1", document=wide_fixture
    )
    assert overridden == colored, "command line options must override .pdfcuberc"
    transition = snapshot(
        root / "transition.png",
        "--program", "1:c",
        "--snapshot-progress", "0.5",
        "--top-color", "0:1:0",
    )
    assert transition not in (first, colored), "cube transition frame must be distinct"
    completed = snapshot(
        root / "completed.png",
        "--program", "1:c",
        "--snapshot-progress", "1",
    )
    assert completed == first, "a completed cube transition must exactly fill the page viewport"
    plain = snapshot(
        root / "plain.png",
        "--program", "1:p",
        "--snapshot-progress", "0.5",
    )
    assert plain == first, "plain programmed transitions must not render the cube"
    alternate_top = snapshot(
        root / "alternate-top.png",
        "--program", "1:c",
        "--snapshot-progress", "0.5",
        "--top-color", "1:0:0",
    )
    assert alternate_top != transition, "top color must affect the cube transition"
    frames.close()


