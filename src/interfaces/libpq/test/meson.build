# Copyright (c) 2022-2025, PostgreSQL Global Development Group

libpq_test_deps = []

libpq_uri_regress_sources = files(
  'libpq_uri_regress.c',
)

if host_system == 'windows'
  libpq_uri_regress_sources += rc_bin_gen.process(win32ver_rc, extra_args: [
    '--NAME', 'libpq_uri_regress',
    '--FILEDESC', 'libpq test program',])
endif

libpq_test_deps += executable('libpq_uri_regress',
  libpq_uri_regress_sources,
  dependencies: [frontend_no_fe_utils_code, libpq],
  kwargs: default_bin_args + {
    'install': false,
  }
)


libpq_testclient_sources = files(
  'libpq_testclient.c',
)

if host_system == 'windows'
  libpq_testclient_sources += rc_bin_gen.process(win32ver_rc, extra_args: [
    '--NAME', 'libpq_testclient',
    '--FILEDESC', 'libpq test program',])
endif

libpq_test_deps += executable('libpq_testclient',
  libpq_testclient_sources,
  dependencies: [frontend_no_fe_utils_code, libpq],
  kwargs: default_bin_args + {
    'install': false,
  }
)

if libcurl.found()
  oauth_test_sources = libpq_sources

  if host_system == 'windows'
    oauth_test_sources += rc_bin_gen.process(win32ver_rc, extra_args: [
      '--NAME', 'oauth_tests',
      '--FILEDESC', 'OAuth unit test program',])
  endif

  oauth_test_cargs = [
    '-DPG_BUILD_OAUTH_UNIT_TESTS',
  ]

  libpq_test_deps += executable('oauth_tests',
    oauth_test_sources,
    dependencies: [frontend_shlib_code, libpq_deps],
    kwargs: default_bin_args + {
      'c_args': default_bin_args.get('c_args', []) + oauth_test_cargs,
      'c_pch': pch_postgres_fe_h,
      'include_directories': [libpq_inc, postgres_inc],
      'install': false,
    }
  )
endif

testprep_targets += libpq_test_deps
