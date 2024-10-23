/*-------------------------------------------------------------------------
 *
 * oauth.h
 *	  Interface to libpq/auth-oauth.c
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/libpq/oauth.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_OAUTH_H
#define PG_OAUTH_H

#include "libpq/libpq-be.h"
#include "libpq/sasl.h"

extern PGDLLIMPORT char *oauth_validator_libraries_string;

typedef struct ValidatorModuleState
{
	void	   *private_data;
} ValidatorModuleState;

typedef struct ValidatorModuleResult
{
	bool		authorized;
	char	   *authn_id;
} ValidatorModuleResult;

typedef void (*ValidatorStartupCB) (ValidatorModuleState *state);
typedef void (*ValidatorShutdownCB) (ValidatorModuleState *state);
typedef ValidatorModuleResult *(*ValidatorValidateCB) (ValidatorModuleState *state, const char *token, const char *role);

typedef struct OAuthValidatorCallbacks
{
	ValidatorStartupCB startup_cb;
	ValidatorShutdownCB shutdown_cb;
	ValidatorValidateCB validate_cb;
} OAuthValidatorCallbacks;

typedef const OAuthValidatorCallbacks *(*OAuthValidatorModuleInit) (void);
extern PGDLLEXPORT const OAuthValidatorCallbacks *_PG_oauth_validator_module_init(void);

/* Implementation */
extern const pg_be_sasl_mech pg_be_oauth_mech;

/*
 * Ensure a validator named in the HBA is permitted by the configuration.
 */
extern bool check_oauth_validator(HbaLine *hba, int elevel, char **err_msg);

#endif							/* PG_OAUTH_H */
