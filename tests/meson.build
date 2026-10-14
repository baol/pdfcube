python = import('python').find_installation('python3')

test(
  'version derivation',
  python,
  args: [
    files('test_version.py'),
    files('../tools/version.py'),
    files('../tools/next_version.py'),
  ],
)

navigation_test = executable(
  'navigation-test',
  files('test_navigation.cc'),
  include_directories: include_directories('../src'),
)

test('numeric navigation bindings', navigation_test)

zoom_test = executable(
  'zoom-test',
  files('test_zoom.cc'),
  include_directories: include_directories('../src'),
)
test('legacy zoom framing and animation', zoom_test)

document_resolution_test = executable(
  'document-resolution-test',
  files('test_document.cc', '../src/document.cc'),
  dependencies: [poppler, cairo],
  include_directories: include_directories('../src'),
)

test(
  'document rendering resolution',
  document_resolution_test,
  args: [files('fixture-wide.pdf')],
)

test(
  'command-line compatibility',
  python,
  args: [files('test_cli.py'), pdfcube_exe, meson.project_version()],
)

xvfb_run = find_program('xvfb-run', required: false)
transition_metadata_test = executable(
  'transition-metadata-test',
  files('test_transition_metadata.cc', '../src/document.cc'),
  dependencies: [poppler, cairo],
  include_directories: include_directories('../src'),
)
if xvfb_run.found()
  render_test_environment = environment()
  render_test_environment.set('ASAN_OPTIONS', 'detect_leaks=0:halt_on_error=1:abort_on_error=1')
  test(
    'blinds rendering and PDF metadata',
    python,
    args: [files('test_blinds.py'), xvfb_run.full_path(), pdfcube_exe, transition_metadata_test],
    timeout: 120,
    env: render_test_environment,
  )
  test(
    'rendering compatibility',
    python,
    args: [
      files('test_render.py'),
      xvfb_run.full_path(),
      pdfcube_exe,
      files('fixture.pdf'),
      files('fixture-wide.pdf'),
    ],
    timeout: 60,
    env: render_test_environment,
  )
endif

