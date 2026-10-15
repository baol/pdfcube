# Beamer transition showcase

[showcase.tex](showcase.tex) is a self-contained 48-slide, 16:9 Beamer deck.
Every slide has vector TikZ artwork and a **Next:** label naming the transition
that will play when you advance. Alternating navy, teal and plum backgrounds,
fine lines, readable text and asymmetric graphics make the motion visible.
No external images, fonts, shell escape or generated assets are required.

## Build

Install a TeX distribution with Beamer and PGF/TikZ. On Debian/Ubuntu these
are provided by `texlive-latex-recommended` and `texlive-pictures`.
From the repository root:

```sh
mkdir -p build/showcase
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=build/showcase examples/showcase/showcase.tex
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=build/showcase examples/showcase/showcase.tex
./build/src/pdfcube build/showcase/showcase.pdf
```

Use pdfLaTeX with the normal Beamer presentation mode. Do not enable
`\DocumentMetadata`: this source uses Hyperref's classic PDF transition
syntax, including explicit Fly scale and motion parameters.

## Playback

Press **Space** or **Page Down** to play the advertised next transition.
Press **Left** to return instantly, then Space to replay. Arrow keys navigate
instantly; `c` explicitly selects a cube rotation rather than the PDF effect.

For automatic playback:

```sh
./build/src/pdfcube --demo build/showcase/showcase.pdf
```

Each slide sets five seconds of dwell time (`/Dur`); animated transitions
request 1.25 seconds (`/Trans /D`). Change `\ShowcaseDwell` in the preamble to
adjust dwell time. The final slide wraps to the cover with Replace. Press `1` to return to the
first slide at any time, or `t` to toggle automatic playback.

## Coverage

- Box inward/outward: both select PDFCube's classic cube.
- Blinds: horizontal and vertical.
- Split: horizontal/vertical, opening/closing.
- Cover and Uncover: right, up, left and down.
- Fly: inward and outward in all eight compass directions, with scale 0.35
  and rectangular backing enabled.
- Push and Wipe: all four cardinal directions.
- Dissolve, Glitter (right/down/lower right), Fade and Replace.

Push, Dissolve, Glitter, Wipe and Fade currently fall back to an instant change
in PDFCube. Their slides say so; they preserve the real PDF metadata to provide
a showcase as these effects are implemented. Other viewers may implement them
or ignore PDF transitions entirely. Replace is deliberately instant.

PDF transitions are stored on the **destination** page. Each `\ShowSlide` call
therefore declares the effect entering that slide, while its fourth argument
names the next slide's effect. Keep these aligned if you reorder or add slides.
The first and last pages use Replace; advancing from the final slide wraps
to the first page. Artwork is generated from the fifth argument (a numeric seed), and the
sixth argument chooses the background palette.
