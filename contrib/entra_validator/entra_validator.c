/*-------------------------------------------------------------------------
 *
 * entra_validator.c
 *	  Module for validating Entra ID tokens
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <dlfcn.h>

#include "fmgr.h"
#include "libpq/oauth.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/guc.h"
#include "utils/memutils.h"

PG_MODULE_MAGIC;

static bool validate_token(const ValidatorModuleState *state,
						   const char *token,
						   const char *role,
						   ValidatorModuleResult *res);

static bool check_exit(FILE **fh, const char *command);
static bool set_cloexec(int fd);
static char *find_entra_validator_script(void);

static const OAuthValidatorCallbacks validator_callbacks = {
	PG_OAUTH_VALIDATOR_MAGIC,
	.validate_cb = validate_token,
};

const OAuthValidatorCallbacks *
_PG_oauth_validator_module_init(void)
{
	return &validator_callbacks;
}

static bool
run_validator_command(Port *port, const char *token, char **authn_id)
{
	bool		success = false;
	int			rc;
	int			pipefd[2];
	int			rfd = -1;
	int			wfd = -1;

	const char *cmd;
	const char *issuer = MyProcPort->hba->oauth_issuer;
	FILE	   *fh = NULL;

	ssize_t		written;
	char	   *line = NULL;
	size_t		size = 0;
	ssize_t		len;

	/*
	 * Since popen() is unidirectional, open up a pipe for the other
	 * direction. Use CLOEXEC to ensure that our write end doesn't
	 * accidentally get copied into child processes, which would prevent us
	 * from closing it cleanly.
	 */
	rc = pipe(pipefd);
	if (rc < 0)
	{
		ereport(COMMERROR,
				(errcode_for_file_access(),
				 errmsg("could not create child pipe: %m")));
		return false;
	}

	rfd = pipefd[0];
	wfd = pipefd[1];

	cmd = find_entra_validator_script();
	cmd = psprintf("%s --token-fd %d --issuer '%s'", cmd, rfd, issuer);

	if (!set_cloexec(wfd))
	{
		/* error message was already logged */
		goto cleanup;
	}

	/* Execute the command. */
	fh = OpenPipeStream(cmd, "r");
	if (!fh)
	{
		ereport(COMMERROR,
				(errcode_for_file_access(),
				 errmsg("opening pipe to OAuth validator: %m")));
		goto cleanup;
	}

	/* We don't need the read end of the pipe anymore. */
	close(rfd);
	rfd = -1;

	/* Give the command the token to validate. */
	written = write(wfd, token, strlen(token));
	if (written != strlen(token))
	{
		/* TODO must loop for short writes, EINTR et al */
		ereport(COMMERROR,
				(errcode_for_file_access(),
				 errmsg("could not write token to child pipe: %m")));
		goto cleanup;
	}

	close(wfd);
	wfd = -1;

	/*
	 * Read the command's response.
	 *
	 * TODO: getline() is probably too new to use, unfortunately.
	 * TODO: loop over all lines
	 */
	if ((len = getline(&line, &size, fh)) >= 0)
	{
		/* TODO: fail if the authn_id doesn't end with a newline */
		if (len > 0)
			line[len - 1] = '\0';

		*authn_id = pstrdup(line);
	}
	else if (ferror(fh))
	{
		ereport(COMMERROR,
				(errcode_for_file_access(),
				 errmsg("could not read from command \"%s\": %m", cmd)));
		goto cleanup;
	}

	/* Make sure the command exits cleanly. */
	if (!check_exit(&fh, cmd))
	{
		/* error message already logged */
		goto cleanup;
	}

	/* Done. */
	success = true;

cleanup:
	if (line)
		free(line);

	/*
	 * In the successful case, the pipe fds are already closed. For the error
	 * case, always close out the pipe before waiting for the command, to
	 * prevent deadlock.
	 */
	if (rfd >= 0)
		close(rfd);
	if (wfd >= 0)
		close(wfd);

	if (fh)
	{
		Assert(!success);
		check_exit(&fh, cmd);
	}

	return success;
}

static bool
check_exit(FILE **fh, const char *command)
{
	int			rc;

	rc = ClosePipeStream(*fh);
	*fh = NULL;

	if (rc == -1)
	{
		/* pclose() itself failed. */
		ereport(COMMERROR,
				(errcode_for_file_access(),
				 errmsg("could not close pipe to command \"%s\": %m",
						command)));
	}
	else if (rc != 0)
	{
		char	   *reason = wait_result_to_str(rc);

		ereport(COMMERROR,
				(errmsg("failed to execute command \"%s\": %s",
						command, reason)));

		pfree(reason);
	}

	return (rc == 0);
}

static bool
set_cloexec(int fd)
{
	int			flags;
	int			rc;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
	{
		ereport(COMMERROR,
				(errcode_for_file_access(),
				 errmsg("could not get fd flags for child pipe: %m")));
		return false;
	}

	rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	if (rc < 0)
	{
		ereport(COMMERROR,
				(errcode_for_file_access(),
				 errmsg("could not set FD_CLOEXEC for child pipe: %m")));
		return false;
	}

	return true;
}

/*
 * Returns the path to the entra_validator script, which should be next to this
 * validator library, with the same basename, and a .py extension.
 *
 * XXX Only works on *nix.
 */
static char *
find_entra_validator_script(void)
{
	Dl_info		info = {0};
	char	   *script_path;
	char	   *dot;

	if (!dladdr(_PG_oauth_validator_module_init, &info))
		ereport(ERROR,
				errmsg("could not locate validator library on disk: %m"));

	script_path = pstrdup(info.dli_fname);

	/* Replace the shared object extension to form our script path. */
	dot = strrchr(script_path, '.');
	if (!dot || strlen(dot) < 3)
		ereport(ERROR,
				errmsg("unable to form script path from \"%s\"", script_path));

	dot[1] = 'p';
	dot[2] = 'y';
	dot[3] = '\0';

	return script_path;
}

static bool
validate_token(const ValidatorModuleState *state, const char *token,
			   const char *role, ValidatorModuleResult *res)
{
	if (run_validator_command(MyProcPort, token, &res->authn_id))
		res->authorized = true;

	return true;
}
