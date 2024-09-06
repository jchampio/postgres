# Copyright (c) 2024, PostgreSQL Global Development Group

rpr_prefer_sources = files(
  'parser.c',
  'rpr_prefer.c',
)

rpr_grammar = custom_target('preproc',
  input: 'preproc.y',
  kwargs: bison_kw,
)
generated_sources += rpr_grammar.to_list()
rpr_prefer_sources += rpr_grammar

rpr_scanner = custom_target('scan',
  input: 'scan.l',
  output: 'scan.c',
  command: [flex_cmd, '--fix-warnings', '--', '-CF', '-p', '-p'],
)
generated_sources += rpr_scanner
rpr_prefer_sources += rpr_scanner

if host_system == 'windows'
  rpr_prefer_sources += rc_bin_gen.process(win32ver_rc, extra_args: [
    '--NAME', 'rpr_prefer',
    '--FILEDESC', 'SQL/RPR preferment test program',
  ])
endif

rpr_prefer = executable('rpr_prefer',
  rpr_prefer_sources,
  c_args: ['-DFRONTEND'],
  dependencies: [frontend_code],
  include_directories: include_directories('.'),
  kwargs: default_bin_args + {
    'install': false,
  },
)

tests += {
  'name': 'rpr',
  'sd': meson.current_source_dir(),
  'bd': meson.current_build_dir(),
  'tap': {
    'tests': [],
  },
}
