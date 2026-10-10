project(
  'pdfcube',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.6.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=false',
  ],
)

gtk = dependency('gtk4', version: '>=4.10')
poppler = dependency('poppler-glib', version: '>=22.0')
cairo = dependency('cairo')
epoxy = dependency('epoxy')
boost = dependency('boost', modules: ['program_options'])

subdir('src')
subdir('tests')

install_data('pdfcube.desktop', install_dir: get_option('datadir') / 'applications')
install_data(
  'org.pdfcube.pdfcube.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo',
)
install_data(
  'data/org.pdfcube.pdfcube.svg',
  install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
)
install_man('man/pdfcube.1')

