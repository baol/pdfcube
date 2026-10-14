python = import('python').find_installation('python3')

fly_test = executable('fly-test', files('test_fly.cc'),
                      include_directories: include_directories('../src'))
test('Fly leaf trajectory', fly_test)

flip_test = executable('page-flip-test', files('test_page_flip.cc'),
                       include_directories: include_directories('../src'))
test('page flip connected geometry', flip_test)

split_test = executable('split-test', files('test_split.cc'),
                        include_directories: include_directories('../src'))
test('split hinged geometry', split_test)

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
  foreach effect : ['Cover', 'Uncover']
    test(
      effect + ' rendering and PDF metadata',
      python,
      args: [files('test_transitions.py'), xvfb_run.full_path(), pdfcube_exe,
             transition_metadata_test, effect],
      timeout: 240,
      env: render_test_environment,
    )
  endforeach
  foreach motion : ['I', 'O']
    test(
      'Fly rendering and PDF metadata ' + motion,
      python,
      args: [files('test_transitions.py'), xvfb_run.full_path(), pdfcube_exe,
             transition_metadata_test, 'Fly', motion],
      timeout: 360,
      env: render_test_environment,
    )
  endforeach
  foreach motion : ['I', 'O']
    test(
      'split rendering and PDF metadata ' + motion,
      python,
      args: [files('test_transitions.py'), xvfb_run.full_path(), pdfcube_exe,
             transition_metadata_test, 'Split', motion],
      timeout: 120,
      env: render_test_environment,
    )
  endforeach
  test(
    'blinds rendering and PDF metadata',
    python,
    args: [files('test_transitions.py'), xvfb_run.full_path(), pdfcube_exe, transition_metadata_test],
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
  dbus_run_session = find_program('dbus-run-session', required: false)
  if dbus_run_session.found()
    test(
      'parallel snapshot isolation',
      dbus_run_session,
      args: [
        python.full_path(),
        files('test_snapshot_isolation.py'),
        xvfb_run.full_path(),
        pdfcube_exe,
        files('fixture.pdf'),
        files('fixture-wide.pdf'),
      ],
      timeout: 60,
      env: render_test_environment,
    )
  endif
endif

