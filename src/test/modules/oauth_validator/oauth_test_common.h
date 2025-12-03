/*-------------------------------------------------------------------------
 *
 * oauth_test_common.h
 *	  Shared functionality for oauth_hook_client and oauth_flow
 *
 * Copyright (c) 2025, PostgreSQL Global Development Group
 *
 *-------------------------------------------------------------------------
 */

#ifndef OAUTH_TEST_COMMON_H
#define OAUTH_TEST_COMMON_H

/*
 * Only public headers can be here, since oauth_flow.c is trying to test only
 * the public API.
 */
#include "libpq-fe.h"

extern int	stress_async;		/* for oauth_hook_client */

extern char *oauth_test_parse_argv(int argc, char *argv[], int for_plugin);
extern int	oauth_test_authdata_hook(PGauthData type, PGconn *conn, void *data);
extern int	oauth_test_start_flow(PGconn *conn, PGoauthBearerRequestV2 *request);

#endif							/* OAUTH_TEST_COMMON_H */
