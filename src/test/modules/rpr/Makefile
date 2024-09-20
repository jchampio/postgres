
PGFILEDESC = "SQL/RPR preferment test program"
PGAPPICON = win32

TAP_TESTS = 1
REGRESS = rpr

OBJS = rpr_prefer.o $(WIN32RES)

EXTRA_CLEAN = rpr_prefer$(X)

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = src/test/modules/rpr
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

all: rpr_prefer$(X)

rpr_prefer$(X): rpr_prefer.o $(WIN32RES)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDFLAGS_EX) -o $@
