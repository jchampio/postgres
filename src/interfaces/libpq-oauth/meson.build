# Copyright (c) 2022-2025, PostgreSQL Global Development Group

if not libcurl.found() or host_system == 'windows'
  subdir_done()
endif

libpq_sources = files(
  'fe-auth-oauth-curl.c',
)
libpq_so_sources = [] # for shared lib, in addition to the above

export_file = custom_target('libpq-oauth.exports',
  kwargs: gen_export_kwargs,
)

# port needs to be in include path due to pthread-win32.h
libpq_oauth_inc = include_directories('.', '../libpq', '../../port')
libpq_c_args = ['-DSO_MAJOR_VERSION=1']

# Not using both_libraries() here as
# 1) resource files should only be in the shared library
# 2) we want the .pc file to include a dependency to {pgport,common}_static for
#    libpq_st, and {pgport,common}_shlib for libpq_sh
#
# We could try to avoid building the source files twice, but it probably adds
# more complexity than its worth (reusing object files requires also linking
# to the library on windows or breaks precompiled headers).
libpq_oauth_st = static_library('libpq-oauth',
  libpq_sources,
  include_directories: [libpq_oauth_inc],
  c_args: libpq_c_args,
  c_pch: pch_postgres_fe_h,
  dependencies: [frontend_stlib_code, libpq_deps],
  kwargs: default_lib_args,
)

libpq_oauth_so = shared_library('libpq-oauth',
  libpq_sources + libpq_so_sources,
  include_directories: [libpq_oauth_inc, postgres_inc],
  c_args: libpq_c_args,
  c_pch: pch_postgres_fe_h,
  version: '1.' + pg_version_major.to_string(),
  soversion: host_system != 'windows' ? '1' : '',
  darwin_versions: ['1', '1.' + pg_version_major.to_string()],
  dependencies: [frontend_shlib_code, libpq, libpq_oauth_deps],
  link_depends: export_file,
  link_args: export_fmt.format(export_file.full_path()),
  kwargs: default_lib_args,
)

libpq_oauth = declare_dependency(
  link_with: [libpq_oauth_so],
  include_directories: [include_directories('.')]
)

pkgconfig.generate(
  name: 'libpq-oauth',
  description: 'PostgreSQL libpq library, device authorization oauth support',
  url: pg_url,
  libraries: libpq_oauth,
  libraries_private: [frontend_stlib_code, libpq_oauth_deps],
)

subdir('po', if_found: libintl)
