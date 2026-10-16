# Documentation and usability roadmap

This review covers the README, manual, install instructions, compatibility guide,
transition design, diagnostic examples, Beamer showcase, CLI help, and application
key handling. It compares their descriptions with the current source code.

## 1. Give readers a clear first-run path

The README opens with a long catalogue of rendering details; installation and
controls come later. Release binaries are described in the packaging section,
while the opening path assumes a source build.

Lead with a short product description, the showcase, a link to release downloads,
and one command to open a PDF. Follow with a compact controls table and links to
source builds, transition design, and maintainer documentation. Keep the detailed
motion descriptions in the transition document.

Acceptance: a new user can find a suitable download, open a deck, advance, go
back, toggle automatic playback, and quit from the first screen of instructions.
Check the path on Linux, Windows, and macOS.

## 2. Make the feature and control reference consistent

Fade reached the renderer before the README, showcase labels, manual, and CLI
help were updated. The numeric-page formula in the manual used an internal
zero-based index. Test dependencies also differed between README and INSTALL.
The accompanying documentation edits align these descriptions; CLI help still
needs Fade added to its effect list. The gallery generates Fade by default,
but its `--effects` choices also need Fade added for focused captures.

Create a small authoritative effect catalogue for names, program tokens, PDF
mapping, and applicable parameters. Use it for CLI help and documentation checks.
Keep one user-facing controls reference linked from the README and showcase.
Describe `/Dur`, `/Trans /D`, programmed dwell, and automatic playback together,
with one worked example. Include `t` in the README's common controls.

Acceptance: every implemented effect appears in help, the manual, diagnostic
examples, and showcase with matching names. Examples match the parser. Page
shortcuts use visible page numbers. Validate local links and the generated man
page as part of documentation checks.

## 3. Complete and validate `--program`

The current implementation selects every implemented effect, including Fade,
through `duration:type` pairs. It repeats the final pair and applies each effect
to the following slide, including wraparound. Animation duration is fixed at
750 ms; axis, direction, and other effect parameters come from the PDF.

The remaining work is input validation and a complete, documented contract.
Unknown effect names currently select an instant change. Duration parsing uses
`std::stod` with a conversion to integer milliseconds, accepting numeric prefixes
and leaving finite-value and range checks implicit. Incomplete pairs, empty
fields, and entries beyond the page count also need explicit handling. CLI help
needs to list Fade.

Define the grammar and validate the entire program before applying it. Require
complete pairs, recognized tokens, and finite durations representable in the
scheduler's millisecond range. Return actionable errors identifying the offending
pair. Define zero-duration behavior, the final-pair repetition rule, wraparound,
and precedence over PDF timing. Preserve the existing `c` and `p` tokens.
Use the shared effect catalogue from priority 2 for program names and help.

Assess whether full programming should also expose animation duration, direction,
axis, and Fly parameters. Record the chosen scope before extending the syntax;
keep existing programs compatible and document PDF parameter inheritance.

Acceptance: parser and playback tests cover every effect token, fractional and
zero dwell times, repeated final pairs, wraparound, one-page decks, PDF overrides,
and CLI/config precedence. Invalid-input cases cover unknown tokens, empty or
incomplete pairs, trailing garbage, negative/nonfinite/overflowing durations, and
extra pairs. Validate every input pair even when the deck is shorter than the
program. Tests assert deterministic errors and scheduled delays using injected
time and a recording scheduler.

## 4. Add help at the moment of presentation

Application key handling exposes many commands, while discovering them requires
leaving the presentation to read the manual. Automatic playback toggles with `t`
but provides no explicit mode indicator in the current key-handling path.
File and renderer errors are printed to standard error, which makes failures
hard to discover when launching from the desktop.

Add a keyboard-help overlay opened by `?` or F1. Group controls into navigation,
zoom, playback, and exit. Show the current page and automatic-playback state in
a short-lived status overlay when those states change. Show file and graphics
errors in an actionable dialog with a way to choose another PDF or exit. Keep focus and keyboard
behavior predictable when overlays open and close.

Acceptance: help is readable over light and dark slides, works in fullscreen
and windowed modes, and can be dismissed by keyboard. Page numbers match the
manual. Desktop-launched failures produce a visible explanation. Overlay
visibility follows injected time and scheduler tests; exported
frames remain tied to the presentation content.

## 5. Keep the presentation and documentation current

The transition document accumulated implementation history, profiling results,
approval labels, and product descriptions in one place. The accompanying rewrite
makes it a reference for current behavior. Historical measurements belong in
commit/PR records; future changes belong here.

Use the Beamer showcase as the main visual introduction and diagnostic PDFs for
parameter inspection. Add a short documentation checklist to feature changes:
help, manual, examples, showcase labels, and transition design. Generate or
validate repeated feature lists and check local links. Keep platform evidence in
the compatibility guide and test procedures in the test contract.

Acceptance: product docs describe current behavior in positive terms; future
work is collected in the roadmap. Showcase labels describe the rendered effect,
and generated media identifies its source revision. A reviewer can trace each
example to its generator and reproduce its output.

Implement in this order: first-run documentation, reference consistency,
`--program` completion, presentation help, then automated documentation checks.
Program validation and presentation help are separate application changes; the
documentation and consistency work can ship in small increments.
