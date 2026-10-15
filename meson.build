project(
  'pdfcube',
  'cpp',
  version: run_command('python3', 'tools/version.py', check: true).stdout().strip(),
  license: 'GPL-2.0-or-later',
  license_files: 'LICENSE',
  meson_version: '>=1.3.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=false',
  ],
)

gtk = dependency('gtk4', version: '>=4.12')
poppler = dependency('poppler-glib', version: '>=22.0')
cairo = dependency('cairo')
epoxy = dependency('epoxy')
boost = dependency('boost', modules: ['program_options'])

version_configuration = configuration_data()
version_configuration.set_quoted('PDFCUBE_VERSION', meson.project_version())
configure_file(output: 'pdfcube-config.hpp', configuration: version_configuration)

release_configuration = configuration_data()
release_configuration.set('VERSION', meson.project_version())
configured_metainfo = configure_file(
  input: 'org.pdfcube.pdfcube.metainfo.xml',
  output: 'org.pdfcube.pdfcube.metainfo.xml',
  configuration: release_configuration,
)
configured_manpage = configure_file(
  input: 'man/pdfcube.1',
  output: 'pdfcube.1',
  configuration: release_configuration,
)

subdir('src')
subdir('tests')

subdir('examples/showcase')

if host_machine.system() != 'windows' and host_machine.system() != 'darwin'
  install_data(
    'org.pdfcube.pdfcube.desktop',
    install_dir: get_option('datadir') / 'applications',
  )
  install_data(
    configured_metainfo,
    install_dir: get_option('datadir') / 'metainfo',
  )
  install_data(
    'data/org.pdfcube.pdfcube.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
  )
  install_man(configured_manpage)
endif

