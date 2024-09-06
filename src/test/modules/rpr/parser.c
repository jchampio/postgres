/*-------------------------------------------------------------------------
 *
 * parser.c
 *		Main entry point/driver for SQL/RPR pattern grammar
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/test/modules/rpr/parser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "gramparse.h"
#include "mb/pg_wchar.h"
#include "parser/parser.h"
#include "parser/scansup.h"


/*
 * raw_parser
 *		Given a query in string form, do lexical and grammatical analysis.
 *
 * Returns a list of raw (un-analyzed) parse trees.  The contents of the
 * list have the form required by the specified RawParseMode.
 */
List *
raw_parser(const char *str, RawParseMode mode)
{
	core_yyscan_t yyscanner;
	base_yy_extra_type yyextra;
	int			yyresult;

	/* initialize the flex scanner */
	yyscanner = scanner_init(str, &yyextra.core_yy_extra,
							 &ScanKeywords, ScanKeywordTokens);

	/* base_yylex() only needs us to initialize the lookahead token, if any */
	if (mode == RAW_PARSE_DEFAULT)
		yyextra.have_lookahead = false;
	else
	{
		/* this array is indexed by RawParseMode enum */
		static const int mode_token[] = {
			[RAW_PARSE_DEFAULT] = 0,
			[RAW_PARSE_TYPE_NAME] = MODE_TYPE_NAME,
			[RAW_PARSE_PLPGSQL_EXPR] = MODE_PLPGSQL_EXPR,
			[RAW_PARSE_PLPGSQL_ASSIGN1] = MODE_PLPGSQL_ASSIGN1,
			[RAW_PARSE_PLPGSQL_ASSIGN2] = MODE_PLPGSQL_ASSIGN2,
			[RAW_PARSE_PLPGSQL_ASSIGN3] = MODE_PLPGSQL_ASSIGN3,
		};

		yyextra.have_lookahead = true;
		yyextra.lookahead_token = mode_token[mode];
		yyextra.lookahead_yylloc = 0;
		yyextra.lookahead_end = NULL;
	}

	/* initialize the bison parser */
	parser_init(&yyextra);

	/* Parse! */
	yyresult = base_yyparse(yyscanner);

	/* Clean up (release memory) */
	scanner_finish(yyscanner);

	if (yyresult)				/* error */
		return NIL;

	return yyextra.parsetree;
}


int
base_yylex(YYSTYPE *lvalp, YYLTYPE *llocp, core_yyscan_t yyscanner)
{
	return core_yylex(lvalp, llocp, yyscanner);
}
