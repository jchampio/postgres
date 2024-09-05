# Copyright (c) 2024, PostgreSQL Global Development Group

rpr_prefer_sources = files(
  'rpr_prefer.c',
)

if host_system == 'windows'
  rpr_prefer_sources += rc_bin_gen.process(win32ver_rc, extra_args: [
    '--NAME', 'rpr_prefer',
    '--FILEDESC', 'SQL/RPR preferment test program',
  ])
endif

rpr_prefer = executable('rpr_prefer',
  rpr_prefer_sources,
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
