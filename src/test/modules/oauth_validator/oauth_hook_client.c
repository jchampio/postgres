/*-------------------------------------------------------------------------
 *
 * oauth_hook_client.c
 *		Test driver for t/002_client.pl, which verifies OAuth hook
 *		functionality in libpq.
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *		src/test/modules/oauth_validator/oauth_hook_client.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <stdio.h>
#include <stdlib.h>

#include "getopt_long.h"
#include "libpq-fe.h"

static int	handle_auth_data(PGauthData type, PGconn *conn, void *data);

static void
usage(char *argv[])
{
	fprintf(stderr, "usage: %s [flags] CONNINFO\n\n", argv[0]);

	fprintf(stderr, "recognized flags:\n");
	fprintf(stderr, " -h, --help				show this message\n");
	fprintf(stderr, " --expected-scope SCOPE	fail if received scopes do not match SCOPE\n");
	fprintf(stderr, " --expected-uri URI		fail if received configuration link does not match URI\n");
	fprintf(stderr, " --no-hook					don't install OAuth hooks (connection will fail)\n");
	fprintf(stderr, " --token TOKEN				use the provided TOKEN value\n");
}

static bool no_hook = false;
static const char *expected_uri = NULL;
static const char *expected_scope = NULL;
static char *token = NULL;

int
main(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},

		{"expected-scope", required_argument, NULL, 1000},
		{"expected-uri", required_argument, NULL, 1001},
		{"no-hook", no_argument, NULL, 1002},
		{"token", required_argument, NULL, 1003},
		{0}
	};

	const char *conninfo;
	PGconn	   *conn;
	int			c;

	while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1)
	{
		switch (c)
		{
			case 'h':
				usage(argv);
				return 0;

			case 1000:			/* --expected-scope */
				expected_scope = optarg;
				break;

			case 1001:			/* --expected-uri */
				expected_uri = optarg;
				break;

			case 1002:			/* --no-hook */
				no_hook = true;
				break;

			case 1003:			/* --token */
				token = optarg;
				break;

			default:
				usage(argv);
				return 1;
		}
	}

	if (argc != optind + 1)
	{
		usage(argv);
		return 1;
	}

	conninfo = argv[optind];

	/* Set up our OAuth hooks. */
	PQsetAuthDataHook(handle_auth_data);

	/* Connect. (All the actual work is in the hook.) */
	conn = PQconnectdb(conninfo);
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "Connection to database failed: %s\n",
				PQerrorMessage(conn));
		PQfinish(conn);
		return 1;
	}

	printf("connection succeeded\n");
	PQfinish(conn);
	return 0;
}

/*
 * PQauthDataHook implementation. Replaces the default client flow by handling
 * PQAUTHDATA_OAUTH_BEARER_TOKEN.
 */
static int
handle_auth_data(PGauthData type, PGconn *conn, void *data)
{
	PGoauthBearerRequest *req = data;

	if (no_hook || (type != PQAUTHDATA_OAUTH_BEARER_TOKEN))
		return 0;

	if (expected_uri)
	{
		if (!req->openid_configuration)
		{
			fprintf(stderr, "expected URI \"%s\", got NULL\n", expected_uri);
			return -1;
		}

		if (strcmp(expected_uri, req->openid_configuration) != 0)
		{
			fprintf(stderr, "expected URI \"%s\", got \"%s\"\n", expected_uri, req->openid_configuration);
			return -1;
		}
	}

	if (expected_scope)
	{
		if (!req->scope)
		{
			fprintf(stderr, "expected scope \"%s\", got NULL\n", expected_scope);
			return -1;
		}

		if (strcmp(expected_scope, req->scope) != 0)
		{
			fprintf(stderr, "expected scope \"%s\", got \"%s\"\n", expected_scope, req->scope);
			return -1;
		}
	}

	req->token = token;
	return 1;
}
