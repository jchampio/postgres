/* header */
/* src/interfaces/ecpg/preproc/ecpg.header */

/* Copyright comment */
%{
#include "postgres_fe.h"

#include "preproc_extern.h"
#include "preproc.h"
#include <unistd.h>

/* silence -Wmissing-variable-declarations */
extern int base_yychar;
extern int base_yynerrs;

/* Location tracking support --- simpler than bison's default */
#define YYLLOC_DEFAULT(Current, Rhs, N) \
	do { \
		if (N)						\
			(Current) = (Rhs)[1];	\
		else						\
			(Current) = (Rhs)[0];	\
	} while (0)

/*
 * The %name-prefix option below will make bison call base_yylex, but we
 * really want it to call filtered_base_yylex (see parser.c).
 */
#define base_yylex filtered_base_yylex

/*
 * This is only here so the string gets into the POT.  Bison uses it
 * internally.
 */
#define bison_gettext_dummy gettext_noop("syntax error")

/*
 * Variables containing simple states.
 */
int			struct_level = 0;
int			braces_open;		/* brace level counter */
char	   *current_function;
int			ecpg_internal_var = 0;
char	   *connection = NULL;
char	   *input_filename = NULL;

/* temporarily store struct members while creating the data structure */
struct ECPGstruct_member *struct_member_list[STRUCT_DEPTH] = {NULL};

static Node * makeIntConst(int val, int location);

static void vmmerror(int error_code, enum errortype type, const char *error, va_list ap) pg_attribute_printf(3, 0);

/*
 * Handle parsing errors and warnings
 */
static void
vmmerror(int error_code, enum errortype type, const char *error, va_list ap)
{
	/* localize the error message string */
	error = _(error);

	fprintf(stderr, "%s:%d: ", input_filename, base_yylineno);

	switch (type)
	{
		case ET_WARNING:
			fprintf(stderr, _("WARNING: "));
			break;
		case ET_ERROR:
			fprintf(stderr, _("ERROR: "));
			break;
	}

	vfprintf(stderr, error, ap);

	fprintf(stderr, "\n");

	switch (type)
	{
		case ET_WARNING:
			break;
		case ET_ERROR:
			ret_value = error_code;
			break;
	}
}

void
mmerror(int error_code, enum errortype type, const char *error,...)
{
	va_list		ap;

	va_start(ap, error);
	vmmerror(error_code, type, error, ap);
	va_end(ap);
}

void
mmfatal(int error_code, const char *error,...)
{
	va_list		ap;

	va_start(ap, error);
	vmmerror(error_code, ET_ERROR, error, ap);
	va_end(ap);

	if (base_yyin)
		fclose(base_yyin);
	if (base_yyout)
		fclose(base_yyout);

	exit(error_code);
}

/*
 * string concatenation
 */

static char *
make3_str(char *str1, char *str2, char *str3)
{
	char	   *res_str = (char *) mm_alloc(strlen(str1) + strlen(str2) + strlen(str3) + 1);

	strcpy(res_str, str1);
	strcat(res_str, str2);
	strcat(res_str, str3);
	free(str1);
	free(str2);
	free(str3);
	return res_str;
}

%}

%expect 0
%name-prefix="base_yy"
%locations

/* tokens */
/* src/interfaces/ecpg/preproc/ecpg.tokens */

/* special embedded SQL tokens */
%token  SQL_ALLOCATE SQL_AUTOCOMMIT SQL_BOOL SQL_BREAK
                SQL_CARDINALITY SQL_CONNECT
                SQL_COUNT
                SQL_DATETIME_INTERVAL_CODE
                SQL_DATETIME_INTERVAL_PRECISION SQL_DESCRIBE
                SQL_DESCRIPTOR SQL_DISCONNECT SQL_FOUND
                SQL_FREE SQL_GET SQL_GO SQL_GOTO SQL_IDENTIFIED
                SQL_INDICATOR SQL_KEY_MEMBER SQL_LENGTH
                SQL_LONG SQL_NULLABLE SQL_OCTET_LENGTH
                SQL_OPEN SQL_OUTPUT SQL_REFERENCE
                SQL_RETURNED_LENGTH SQL_RETURNED_OCTET_LENGTH SQL_SCALE
                SQL_SECTION SQL_SHORT SQL_SIGNED SQL_SQLERROR
                SQL_SQLPRINT SQL_SQLWARNING SQL_START SQL_STOP
                SQL_STRUCT SQL_UNSIGNED SQL_VAR SQL_WHENEVER

/* C tokens */
%token  S_ADD S_AND S_ANYTHING S_AUTO S_CONST S_DEC S_DIV
                S_DOTPOINT S_EQUAL S_EXTERN S_INC S_LSHIFT S_MEMPOINT
                S_MEMBER S_MOD S_MUL S_NEQUAL S_OR S_REGISTER S_RSHIFT
                S_STATIC S_SUB S_VOLATILE
                S_TYPEDEF

%token CSTRING CVARIABLE CPP_LINE IP

/* types */

%union {
	double		dval;
	char	   *str;
	int			ival;
	bool		boolean;
	Node	   *node;
	List	   *list;
}

%type <ival> Iconst
%type <ival> ICONST

%type <str>	ColId
%type <str> CSTRING
%type <str> IDENT

%type <str> ecpg_ident
%type <str> all_unreserved_keyword
%type <str> unreserved_keyword
%type <str> col_name_keyword
%type <str> ECPGCKeywords
%type <str> ECPGKeywords
%type <str> ECPGKeywords_rest
%type <str> ECPGKeywords_vanames
%type <str> ECPGunreserved_interval

%type <node>	row_pattern opt_row_pattern row_pattern_term
				row_pattern_alternation row_pattern_factor row_pattern_primary
				opt_quantifier opt_iconst
%type <boolean>	opt_reluctant

/* orig_tokens */
 %token IDENT UIDENT FCONST SCONST USCONST BCONST XCONST Op
 %token ICONST PARAM
 %token TYPECAST DOT_DOT COLON_EQUALS EQUALS_GREATER
 %token LEFT_BRACE_MINUS RIGHT_MINUS_BRACE
 %token LESS_EQUALS GREATER_EQUALS NOT_EQUALS









 %token ABORT_P ABSENT ABSOLUTE_P ACCESS ACTION ADD_P ADMIN AFTER
 AGGREGATE ALL ALSO ALTER ALWAYS ANALYSE ANALYZE AND ANY ARRAY AS ASC
 ASENSITIVE ASSERTION ASSIGNMENT ASYMMETRIC ATOMIC AT ATTACH ATTRIBUTE AUTHORIZATION

 BACKWARD BEFORE BEGIN_P BETWEEN BIGINT BINARY BIT
 BOOLEAN_P BOTH BREADTH BY

 CACHE CALL CALLED CASCADE CASCADED CASE CAST CATALOG_P CHAIN CHAR_P
 CHARACTER CHARACTERISTICS CHECK CHECKPOINT CLASS CLOSE
 CLUSTER COALESCE COLLATE COLLATION COLUMN COLUMNS COMMENT COMMENTS COMMIT
 COMMITTED COMPRESSION CONCURRENTLY CONDITIONAL CONFIGURATION CONFLICT
 CONNECTION CONSTRAINT CONSTRAINTS CONTENT_P CONTINUE_P CONVERSION_P COPY
 COST CREATE CROSS CSV CUBE CURRENT_P
 CURRENT_CATALOG CURRENT_DATE CURRENT_ROLE CURRENT_SCHEMA
 CURRENT_TIME CURRENT_TIMESTAMP CURRENT_USER CURSOR CYCLE

 DATA_P DATABASE DAY_P DEALLOCATE DEC DECIMAL_P DECLARE DEFAULT DEFAULTS
 DEFERRABLE DEFERRED DEFINE DEFINER DELETE_P DELIMITER DELIMITERS DEPENDS DEPTH DESC
 DETACH DICTIONARY DISABLE_P DISCARD DISTINCT DO DOCUMENT_P DOMAIN_P
 DOUBLE_P DROP

 EACH ELSE EMPTY_P ENABLE_P ENCODING ENCRYPTED END_P ENUM_P ERROR_P ESCAPE
 EVENT EXCEPT EXCLUDE EXCLUDING EXCLUSIVE EXECUTE EXISTS EXPLAIN EXPRESSION
 EXTENSION EXTERNAL EXTRACT

 FALSE_P FAMILY FETCH FILTER FINALIZE FIRST_P FLOAT_P FOLLOWING FOR
 FORCE FOREIGN FORMAT FORWARD FREEZE FROM FULL FUNCTION FUNCTIONS

 GENERATED GLOBAL GRANT GRANTED GREATEST GROUP_P GROUPING GROUPS

 HANDLER HAVING HEADER_P HOLD HOUR_P

 IDENTITY_P IF_P ILIKE IMMEDIATE IMMUTABLE IMPLICIT_P IMPORT_P IN_P INCLUDE
 INCLUDING INCREMENT INDENT INDEX INDEXES INHERIT INHERITS INITIAL INITIALLY INLINE_P
 INNER_P INOUT INPUT_P INSENSITIVE INSERT INSTEAD INT_P INTEGER
 INTERSECT INTERVAL INTO INVOKER IS ISNULL ISOLATION

 JOIN JSON JSON_ARRAY JSON_ARRAYAGG JSON_EXISTS JSON_OBJECT JSON_OBJECTAGG
 JSON_QUERY JSON_SCALAR JSON_SERIALIZE JSON_TABLE JSON_VALUE

 KEEP KEY KEYS

 LABEL LANGUAGE LARGE_P LAST_P LATERAL_P
 LEADING LEAKPROOF LEAST LEFT LEVEL LIKE LIMIT LISTEN LOAD LOCAL
 LOCALTIME LOCALTIMESTAMP LOCATION LOCK_P LOCKED LOGGED

 MAPPING MATCH MATCHED MATERIALIZED MAXVALUE MEASURES MERGE MERGE_ACTION METHOD
 MINUTE_P MINVALUE MODE MONTH_P MOVE

 NAME_P NAMES NATIONAL NATURAL NCHAR NESTED NEW NEXT NFC NFD NFKC NFKD NO
 NONE NORMALIZE NORMALIZED
 NOT NOTHING NOTIFY NOTNULL NOWAIT NULL_P NULLIF
 NULLS_P NUMERIC

 OBJECT_P OF OFF OFFSET OIDS OLD OMIT ON ONLY OPERATOR OPTION OPTIONS OR
 ORDER ORDINALITY OTHERS OUT_P OUTER_P
 OVER OVERLAPS OVERLAY OVERRIDING OWNED OWNER

 PARALLEL PARAMETER PARSER PARTIAL PARTITION PASSING PASSWORD PAST
 PATH PATTERN_P PERMUTE PLACING PLAN PLANS POLICY

 POSITION PRECEDING PRECISION PRESERVE PREPARE PREPARED PRIMARY
 PRIOR PRIVILEGES PROCEDURAL PROCEDURE PROCEDURES PROGRAM PUBLICATION

 QUOTE QUOTES

 RANGE READ REAL REASSIGN RECURSIVE REF_P REFERENCES REFERENCING
 REFRESH REINDEX RELATIVE_P RELEASE RENAME REPEATABLE REPLACE REPLICA
 RESET RESTART RESTRICT RETURN RETURNING RETURNS REVOKE RIGHT ROLE ROLLBACK ROLLUP
 ROUTINE ROUTINES ROW ROWS RULE

 SAVEPOINT SCALAR SCHEMA SCHEMAS SCROLL SEARCH SECOND_P SECURITY SEEK SELECT
 SEQUENCE SEQUENCES

 SERIALIZABLE SERVER SESSION SESSION_USER SET SETS SETOF SHARE SHOW
 SIMILAR SIMPLE SKIP SMALLINT SNAPSHOT SOME SOURCE SQL_P STABLE STANDALONE_P
 START STATEMENT STATISTICS STDIN STDOUT STORAGE STORED STRICT_P STRING_P STRIP_P
 SUBSCRIPTION SUBSET SUBSTRING SUPPORT SYMMETRIC SYSID SYSTEM_P SYSTEM_USER

 TABLE TABLES TABLESAMPLE TABLESPACE TARGET TEMP TEMPLATE TEMPORARY TEXT_P THEN
 TIES TIME TIMESTAMP TO TRAILING TRANSACTION TRANSFORM
 TREAT TRIGGER TRIM TRUE_P
 TRUNCATE TRUSTED TYPE_P TYPES_P

 UESCAPE UNBOUNDED UNCONDITIONAL UNCOMMITTED UNENCRYPTED UNION UNIQUE UNKNOWN
 UNLISTEN UNLOGGED UNTIL UPDATE USER USING

 VACUUM VALID VALIDATE VALIDATOR VALUE_P VALUES VARCHAR VARIADIC VARYING
 VERBOSE VERSION_P VIEW VIEWS VOLATILE

 WHEN WHERE WHITESPACE_P WINDOW WITH WITHIN WITHOUT WORK WRAPPER WRITE

 XML_P XMLATTRIBUTES XMLCONCAT XMLELEMENT XMLEXISTS XMLFOREST XMLNAMESPACES
 XMLPARSE XMLPI XMLROOT XMLSERIALIZE XMLTABLE

 YEAR_P YES_P

 ZONE












 %token FORMAT_LA NOT_LA NULLS_LA WITH_LA WITHOUT_LA








 %token MODE_TYPE_NAME
 %token MODE_PLPGSQL_EXPR
 %token MODE_PLPGSQL_ASSIGN1
 %token MODE_PLPGSQL_ASSIGN2
 %token MODE_PLPGSQL_ASSIGN3



 %left UNION EXCEPT
 %left INTERSECT
 %left OR
 %left AND
 %right NOT
 %nonassoc IS ISNULL NOTNULL
 %nonassoc '<' '>' '=' LESS_EQUALS GREATER_EQUALS NOT_EQUALS
 %nonassoc BETWEEN IN_P LIKE ILIKE SIMILAR NOT_LA
 %nonassoc ESCAPE















































 %nonassoc UNBOUNDED NESTED
 %nonassoc IDENT
%nonassoc CSTRING PARTITION RANGE ROWS GROUPS PRECEDING FOLLOWING CUBE ROLLUP
 SET KEYS OBJECT_P SCALAR VALUE_P WITH WITHOUT PATH
 MEASURES AFTER INITIAL SEEK PATTERN_P
 %left Op OPERATOR
 %left '+' '-'
 %left '*' '/' '%'
 %left '^'

 %left AT
 %left COLLATE
 %right UMINUS
 %left '[' ']'
 %left '(' ')'
 %left TYPECAST
 %left '.'







 %left JOIN CROSS LEFT FULL RIGHT INNER_P NATURAL

%%
prog: row_pattern								{ parsed_pattern = $1; };

row_pattern:
			row_pattern_term						{ $$ = $1; }
			| row_pattern_alternation				{ $$ = $1; }
		;

row_pattern_term:
			row_pattern_factor						{ $$ = $1; }
			| row_pattern_term row_pattern_factor
				{
					List *l = list_make1($1);
					$$ = (Node *) lappend(l, $2);
				}
		;

row_pattern_alternation:
			row_pattern '|' row_pattern_term
				{
					RowPatternAlternation *a = makeNode(RowPatternAlternation);
					a->left = $1;
					a->right = $3;

					$$ = (Node *) a;
				}
		;

row_pattern_factor:
			row_pattern_primary opt_quantifier
				{
					if ($2)
					{
						RowPatternFactor *f = makeNode(RowPatternFactor);
						f->primary = $1;
						f->quantifier = (RowPatternQuantifier *) $2;

						$$ = (Node *) f;
					}
					else
						$$ = $1;
				}
		;

opt_quantifier:
			'*' opt_reluctant
				{
					RowPatternQuantifier *q = makeNode(RowPatternQuantifier);
					q->min = NULL;
					q->max = NULL;
					q->reluctant = $2;

					$$ = (Node *) q;
				}
			| '+' opt_reluctant
				{
					RowPatternQuantifier *q = makeNode(RowPatternQuantifier);
					q->min = (A_Const *) makeIntConst(1, -1);
					q->max = NULL;
					q->reluctant = $2;

					$$ = (Node *) q;
				}
			| '?' opt_reluctant
				{
					RowPatternQuantifier *q = makeNode(RowPatternQuantifier);
					q->min = NULL;
					q->max = (A_Const *) makeIntConst(1, -1);
					q->reluctant = $2;

					$$ = (Node *) q;
				}
			| '{' opt_iconst ',' opt_iconst '}' opt_reluctant
				{
					RowPatternQuantifier *q = makeNode(RowPatternQuantifier);
					q->min = (A_Const *) $2;
					q->max = (A_Const *) $4;
					q->reluctant = $6;

					$$ = (Node *) q;
				}
			| '{' Iconst '}'
				{
					RowPatternQuantifier *q = makeNode(RowPatternQuantifier);
					q->min = (A_Const *) makeIntConst($2, -1);
					q->max = (A_Const *) makeIntConst($2, -1);
					/*
					 * The grammar doesn't allow exact quantifiers to be
					 * reluctant. (It wouldn't do anything useful.)
					 */
					q->reluctant = false;

					$$ = (Node *) q;
				}
			| /* EMPTY */							{ $$ = NULL; }
		;

opt_reluctant:
			'?'										{ $$ = true; }
			| /* EMPTY */							{ $$ = false; }
		;

opt_iconst:
			Iconst									{ $$ = makeIntConst($1, -1); }
			| /* EMPTY */							{ $$ = NULL; }
		;

row_pattern_primary:
			ColId									{ $$ = (Node *) makeString($1); }
			| '$'									{ $$ = (Node *) makeString("$"); }
			| '^'									{ $$ = (Node *) makeString("^"); }
			| '(' opt_row_pattern ')'				{ $$ = (Node *) list_make1($2); }
			| LEFT_BRACE_MINUS row_pattern RIGHT_MINUS_BRACE
				{
					RowPatternExclusion *e = makeNode(RowPatternExclusion);
					e->pattern = $2;
					$$ = (Node *) e;
				}
		;

opt_row_pattern:
			row_pattern								{ $$ = $1; }
			| /* EMPTY */							{ $$ = NULL; }
		;


 Iconst:
 ICONST
	{
		$$ = $1;
	}
;


 col_name_keyword:
 BETWEEN
 { 
 $$ = mm_strdup("between");
}
|  BIGINT
 { 
 $$ = mm_strdup("bigint");
}
|  BIT
 { 
 $$ = mm_strdup("bit");
}
|  BOOLEAN_P
 { 
 $$ = mm_strdup("boolean");
}
|  CHARACTER
 { 
 $$ = mm_strdup("character");
}
|  COALESCE
 { 
 $$ = mm_strdup("coalesce");
}
|  DEC
 { 
 $$ = mm_strdup("dec");
}
|  DECIMAL_P
 { 
 $$ = mm_strdup("decimal");
}
|  EXISTS
 { 
 $$ = mm_strdup("exists");
}
|  EXTRACT
 { 
 $$ = mm_strdup("extract");
}
|  FLOAT_P
 { 
 $$ = mm_strdup("float");
}
|  GREATEST
 { 
 $$ = mm_strdup("greatest");
}
|  GROUPING
 { 
 $$ = mm_strdup("grouping");
}
|  INOUT
 { 
 $$ = mm_strdup("inout");
}
|  INTEGER
 { 
 $$ = mm_strdup("integer");
}
|  INTERVAL
 { 
 $$ = mm_strdup("interval");
}
|  JSON
 { 
 $$ = mm_strdup("json");
}
|  JSON_ARRAY
 { 
 $$ = mm_strdup("json_array");
}
|  JSON_ARRAYAGG
 { 
 $$ = mm_strdup("json_arrayagg");
}
|  JSON_EXISTS
 { 
 $$ = mm_strdup("json_exists");
}
|  JSON_OBJECT
 { 
 $$ = mm_strdup("json_object");
}
|  JSON_OBJECTAGG
 { 
 $$ = mm_strdup("json_objectagg");
}
|  JSON_QUERY
 { 
 $$ = mm_strdup("json_query");
}
|  JSON_SCALAR
 { 
 $$ = mm_strdup("json_scalar");
}
|  JSON_SERIALIZE
 { 
 $$ = mm_strdup("json_serialize");
}
|  JSON_TABLE
 { 
 $$ = mm_strdup("json_table");
}
|  JSON_VALUE
 { 
 $$ = mm_strdup("json_value");
}
|  LEAST
 { 
 $$ = mm_strdup("least");
}
|  MERGE_ACTION
 { 
 $$ = mm_strdup("merge_action");
}
|  NATIONAL
 { 
 $$ = mm_strdup("national");
}
|  NCHAR
 { 
 $$ = mm_strdup("nchar");
}
|  NONE
 { 
 $$ = mm_strdup("none");
}
|  NORMALIZE
 { 
 $$ = mm_strdup("normalize");
}
|  NULLIF
 { 
 $$ = mm_strdup("nullif");
}
|  NUMERIC
 { 
 $$ = mm_strdup("numeric");
}
|  OUT_P
 { 
 $$ = mm_strdup("out");
}
|  OVERLAY
 { 
 $$ = mm_strdup("overlay");
}
|  POSITION
 { 
 $$ = mm_strdup("position");
}
|  PRECISION
 { 
 $$ = mm_strdup("precision");
}
|  REAL
 { 
 $$ = mm_strdup("real");
}
|  ROW
 { 
 $$ = mm_strdup("row");
}
|  SETOF
 { 
 $$ = mm_strdup("setof");
}
|  SMALLINT
 { 
 $$ = mm_strdup("smallint");
}
|  SUBSTRING
 { 
 $$ = mm_strdup("substring");
}
|  TIME
 { 
 $$ = mm_strdup("time");
}
|  TIMESTAMP
 { 
 $$ = mm_strdup("timestamp");
}
|  TREAT
 { 
 $$ = mm_strdup("treat");
}
|  TRIM
 { 
 $$ = mm_strdup("trim");
}
|  VARCHAR
 { 
 $$ = mm_strdup("varchar");
}
|  XMLATTRIBUTES
 { 
 $$ = mm_strdup("xmlattributes");
}
|  XMLCONCAT
 { 
 $$ = mm_strdup("xmlconcat");
}
|  XMLELEMENT
 { 
 $$ = mm_strdup("xmlelement");
}
|  XMLEXISTS
 { 
 $$ = mm_strdup("xmlexists");
}
|  XMLFOREST
 { 
 $$ = mm_strdup("xmlforest");
}
|  XMLNAMESPACES
 { 
 $$ = mm_strdup("xmlnamespaces");
}
|  XMLPARSE
 { 
 $$ = mm_strdup("xmlparse");
}
|  XMLPI
 { 
 $$ = mm_strdup("xmlpi");
}
|  XMLROOT
 { 
 $$ = mm_strdup("xmlroot");
}
|  XMLSERIALIZE
 { 
 $$ = mm_strdup("xmlserialize");
}
|  XMLTABLE
 { 
 $$ = mm_strdup("xmltable");
}
;


/* trailer */
/* src/interfaces/ecpg/preproc/ecpg.trailer */


/*
 * Name classification hierarchy.
 *
 * These productions should match those in the core grammar, except that
 * we use all_unreserved_keyword instead of unreserved_keyword, and
 * where possible include ECPG keywords as well as core keywords.
 */

/* Column identifier --- names that can be column, table, etc names.
 */
ColId: ecpg_ident						{ $$ = $1; }
	| all_unreserved_keyword			{ $$ = $1; }
	| col_name_keyword					{ $$ = $1; }
	| ECPGKeywords						{ $$ = $1; }
	| ECPGCKeywords						{ $$ = $1; }
	| CHAR_P							{ $$ = mm_strdup("char"); }
	| VALUES							{ $$ = mm_strdup("values"); }
	;

ECPGCKeywords: S_AUTO					{ $$ = mm_strdup("auto"); }
	| S_CONST							{ $$ = mm_strdup("const"); }
	| S_EXTERN							{ $$ = mm_strdup("extern"); }
	| S_REGISTER						{ $$ = mm_strdup("register"); }
	| S_STATIC							{ $$ = mm_strdup("static"); }
	| S_TYPEDEF							{ $$ = mm_strdup("typedef"); }
	| S_VOLATILE						{ $$ = mm_strdup("volatile"); }
	;

ECPGKeywords: ECPGKeywords_vanames		{ $$ = $1; }
	| ECPGKeywords_rest					{ $$ = $1; }
	;

ecpg_ident: IDENT
	{
		$$ = $1;
	}
	| CSTRING
	{
		$$ = make3_str(mm_strdup("\""), $1, mm_strdup("\""));
	}
	;


/* "Unreserved" keywords --- available for use as any kind of name.
 */

/*
 * The following symbols must be excluded from ECPGColLabel and directly
 * included into ColLabel to enable C variables to get names from ECPGColLabel:
 * DAY_P, HOUR_P, MINUTE_P, MONTH_P, SECOND_P, YEAR_P.
 *
 * We also have to exclude CONNECTION, CURRENT, and INPUT for various reasons.
 * CONNECTION can be added back in all_unreserved_keyword, but CURRENT and
 * INPUT are reserved for ecpg purposes.
 *
 * The mentioned exclusions are done by $replace_line settings in parse.pl.
 */
ECPGunreserved_interval: DAY_P			{ $$ = mm_strdup("day"); }
	| HOUR_P							{ $$ = mm_strdup("hour"); }
	| MINUTE_P							{ $$ = mm_strdup("minute"); }
	| MONTH_P							{ $$ = mm_strdup("month"); }
	| SECOND_P							{ $$ = mm_strdup("second"); }
	| YEAR_P							{ $$ = mm_strdup("year"); }
	;

ECPGKeywords_vanames: SQL_BREAK			{ $$ = mm_strdup("break"); }
	| SQL_CARDINALITY					{ $$ = mm_strdup("cardinality"); }
	| SQL_COUNT							{ $$ = mm_strdup("count"); }
	| SQL_DATETIME_INTERVAL_CODE		{ $$ = mm_strdup("datetime_interval_code"); }
	| SQL_DATETIME_INTERVAL_PRECISION	{ $$ = mm_strdup("datetime_interval_precision"); }
	| SQL_FOUND							{ $$ = mm_strdup("found"); }
	| SQL_GO							{ $$ = mm_strdup("go"); }
	| SQL_GOTO							{ $$ = mm_strdup("goto"); }
	| SQL_IDENTIFIED					{ $$ = mm_strdup("identified"); }
	| SQL_INDICATOR						{ $$ = mm_strdup("indicator"); }
	| SQL_KEY_MEMBER					{ $$ = mm_strdup("key_member"); }
	| SQL_LENGTH						{ $$ = mm_strdup("length"); }
	| SQL_NULLABLE						{ $$ = mm_strdup("nullable"); }
	| SQL_OCTET_LENGTH					{ $$ = mm_strdup("octet_length"); }
	| SQL_RETURNED_LENGTH				{ $$ = mm_strdup("returned_length"); }
	| SQL_RETURNED_OCTET_LENGTH			{ $$ = mm_strdup("returned_octet_length"); }
	| SQL_SCALE							{ $$ = mm_strdup("scale"); }
	| SQL_SECTION						{ $$ = mm_strdup("section"); }
	| SQL_SQLERROR						{ $$ = mm_strdup("sqlerror"); }
	| SQL_SQLPRINT						{ $$ = mm_strdup("sqlprint"); }
	| SQL_SQLWARNING					{ $$ = mm_strdup("sqlwarning"); }
	| SQL_STOP							{ $$ = mm_strdup("stop"); }
	;

ECPGKeywords_rest: SQL_CONNECT			{ $$ = mm_strdup("connect"); }
	| SQL_DESCRIBE						{ $$ = mm_strdup("describe"); }
	| SQL_DISCONNECT					{ $$ = mm_strdup("disconnect"); }
	| SQL_OPEN							{ $$ = mm_strdup("open"); }
	| SQL_VAR							{ $$ = mm_strdup("var"); }
	| SQL_WHENEVER						{ $$ = mm_strdup("whenever"); }
	;

all_unreserved_keyword: unreserved_keyword	{ $$ = $1; }
	| ECPGunreserved_interval			{ $$ = $1; }
	| CONNECTION						{ $$ = mm_strdup("connection"); }
	;


 unreserved_keyword:
 ABORT_P
 { 
 $$ = mm_strdup("abort");
}
|  ABSENT
 { 
 $$ = mm_strdup("absent");
}
|  ABSOLUTE_P
 { 
 $$ = mm_strdup("absolute");
}
|  ACCESS
 { 
 $$ = mm_strdup("access");
}
|  ACTION
 { 
 $$ = mm_strdup("action");
}
|  ADD_P
 { 
 $$ = mm_strdup("add");
}
|  ADMIN
 { 
 $$ = mm_strdup("admin");
}
|  AFTER
 { 
 $$ = mm_strdup("after");
}
|  AGGREGATE
 { 
 $$ = mm_strdup("aggregate");
}
|  ALSO
 { 
 $$ = mm_strdup("also");
}
|  ALTER
 { 
 $$ = mm_strdup("alter");
}
|  ALWAYS
 { 
 $$ = mm_strdup("always");
}
|  ASENSITIVE
 { 
 $$ = mm_strdup("asensitive");
}
|  ASSERTION
 { 
 $$ = mm_strdup("assertion");
}
|  ASSIGNMENT
 { 
 $$ = mm_strdup("assignment");
}
|  AT
 { 
 $$ = mm_strdup("at");
}
|  ATOMIC
 { 
 $$ = mm_strdup("atomic");
}
|  ATTACH
 { 
 $$ = mm_strdup("attach");
}
|  ATTRIBUTE
 { 
 $$ = mm_strdup("attribute");
}
|  BACKWARD
 { 
 $$ = mm_strdup("backward");
}
|  BEFORE
 { 
 $$ = mm_strdup("before");
}
|  BEGIN_P
 { 
 $$ = mm_strdup("begin");
}
|  BREADTH
 { 
 $$ = mm_strdup("breadth");
}
|  BY
 { 
 $$ = mm_strdup("by");
}
|  CACHE
 { 
 $$ = mm_strdup("cache");
}
|  CALL
 { 
 $$ = mm_strdup("call");
}
|  CALLED
 { 
 $$ = mm_strdup("called");
}
|  CASCADE
 { 
 $$ = mm_strdup("cascade");
}
|  CASCADED
 { 
 $$ = mm_strdup("cascaded");
}
|  CATALOG_P
 { 
 $$ = mm_strdup("catalog");
}
|  CHAIN
 { 
 $$ = mm_strdup("chain");
}
|  CHARACTERISTICS
 { 
 $$ = mm_strdup("characteristics");
}
|  CHECKPOINT
 { 
 $$ = mm_strdup("checkpoint");
}
|  CLASS
 { 
 $$ = mm_strdup("class");
}
|  CLOSE
 { 
 $$ = mm_strdup("close");
}
|  CLUSTER
 { 
 $$ = mm_strdup("cluster");
}
|  COLUMNS
 { 
 $$ = mm_strdup("columns");
}
|  COMMENT
 { 
 $$ = mm_strdup("comment");
}
|  COMMENTS
 { 
 $$ = mm_strdup("comments");
}
|  COMMIT
 { 
 $$ = mm_strdup("commit");
}
|  COMMITTED
 { 
 $$ = mm_strdup("committed");
}
|  COMPRESSION
 { 
 $$ = mm_strdup("compression");
}
|  CONDITIONAL
 { 
 $$ = mm_strdup("conditional");
}
|  CONFIGURATION
 { 
 $$ = mm_strdup("configuration");
}
|  CONFLICT
 { 
 $$ = mm_strdup("conflict");
}
|  CONSTRAINTS
 { 
 $$ = mm_strdup("constraints");
}
|  CONTENT_P
 { 
 $$ = mm_strdup("content");
}
|  CONTINUE_P
 { 
 $$ = mm_strdup("continue");
}
|  CONVERSION_P
 { 
 $$ = mm_strdup("conversion");
}
|  COPY
 { 
 $$ = mm_strdup("copy");
}
|  COST
 { 
 $$ = mm_strdup("cost");
}
|  CSV
 { 
 $$ = mm_strdup("csv");
}
|  CUBE
 { 
 $$ = mm_strdup("cube");
}
|  CURSOR
 { 
 $$ = mm_strdup("cursor");
}
|  CYCLE
 { 
 $$ = mm_strdup("cycle");
}
|  DATA_P
 { 
 $$ = mm_strdup("data");
}
|  DATABASE
 { 
 $$ = mm_strdup("database");
}
|  DEALLOCATE
 { 
 $$ = mm_strdup("deallocate");
}
|  DECLARE
 { 
 $$ = mm_strdup("declare");
}
|  DEFAULTS
 { 
 $$ = mm_strdup("defaults");
}
|  DEFERRED
 { 
 $$ = mm_strdup("deferred");
}
|  DEFINER
 { 
 $$ = mm_strdup("definer");
}
|  DELETE_P
 { 
 $$ = mm_strdup("delete");
}
|  DELIMITER
 { 
 $$ = mm_strdup("delimiter");
}
|  DELIMITERS
 { 
 $$ = mm_strdup("delimiters");
}
|  DEPENDS
 { 
 $$ = mm_strdup("depends");
}
|  DEPTH
 { 
 $$ = mm_strdup("depth");
}
|  DETACH
 { 
 $$ = mm_strdup("detach");
}
|  DICTIONARY
 { 
 $$ = mm_strdup("dictionary");
}
|  DISABLE_P
 { 
 $$ = mm_strdup("disable");
}
|  DISCARD
 { 
 $$ = mm_strdup("discard");
}
|  DOCUMENT_P
 { 
 $$ = mm_strdup("document");
}
|  DOMAIN_P
 { 
 $$ = mm_strdup("domain");
}
|  DOUBLE_P
 { 
 $$ = mm_strdup("double");
}
|  DROP
 { 
 $$ = mm_strdup("drop");
}
|  EACH
 { 
 $$ = mm_strdup("each");
}
|  EMPTY_P
 { 
 $$ = mm_strdup("empty");
}
|  ENABLE_P
 { 
 $$ = mm_strdup("enable");
}
|  ENCODING
 { 
 $$ = mm_strdup("encoding");
}
|  ENCRYPTED
 { 
 $$ = mm_strdup("encrypted");
}
|  ENUM_P
 { 
 $$ = mm_strdup("enum");
}
|  ERROR_P
 { 
 $$ = mm_strdup("error");
}
|  ESCAPE
 { 
 $$ = mm_strdup("escape");
}
|  EVENT
 { 
 $$ = mm_strdup("event");
}
|  EXCLUDE
 { 
 $$ = mm_strdup("exclude");
}
|  EXCLUDING
 { 
 $$ = mm_strdup("excluding");
}
|  EXCLUSIVE
 { 
 $$ = mm_strdup("exclusive");
}
|  EXECUTE
 { 
 $$ = mm_strdup("execute");
}
|  EXPLAIN
 { 
 $$ = mm_strdup("explain");
}
|  EXPRESSION
 { 
 $$ = mm_strdup("expression");
}
|  EXTENSION
 { 
 $$ = mm_strdup("extension");
}
|  EXTERNAL
 { 
 $$ = mm_strdup("external");
}
|  FAMILY
 { 
 $$ = mm_strdup("family");
}
|  FILTER
 { 
 $$ = mm_strdup("filter");
}
|  FINALIZE
 { 
 $$ = mm_strdup("finalize");
}
|  FIRST_P
 { 
 $$ = mm_strdup("first");
}
|  FOLLOWING
 { 
 $$ = mm_strdup("following");
}
|  FORCE
 { 
 $$ = mm_strdup("force");
}
|  FORMAT
 { 
 $$ = mm_strdup("format");
}
|  FORWARD
 { 
 $$ = mm_strdup("forward");
}
|  FUNCTION
 { 
 $$ = mm_strdup("function");
}
|  FUNCTIONS
 { 
 $$ = mm_strdup("functions");
}
|  GENERATED
 { 
 $$ = mm_strdup("generated");
}
|  GLOBAL
 { 
 $$ = mm_strdup("global");
}
|  GRANTED
 { 
 $$ = mm_strdup("granted");
}
|  GROUPS
 { 
 $$ = mm_strdup("groups");
}
|  HANDLER
 { 
 $$ = mm_strdup("handler");
}
|  HEADER_P
 { 
 $$ = mm_strdup("header");
}
|  HOLD
 { 
 $$ = mm_strdup("hold");
}
|  IDENTITY_P
 { 
 $$ = mm_strdup("identity");
}
|  IF_P
 { 
 $$ = mm_strdup("if");
}
|  IMMEDIATE
 { 
 $$ = mm_strdup("immediate");
}
|  IMMUTABLE
 { 
 $$ = mm_strdup("immutable");
}
|  IMPLICIT_P
 { 
 $$ = mm_strdup("implicit");
}
|  IMPORT_P
 { 
 $$ = mm_strdup("import");
}
|  INCLUDE
 { 
 $$ = mm_strdup("include");
}
|  INCLUDING
 { 
 $$ = mm_strdup("including");
}
|  INCREMENT
 { 
 $$ = mm_strdup("increment");
}
|  INDENT
 { 
 $$ = mm_strdup("indent");
}
|  INDEX
 { 
 $$ = mm_strdup("index");
}
|  INDEXES
 { 
 $$ = mm_strdup("indexes");
}
|  INHERIT
 { 
 $$ = mm_strdup("inherit");
}
|  INHERITS
 { 
 $$ = mm_strdup("inherits");
}
|  INITIAL
 { 
 $$ = mm_strdup("initial");
}
|  INLINE_P
 { 
 $$ = mm_strdup("inline");
}
|  INSENSITIVE
 { 
 $$ = mm_strdup("insensitive");
}
|  INSERT
 { 
 $$ = mm_strdup("insert");
}
|  INSTEAD
 { 
 $$ = mm_strdup("instead");
}
|  INVOKER
 { 
 $$ = mm_strdup("invoker");
}
|  ISOLATION
 { 
 $$ = mm_strdup("isolation");
}
|  KEEP
 { 
 $$ = mm_strdup("keep");
}
|  KEY
 { 
 $$ = mm_strdup("key");
}
|  KEYS
 { 
 $$ = mm_strdup("keys");
}
|  LABEL
 { 
 $$ = mm_strdup("label");
}
|  LANGUAGE
 { 
 $$ = mm_strdup("language");
}
|  LARGE_P
 { 
 $$ = mm_strdup("large");
}
|  LAST_P
 { 
 $$ = mm_strdup("last");
}
|  LEAKPROOF
 { 
 $$ = mm_strdup("leakproof");
}
|  LEVEL
 { 
 $$ = mm_strdup("level");
}
|  LISTEN
 { 
 $$ = mm_strdup("listen");
}
|  LOAD
 { 
 $$ = mm_strdup("load");
}
|  LOCAL
 { 
 $$ = mm_strdup("local");
}
|  LOCATION
 { 
 $$ = mm_strdup("location");
}
|  LOCK_P
 { 
 $$ = mm_strdup("lock");
}
|  LOCKED
 { 
 $$ = mm_strdup("locked");
}
|  LOGGED
 { 
 $$ = mm_strdup("logged");
}
|  MAPPING
 { 
 $$ = mm_strdup("mapping");
}
|  MATCH
 { 
 $$ = mm_strdup("match");
}
|  MATCHED
 { 
 $$ = mm_strdup("matched");
}
|  MATERIALIZED
 { 
 $$ = mm_strdup("materialized");
}
|  MAXVALUE
 { 
 $$ = mm_strdup("maxvalue");
}
|  MEASURES
 { 
 $$ = mm_strdup("measures");
}
|  MERGE
 { 
 $$ = mm_strdup("merge");
}
|  METHOD
 { 
 $$ = mm_strdup("method");
}
|  MINVALUE
 { 
 $$ = mm_strdup("minvalue");
}
|  MODE
 { 
 $$ = mm_strdup("mode");
}
|  MOVE
 { 
 $$ = mm_strdup("move");
}
|  NAME_P
 { 
 $$ = mm_strdup("name");
}
|  NAMES
 { 
 $$ = mm_strdup("names");
}
|  NESTED
 { 
 $$ = mm_strdup("nested");
}
|  NEW
 { 
 $$ = mm_strdup("new");
}
|  NEXT
 { 
 $$ = mm_strdup("next");
}
|  NFC
 { 
 $$ = mm_strdup("nfc");
}
|  NFD
 { 
 $$ = mm_strdup("nfd");
}
|  NFKC
 { 
 $$ = mm_strdup("nfkc");
}
|  NFKD
 { 
 $$ = mm_strdup("nfkd");
}
|  NO
 { 
 $$ = mm_strdup("no");
}
|  NORMALIZED
 { 
 $$ = mm_strdup("normalized");
}
|  NOTHING
 { 
 $$ = mm_strdup("nothing");
}
|  NOTIFY
 { 
 $$ = mm_strdup("notify");
}
|  NOWAIT
 { 
 $$ = mm_strdup("nowait");
}
|  NULLS_P
 { 
 $$ = mm_strdup("nulls");
}
|  OBJECT_P
 { 
 $$ = mm_strdup("object");
}
|  OF
 { 
 $$ = mm_strdup("of");
}
|  OFF
 { 
 $$ = mm_strdup("off");
}
|  OIDS
 { 
 $$ = mm_strdup("oids");
}
|  OLD
 { 
 $$ = mm_strdup("old");
}
|  OMIT
 { 
 $$ = mm_strdup("omit");
}
|  OPERATOR
 { 
 $$ = mm_strdup("operator");
}
|  OPTION
 { 
 $$ = mm_strdup("option");
}
|  OPTIONS
 { 
 $$ = mm_strdup("options");
}
|  ORDINALITY
 { 
 $$ = mm_strdup("ordinality");
}
|  OTHERS
 { 
 $$ = mm_strdup("others");
}
|  OVER
 { 
 $$ = mm_strdup("over");
}
|  OVERRIDING
 { 
 $$ = mm_strdup("overriding");
}
|  OWNED
 { 
 $$ = mm_strdup("owned");
}
|  OWNER
 { 
 $$ = mm_strdup("owner");
}
|  PARALLEL
 { 
 $$ = mm_strdup("parallel");
}
|  PARAMETER
 { 
 $$ = mm_strdup("parameter");
}
|  PARSER
 { 
 $$ = mm_strdup("parser");
}
|  PARTIAL
 { 
 $$ = mm_strdup("partial");
}
|  PARTITION
 { 
 $$ = mm_strdup("partition");
}
|  PASSING
 { 
 $$ = mm_strdup("passing");
}
|  PASSWORD
 { 
 $$ = mm_strdup("password");
}
|  PAST
 { 
 $$ = mm_strdup("past");
}
|  PATH
 { 
 $$ = mm_strdup("path");
}
|  PATTERN_P
 { 
 $$ = mm_strdup("pattern");
}
|  PERMUTE
 { 
 $$ = mm_strdup("permute");
}
|  PLAN
 { 
 $$ = mm_strdup("plan");
}
|  PLANS
 { 
 $$ = mm_strdup("plans");
}
|  POLICY
 { 
 $$ = mm_strdup("policy");
}
|  PRECEDING
 { 
 $$ = mm_strdup("preceding");
}
|  PREPARE
 { 
 $$ = mm_strdup("prepare");
}
|  PREPARED
 { 
 $$ = mm_strdup("prepared");
}
|  PRESERVE
 { 
 $$ = mm_strdup("preserve");
}
|  PRIOR
 { 
 $$ = mm_strdup("prior");
}
|  PRIVILEGES
 { 
 $$ = mm_strdup("privileges");
}
|  PROCEDURAL
 { 
 $$ = mm_strdup("procedural");
}
|  PROCEDURE
 { 
 $$ = mm_strdup("procedure");
}
|  PROCEDURES
 { 
 $$ = mm_strdup("procedures");
}
|  PROGRAM
 { 
 $$ = mm_strdup("program");
}
|  PUBLICATION
 { 
 $$ = mm_strdup("publication");
}
|  QUOTE
 { 
 $$ = mm_strdup("quote");
}
|  QUOTES
 { 
 $$ = mm_strdup("quotes");
}
|  RANGE
 { 
 $$ = mm_strdup("range");
}
|  READ
 { 
 $$ = mm_strdup("read");
}
|  REASSIGN
 { 
 $$ = mm_strdup("reassign");
}
|  RECURSIVE
 { 
 $$ = mm_strdup("recursive");
}
|  REF_P
 { 
 $$ = mm_strdup("ref");
}
|  REFERENCING
 { 
 $$ = mm_strdup("referencing");
}
|  REFRESH
 { 
 $$ = mm_strdup("refresh");
}
|  REINDEX
 { 
 $$ = mm_strdup("reindex");
}
|  RELATIVE_P
 { 
 $$ = mm_strdup("relative");
}
|  RELEASE
 { 
 $$ = mm_strdup("release");
}
|  RENAME
 { 
 $$ = mm_strdup("rename");
}
|  REPEATABLE
 { 
 $$ = mm_strdup("repeatable");
}
|  REPLACE
 { 
 $$ = mm_strdup("replace");
}
|  REPLICA
 { 
 $$ = mm_strdup("replica");
}
|  RESET
 { 
 $$ = mm_strdup("reset");
}
|  RESTART
 { 
 $$ = mm_strdup("restart");
}
|  RESTRICT
 { 
 $$ = mm_strdup("restrict");
}
|  RETURN
 { 
 $$ = mm_strdup("return");
}
|  RETURNS
 { 
 $$ = mm_strdup("returns");
}
|  REVOKE
 { 
 $$ = mm_strdup("revoke");
}
|  ROLE
 { 
 $$ = mm_strdup("role");
}
|  ROLLBACK
 { 
 $$ = mm_strdup("rollback");
}
|  ROLLUP
 { 
 $$ = mm_strdup("rollup");
}
|  ROUTINE
 { 
 $$ = mm_strdup("routine");
}
|  ROUTINES
 { 
 $$ = mm_strdup("routines");
}
|  ROWS
 { 
 $$ = mm_strdup("rows");
}
|  RULE
 { 
 $$ = mm_strdup("rule");
}
|  SAVEPOINT
 { 
 $$ = mm_strdup("savepoint");
}
|  SCALAR
 { 
 $$ = mm_strdup("scalar");
}
|  SCHEMA
 { 
 $$ = mm_strdup("schema");
}
|  SCHEMAS
 { 
 $$ = mm_strdup("schemas");
}
|  SCROLL
 { 
 $$ = mm_strdup("scroll");
}
|  SEARCH
 { 
 $$ = mm_strdup("search");
}
|  SECURITY
 { 
 $$ = mm_strdup("security");
}
|  SEEK
 { 
 $$ = mm_strdup("seek");
}
|  SEQUENCE
 { 
 $$ = mm_strdup("sequence");
}
|  SEQUENCES
 { 
 $$ = mm_strdup("sequences");
}
|  SERIALIZABLE
 { 
 $$ = mm_strdup("serializable");
}
|  SERVER
 { 
 $$ = mm_strdup("server");
}
|  SESSION
 { 
 $$ = mm_strdup("session");
}
|  SET
 { 
 $$ = mm_strdup("set");
}
|  SETS
 { 
 $$ = mm_strdup("sets");
}
|  SHARE
 { 
 $$ = mm_strdup("share");
}
|  SHOW
 { 
 $$ = mm_strdup("show");
}
|  SIMPLE
 { 
 $$ = mm_strdup("simple");
}
|  SKIP
 { 
 $$ = mm_strdup("skip");
}
|  SNAPSHOT
 { 
 $$ = mm_strdup("snapshot");
}
|  SOURCE
 { 
 $$ = mm_strdup("source");
}
|  SQL_P
 { 
 $$ = mm_strdup("sql");
}
|  STABLE
 { 
 $$ = mm_strdup("stable");
}
|  STANDALONE_P
 { 
 $$ = mm_strdup("standalone");
}
|  START
 { 
 $$ = mm_strdup("start");
}
|  STATEMENT
 { 
 $$ = mm_strdup("statement");
}
|  STATISTICS
 { 
 $$ = mm_strdup("statistics");
}
|  STDIN
 { 
 $$ = mm_strdup("stdin");
}
|  STDOUT
 { 
 $$ = mm_strdup("stdout");
}
|  STORAGE
 { 
 $$ = mm_strdup("storage");
}
|  STORED
 { 
 $$ = mm_strdup("stored");
}
|  STRICT_P
 { 
 $$ = mm_strdup("strict");
}
|  STRING_P
 { 
 $$ = mm_strdup("string");
}
|  STRIP_P
 { 
 $$ = mm_strdup("strip");
}
|  SUBSCRIPTION
 { 
 $$ = mm_strdup("subscription");
}
|  SUBSET
 { 
 $$ = mm_strdup("subset");
}
|  SUPPORT
 { 
 $$ = mm_strdup("support");
}
|  SYSID
 { 
 $$ = mm_strdup("sysid");
}
|  SYSTEM_P
 { 
 $$ = mm_strdup("system");
}
|  TABLES
 { 
 $$ = mm_strdup("tables");
}
|  TABLESPACE
 { 
 $$ = mm_strdup("tablespace");
}
|  TARGET
 { 
 $$ = mm_strdup("target");
}
|  TEMP
 { 
 $$ = mm_strdup("temp");
}
|  TEMPLATE
 { 
 $$ = mm_strdup("template");
}
|  TEMPORARY
 { 
 $$ = mm_strdup("temporary");
}
|  TEXT_P
 { 
 $$ = mm_strdup("text");
}
|  TIES
 { 
 $$ = mm_strdup("ties");
}
|  TRANSACTION
 { 
 $$ = mm_strdup("transaction");
}
|  TRANSFORM
 { 
 $$ = mm_strdup("transform");
}
|  TRIGGER
 { 
 $$ = mm_strdup("trigger");
}
|  TRUNCATE
 { 
 $$ = mm_strdup("truncate");
}
|  TRUSTED
 { 
 $$ = mm_strdup("trusted");
}
|  TYPE_P
 { 
 $$ = mm_strdup("type");
}
|  TYPES_P
 { 
 $$ = mm_strdup("types");
}
|  UESCAPE
 { 
 $$ = mm_strdup("uescape");
}
|  UNBOUNDED
 { 
 $$ = mm_strdup("unbounded");
}
|  UNCOMMITTED
 { 
 $$ = mm_strdup("uncommitted");
}
|  UNCONDITIONAL
 { 
 $$ = mm_strdup("unconditional");
}
|  UNENCRYPTED
 { 
 $$ = mm_strdup("unencrypted");
}
|  UNKNOWN
 { 
 $$ = mm_strdup("unknown");
}
|  UNLISTEN
 { 
 $$ = mm_strdup("unlisten");
}
|  UNLOGGED
 { 
 $$ = mm_strdup("unlogged");
}
|  UNTIL
 { 
 $$ = mm_strdup("until");
}
|  UPDATE
 { 
 $$ = mm_strdup("update");
}
|  VACUUM
 { 
 $$ = mm_strdup("vacuum");
}
|  VALID
 { 
 $$ = mm_strdup("valid");
}
|  VALIDATE
 { 
 $$ = mm_strdup("validate");
}
|  VALIDATOR
 { 
 $$ = mm_strdup("validator");
}
|  VALUE_P
 { 
 $$ = mm_strdup("value");
}
|  VARYING
 { 
 $$ = mm_strdup("varying");
}
|  VERSION_P
 { 
 $$ = mm_strdup("version");
}
|  VIEW
 { 
 $$ = mm_strdup("view");
}
|  VIEWS
 { 
 $$ = mm_strdup("views");
}
|  VOLATILE
 { 
 $$ = mm_strdup("volatile");
}
|  WHITESPACE_P
 { 
 $$ = mm_strdup("whitespace");
}
|  WITHIN
 { 
 $$ = mm_strdup("within");
}
|  WITHOUT
 { 
 $$ = mm_strdup("without");
}
|  WORK
 { 
 $$ = mm_strdup("work");
}
|  WRAPPER
 { 
 $$ = mm_strdup("wrapper");
}
|  WRITE
 { 
 $$ = mm_strdup("write");
}
|  XML_P
 { 
 $$ = mm_strdup("xml");
}
|  YES_P
 { 
 $$ = mm_strdup("yes");
}
|  ZONE
 { 
 $$ = mm_strdup("zone");
}
;


%%

void
base_yyerror(const char *error)
{
	/* translator: %s is typically the translation of "syntax error" */
	mmerror(PARSE_ERROR, ET_ERROR, "%s at or near \"%s\"",
			_(error), token_start ? token_start : base_yytext);
}

static Node *
makeIntConst(int val, int location)
{
	A_Const	   *n = makeNode(A_Const);

	n->val.ival.type = T_Integer;
	n->val.ival.ival = val;
	n->location = location;

   return (Node *) n;
}

void
parser_init(void)
{
	/*
	 * This function is empty. It only exists for compatibility with the
	 * backend parser right now.
	 */
}
