gnome = import('gnome')

pdfcube_resources = gnome.compile_resources(
  'pdfcube-resources',
  '../data/pdfcube.gresource.xml',
  source_dir: '../data',
)

pdfcube_sources = files(
  'main.cc',
  'options.cc',
  'document.cc',
  'renderer.cc',
  'application.cc',
) + pdfcube_resources

pdfcube_exe = executable(
  'pdfcube',
  pdfcube_sources,
  dependencies: [gtk, poppler, cairo, epoxy, boost],
  include_directories: include_directories('..'),
  install: true,
)

