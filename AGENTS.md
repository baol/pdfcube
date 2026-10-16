# Development rules

- Fetch latest `origin/master` and rebase before pushing. Do not merge master
  into feature branches. State the exact branch and pushed commit in handoffs.
- Boolean CLI switches must not consume a following PDF filename. With Boost
  Program Options, normalize bare switches to explicit `=true`, preserving
  explicit `=false` and literal filenames after `--`. Cover all three forms
  with parser tests, especially a bare switch immediately before a PDF path.
- Target 500 ms for every test, and check every measured result before pushing.
  These are performance SLOs, not functional pass/fail thresholds. Report
  misses without failing correct tests; use a separate watchdog for hangs.
  Exercise renderer frames at explicit progress values in a shared headless
  context rather than starting a GUI/X server per frame.
- Never verify pauses by sleeping or waiting for wall-clock time. Inject time
  and a scheduler, and assert the requested delays and rendered frame states.
- Test snapshot isolation with separate processes and, on Linux, a private
  session bus. A no-bus fallback can hide D-Bus bugs; use readiness messages.
- Retain endpoint, aspect-ratio, metadata, determinism, and optional-rendering
  coverage when optimizing tests. Do not hide slow tests by merely increasing
  timeouts or dropping assertions.
