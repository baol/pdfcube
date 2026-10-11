project(
  'pdfcube',
  'cpp',
  version: '0.1.0',
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

subdir('src')
subdir('tests')

if host_machine.system() != 'windows' and host_machine.system() != 'darwin'
  install_data(
    'org.pdfcube.pdfcube.desktop',
    install_dir: get_option('datadir') / 'applications',
  )
  install_data(
    'org.pdfcube.pdfcube.metainfo.xml',
    install_dir: get_option('datadir') / 'metainfo',
  )
  install_data(
    'data/org.pdfcube.pdfcube.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
  )
  install_man('man/pdfcube.1')
endif

