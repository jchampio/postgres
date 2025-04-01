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

#include "libpq-int.h"

extern PGDLLEXPORT pgthreadlock_t pg_g_threadlock;

void		libpq_append_conn_error(PGconn *conn, const char *fmt,...) pg_attribute_printf(2, 3);
bool		oauth_unsafe_debugging_enabled(void);
int			pq_block_sigpipe(sigset_t *osigset, bool *sigpipe_pending);
void		pq_reset_sigpipe(sigset_t *osigset, bool sigpipe_pending, bool got_epipe);
