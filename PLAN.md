# Review follow-up plan

Implement correctness and crash fixes before rendering optimizations and cleanup.
Status reflects the current checkout; unchecked items still need implementation and validation.

## P0 — Lifetime and unsafe inputs

- [x] Run launches independently and guard repeated activation so a renderer cannot outlive its document through reactivation.
- [x] Validate every program pair before GTK starts, including unused tail entries; reject unknown effects, nonfinite/out-of-range durations and incomplete pairs.
- [x] Reject nonfinite colors and snapshot progress.
- [x] Check page availability, positive finite dimensions, Cairo allocation/context errors; release Poppler resources if construction fails.
- [x] Forward worker rendering failures to the UI instead of terminating the process.
- [x] Add live normal-launch regression coverage alongside snapshot isolation.

## P1 — Interaction correctness

- [x] Normalize letter keys and reject unsupported Ctrl/Alt/Super shortcuts while preserving Ctrl-number navigation.
- [x] Treat GTK dialog dismissal/cancellation as a clean exit.
- [x] Cancel old slide timers during navigation; restart after arrival; enabling demo without a duration waits.
- [x] Reset zoom on numeric and section jumps.
- [x] Add stateful keyboard, timer and zoom regressions.
- [ ] Automate native chooser dismissal coverage.
- [x] Fit mixed-aspect cube faces independently in a common viewport, with endpoint image regressions.

## P2 — Configuration and delivery

- [x] Help/version bypass broken rc files; use USERPROFILE if HOME is absent.
- [x] Keep bare boolean switches from consuming the following PDF filename (found by normal-launch regression).
- [x] Distinguish config diagnostics from command-line diagnostics.
- [x] Gate release publication on successful CI for the same commit; use the same Ubuntu version and enable Linux warnings-as-errors.
- [ ] Remove duplicated packaging work where release versioning permits artifact reuse.
- [ ] Pin all third-party workflow actions to verified commit hashes.

## P3 — Performance and maintainability

- [x] Remove redundant cube-transition state; derive sections from the transition vector.
- [x] Remove duplicate GL context activation per render.
- [x] Stop idle frame callbacks and restart for animation/preparation; poll only while preparation is outstanding.
- [x] Convert background-prepared pages in the worker and release their Cairo surfaces before upload. Initial uncached rendering retains a synchronous fallback.
- [ ] Batch repeated texture/uniform setup for segmented effects.
- [x] Consolidate direction helpers, frame metadata flags, shader bodies and pi constants.
- [ ] Consolidate remaining transition mappings and page-fitting helpers.
- [ ] Cache legacy cube tables and repeated zoom calculations.
- [ ] Measure startup metadata cost and rotating-text quality before choosing lazy loading and mipmaps.
- [x] Keep LICENSE as the authoritative GPL text and COPYING as a compatibility symlink.

## Corrections and behavior decisions

- `--no-fullscreen=false` already overrides `no-fullscreen=true` from the rc file; no fix needed.
- Wrapping from the final slide is existing navigation behavior, not an established defect. Preserve it pending a product decision.
- The first program pair controls the first page duration and the following transition; the last complete pair repeats. Incomplete pairs now produce actionable errors.
- Allocation failures must also be caught in the background worker; adding Document exceptions alone would introduce std::terminate.
- Performance claims and GTK-dependent behavior need runtime evidence, not just code inspection.

## Validation

- Local release build succeeds with `werror=true`.
- All 20 test checks pass across the full rendering run and final targeted reruns.
- Added checks cover invalid CLI numbers/programs, broken rc files, boolean switches
  before filenames, application keyboard/timer/zoom state, mixed-aspect cube
  endpoints, and concurrent snapshot/interactive launches.
- Launch isolation uses a dedicated D-Bus configuration without desktop service
  activation. Host portal activation otherwise polluted the test and delayed exit.
- `git diff --check` passes. Workflow scheduling/publication awaits GitHub execution;
  native chooser dismissal and cross-platform configuration fallback still need
  dedicated runtime coverage.
