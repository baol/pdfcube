PDFCube 0.1.0
=============

PDFCube presents PDF slide decks with animated 3D cube transitions,
programmed slide timing, and keyboard-controlled zoom regions. It is especially
useful with presentations created by LaTeX Beamer or office suites.

Please see the man page for usage information.

Build
-----

The maintained application uses C++20, GTK 4 `GtkGLArea`, libepoxy,
shader-based OpenGL 3.3, Poppler GLib, Cairo, Boost Program Options, and Meson.
The previous GtkGLExt/fixed-function implementation remains in
`src/pdfcube.cc` as historical source, but is not part of the Meson build.

On Debian 13 or Ubuntu 24.04, install Meson, Ninja, a C++ compiler, and the
development packages for GTK 4, libepoxy, Poppler GLib, Cairo, and Boost
Program Options. Then run:

```
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Run a presentation with:

```
./build/src/pdfcube presentation.pdf
```

The test suite verifies the command-line contract and produces deterministic
headless rendering snapshots under Xvfb. This covers page rendering, cube
transitions, configured colors, and command-line-over-config precedence.
`--snapshot FILE` and `--snapshot-progress VALUE` are private test hooks.

Contact information
-------------------

Feel free to write to <mirko.maischberger@gmail.com>

Word of Mouth
-------------

If you like this project please support by voting on this sites or blogging it:

 * http://freshmeat.net/projects/pdfcube
 * http://www.gnomefiles.org/app.php/PDF_Cube

Happy Hacking
> Mirko Maischberger
