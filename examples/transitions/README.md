# Transition examples

Ready-to-open diagnostic slides:

- [Horizontal Blinds](blinds-horizontal.pdf)
- [Vertical Blinds](blinds-vertical.pdf)

From the repository root:

```sh
./build/src/pdfcube examples/transitions/blinds-horizontal.pdf
```

Press **Space** or **Page Down** to play the transition to the second slide.
Use **Left** to return, then Space to replay. The PDF requests a 1.25-second
animation. Contrasting colors, grid lines and labeled corners help reveal
mirroring, gaps and framing errors. These are diagnostic examples, not a
realistic Beamer showcase. Ordinary PDF viewers may ignore the transitions.

Regenerate both PDFs deterministically with Python's standard library:

```sh
python3 tests/transition_fixture.py examples/transitions
```

The generator is the source of truth; do not edit the PDFs by hand.
See the [review roadmap](../../docs/transitions/README.md) for videos and tests.
