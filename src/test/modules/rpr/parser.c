/*-------------------------------------------------------------------------
 *
 * parser.c
 *		Main entry point/driver for PostgreSQL grammar
 *
 * This should match src/backend/parser/parser.c, except that we do not
 * need to bother with re-entrant interfaces.
 *
 * Note: ECPG doesn't report error location like the backend does.
 * This file will need work if we ever want it to.
 *
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/interfaces/ecpg/preproc/parser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include "preproc_extern.h"
#include "gram.h"

/*
 * Intermediate filter between parser and base lexer (base_yylex in scan.l).
 *
 * This filter is needed because in some cases the standard SQL grammar
 * requires more than one token lookahead.  We reduce these cases to one-token
 * lookahead by replacing tokens here, in order to keep the grammar LALR(1).
 *
 * Using a filter is simpler than trying to recognize multiword tokens
 * directly in scan.l, because we'd have to allow for comments between the
 * words.  Furthermore it's not clear how to do that without re-introducing
 * scanner backtrack, which would cost more performance than this filter
 * layer does.
 *
 * We also use this filter to convert UIDENT and USCONST sequences into
 * plain IDENT and SCONST tokens.  While that could be handled by additional
 * productions in the main grammar, it's more efficient to do it like this.
 */
int
filtered_base_yylex(void)
{
	return base_yylex();
}
