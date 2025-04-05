# Copyright (c) 2022-2025, PostgreSQL Global Development Group

if not libcurl.found() or host_system == 'windows'
  subdir_done()
endif

libpq_oauth_sources = files(
  'oauth-curl.c',
  'oauth-utils.c',
)

export_file = custom_target('libpq-oauth.exports',
  kwargs: gen_export_kwargs,
)

# port needs to be in include path due to pthread-win32.h
libpq_oauth_inc = include_directories('.', '../libpq', '../../port')

# This is an internal module; we don't want an SONAME and therefore do not set
# SO_MAJOR_VERSION. (We still put the major version into the name, to make it
# obvious where the library belongs.)
libpq_oauth_so = shared_module('libpq-oauth-' + pg_version_major.to_string(),
  libpq_oauth_sources,
  include_directories: [libpq_oauth_inc, postgres_inc],
  c_pch: pch_postgres_fe_h,
  dependencies: [frontend_shlib_code, libpq, libpq_oauth_deps],
  link_depends: export_file,
  link_args: export_fmt.format(export_file.full_path()),
  kwargs: default_lib_args,
)

subdir('po', if_found: libintl)
