/*-------------------------------------------------------------------------
 *
 * oauth_test_common.c
 *	  Shared functionality for oauth_hook_client and oauth_flow
 *
 * Copyright (c) 2025, PostgreSQL Global Development Group
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <sys/socket.h>

#include "getopt_long.h"
#include "libpq-fe.h"
#include "pqexpbuffer.h"

#include "oauth_test_common.h"

static PostgresPollingStatusType async_cb(PGconn *conn,
										  PGoauthBearerRequest *req,
										  pgsocket *altsock);
static PostgresPollingStatusType misbehave_cb(PGconn *conn,
											  PGoauthBearerRequest *req,
											  pgsocket *altsock);

/* --options */
static bool no_hook = false;
static bool hang_forever = false;
static const char *expected_uri = NULL;
static const char *expected_issuer = NULL;
static const char *expected_scope = NULL;
static const char *misbehave_mode = NULL;
static char *token = NULL;
static char *errmsg = NULL;
static int	hook_version = PQAUTHDATA_OAUTH_BEARER_TOKEN_V2;

/*
 * XXX: stress_async is exported for the benefit of oauth_hook_client. Since
 * we only use public headers (libpq-fe.h) for oauth_flow, it needs to be an int
 * rather than a bool.
 */
int			stress_async = false;

static void
usage(char *argv[])
{
	printf("usage: %s [flags] CONNINFO\n\n", argv[0]);

	printf("recognized flags:\n");
	printf("  -h, --help              show this message\n");
	printf("  -v VERSION              select the hook API version (default 2)\n");
	printf("  --expected-scope SCOPE  fail if received scopes do not match SCOPE\n");
	printf("  --expected-uri URI      fail if received configuration link does not match URI\n");
	printf("  --expected-issuer ISS   fail if received issuer does not match ISS (v2 only)\n");
	printf("  --misbehave=MODE        have the hook fail required postconditions\n"
		   "                          (MODEs: no-hook, fail-async, no-token, no-socket)\n");
	printf("  --no-hook               don't install OAuth hooks\n");
	printf("  --hang-forever          don't ever return a token (combine with connect_timeout)\n");
	printf("  --token TOKEN           use the provided TOKEN value\n");
	printf("  --error ERRMSG          fail instead, with the given ERRMSG (v2 only)\n");
	printf("  --stress-async          busy-loop on PQconnectPoll rather than polling\n");
}

char *
oauth_test_parse_argv(int argc, char *argv[], int for_plugin)
{
	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},

		{"expected-scope", required_argument, NULL, 1000},
		{"expected-uri", required_argument, NULL, 1001},
		{"no-hook", no_argument, NULL, 1002},
		{"token", required_argument, NULL, 1003},
		{"hang-forever", no_argument, NULL, 1004},
		{"misbehave", required_argument, NULL, 1005},
		{"stress-async", no_argument, NULL, 1006},
		{"expected-issuer", required_argument, NULL, 1007},
		{"error", required_argument, NULL, 1008},
		{0}
	};

	int			c;

	if (for_plugin)
	{
		/* The "real" argv has already been parsed. Reset optind. */
		optind = 1;
	}

	while ((c = getopt_long(argc, argv, "hv:", long_options, NULL)) != -1)
	{
		switch (c)
		{
			case 'h':
				usage(argv);
				exit(0);

			case 'v':
				if (strcmp(optarg, "1") == 0)
					hook_version = PQAUTHDATA_OAUTH_BEARER_TOKEN;
				else if (strcmp(optarg, "2") == 0)
					hook_version = PQAUTHDATA_OAUTH_BEARER_TOKEN_V2;
				else
				{
					usage(argv);
					exit(1);
				}
				break;

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

			case 1004:			/* --hang-forever */
				hang_forever = true;
				break;

			case 1005:			/* --misbehave */
				misbehave_mode = optarg;
				break;

			case 1006:			/* --stress-async */
				stress_async = true;
				break;

			case 1007:			/* --expected-issuer */
				expected_issuer = optarg;
				break;

			case 1008:			/* --error */
				errmsg = optarg;
				break;

			default:
				usage(argv);
				exit(1);
		}
	}

	if (argc != (for_plugin ? optind : optind + 1))
	{
		usage(argv);
		exit(1);
	}

	return argv[optind];
}

/*
 * PQauthDataHook implementation. Replaces the default client flow by handling
 * PQAUTHDATA_OAUTH_BEARER_TOKEN[_V2].
 */
int
oauth_test_authdata_hook(PGauthData type, PGconn *conn, void *data)
{
	PGoauthBearerRequest *req;
	PGoauthBearerRequestV2 *req2 = NULL;

	Assert(hook_version == PQAUTHDATA_OAUTH_BEARER_TOKEN ||
		   hook_version == PQAUTHDATA_OAUTH_BEARER_TOKEN_V2);

	if (no_hook || type != hook_version)
		return 0;

	req = data;
	if (type == PQAUTHDATA_OAUTH_BEARER_TOKEN_V2)
		req2 = data;

	if (hang_forever)
	{
		/* Start asynchronous processing. */
		req->async = async_cb;
		return 1;
	}

	if (misbehave_mode)
	{
		if (strcmp(misbehave_mode, "no-hook") != 0)
			req->async = misbehave_cb;
		return 1;
	}

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

	if (expected_issuer)
	{
		if (!req2)
		{
			fprintf(stderr, "--expected-issuer cannot be combined with -v1\n");
			return -1;
		}

		if (!req2->issuer)
		{
			fprintf(stderr, "expected issuer \"%s\", got NULL\n", expected_issuer);
			return -1;
		}

		if (strcmp(expected_issuer, req2->issuer) != 0)
		{
			fprintf(stderr, "expected issuer \"%s\", got \"%s\"\n", expected_issuer, req2->issuer);
			return -1;
		}
	}

	if (errmsg)
	{
		if (token)
		{
			fprintf(stderr, "--error cannot be combined with --token\n");
			return -1;
		}
		else if (!req2)
		{
			fprintf(stderr, "--error cannot be combined with -v1\n");
			return -1;
		}

		req2->error = errmsg;
		return -1;
	}

	req->token = token;
	return 1;
}

/*
 * Sets up a request for a plugin module (pg_start_oauthbearer()) rather than
 * using the hook.
 */
int
oauth_test_start_flow(PGconn *conn, PGoauthBearerRequestV2 *request)
{
	int			ret;

	/*
	 * We can still defer to the hook above to avoid copying code; we just
	 * have to translate the return value.
	 */
	ret = oauth_test_authdata_hook(PQAUTHDATA_OAUTH_BEARER_TOKEN_V2, conn,
								   request);

	if (ret == 0)
	{
		/* This is a bug in the test. */
		fprintf(stderr, "plugin tests cannot make use of -v1 or --no-hook\n");
		exit(1);
	}

	return (ret == 1) ? 0 : -1;
}

static PostgresPollingStatusType
async_cb(PGconn *conn, PGoauthBearerRequest *req, pgsocket *altsock)
{
	if (hang_forever)
	{
		/*
		 * This code tests that nothing is interfering with libpq's handling
		 * of connect_timeout.
		 */
		static pgsocket sock = PGINVALID_SOCKET;

		if (sock == PGINVALID_SOCKET)
		{
			/* First call. Create an unbound socket to wait on. */
#ifdef WIN32
			WSADATA		wsaData;
			int			err;

			err = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (err)
			{
				perror("WSAStartup failed");
				return PGRES_POLLING_FAILED;
			}
#endif
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock == PGINVALID_SOCKET)
			{
				perror("failed to create datagram socket");
				return PGRES_POLLING_FAILED;
			}
		}

		/* Make libpq wait on the (unreadable) socket. */
		*altsock = sock;
		return PGRES_POLLING_READING;
	}

	req->token = token;
	return PGRES_POLLING_OK;
}

static PostgresPollingStatusType
misbehave_cb(PGconn *conn, PGoauthBearerRequest *req, pgsocket *altsock)
{
	if (strcmp(misbehave_mode, "fail-async") == 0)
	{
		/* Just fail "normally". */
		if (errmsg)
		{
			PGoauthBearerRequestV2 *req2;

			if (hook_version == PQAUTHDATA_OAUTH_BEARER_TOKEN)
			{
				fprintf(stderr, "--error cannot be combined with -v1\n");
				exit(1);
			}

			req2 = (PGoauthBearerRequestV2 *) req;
			req2->error = errmsg;
		}

		return PGRES_POLLING_FAILED;
	}
	else if (strcmp(misbehave_mode, "no-token") == 0)
	{
		/* Callbacks must assign req->token before returning OK. */
		return PGRES_POLLING_OK;
	}
	else if (strcmp(misbehave_mode, "no-socket") == 0)
	{
		/* Callbacks must assign *altsock before asking for polling. */
		return PGRES_POLLING_READING;
	}
	else
	{
		fprintf(stderr, "unrecognized --misbehave mode: %s\n", misbehave_mode);
		exit(1);
	}
}
