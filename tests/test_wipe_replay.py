"""Replay in one live headless renderer; RSS growth must stay bounded."""
from pathlib import Path
import sys
import tempfile

from render_client import Frames
from transition_fixture import make_pdf

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    deck = root / 'wipe.pdf'
    make_pdf(deck,effect='Wipe',angle=270)
    frames = Frames(Path(sys.argv[1]),root)
    command = ('@sequence','--snapshot',root/'unused.png','--snapshot-progress','0',deck)
    frames.render(*command)  # Warm font, texture and shader caches in this context.
    def rss():
        status = Path(f'/proc/{frames.process.pid}/status').read_text()
        return int(next(line.split()[1] for line in status.splitlines() if line.startswith('VmRSS:')))*1024
    baseline = rss()
    for _ in range(4):
        frames.render(*command)
        assert rss() <= baseline + 4*1024*1024, 'Wipe memory grew during repeated playback'
    frames.close()
