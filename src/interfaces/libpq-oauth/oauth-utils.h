/*-------------------------------------------------------------------------
 *
 * oauth-utils.h
 *
 *	  Definitions providing missing libpq internal APIs
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/interfaces/libpq-oauth/oauth-utils.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef OAUTH_UTILS_H
#define OAUTH_UTILS_H

#include "libpq-fe.h"
#include "pqexpbuffer.h"

typedef char *(*libpq_gettext_func) (const char *msgid);
typedef PQExpBuffer (*conn_errorMessage_func) (PGconn *conn);

/* Initializes libpq-oauth. */
extern PGDLLEXPORT void libpq_oauth_init(pgthreadlock_t threadlock,
										 libpq_gettext_func gettext_impl,
										 conn_errorMessage_func errmsg_impl);

/* Duplicated APIs, copied from libpq. */
extern void libpq_append_conn_error(PGconn *conn, const char *fmt,...) pg_attribute_printf(2, 3);
extern bool oauth_unsafe_debugging_enabled(void);
extern int	pq_block_sigpipe(sigset_t *osigset, bool *sigpipe_pending);
extern void pq_reset_sigpipe(sigset_t *osigset, bool sigpipe_pending, bool got_epipe);

#endif							/* OAUTH_UTILS_H */
