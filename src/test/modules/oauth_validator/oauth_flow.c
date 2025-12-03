/*-------------------------------------------------------------------------
 *
 * oauth_flow.c
 *	  Test plugin for clientside OAuth flows
 *
 * Copyright (c) 2025, PostgreSQL Global Development Group
 *
 *-------------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>

/* Since we want to test the public API, only include public headers here. */
#include "libpq-fe.h"
#include "libpq-oauth.h"
#include "oauth_test_common.h"

static void
load_test_flags(void)
{
	int			argc;
	char	  **argv;
	char	   *env = getenv("OAUTH_TEST_FLAGS");
	int			flag_count;
	int			i;

	if (!env || !env[0])
	{
		fprintf(stderr, "OAUTH_TEST_FLAGS must be set\n");
		exit(1);
	}

	flag_count = 1;
	for (char *c = env; *c; c++)
	{
		if (*c == '\x01')
			flag_count++;
	}

	/* Slice OAUTH_TEST_FLAGS into a fake argv array. */
	env = strdup(env);
	argc = flag_count + 1;
	argv = malloc(sizeof(*argv) * (argc + 1));

	if (!env || !argv)
	{
		fprintf(stderr, "out of memory");
		exit(1);
	}

	argv[0] = "[plugin test]";
	for (i = 1; i < flag_count; i++)
	{
		argv[i] = env;

		env = strchr(env, '\x01');
		*env++ = '\0';
	}
	argv[flag_count] = env;
	argv[argc] = NULL;

	oauth_test_parse_argv(argc, argv, 1 /* plugin */ );
}

int
pg_start_oauthbearer(PGconn *conn, PGoauthBearerRequestV2 *request)
{
	load_test_flags();

	return oauth_test_start_flow(conn, request);
}
