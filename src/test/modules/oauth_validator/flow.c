/*-------------------------------------------------------------------------
 *
 * flow.c
 *	  Test plugin for clientside OAuth flows
 *
 * Copyright (c) 2025, PostgreSQL Global Development Group
 *
 * src/test/modules/oauth_validator/flow.c
 *
 *-------------------------------------------------------------------------
 */

/* Since we want to test the public API, only include public headers here. */
#include "libpq-fe.h"
#include "libpq-oauth.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#define SOCKET int
#endif

static PostgresPollingStatusType
flow_async(PGconn *conn, PGoauthBearerRequest *request, SOCKET *altsock)
{
	request->token = "flowtoken";
	return PGRES_POLLING_OK;
}

static void
flow_cleanup(PGconn *conn, PGoauthBearerRequest *request)
{
}

int
pg_run_oauthbearer(PGconn *conn, PGoauthBearerRequest *request)
{
	request->async = flow_async;
	request->cleanup = flow_cleanup;
	return 0;
}
