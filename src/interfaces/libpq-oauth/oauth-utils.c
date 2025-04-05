/*-------------------------------------------------------------------------
 *
 * oauth-utils.c
 *
 *	  "Glue" helpers providing a copy of some internal APIs from libpq. At
 *	  some point in the future, we might be able to deduplicate.
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/interfaces/libpq-oauth/oauth-utils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include "libpq-int.h"
#include "oauth-utils.h"
#include "pg_config_paths.h"

pgthreadlock_t pg_g_threadlock;

/*
 * Append a formatted string to the error message buffer of the given
 * connection, after translating it.  A newline is automatically appended; the
 * format should not end with a newline.
 */
void
libpq_append_conn_error(PGconn *conn, const char *fmt,...)
{
	int			save_errno = errno;
	bool		done;
	va_list		args;

	Assert(fmt[strlen(fmt) - 1] != '\n');

	if (PQExpBufferBroken(&conn->errorMessage))
		return;					/* already failed */

	/* Loop in case we have to retry after enlarging the buffer. */
	do
	{
		errno = save_errno;
		va_start(args, fmt);
		done = appendPQExpBufferVA(&conn->errorMessage, libpq_gettext(fmt), args);
		va_end(args);
	} while (!done);

	appendPQExpBufferChar(&conn->errorMessage, '\n');
}

/*
 * Returns true if the PGOAUTHDEBUG=UNSAFE flag is set in the environment.
 */
bool
oauth_unsafe_debugging_enabled(void)
{
	const char *env = getenv("PGOAUTHDEBUG");

	return (env && strcmp(env, "UNSAFE") == 0);
}

int
pq_block_sigpipe(sigset_t *osigset, bool *sigpipe_pending)
{
	sigset_t	sigpipe_sigset;
	sigset_t	sigset;

	sigemptyset(&sigpipe_sigset);
	sigaddset(&sigpipe_sigset, SIGPIPE);

	/* Block SIGPIPE and save previous mask for later reset */
	SOCK_ERRNO_SET(pthread_sigmask(SIG_BLOCK, &sigpipe_sigset, osigset));
	if (SOCK_ERRNO)
		return -1;

	/* We can have a pending SIGPIPE only if it was blocked before */
	if (sigismember(osigset, SIGPIPE))
	{
		/* Is there a pending SIGPIPE? */
		if (sigpending(&sigset) != 0)
			return -1;

		if (sigismember(&sigset, SIGPIPE))
			*sigpipe_pending = true;
		else
			*sigpipe_pending = false;
	}
	else
		*sigpipe_pending = false;

	return 0;
}

void
pq_reset_sigpipe(sigset_t *osigset, bool sigpipe_pending, bool got_epipe)
{
	int			save_errno = SOCK_ERRNO;
	int			signo;
	sigset_t	sigset;

	/* Clear SIGPIPE only if none was pending */
	if (got_epipe && !sigpipe_pending)
	{
		if (sigpending(&sigset) == 0 &&
			sigismember(&sigset, SIGPIPE))
		{
			sigset_t	sigpipe_sigset;

			sigemptyset(&sigpipe_sigset);
			sigaddset(&sigpipe_sigset, SIGPIPE);

			sigwait(&sigpipe_sigset, &signo);
		}
	}

	/* Restore saved block mask */
	pthread_sigmask(SIG_SETMASK, osigset, NULL);

	SOCK_ERRNO_SET(save_errno);
}

#ifdef ENABLE_NLS

static void
libpq_binddomain(void)
{
	/*
	 * At least on Windows, there are gettext implementations that fail if
	 * multiple threads call bindtextdomain() concurrently.  Use a mutex and
	 * flag variable to ensure that we call it just once per process.  It is
	 * not known that similar bugs exist on non-Windows platforms, but we
	 * might as well do it the same way everywhere.
	 */
	static volatile bool already_bound = false;
	static pthread_mutex_t binddomain_mutex = PTHREAD_MUTEX_INITIALIZER;

	if (!already_bound)
	{
		/* bindtextdomain() does not preserve errno */
#ifdef WIN32
		int			save_errno = GetLastError();
#else
		int			save_errno = errno;
#endif

		(void) pthread_mutex_lock(&binddomain_mutex);

		if (!already_bound)
		{
			const char *ldir;

			/*
			 * No relocatable lookup here because the calling executable could
			 * be anywhere
			 */
			ldir = getenv("PGLOCALEDIR");
			if (!ldir)
				ldir = LOCALEDIR;
			bindtextdomain(PG_TEXTDOMAIN("libpq-oauth"), ldir);
			already_bound = true;
		}

		(void) pthread_mutex_unlock(&binddomain_mutex);

#ifdef WIN32
		SetLastError(save_errno);
#else
		errno = save_errno;
#endif
	}
}

char *
libpq_gettext(const char *msgid)
{
	libpq_binddomain();
	return dgettext(PG_TEXTDOMAIN("libpq-oauth"), msgid);
}

char *
libpq_ngettext(const char *msgid, const char *msgid_plural, unsigned long n)
{
	libpq_binddomain();
	return dngettext(PG_TEXTDOMAIN("libpq-oauth"), msgid, msgid_plural, n);
}

#endif							/* ENABLE_NLS */
