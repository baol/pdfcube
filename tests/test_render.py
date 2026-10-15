#!/usr/bin/env python3
import hashlib
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib

xvfb, binary, fixture, wide_fixture = map(pathlib.Path, sys.argv[1:5])

def snapshot(path, *extra, home=None, skip_if_no_gl=False, document=fixture):
    pathlib.Path(path).unlink(missing_ok=True)
    environment = os.environ.copy()
    environment["GDK_BACKEND"] = "x11"
    environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
    if home is not None:
        environment["HOME"] = str(home)
    result = subprocess.run(
        [xvfb, "-a", binary, "--no-fullscreen", "--snapshot", path, *extra, document],
        text=True,
        capture_output=True,
        timeout=30,
        env=environment,
    )
    if result.returncode != 0:
        if skip_if_no_gl and "OpenGL initialization failed:" in result.stderr:
            print(
                "SKIP: Xvfb cannot create a software OpenGL context; "
                "install Mesa DRI support",
                file=sys.stderr,
            )
            raise SystemExit(77)
        raise AssertionError(result.stderr)
    assert pathlib.Path(path).is_file(), (
        "snapshot command exited successfully but produced no PNG; another "
        "GtkApplication instance may have claimed the fixed application ID\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )
    data = pathlib.Path(path).read_bytes()
    assert data.startswith(b"\x89PNG\r\n\x1a\n")
    return hashlib.sha256(data).hexdigest()

def png_pixels(path):
    data = pathlib.Path(path).read_bytes()
    position = 8
    compressed = bytearray()
    while position < len(data):
        length = struct.unpack(">I", data[position:position + 4])[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + length]
        position += length + 12
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", payload[:10])
            assert depth == 8 and color in (2, 6), "snapshots must be 8-bit RGB/RGBA PNGs"
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(compressed)
    channels = 4 if color == 6 else 3
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        current = bytearray(raw[offset + 1:offset + stride + 1])
        offset += stride + 1
        for index in range(stride):
            left = current[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 1:
                current[index] = (current[index] + left) & 0xff
            elif filter_type == 2:
                current[index] = (current[index] + above) & 0xff
            elif filter_type == 3:
                current[index] = (current[index] + ((left + above) // 2)) & 0xff
            elif filter_type == 4:
                estimate = left + above - upper_left
                distances = (abs(estimate - left), abs(estimate - above), abs(estimate - upper_left))
                current[index] = (current[index] + (left, above, upper_left)[distances.index(min(distances))]) & 0xff
            else:
                assert filter_type == 0
        if channels == 3:
            rgba = bytearray()
            for index in range(0, len(current), 3):
                rgba.extend(current[index:index + 3])
                rgba.append(255)
            rows.append(rgba)
        else:
            rows.append(current)
        previous = current
    return width, height, rows

def pixel(row, column):
    offset = column * 4
    return tuple(row[offset:offset + 4])

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    first = snapshot(root / "first.png", skip_if_no_gl=True)
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
        root / "overridden.png", "--bgcolor", "0:0:1", home=root, document=wide_fixture
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



# Cube endpoints must preserve the fitted dimensions of heterogeneous pages.
from transition_fixture import make_pdf
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    sizes = ((640, 360), (360, 640))
    deck, first_page, last_page = (root / name for name in ("mixed.pdf", "first.pdf", "last.pdf"))
    make_pdf(deck, sizes=sizes, effect="Box")
    make_pdf(first_page, sizes=sizes, pages=(0,), effect="Box")
    make_pdf(last_page, sizes=sizes, pages=(1,), effect="Box")
    for progress, page in (("0", first_page), ("1", last_page)):
        reference = root / f"reference-{progress}.png"
        actual = root / f"cube-{progress}.png"
        snapshot(reference, document=page)
        snapshot(actual, "--program", "1:c", "--snapshot-progress", progress, document=deck)
        width, height, expected = png_pixels(reference)
        _, _, observed = png_pixels(actual)
        # Compare colored page bounds, ignoring the cube caps and antialiasing.
        def bounds(rows):
            points = [(x, y) for y, row in enumerate(rows) for x in range(width)
                      if (lambda c: (c[2] > 100 and c[0] < 80) or
                                     (c[0] > 130 and 30 < c[1] < 90))(pixel(row, x))]
            assert points
            return min(x for x, y in points), min(y for x, y in points), max(x for x, y in points), max(y for x, y in points)
        assert all(abs(a - b) <= 2 for a, b in zip(bounds(expected), bounds(observed))), (progress, bounds(expected), bounds(observed))
