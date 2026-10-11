# PDFCube

[![CI](https://github.com/baol/pdfcube/actions/workflows/ci.yml/badge.svg)](https://github.com/baol/pdfcube/actions/workflows/ci.yml)

PDFCube is a focused PDF presentation viewer with animated 3D cube
transitions, programmed slide timing, and keyboard-controlled zoom regions.
It is especially useful for slide decks produced by LaTeX Beamer and office
suites.

PDFCube 0.1.0 is a complete modernization of the original application. It
uses C++20, GTK 4 `GtkGLArea`, shader-based OpenGL 3.3, libepoxy, Poppler
GLib, Cairo, Boost Program Options, and Meson. The obsolete GtkGLExt,
fixed-function OpenGL, and Autotools implementation has been removed.

## Requirements

PDFCube requires GTK 4.10 or newer, Poppler GLib 22.0 or newer, and an
OpenGL 3.3-capable driver. On Debian 13 or Ubuntu 24.04, install the build
dependencies with:

```sh
sudo apt install g++ meson ninja-build pkg-config \
  libgtk-4-dev libepoxy-dev libpoppler-glib-dev libcairo2-dev \
  libboost-program-options-dev
```

The test suite additionally uses Python 3, Xvfb, and Xauth.

## Build and install

```sh
meson setup build --buildtype=release
meson compile -C build
meson test -C build --print-errorlogs
sudo meson install -C build
```

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
or Ctrl-Q to quit. See `man pdfcube` for the full list.

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
meson dist -C build
```

Debian packaging is maintained in `debian/`. Maintainers should follow
[RELEASING.md](RELEASING.md) before creating a release tag.

## License

PDFCube is licensed under the GNU General Public License, version 2 or later.
See [LICENSE](LICENSE).

PDFCube was created by Mirko Maischberger, with contributions from Karol
Sokolowski and others listed in the Git history.
