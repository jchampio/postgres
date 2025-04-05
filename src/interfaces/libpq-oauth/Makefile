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

PGFILEDESC = "libpq-oauth - device authorization oauth support"

# This is an internal module; we don't want an SONAME and therefore do not set
# SO_MAJOR_VERSION. (We still put the major version into the name, to make it
# obvious where the library belongs.)
NAME = libpq-oauth-$(MAJORVERSION)

override CPPFLAGS := -I$(libpq_srcdir) -I$(top_builddir)/src/port $(CPPFLAGS)

OBJS = \
	$(WIN32RES) \
	oauth-curl.o \
	oauth-utils.o

SHLIB_LINK_INTERNAL = $(libpq_pgport_shlib)
SHLIB_LINK = -lcurl
SHLIB_PREREQS = submake-libpq

SHLIB_EXPORTS = exports.txt

PKG_CONFIG_REQUIRES_PRIVATE = libpq
#
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
