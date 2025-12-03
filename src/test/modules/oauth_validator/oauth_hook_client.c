/*-------------------------------------------------------------------------
 *
 * oauth_hook_client.c
 *		Test driver for t/002_client.pl, which verifies OAuth hook
 *		functionality in libpq.
 *
 * Portions Copyright (c) 1996-2026, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *		src/test/modules/oauth_validator/oauth_hook_client.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <sys/socket.h>

#include "libpq-fe.h"

#include "oauth_test_common.h"

int
main(int argc, char *argv[])
{
	const char *conninfo = oauth_test_parse_argv(argc, argv, 0 /* hook */ );
	PGconn	   *conn;

	/* Set up our OAuth hooks. */
	PQsetAuthDataHook(oauth_test_authdata_hook);

	/* Connect. (All the actual work is in the hook.) */
	if (stress_async)
	{
		/*
		 * Perform an asynchronous connection, busy-looping on PQconnectPoll()
		 * without actually waiting on socket events. This stresses code paths
		 * that rely on asynchronous work to be done before continuing with
		 * the next step in the flow.
		 */
		PostgresPollingStatusType res;

		conn = PQconnectStart(conninfo);

		do
		{
			res = PQconnectPoll(conn);
		} while (res != PGRES_POLLING_FAILED && res != PGRES_POLLING_OK);
	}
	else
	{
		/* Perform a standard synchronous connection. */
		conn = PQconnectdb(conninfo);
	}

	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "connection to database failed: %s\n",
				PQerrorMessage(conn));
		PQfinish(conn);
		return 1;
	}

	printf("connection succeeded\n");
	PQfinish(conn);
	return 0;
}
