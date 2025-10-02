/*
 * gen_libpqrc: prints the compile-time defaults for all libpq connection
 * options in the libpqrc file format.
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 */

#include "postgres_fe.h"

#include <stdio.h>

#include "libpq-fe.h"

#define DISPCHAR_DISPLAY	""
#define DISPCHAR_PASSWORD	"*"
#define DISPCHAR_DEBUG		"D"

static void print_option_section(PQconninfoOption *opts, const char *dispchar);

int
main(int argc, char *argv[])
{
	PQconninfoOption *options = PQconndefaults();

	if (!options)
	{
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	printf(
		   "# ------------------------\n"
		   "# libpq configuration file\n"
		   "# ------------------------\n"
		);

	/*
	 * Only the "display" and "password" option types are added to the file
	 * (and the password options are added only to advise against their use).
	 * Debug options are purposefully suppressed, since they won't be loaded
	 * from file anyway.
	 */
	printf("\n"
		   "#-------------------------------------------------------------------------------\n"
		   "# STANDARD OPTIONS\n"
		   "#-------------------------------------------------------------------------------\n"
		   "\n");
	print_option_section(options, DISPCHAR_DISPLAY);

	printf("\n"
		   "#-------------------------------------------------------------------------------\n"
		   "# PASSWORD OPTIONS\n"
		   "#\n"
		   "# These options are connection secrets. Hardcoding them in this (globally\n"
		   "# readable) configuration is not recommended.\n"
		   "#-------------------------------------------------------------------------------\n"
		   "\n");
	print_option_section(options, DISPCHAR_PASSWORD);

	return 0;
}

static void
print_option_section(PQconninfoOption *options, const char *dispchar)
{
	char	   *defval;

	for (PQconninfoOption *opt = options; opt->keyword; opt++)
	{
		/* Skip unwanted option types. */
		if (strcmp(opt->dispchar, dispchar) != 0)
			continue;

		defval = opt->compiled ? opt->compiled : "";
		printf("#%s = '%s'\n", opt->keyword, defval);
	}
}
