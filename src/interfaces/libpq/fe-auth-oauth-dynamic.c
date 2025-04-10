/*-------------------------------------------------------------------------
 *
 * fe-auth-oauth-dynamic.c
 *
 *     Implements the builtin flow by loading the libpq-oauth plugin.
 *     See also fe-auth-oauth-static.c, for static builds.
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/interfaces/libpq/fe-auth-oauth-dynamic.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#ifndef USE_LIBCURL
#error this should only be compiled when OAuth support is enabled
#endif

#include <dlfcn.h>

#include "fe-auth-oauth.h"

typedef char *(*libpq_gettext_func) (const char *msgid);
typedef PQExpBuffer (*conn_errorMessage_func) (PGconn *conn);

/*
 * This shim is injected into libpq-oauth so that it doesn't depend on the
 * offset of conn->errorMessage.
 *
 * TODO: look into exporting libpq_append_conn_error or a comparable API from
 * libpq, instead.
 */
static PQExpBuffer
conn_errorMessage(PGconn *conn)
{
	return &conn->errorMessage;
}

/*
 * Loads the libpq-oauth plugin via dlopen(), initializes it, and plugs its
 * callbacks into the connection's async auth handlers.
 *
 * Failure to load here results in a relatively quiet connection error, to
 * handle the use case where the build supports loading a flow but a user does
 * not want to install it. Troubleshooting of linker/loader failures can be done
 * via PGOAUTHDEBUG.
 */
bool
use_builtin_flow(PGconn *conn, fe_oauth_state *state)
{
	void		(*init) (pgthreadlock_t threadlock,
						 libpq_gettext_func gettext_impl,
						 conn_errorMessage_func errmsg_impl);
	PostgresPollingStatusType (*flow) (PGconn *conn);
	void		(*cleanup) (PGconn *conn);

	state->builtin_flow = dlopen("libpq-oauth-" PG_MAJORVERSION DLSUFFIX,
								 RTLD_NOW | RTLD_LOCAL);
	if (!state->builtin_flow)
	{
		/*
		 * For end users, this probably isn't an error condition, it just
		 * means the flow isn't installed. Developers and package maintainers
		 * may want to debug this via the PGOAUTHDEBUG envvar, though.
		 *
		 * Note that POSIX dlerror() isn't guaranteed to be threadsafe.
		 */
		if (oauth_unsafe_debugging_enabled())
			fprintf(stderr, "failed dlopen for libpq-oauth: %s\n", dlerror());

		return false;
	}

	if ((init = dlsym(state->builtin_flow, "libpq_oauth_init")) == NULL
		|| (flow = dlsym(state->builtin_flow, "pg_fe_run_oauth_flow")) == NULL
		|| (cleanup = dlsym(state->builtin_flow, "pg_fe_cleanup_oauth_flow")) == NULL)
	{
		/*
		 * This is more of an error condition than the one above, but due to
		 * the dlerror() threadsafety issue, lock it behind PGOAUTHDEBUG too.
		 */
		if (oauth_unsafe_debugging_enabled())
			fprintf(stderr, "failed dlsym for libpq-oauth: %s\n", dlerror());

		dlclose(state->builtin_flow);
		return false;
	}

	/*
	 * Inject necessary function pointers into the module.
	 */
	init(pg_g_threadlock,
#ifdef ENABLE_NLS
		 libpq_gettext,
#else
		 NULL,
#endif
		 conn_errorMessage);

	/* Set our asynchronous callbacks. */
	conn->async_auth = flow;
	conn->cleanup_async_auth = cleanup;

	return true;
}
