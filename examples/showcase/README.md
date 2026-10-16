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
meson setup build  # once, if not already configured
meson compile -C build doc
./build/src/pdfcube build/examples/showcase/showcase.pdf
```

The explicit `doc` target runs pdfLaTeX twice and writes
`build/examples/showcase/showcase.pdf`. It is always defined and is excluded from the
default application build. Invoking it without pdfLaTeX or the required TeX
packages fails; documentation is never silently skipped. CI runs this same
target and uploads the PDF as the `beamer-showcase` artifact.

For package installation, configure with `-Dinstall_docs=true`, build `doc`,
then install normally. This installs `showcase.pdf`, `showcase.tex` and this
README under `<prefix>/share/doc/pdfcube/examples/` (or the configured
`datadir`). The option defaults to false and does not depend on installed tools.
The target tracks source changes, so unchanged PDFs are not rebuilt.

Debian packages and AppImages include these files. GitHub releases also attach
`pdfcube-VERSION-showcase.pdf`, covered by the release's `SHA256SUMS`.
The source archive contains the editable source and build instructions.

Use pdfLaTeX with the normal Beamer presentation mode. Do not enable
`\DocumentMetadata`: this source uses Hyperref's classic PDF transition
syntax, including explicit Fly scale and motion parameters.

## Playback

Press **Space** or **Page Down** to play the advertised next transition.
Press **Left** to return instantly, then Space to replay. Arrow keys navigate
instantly; `c` explicitly selects a cube rotation rather than the PDF effect.

For automatic playback:

```sh
./build/src/pdfcube --demo build/examples/showcase/showcase.pdf
```

Each slide sets five seconds of dwell time (`/Dur`); animated transitions
request 1.25 seconds (`/Trans /D`). Change `\ShowcaseDwell` in the preamble to
adjust dwell time. The final slide wraps to the cover with Replace. Press `1` to return to the
first slide at any time, or `t` to toggle automatic playback.

## Record a web video

On Linux, install FFmpeg (with libx264), Xvfb and xauth.
Build the application and `doc` target above, then run:

```sh
python3 tools/record_showcase.py build/src/pdfcube \
  build/examples/showcase/showcase.pdf build/showcase-web.mp4
```

The default is a silent 1280×720, **30 fps** H.264 MP4 with YUV420 pixels and
`faststart` metadata for browser playback. `--crf 23` controls compression;
raise it (for example `--crf 28`) for a smaller file, or lower it for sharper
text. You can also set `--width`, `--height`, `--fps`, and `--preset`.
Output files are never overwritten.

The recorder uses PDFCube's actual OpenGL renderer on a private virtual
display. It advances by fixed frame steps rather than capturing the desktop,
so slow rendering cannot drop animation frames. It streams directly to FFmpeg
without storing raw frames. Every slide retains its PDF dwell time (five seconds
when unspecified); transition durations are rounded to the nearest frame.
It records one complete cycle, including the return to the cover. Transitions
unsupported by PDFCube keep the same instant fallback as normal playback.
The internal `--record-raw` CLI is intended for this script; use the script for
MP4 output. Recording ignores interactive demo mode and requires no `--program`.

## Coverage

- Box inward/outward: both select PDFCube's classic cube.
- Blinds: horizontal and vertical.
- Split: horizontal/vertical, opening/closing.
- Cover and Uncover: right, up, left and down.
- Fly: inward and outward in all eight compass directions, with scale 0.35
  and rectangular backing enabled.
- Push and Wipe: all four cardinal directions.
- Dissolve, Glitter (right/down/lower right), Fade and Replace.

Glitter, Wipe and Fade currently fall back to an instant change
in PDFCube. Their slides say so; they preserve the real PDF metadata to provide
a showcase as these effects are implemented. Other viewers may implement them
or ignore PDF transitions entirely. Replace is deliberately instant.

PDF transitions are stored on the **destination** page. Each `\ShowSlide` call
therefore declares the effect entering that slide, while its fourth argument
names the next slide's effect. Keep these aligned if you reorder or add slides.
The first and last pages use Replace; advancing from the final slide wraps
to the first page. Artwork is generated from the fifth argument (a numeric seed), and the
sixth argument chooses the background palette.

## Automatic README publication

`.github/workflows/showcase-video.yml` rebuilds the presentation and records a
30 fps MP4 when showcase or renderer sources change on `master`. Feature-branch
runs upload a downloadable Actions artifact; only `master` publishes the README.
The recording is verified before publication and kept below 10 MB. No video file
is committed to Git.

Publication uses GitHub CLI 2.99 or newer to attach the video to a single
**Showcase video (CI media)** issue, then places the returned attachment URL in
the marked README section. That URL renders as an inline player. Each successful
run replaces the issue body and README URL. Older attachments are not deleted.
The README includes the recorded source commit; stale runs cannot overwrite a
newer source revision. README-only bot commits do not trigger recording or CI.

One-time setup: create a repository Actions secret named `DEMO_MEDIA_TOKEN`
containing a fine-grained personal access token restricted to this repository,
with **Contents: read and write** and **Issues: read and write**. The token owner
must have push access. GitHub's attachment endpoint rejects the built-in Actions
installation token, so `GITHUB_TOKEN` cannot substitute for this secret.
The built-in token is used separately for the README commit. Repository rules
must permit that bot to update `master`; protected branches requiring pull
requests need a corresponding publication policy before enabling the workflow.
Do not paste a token into workflow YAML or commit it.

Without the secret, recording and the artifact upload still run; publication
fails with setup instructions. Use **Actions → Showcase video → Run workflow**
on `master` after adding the secret. Fork pull requests never receive it.
