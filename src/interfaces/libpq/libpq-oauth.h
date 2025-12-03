/*-------------------------------------------------------------------------
 *
 * libpq-oauth.h
 *	  This file contains structs and functions used by custom OAuth plugins.
 *
 * Copyright (c) 2025, PostgreSQL Global Development Group
 *
 * src/interfaces/libpq/libpq-oauth.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef LIBPQ_OAUTH_H
#define LIBPQ_OAUTH_H

#include "libpq-fe.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* XXX can't rely on c.h, but duplicating this is asking for trouble */
#ifndef PGDLLEXPORT
#ifdef _WIN32
#define PGDLLEXPORT __declspec (dllexport)
#elif defined(__has_attribute)
#if __has_attribute(visibility)
#define PGDLLEXPORT __attribute__((visibility("default")))
#else
#define PGDLLEXPORT
#endif
#else
#define PGDLLEXPORT
#endif
#endif

/*
 * V1 API
 *
 * Flow plugins must provide an implementation of this callback.
 *
 * TODO: provide a magic struct that allows backwards but not forwards compat?
 */
extern PGDLLEXPORT int pg_start_oauthbearer(PGconn *conn,
											PGoauthBearerRequestV2 *request);

#ifdef __cplusplus
}
#endif

#endif							/* LIBPQ_OAUTH_H */
