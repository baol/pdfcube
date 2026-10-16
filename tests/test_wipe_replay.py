"""Replay in one live headless renderer; RSS growth must stay bounded."""
import os
from pathlib import Path
import sys
import tempfile

from render_client import Frames
from transition_fixture import make_pdf

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    deck = root / 'wipe.pdf'
    make_pdf(deck,effect='Wipe',angle=270)
    # RSS measures live/pool storage, not ASan's deliberately retained freed blocks.
    # Other frame tests keep the standard quarantine for use-after-free coverage.
    sanitizer = os.environ.get('ASAN_OPTIONS','')
    sanitizer += ':quarantine_size_mb=0:thread_local_quarantine_size_kb=0'
    frames = Frames(Path(sys.argv[1]),root,environment={'ASAN_OPTIONS': sanitizer})
    report = root/'memory.txt'
    frames.render('@replay','--snapshot',report,'--snapshot-progress','0',deck)
    usage = [int(line) for line in report.read_text().splitlines()]
    assert len(usage) == 5 and min(usage) > 0
    baseline = usage[0]  # First measured cycle follows three explicit warmup cycles.
    assert max(usage[1:]) <= baseline + 4*1024*1024, f'Wipe RSS grew: {usage}'
    frames.close()
