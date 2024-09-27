
PGFILEDESC = "SQL/RPR preferment test program"
PGAPPICON = win32

TAP_TESTS = 1
REGRESS = rpr

OBJS = \
	gram.o \
	parser.o \
	rpr_prefer.o \
	scan.o \
	$(WIN32RES)

EXTRA_CLEAN = rpr_prefer$(X) gram.c gram.h scan.c lex.backup

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

rpr_prefer$(X): $(OBJS) | submake-libpgport
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@$(X)

# See notes in src/backend/parser/Makefile about the following two rules
gram.h: gram.c
	touch $@

gram.c: BISONFLAGS += -d

scan.c: FLEXFLAGS = -CF -p -p
#scan.c: FLEX_NO_BACKUP=yes
scan.c: FLEX_FIX_WARNING=yes

# Force these dependencies to be known even without dependency info built:
gram.o scan.o parser.o: gram.h
