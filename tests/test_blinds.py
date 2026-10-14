import os
from pathlib import Path
import subprocess
import sys
import tempfile

from transition_fixture import make_pdf

xvfb, binary, metadata_test = map(lambda p: str(Path(p).resolve()), sys.argv[1:])
environment = dict(os.environ, GDK_BACKEND="x11", LIBGL_ALWAYS_SOFTWARE="1")

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    def capture(pdf, progress=None):
        output = root / "frame.png"
        command = [xvfb, "-a", binary, "--no-fullscreen", "--snapshot", str(output)]
        if progress is not None:
            command += ["--snapshot-progress", str(progress)]
        subprocess.run(command + [str(pdf)], check=True, env=environment,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        return output.read_bytes()

    middles = []
    for orientation in ("H", "V"):
        for sizes in (((640, 360), (640, 360)), ((360, 640), (360, 640)),
                      ((640, 360), (360, 640))):
            deck, first, last = (root / name for name in ("deck.pdf", "first.pdf", "last.pdf"))
            make_pdf(deck, orientation, sizes)
            subprocess.run([metadata_test, str(deck), orientation], check=True)
            make_pdf(first, orientation, sizes, (0,))
            make_pdf(last, orientation, sizes, (1,))
            start, end = capture(first), capture(last)
            assert capture(deck, 0) == start, "first frame changed"
            assert capture(deck, 1) == end, "last frame has seams, distortion or wrong page"
            middle = capture(deck, .5)
            assert middle not in (start, end), "transition did not animate"
            assert middle == capture(deck, .5), "transition is not deterministic"
            quarter, three_quarters = capture(deck, .25), capture(deck, .75)
            assert len({start, quarter, middle, three_quarters, end}) == 5, "cascade stalled"
            middles.append(middle)
    assert middles[0] != middles[3], "orientation ignored"
