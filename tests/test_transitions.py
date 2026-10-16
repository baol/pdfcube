from pathlib import Path
from io import BytesIO
from PIL import Image
import shlex
import subprocess
import sys
import tempfile

from transition_fixture import make_pdf
from render_client import Frames

driver, metadata_test = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
effect, motion, selected_axis, selected_angle, selected_ratio = sys.argv[3:8]
mode = sys.argv[8]
selected_angle = None if selected_angle == 'none' else int(selected_angle)
selected_ratio = int(selected_ratio)
flip = effect in ("Cover", "Uncover")
fly = effect == "Fly"
push = effect == "Push"
dissolve = effect == "Dissolve"
glitter = effect == "Glitter"
wipe = effect == "Wipe"

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    frames = Frames(driver, root)

    def capture(pdf, progress=None, *extra):
        output = root / "frame.png"
        # A missing write must never pass by reusing the previous frame.
        output.unlink(missing_ok=True)
        command = ["--no-fullscreen", "--snapshot", str(output)]
        if progress is not None:
            command += ["--snapshot-progress", str(progress)]
        frames.render(*(command + list(extra) + [str(pdf)]))
        assert output.is_file(), (
            "native renderer completed but produced no PNG\n"
            + shlex.join(command + list(extra) + [str(pdf)])
        )
        return output.read_bytes()

    cases = [(selected_axis,selected_angle)]
    for orientation, angle in cases:
        dimensions = (((640, 360), (640, 360)),
                      ((360, 640), (360, 640)),
                      ((640, 360), (360, 640))) if dissolve else \
                     (((640, 360), (640, 360)),) if fly and angle not in (0, 90) else \
                     (((640, 360), (640, 360)), ((360, 640), (360, 640)),
                      ((640, 360), (360, 640)))
        for sizes in [dimensions[selected_ratio]]:
            deck, first, last = (root / name for name in ("deck.pdf", "first.pdf", "last.pdf"))
            make_pdf(deck, orientation, sizes, effect=effect, motion=motion, angle=angle)
            if mode == "frames":
                frames.render("@sequence", "--snapshot", root / "sequence.png",
                              "--snapshot-progress", "0", deck)
                if dissolve:
                    frames.render("@sequence", "--snapshot", root / "sequence.png",
                                  "--snapshot-progress", "0", "--experimental", deck)
                frames.close()
                sys.exit(0)
            assert mode == "checkpoints", "unknown rendering test mode"
            metadata = [metadata_test, str(deck), orientation, effect, motion]
            extra_metadata = [str(angle)] if flip or push or glitter or wipe else \
                             [str(angle), ".35", "1"] if fly else []
            subprocess.run(metadata + extra_metadata, check=True)
            make_pdf(first, orientation, sizes, (0,))
            make_pdf(last, orientation, sizes, (1,))
            start, end = capture(first), capture(last)
            assert capture(deck, 0) == start, "first frame changed"
            assert capture(deck, 1) == end, "last frame has seams, distortion or wrong page"
            if dissolve:
                assert capture(deck, .99) == end, "shards remain before the final frame"
                for progress, expected in ((0, start), (.99, end), (1, end)):
                    assert capture(deck, progress, "--experimental") == expected, \
                        "experimental glass changed an intact/clear endpoint"
            middle = capture(deck, .5)
            if dissolve:
                assert capture(deck, .5, "--experimental") != middle, \
                    "experimental glass thickness was not rendered"
            if effect == "Fade":
                # Compare actual static compositions, including margins, to the
                # sRGB transfer function rather than duplicating shader samples.
                a, b = (Image.open(BytesIO(data)).convert("RGB") for data in (start, end))
                def linear(c):
                    c /= 255
                    return c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4
                def encoded(c):
                    return 255 * (12.92 * c if c <= .0031308 else 1.055 * c ** (1/2.4) - .055)
                for t in (.25, .5, .75):
                    image = Image.open(BytesIO(capture(deck, t))).convert("RGB")
                    weight = t*t*(3-2*t)
                    for y in range(7, image.height, 19):
                        for x in range(7, image.width, 19):
                            expected = [encoded((1-weight)*linear(c)+weight*linear(d))
                                        for c, d in zip(a.getpixel((x,y)), b.getpixel((x,y)))]
                            assert all(abs(c-d) <= 2 for c,d in zip(image.getpixel((x,y)),expected)), \
                                "fade changed brightness, fit, or easing"
                identical = root / "identical.pdf"
                make_pdf(identical, orientation, sizes, (0, 0), effect="Fade")
                assert capture(identical, .5, "--program", "4:fade") == start, "shared artwork changed during fade"
            if flip and angle == 0:
                for value in (None, 45, -90):
                    fallback = root / "fallback.pdf"
                    make_pdf(fallback, orientation, sizes, effect=effect, angle=value)
                    assert capture(fallback, .5) == middle, "default/invalid direction differs"
            if (push or glitter or wipe) and angle == 0:
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
            if wipe:
                from PIL import Image
                from io import BytesIO
                # Pixels safely behind/ahead of the blade must be exact page
                # pixels, including the independent letterboxes in mixed ratios.
                before = Image.open(BytesIO(start))
                after = Image.open(BytesIO(end))
                wiped = Image.open(BytesIO(middle))
                w,h = wiped.size
                if angle in (0,180):
                    low,high = (0,0,w*2//5,h),(w*3//5,0,w,h)
                else:
                    low,high = (0,0,w,h*2//5),(0,h*3//5,w,h)
                revealed,covered = (low,high) if angle in (0,270) else (high,low)
                assert wiped.crop(revealed).tobytes() == after.crop(revealed).tobytes()
                assert wiped.crop(covered).tobytes() == before.crop(covered).tobytes()
                assert capture(first) == start, "wiper leaked GL state into a static frame"
            quarter, three_quarters = capture(deck, .25), capture(deck, .75)
            assert len({start, quarter, middle, three_quarters, end}) == 5, "cascade stalled"
            if not dissolve and effect != "Fade":
                changed = root / "changed-direction.pdf"
                if glitter:
                    make_pdf(changed, orientation, sizes, effect=effect,
                             angle={0: 270, 270: 315, 315: 0}[angle])
                elif flip or fly or push or wipe:
                    make_pdf(changed, orientation, sizes, effect=effect, motion=motion,
                             angle=(angle+180)%360)
                else:
                    make_pdf(changed, "V" if orientation == "H" else "H", sizes,
                             effect=effect, motion=motion)
                assert capture(changed,.5) != middle, "orientation/direction ignored"
    frames.close()
