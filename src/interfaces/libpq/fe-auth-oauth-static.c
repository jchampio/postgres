/*-------------------------------------------------------------------------
 *
 * fe-auth-oauth-static.c
 *
 *     Implements the builtin flow using the libpq-oauth.a staticlib.
 *     See also fe-auth-oauth-dynamic.c, which loads a plugin at runtime.
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/interfaces/libpq/fe-auth-oauth-static.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#ifndef USE_LIBCURL
#error this should only be compiled when OAuth support is enabled
#endif

#include "fe-auth-oauth.h"

/* see libpq-oauth/oauth-curl.h */
extern PostgresPollingStatusType pg_fe_run_oauth_flow(PGconn *conn);
extern void pg_fe_cleanup_oauth_flow(PGconn *conn);

/*
 * Loads the builtin flow from libpq-oauth.a.
 */
bool
use_builtin_flow(PGconn *conn, fe_oauth_state *state)
{
	/* Set our asynchronous callbacks. */
	conn->async_auth = pg_fe_run_oauth_flow;
	conn->cleanup_async_auth = pg_fe_cleanup_oauth_flow;

	return true;
}
