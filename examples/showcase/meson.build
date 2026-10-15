showcase_install_dir = get_option('datadir') / 'doc/pdfcube/examples'
showcase_pdf = custom_target(
  'beamer-showcase',
  input: 'showcase.tex',
  output: 'showcase.pdf',
  command: [
    python,
    files('../../tools/build_showcase.py'),
    '@INPUT@',
    '@OUTDIR@',
  ],
  build_by_default: false,
  install: get_option('install_docs'),
  install_dir: showcase_install_dir,
  install_tag: 'doc',
)
alias_target('doc', showcase_pdf)

if get_option('install_docs')
  install_data('showcase.tex', 'README.md', install_dir: showcase_install_dir,
               install_tag: 'doc')
endif
