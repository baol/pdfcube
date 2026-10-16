python = import('python').find_installation('python3')

foreach item : [
  ['deterministic glass fracture', 'test_dissolve.cc'],
  ['Fly leaf trajectory', 'test_fly.cc'],
  ['page flip connected geometry', 'test_page_flip.cc'],
  ['push contact geometry', 'test_push.cc'],
  ['split hinged geometry', 'test_split.cc'],
  ['numeric navigation bindings', 'test_navigation.cc'],
  ['legacy zoom framing and animation', 'test_zoom.cc'],
  ['animation clock and scheduled delays', 'test_timing.cc'],
]
  unit = executable(item[1].split('.')[0], files(item[1]),
                    include_directories: include_directories('../src'))
  test(item[0], python, args: [files('test_budget.py'), '0.45', unit], timeout: 1)
endforeach

test('version derivation', python,
     args: [files('test_budget.py'), '0.45', python.full_path(),
            files('test_version.py'), files('../tools/version.py'), files('../tools/next_version.py')],
     timeout: 1)
test('transition example freshness', python,
     args: [files('test_budget.py'), '0.45', python.full_path(), files('test_examples.py')], timeout: 1)

document_resolution_test = executable('document-resolution-test',
  files('test_document.cc', '../src/document.cc'), dependencies: [poppler, cairo],
  include_directories: include_directories('../src'))
test('document rendering resolution', python,
     args: [files('test_budget.py'), '0.45', document_resolution_test, files('fixture-wide.pdf')], timeout: 1)

options_test = executable('options-test', files('test_options.cc', '../src/options.cc'),
                          dependencies: [boost],
                          include_directories: include_directories('..', '../src'))
test('command-line compatibility', python,
     args: [files('test_budget.py'), '0.45', options_test], timeout: 1)

gio = dependency('gio-2.0')
playback_test = executable('playback-test', files('test_playback.cc'),
  dependencies: [poppler,cairo,epoxy], include_directories: include_directories('../src'))
test('PDF and program playback dispatch', python,
     args: [files('test_budget.py'), '0.45', playback_test], timeout: 1)
isolation_test = executable('snapshot-isolation-test', files('test_snapshot_isolation.cc'),
  dependencies: [gio], include_directories: include_directories('../src'))
test('snapshot registration isolation', python,
     args: [files('test_budget.py'), '0.45', isolation_test], timeout: 1)

if host_machine.system() == 'linux'
  egl = dependency('egl')
  render_driver = executable('render-driver',
    files('render_driver.cc', '../src/renderer.cc', '../src/document.cc', '../src/options.cc'),
    dependencies: [poppler, cairo, epoxy, egl, boost],
    include_directories: include_directories('..', '../src'))
  transition_metadata_test = executable('transition-metadata-test',
    files('test_transition_metadata.cc', '../src/document.cc'),
    dependencies: [poppler, cairo], include_directories: include_directories('../src'))
  render_environment = environment()
  render_environment.set('ASAN_OPTIONS', 'detect_leaks=0:halt_on_error=1:abort_on_error=1')
  render_environment.set('LIBGL_ALWAYS_SOFTWARE', '1')
  render_environment.set('FONTCONFIG_FILE', meson.current_source_dir() / 'fonts.conf')
  render_environment.set('XDG_CACHE_HOME', meson.current_build_dir() / 'native-test-cache')
  # A frame case targets 500 ms. Cold EGL/font/shader setup may use the
  # explicit one-second slow-test allowance. Serialize to avoid GPU contention.
  foreach effect : ['Blinds', 'Split', 'Cover', 'Uncover', 'Fly', 'Push', 'Dissolve']
    axes = ['H']
    angles = ['none']
    motions = ['I']
    if effect == 'Blinds' or effect == 'Split'
      axes = ['H', 'V']
    endif
    if effect == 'Split' or effect == 'Fly'
      motions = ['I', 'O']
    endif
    if effect == 'Cover' or effect == 'Uncover' or effect == 'Push'
      angles = ['0', '90', '180', '270']
    elif effect == 'Fly'
      angles = ['0', '45', '90', '135', '180', '225', '270', '315']
    endif
    foreach axis : axes
      foreach motion : motions
        foreach angle : angles
          ratios = ['0', '1', '2']
          if effect == 'Fly' and angle != '0' and angle != '90'
            ratios = ['0']
          endif
          foreach ratio : ratios
            test(effect + ' frame ' + axis + '-' + motion + '-' + angle + '-ratio' + ratio,
              python,
              args: [files('test_transitions.py'), render_driver, transition_metadata_test,
                     effect, motion, axis, angle, ratio],
              timeout: 1, is_parallel: false, suite: ['render', 'slow'],
              env: render_environment)
          endforeach
        endforeach
      endforeach
    endforeach
  endforeach
  test('rendering compatibility', python,
    args: [files('test_render.py'), render_driver, files('fixture.pdf'), files('fixture-wide.pdf')],
    timeout: 1, is_parallel: false, suite: ['render', 'slow'], env: render_environment)
endif

