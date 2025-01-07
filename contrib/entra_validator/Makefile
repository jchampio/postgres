# contrib/entra_validator/Makefile

MODULES = entra_validator
PGFILEDESC = "entra_validator - validate EntraID tokens"

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/entra_validator
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

install: install-entra-script

.PHONY: install-entra-script
install-entra-script:
	$(INSTALL_SCRIPT) $(addprefix $(srcdir)/, entra_validator.py) '$(DESTDIR)$(pkglibdir)/'
