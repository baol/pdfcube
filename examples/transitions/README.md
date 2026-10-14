# Transition examples

Ready-to-open diagnostic slides:

- [Horizontal Blinds](blinds-horizontal.pdf)
- [Vertical Blinds](blinds-vertical.pdf)
- [Horizontal Split, closing](split-horizontal-inward.pdf)
- [Horizontal Split, opening](split-horizontal-outward.pdf)
- [Vertical Split, closing](split-vertical-inward.pdf)
- [Vertical Split, opening](split-vertical-outward.pdf)
- Cover: [right](cover-right.pdf), [up](cover-up.pdf),
  [left](cover-left.pdf), [down](cover-down.pdf)
- Uncover: [right](uncover-right.pdf), [up](uncover-up.pdf),
  [left](uncover-left.pdf), [down](uncover-down.pdf)

From the repository root:

```sh
./build/src/pdfcube examples/transitions/blinds-horizontal.pdf
```

Press **Space** or **Page Down** to play the transition to the second slide.
Use **Left** to return, then Space to replay. The PDF requests a 1.25-second
animation. Contrasting colors, grid lines and labeled corners help reveal
mirroring, gaps and framing errors. These are diagnostic examples, not a
realistic Beamer showcase. Ordinary PDF viewers may ignore the transitions.

Split opens or closes two perspective doors on fixed outer hinges. Horizontal
uses top/bottom flaps; vertical uses left/right doors.
Cover folds the incoming page over the old one; Uncover peels the old page away.
Both use a single curved sheet. Names describe the PDF travel direction, not
the hinge location; see the roadmap for the exact mapping.

Regenerate all PDFs deterministically with Python's standard library:

```sh
python3 tests/transition_fixture.py examples/transitions
```

The generator is the source of truth; do not edit the PDFs by hand.
See the [review roadmap](../../docs/transitions/README.md) for videos and tests.
