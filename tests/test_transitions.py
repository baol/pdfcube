import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

from transition_fixture import EXAMPLES, make_pdf

xvfb, binary, metadata_test = map(lambda p: str(Path(p).resolve()), sys.argv[1:4])
effect = sys.argv[4] if len(sys.argv) > 4 else "Blinds"
motion = sys.argv[5] if len(sys.argv) > 5 else "I"
flip = effect in ("Cover", "Uncover")
fly = effect == "Fly"
push = effect == "Push"
dissolve = effect == "Dissolve"
environment = dict(os.environ, GDK_BACKEND="x11", LIBGL_ALWAYS_SOFTWARE="1")

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    for name, kind, axis, direction, angle in EXAMPLES:
        generated = root / f"{name}.pdf"
        make_pdf(generated, axis, effect=kind, motion=direction, angle=angle)
        committed = Path(__file__).resolve().parents[1] / "examples" / "transitions" / generated.name
        assert generated.read_bytes() == committed.read_bytes(), f"stale example: {name}"

    def capture(pdf, progress=None, *extra):
        output = root / "frame.png"
        # Every invocation must create its own frame. Without this unlink, a
        # remotely activated GtkApplication can make a later capture read the
        # previous invocation's image and report a false pass.
        output.unlink(missing_ok=True)
        command = [xvfb, "-a", binary, "--no-fullscreen", "--snapshot", str(output)]
        if progress is not None:
            command += ["--snapshot-progress", str(progress)]
        result = subprocess.run(command + list(extra) + [str(pdf)], env=environment,
                                capture_output=True, timeout=30)
        details = (f"command: {shlex.join(command + list(extra) + [str(pdf)])}\n"
                   f"stdout:\n{result.stdout.decode(errors='replace')}\n"
                   f"stderr:\n{result.stderr.decode(errors='replace')}")
        assert result.returncode == 0, f"snapshot command failed ({result.returncode})\n{details}"
        assert output.is_file(), (
            "snapshot command exited successfully but produced no PNG; "
            "another GtkApplication instance may have claimed the fixed application ID\n"
            + details
        )
        return output.read_bytes()

    middles = []
    cases = [("H", angle) for angle in (0, 90, 180, 270)] if flip else \
            [("H", angle) for angle in range(0, 360, 45)] if fly else [("H", None), ("V", None)]
    if push:
        cases = [("H", angle) for angle in (0, 90, 180, 270)]
    if dissolve:
        cases = [("H", None)]
    for orientation, angle in cases:
        dimensions = (((640, 360), (640, 360)),
                      ((360, 640), (360, 640)),
                      ((640, 360), (360, 640))) if dissolve else \
                     (((640, 360), (640, 360)),) if fly and angle not in (0, 90) else \
                     (((640, 360), (640, 360)), ((360, 640), (360, 640)),
                      ((640, 360), (360, 640)))
        for sizes in dimensions:
            deck, first, last = (root / name for name in ("deck.pdf", "first.pdf", "last.pdf"))
            make_pdf(deck, orientation, sizes, effect=effect, motion=motion, angle=angle)
            metadata = [metadata_test, str(deck), orientation, effect, motion]
            extra_metadata = [str(angle)] if flip or push else \
                             [str(angle), ".35", "1"] if fly else []
            subprocess.run(metadata + extra_metadata, check=True)
            make_pdf(first, orientation, sizes, (0,))
            make_pdf(last, orientation, sizes, (1,))
            start, end = capture(first), capture(last)
            assert capture(deck, 0) == start, "first frame changed"
            assert capture(deck, 1) == end, "last frame has seams, distortion or wrong page"
            middle = capture(deck, .5)
            if flip and angle == 0:
                for value in (None, 45, -90):
                    fallback = root / "fallback.pdf"
                    make_pdf(fallback, orientation, sizes, effect=effect, angle=value)
                    assert capture(fallback, .5) == middle, "default/invalid direction differs"
            if push and angle == 0:
                for value in (None, 45, -90):
                    fallback = root / "fallback.pdf"
                    make_pdf(fallback, orientation, sizes, effect=effect, angle=value)
                    assert capture(fallback, .5) == middle, "default/invalid direction differs"
            if flip:
                opposite = root / "opposite.pdf"
                make_pdf(opposite, orientation, sizes,
                         effect="Uncover" if effect == "Cover" else "Cover", angle=angle)
                assert capture(opposite, .5) != middle, "Cover and Uncover confused"
            if fly:
                alternate = root / "alternate.pdf"
                make_pdf(alternate, orientation, sizes, effect=effect, motion=motion,
                         angle=angle, start_scale=.7, rectangular=False)
                assert capture(alternate, .5) != middle, "Fly start scale ignored"
            if effect == "Split" and motion == "I" and orientation == "H":
                defaults = root / "defaults.pdf"
                make_pdf(defaults, None, sizes, effect=effect, motion=None)
                subprocess.run([metadata_test, str(defaults), "H", effect, "I"], check=True)
                assert middle == capture(defaults, .5), "PDF default axis/direction changed"
            assert middle not in (start, end), "transition did not animate"
            assert middle == capture(deck, .5), "transition is not deterministic"
            assert middle == capture(deck, .5, "--program", f"4:{effect.lower()}"), "program effect differs"
            if effect == "Split":
                opposite = root / "opposite.pdf"
                make_pdf(opposite, orientation, sizes, effect=effect,
                         motion="O" if motion == "I" else "I")
                assert middle != capture(opposite, .5), "inward/outward direction ignored"
            quarter, three_quarters = capture(deck, .25), capture(deck, .75)
            assert len({start, quarter, middle, three_quarters, end}) == 5, "cascade stalled"
            middles.append(middle)
    if fly or push or dissolve:
        assert len(set(middles)) == len(middles), f"{effect} direction or page ratio ignored"
    else:
        assert len(set(middles[::3])) == len(cases), "orientation/direction ignored"
