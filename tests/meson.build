python = import('python').find_installation('python3')

test(
  'command-line compatibility',
  python,
  args: [files('test_cli.py'), pdfcube_exe, meson.project_version()],
)

xvfb_run = find_program('xvfb-run', required: false)
if xvfb_run.found()
  render_test_environment = environment()
  render_test_environment.set('ASAN_OPTIONS', 'detect_leaks=0:halt_on_error=1:abort_on_error=1')
  test(
    'rendering compatibility',
    python,
    args: [files('test_render.py'), xvfb_run.full_path(), pdfcube_exe, files('fixture.pdf')],
    timeout: 60,
    env: render_test_environment,
  )
endif

