#-------------------------------------------------------------------------
#
# Makefile for libpq-oauth
#
# Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
# Portions Copyright (c) 1994, Regents of the University of California
#
# src/interfaces/libpq-oauth/Makefile
#
#-------------------------------------------------------------------------

subdir = src/interfaces/libpq-oauth
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

PGFILEDESC = "libpq-oauth - device authorization OAuth support"

# This is an internal module; we don't want an SONAME and therefore do not set
# SO_MAJOR_VERSION.
NAME = libpq-oauth-$(MAJORVERSION)

override CPPFLAGS := -I$(libpq_srcdir) -I$(top_builddir)/src/port $(LIBCURL_CPPFLAGS) $(CPPFLAGS)

OBJS = \
	$(WIN32RES) \
	oauth-curl.o \
	oauth-utils.o

SHLIB_LINK_INTERNAL = $(libpq_pgport_shlib)
SHLIB_LINK = $(LIBCURL_LDFLAGS) $(LIBCURL_LDLIBS)
SHLIB_PREREQS = submake-libpq
SHLIB_EXPORTS = exports.txt

# Disable -bundle_loader on macOS.
BE_DLLLIBS =

# Make dependencies on pg_config_paths.h visible in all builds.
oauth-curl.o: oauth-curl.c $(top_builddir)/src/port/pg_config_paths.h

$(top_builddir)/src/port/pg_config_paths.h:
	$(MAKE) -C $(top_builddir)/src/port pg_config_paths.h

all: all-lib

# Shared library stuff
include $(top_srcdir)/src/Makefile.shlib

install: all installdirs install-lib

installdirs: installdirs-lib

uninstall: uninstall-lib

clean distclean: clean-lib
	rm -f $(OBJS)
