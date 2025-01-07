# Copyright (c) 2022-2024, PostgreSQL Global Development Group

entra_validator_sources = files(
  'entra_validator.c',
)

if host_system == 'windows'
  entra_validator_sources += rc_lib_gen.process(win32ver_rc, extra_args: [
    '--NAME', 'entra_validator',
    '--FILEDESC', 'entra_validator - validate EntraID tokens',])
endif

entra_validator = shared_module('entra_validator',
  entra_validator_sources,
  kwargs: contrib_mod_args,
)
contrib_targets += entra_validator

install_data(
  'entra_validator.py',
  kwargs: {
    'install_dir': dir_lib_pkg,
  },
)
