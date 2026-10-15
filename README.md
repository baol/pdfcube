# PDFCube

The [transition roadmap and review gallery](docs/transitions/README.md) describes
new effects, their validation gates, and how to generate videos and contact sheets.
Ready-to-open [example PDFs](examples/transitions/README.md) are included.
A self-contained [LaTeX/Beamer showcase](examples/showcase/README.md) covers
all standard PDF transitions with vector artwork and next-effect labels.
Blinds uses cascading slats; Split uses opening/closing 3D doors;
Cover/Uncover use curved page flips in all four directions. These have
reproducible screenshots and review videos linked from the roadmap.
Fly adds a controlled leaf-like drift in arbitrary PDF directions, with the
same reproducible review assets.

[![CI](https://github.com/baol/pdfcube/actions/workflows/ci.yml/badge.svg)](https://github.com/baol/pdfcube/actions/workflows/ci.yml)

PDFCube is a focused PDF presentation viewer with animated 3D cube
transitions, programmed slide timing, and keyboard-controlled zoom regions.
It is especially useful for slide decks produced by LaTeX Beamer and office
suites.

The current PDFCube release is a complete modernization of the original
application. It uses C++20, GTK 4 `GtkGLArea`, shader-based OpenGL/OpenGL ES 3,
libepoxy, Poppler GLib, Cairo, Boost Program Options, and Meson. The obsolete
GtkGLExt, fixed-function OpenGL, and Autotools implementation has been removed.

## Requirements

PDFCube requires GTK 4.12 or newer, Poppler GLib 22.0 or newer, and an
OpenGL or OpenGL ES 3.0-capable driver. On Debian 13 or Ubuntu 24.04, install
the build dependencies with:

```sh
sudo apt install g++ meson ninja-build pkg-config \
  libgtk-4-dev libepoxy-dev libpoppler-glib-dev libcairo2-dev \
  libboost-program-options-dev
```

The test suite additionally uses Python 3, Xvfb, Xauth, and Mesa's software
OpenGL driver. Install them with:

```sh
sudo apt install python3 xvfb xauth libgl1-mesa-dri
```

On macOS with Homebrew:

```sh
brew install meson ninja pkg-config gtk4 libepoxy poppler boost
```

On Windows, use an MSYS2 UCRT64 shell and install:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-{toolchain,meson,ninja,python,gtk4,libepoxy,poppler,boost}
```

The renderer selects GLSL 1.30 or GLSL ES 3.00 to match the context supplied
by GTK. Linux is tested on X11 and Wayland; CI also compiles and exercises the
command-line interface on Windows and macOS. Actual GPU/driver combinations
are tracked in [COMPATIBILITY.md](COMPATIBILITY.md) rather than assumed to work
untested.

## Build and install

```sh
meson setup build --buildtype=release
meson compile -C build
meson test -C build --print-errorlogs
sudo meson install -C build
```

Build the Beamer showcase explicitly with `meson compile -C build doc`.
This documentation target requires TeX Live with Beamer and PGF/TikZ;
see the [showcase build instructions](examples/showcase/README.md).
The default application build does not build documentation.

For a local build without installation:

```sh
./build/src/pdfcube presentation.pdf
```

See [INSTALL](INSTALL) for prefix, staging, and uninstall instructions.

## Usage

Run `pdfcube --help` for the complete command-line interface. The established
options remain available:

- `--bgcolor r:g:b` sets the background color.
- `--top-color r:g:b` sets the cube cap color.
- `--demo` enables automatic slide advancement.
- `--program n:x:...` programs durations and cube (`c`) or plain (`p`)
  transitions.
- `--no-fullscreen` starts in a window.

When no PDF is supplied, PDFCube opens a GTK file chooser. Options can also
be stored in `~/.pdfcuberc`; command-line values take precedence.

Common controls include Space or Page Down to advance, `c`/`a` for forward
and backward cube transitions, arrow keys for plain navigation, `h`/`j`/`k`/
`l`/`z` for zoom regions, `g` to reset zoom, `f` for fullscreen, and Escape
or Ctrl-Q to quit. Number keys `1`–`9` jump to pages 1, 6, 11, and so on;
the separate `Ctrl-1`–`Ctrl-9` bindings jump to the corresponding cube
section. Zoom keys `h`, `j`, `k`, and `l` focus the upper-left,
upper-right, lower-left, and lower-right regions; `z` focuses slightly below
the page center. They retain the original framing and 750 ms camera motion.
Any zoom key while zoomed in (or `g`) returns to the full page.
See `man pdfcube` for the full list.

## Tests

```sh
meson test -C build --print-errorlogs
```

The suite checks the public command-line contract and produces deterministic
headless rendering snapshots under Xvfb. It covers page rendering, cube and
plain programs, configured colors, and command-line-over-config precedence.
`--snapshot` and `--snapshot-progress` are private regression-test hooks.

## Release and packaging

Meson can produce a tested source archive with:

```sh
meson dist -C build --formats gztar
```

Every successful push to `master` creates the next patch release automatically.
[`MIN_VERSION`](MIN_VERSION) is the release floor: raise its major or minor
component manually when needed. The workflow creates the corresponding tag and
a GitHub Release containing the source archive, a Debian package built on the
latest Ubuntu LTS runner, an x86-64 AppImage, and SHA-256 checksums.

Debian packaging is maintained in `debian/`. Maintainers should follow
[RELEASING.md](RELEASING.md) before creating a release tag.

## License

PDFCube is licensed under the GNU General Public License, version 2 or later.
See [LICENSE](LICENSE).

PDFCube was created by Mirko Maischberger, with contributions from Karol
Sokolowski and others listed in the Git history.
