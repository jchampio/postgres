/*-------------------------------------------------------------------------
 *
 * fe-auth-oauth.h
 *
 *	  Definitions for OAuth authentication implementations
 *
 * Portions Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * src/interfaces/libpq/fe-auth-oauth.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef FE_AUTH_OAUTH_H
#define FE_AUTH_OAUTH_H

#include "libpq-fe.h"
#include "libpq-int.h"


enum fe_oauth_step
{
	FE_OAUTH_INIT,
	FE_OAUTH_REQUESTING_TOKEN,
	FE_OAUTH_BEARER_SENT,
	FE_OAUTH_SERVER_ERROR,
};

typedef struct
{
	enum fe_oauth_step step;

	PGconn	   *conn;
	char	   *token;

	void	   *async_ctx;
	void		(*free_async_ctx) (PGconn *conn, void *ctx);
} fe_oauth_state;

extern PostgresPollingStatusType pg_fe_run_oauth_flow(PGconn *conn, pgsocket *altsock);
extern bool oauth_unsafe_debugging_enabled(void);

/* Mechanisms in fe-auth-oauth.c */
extern const pg_fe_sasl_mech pg_oauth_mech;

#endif							/* FE_AUTH_OAUTH_H */
