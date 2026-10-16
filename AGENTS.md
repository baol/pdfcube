# Development rules

- Fetch latest `origin/master` and rebase before pushing. Do not merge master
  into feature branches. State the exact branch and pushed commit in handoffs.
- Boolean CLI switches must not consume a following PDF filename. With Boost
  Program Options, normalize bare switches to explicit `=true`, preserving
  explicit `=false` and literal filenames after `--`. Cover all three forms
  with parser tests, especially a bare switch immediately before a PDF path.
- Keep each test within 500 ms; exceptional slow tests may take up to 1 second.
  Exercise renderer frames at explicit progress values in a shared headless
  context rather than starting a GUI/X server per frame.
- Never verify pauses by sleeping or waiting for wall-clock time. Inject time
  and a scheduler, and assert the requested delays and rendered frame states.
- Retain endpoint, aspect-ratio, metadata, determinism, and optional-rendering
  coverage when optimizing tests. Do not hide slow tests by merely increasing
  timeouts or dropping assertions.
