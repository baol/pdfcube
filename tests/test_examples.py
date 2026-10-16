"""Fixture freshness is checked once, not once per rendered frame case."""
from pathlib import Path
import tempfile
from transition_fixture import EXAMPLES, make_pdf

with tempfile.TemporaryDirectory() as directory:
    for name, effect, axis, motion, angle in EXAMPLES:
        generated = Path(directory) / (name+'.pdf')
        make_pdf(generated,axis,effect=effect,motion=motion,angle=angle)
        committed = Path(__file__).resolve().parents[1]/'examples/transitions'/generated.name
        assert generated.read_bytes() == committed.read_bytes(), 'stale example: '+name
