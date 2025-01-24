/*-------------------------------------------------------------------------
 *
 * fail_validator.c
 *	  Test module for serverside OAuth token validation callbacks, which always
 *	  fails
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/test/modules/oauth_validator/fail_validator.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "libpq/oauth.h"

PG_MODULE_MAGIC;

static ValidatorModuleResult *fail_token(ValidatorModuleState *state,
										 const char *token,
										 const char *role);

/* Callback implementations (we only need the main one) */
static const OAuthValidatorCallbacks validator_callbacks = {
	.validate_cb = fail_token,
};

const OAuthValidatorCallbacks *
_PG_oauth_validator_module_init(void)
{
	return &validator_callbacks;
}

static ValidatorModuleResult *
fail_token(ValidatorModuleState *state, const char *token, const char *role)
{
	elog(FATAL, "fail_validator: sentinel error");
	pg_unreachable();
}
