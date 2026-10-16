# Test contract

Ordinary tests should finish within 500 ms. Native graphics tests target the
same budget, with at most 1 second for cold EGL/font/shader initialization.
Fast tests enforce a 450 ms subprocess budget, reserving startup overhead;
Meson gives every test a one-second hard timeout. Renderer cases run serially
to avoid software-GPU contention and enforce a 900 ms internal budget, leaving
room for Python/process startup. Do not increase these limits to hide a failure.

Each effect/direction/page-ratio case creates one surfaceless EGL context and
renders explicit progress values using the actual C++ Renderer, Document,
option parser, program dispatch, and shaders. It retains exact endpoints,
determinism, aspect ratios, metadata, direction/default handling, and optional
glass thickness checks without starting GTK or Xvfb for each frame.
Every case also renders and reads back all 61 frames of a 60 Hz second. A small
Fontconfig fixture uses DejaVu Sans instead of scanning all system font folders;
both cold font and shader initialization remain inside the measured budget.

Clock and auto-advance tests use synthetic timestamps and a recording
scheduler. They verify frame progress and the exact millisecond delays sent to
the backend, including cancellation and idle dispatch. They never sleep.

Parser tests call the actual parser in-process. Every boolean switch is tested
immediately before a PDF filename, with explicit false and with filenames after
the literal `--` delimiter. Snapshot isolation registers two real GApplications
with the same ID and verifies that both remain local, without launching windows.

The optional gallery still launches the full GUI to produce review videos; it
is a review-artifact build step, not a timing/animation test.
