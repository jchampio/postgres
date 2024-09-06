%top{
/*-------------------------------------------------------------------------
 *
 * pgc.l
 *	  lexical scanner for ecpg
 *
 * This is a modified version of src/backend/parser/scan.l
 *
 * The ecpg scanner is not backup-free, so the fail rules are
 * only here to simplify syncing this file with scan.l.
 *
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/interfaces/ecpg/preproc/pgc.l
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include <ctype.h>
#include <limits.h>

#include "common/string.h"

#include "preproc_extern.h"
#include "preproc.h"
}

%{

/* LCOV_EXCL_START */

extern YYSTYPE base_yylval;

static int	xcdepth = 0;		/* depth of nesting in slash-star comments */
static char *dolqstart = NULL;	/* current $foo$ quote start string */

/*
 * literalbuf is used to accumulate literal values when multiple rules
 * are needed to parse a single literal.  Call startlit to reset buffer
 * to empty, addlit to add text.  Note that the buffer is permanently
 * malloc'd to the largest size needed so far in the current run.
 */
static char *literalbuf = NULL; /* expandable buffer */
static int	literallen;			/* actual current length */
static int	literalalloc;		/* current allocated buffer size */

/* Used for detecting global state together with braces_open */
static int	parenths_open;

#define startlit()	(literalbuf[0] = '\0', literallen = 0)
static void addlit(char *ytext, int yleng);
static void addlitchar(unsigned char ychar);
static int	process_integer_literal(const char *token, YYSTYPE *lval, int base);

char	   *token_start;

/* vars to keep track of start conditions when scanning literals */
static int	state_before_str_start;
static int	state_before_str_stop;
%}

%option 8bit
%option never-interactive
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option warn
%option yylineno
%option prefix="base_yy"

/*
 * OK, here is a short description of lex/flex rules behavior.
 * The longest pattern which matches an input string is always chosen.
 * For equal-length patterns, the first occurring in the rules list is chosen.
 * INITIAL is the starting state, to which all non-conditional rules apply.
 * Exclusive states change parsing rules while the state is active.  When in
 * an exclusive state, only those rules defined for that state apply.
 *
 * We use exclusive states for quoted strings, extended comments,
 * and to eliminate parsing troubles for numeric strings.
 * Exclusive states:
 *  <xb> bit string literal
 *  <xc> extended C-style comments
 *  <xd> delimited identifiers (double-quoted identifiers)
 *  <xh> hexadecimal byte string
 *  <xn> national character quoted strings
 *  <xq> standard quoted strings
 *  <xqs> quote stop (detect continued strings)
 *  <xe> extended quoted strings (support backslash escape sequences)
 *  <xdolq> $foo$ quoted strings
 *  <xui> quoted identifier with Unicode escapes
 *  <xus> quoted string with Unicode escapes
 *
 * Note: we intentionally don't mimic the backend's <xeu> state; we have
 * no need to distinguish it from <xe> state.
 *
 * Remember to add an <<EOF>> case whenever you add a new exclusive state!
 * The default one is probably not the right thing.
 */

%x xb
%x xc
%x xd
%x xh
%x xn
%x xq
%x xqs
%x xe
%x xdolq
%x xui
%x xus

/* Additional exclusive states that are specific to ECPG */
%x C SQL incl def def_ident undef

/*
 * In order to make the world safe for Windows and Mac clients as well as
 * Unix ones, we accept either \n or \r as a newline.  A DOS-style \r\n
 * sequence will be seen as two successive newlines, but that doesn't cause
 * any problems.  SQL-style comments, which start with -- and extend to the
 * next newline, are treated as equivalent to a single whitespace character.
 *
 * NOTE a fine point: if there is no newline following --, we will absorb
 * everything to the end of the input as a comment.  This is correct.  Older
 * versions of Postgres failed to recognize -- as a comment if the input
 * did not end with a newline.
 *
 * non_newline_space tracks all space characters except newlines.
 *
 * XXX if you change the set of whitespace characters, fix ecpg_isspace()
 * to agree.
 */

space				[ \t\n\r\f\v]
non_newline_space	[ \t\f\v]
newline				[\n\r]
non_newline			[^\n\r]

comment			("--"{non_newline}*)

whitespace		({space}+|{comment})

/*
 * SQL requires at least one newline in the whitespace separating
 * string literals that are to be concatenated.  Silly, but who are we
 * to argue?  Note that {whitespace_with_newline} should not have * after
 * it, whereas {whitespace} should generally have a * after it...
 */

non_newline_whitespace	({non_newline_space}|{comment})
whitespace_with_newline	({non_newline_whitespace}*{newline}{whitespace}*)

quote			'
/* If we see {quote} then {quotecontinue}, the quoted string continues */
quotecontinue	{whitespace_with_newline}{quote}

/*
 * {quotecontinuefail} is needed to avoid lexer backup when we fail to match
 * {quotecontinue}.  It might seem that this could just be {whitespace}*,
 * but if there's a dash after {whitespace_with_newline}, it must be consumed
 * to see if there's another dash --- which would start a {comment} and thus
 * allow continuation of the {quotecontinue} token.
 */
quotecontinuefail	{whitespace}*"-"?

/* Bit string
 */
xbstart			[bB]{quote}
xbinside		[^']*

/* Hexadecimal byte string */
xhstart			[xX]{quote}
xhinside		[^']*

/* National character */
xnstart			[nN]{quote}

/* Quoted string that allows backslash escapes */
xestart			[eE]{quote}
xeinside		[^\\']+
xeescape		[\\][^0-7]
xeoctesc		[\\][0-7]{1,3}
xehexesc		[\\]x[0-9A-Fa-f]{1,2}
xeunicode		[\\](u[0-9A-Fa-f]{4}|U[0-9A-Fa-f]{8})

/* Extended quote
 * xqdouble implements embedded quote, ''''
 */
xqstart			{quote}
xqdouble		{quote}{quote}
xqinside		[^']+

/* $foo$ style quotes ("dollar quoting")
 * The quoted string starts with $foo$ where "foo" is an optional string
 * in the form of an identifier, except that it may not contain "$",
 * and extends to the first occurrence of an identical string.
 * There is *no* processing of the quoted text.
 *
 * {dolqfailed} is an error rule to avoid scanner backup when {dolqdelim}
 * fails to match its trailing "$".
 */
dolq_start		[A-Za-z\200-\377_]
dolq_cont		[A-Za-z\200-\377_0-9]
dolqdelim		\$({dolq_start}{dolq_cont}*)?\$
dolqfailed		\${dolq_start}{dolq_cont}*
dolqinside		[^$]+

/* Double quote
 * Allows embedded spaces and other special characters into identifiers.
 */
dquote			\"
xdstart			{dquote}
xdstop			{dquote}
xddouble		{dquote}{dquote}
xdinside		[^"]+

/* Quoted identifier with Unicode escapes */
xuistart		[uU]&{dquote}

/* Quoted string with Unicode escapes */
xusstart		[uU]&{quote}


/* C-style comments
 *
 * The "extended comment" syntax closely resembles allowable operator syntax.
 * The tricky part here is to get lex to recognize a string starting with
 * slash-star as a comment, when interpreting it as an operator would produce
 * a longer match --- remember lex will prefer a longer match!  Also, if we
 * have something like plus-slash-star, lex will think this is a 3-character
 * operator whereas we want to see it as a + operator and a comment start.
 * The solution is two-fold:
 * 1. append {op_chars}* to xcstart so that it matches as much text as
 *    {operator} would. Then the tie-breaker (first matching rule of same
 *    length) ensures xcstart wins.  We put back the extra stuff with yyless()
 *    in case it contains a star-slash that should terminate the comment.
 * 2. In the operator rule, check for slash-star within the operator, and
 *    if found throw it back with yyless().  This handles the plus-slash-star
 *    problem.
 * Dash-dash comments have similar interactions with the operator rule.
 */
xcstart			\/\*{op_chars}*
xcstop			\*+\/
xcinside		[^*/]+

ident_start		[A-Za-z\200-\377_]
ident_cont		[A-Za-z\200-\377_0-9\$]

identifier		{ident_start}{ident_cont}*

/* Assorted special-case operators and operator-like tokens */
typecast		"::"
dot_dot			\.\.
colon_equals	":="
left_brace_minus	"{-"
right_minus_brace	"-}"

/*
 * These operator-like tokens (unlike the above ones) also match the {operator}
 * rule, which means that they might be overridden by a longer match if they
 * are followed by a comment start or a + or - character. Accordingly, if you
 * add to this list, you must also add corresponding code to the {operator}
 * block to return the correct token in such cases. (This is not needed in
 * psqlscan.l since the token value is ignored there.)
 */
equals_greater	"=>"
less_equals		"<="
greater_equals	">="
less_greater	"<>"
not_equals		"!="

/*
 * "self" is the set of chars that should be returned as single-character
 * tokens.  "op_chars" is the set of chars that can make up "Op" tokens,
 * which can be one or more characters long (but if a single-char token
 * appears in the "self" set, it is not to be returned as an Op).  Note
 * that the sets overlap, but each has some chars that are not in the other.
 *
 * If you change either set, adjust the character lists appearing in the
 * rule for "operator"!
 */
self			[,()\[\].;\:\+\-\*\/\%\^\<\>\=]
op_chars		[\~\!\@\#\^\&\|\`\?\+\-\*\/\%\<\>\=]
operator		{op_chars}+

/*
 * Numbers
 *
 * Unary minus is not part of a number here.  Instead we pass it separately to
 * the parser, and there it gets coerced via doNegate().
 *
 * {numericfail} is used because we would like "1..10" to lex as 1, dot_dot, 10.
 *
 * {realfail} is added to prevent the need for scanner
 * backup when the {real} rule fails to match completely.
 */
decdigit		[0-9]
hexdigit		[0-9A-Fa-f]
octdigit		[0-7]
bindigit		[0-1]

decinteger		{decdigit}(_?{decdigit})*
hexinteger		0[xX](_?{hexdigit})+
octinteger		0[oO](_?{octdigit})+
bininteger		0[bB](_?{bindigit})+

hexfail			0[xX]_?
octfail			0[oO]_?
binfail			0[bB]_?

numeric			(({decinteger}\.{decinteger}?)|(\.{decinteger}))
numericfail		{decinteger}\.\.

real			({decinteger}|{numeric})[Ee][-+]?{decinteger}
realfail		({decinteger}|{numeric})[Ee][-+]

decinteger_junk	{decinteger}{ident_start}
hexinteger_junk	{hexinteger}{ident_start}
octinteger_junk	{octinteger}{ident_start}
bininteger_junk	{bininteger}{ident_start}
numeric_junk	{numeric}{ident_start}
real_junk		{real}{ident_start}

/* Positional parameters don't accept underscores. */
param			\${decdigit}+
param_junk		\${decdigit}+{ident_start}

other			.

/*
 * Dollar quoted strings are totally opaque, and no escaping is done on them.
 * Other quoted strings must allow some special characters such as single-quote
 *  and newline.
 * Embedded single-quotes are implemented both in the SQL standard
 *  style of two adjacent single quotes "''" and in the Postgres/Java style
 *  of escaped-quote "\'".
 * Other embedded escaped characters are matched explicitly and the leading
 *  backslash is dropped from the string.
 * Note that xcstart must appear before operator, as explained above!
 *  Also whitespace (comment) must appear before operator.
 */

%%

%{
		/* code to execute during start of each call of yylex() */
		token_start = NULL;
%}

<SQL>{
{whitespace}	{
					/* ignore */
				}
} /* <SQL> */

<C,SQL>{
{xcstart}		{
					token_start = yytext;
					state_before_str_start = YYSTATE;
					xcdepth = 0;
					BEGIN(xc);
					/* Put back any characters past slash-star; see above */
					yyless(2);
					fputs("/*", yyout);
				}
} /* <C,SQL> */

<xc>{
{xcstart}		{
					if (state_before_str_start == SQL)
					{
						xcdepth++;
						/* Put back any characters past slash-star; see above */
						yyless(2);
						fputs("/_*", yyout);
					}
					else if (state_before_str_start == C)
					{
						ECHO;
					}
				}

{xcstop}		{
					if (state_before_str_start == SQL)
					{
						if (xcdepth <= 0)
						{
							ECHO;
							BEGIN(SQL);
							token_start = NULL;
						}
						else
						{
							xcdepth--;
							fputs("*_/", yyout);
						}
					}
					else if (state_before_str_start == C)
					{
						ECHO;
						BEGIN(C);
						token_start = NULL;
					}
				}

{xcinside}		{
					ECHO;
				}

{op_chars}		{
					ECHO;
				}

\*+				{
					ECHO;
				}

<<EOF>>			{
					mmfatal(PARSE_ERROR, "unterminated /* comment");
				}
} /* <xc> */

<SQL>{
{xbstart}		{
					token_start = yytext;
					state_before_str_start = YYSTATE;
					BEGIN(xb);
					startlit();
				}
} /* <SQL> */

<xh>{xhinside}	|
<xb>{xbinside}	{
					addlit(yytext, yyleng);
				}
<xb><<EOF>>		{
					mmfatal(PARSE_ERROR, "unterminated bit string literal");
				}

<SQL>{xhstart}	{
					token_start = yytext;
					state_before_str_start = YYSTATE;
					BEGIN(xh);
					startlit();
				}
<xh><<EOF>>		{
					mmfatal(PARSE_ERROR, "unterminated hexadecimal string literal");
				}

<SQL>{
{xnstart}		{
					/* National character. Transfer it as-is to the backend. */
					token_start = yytext;
					state_before_str_start = YYSTATE;
					BEGIN(xn);
					startlit();
				}

{xqstart}		{
					token_start = yytext;
					state_before_str_start = YYSTATE;
					BEGIN(xq);
					startlit();
				}
{xestart}		{
					token_start = yytext;
					state_before_str_start = YYSTATE;
					BEGIN(xe);
					startlit();
				}
{xusstart}		{
					token_start = yytext;
					state_before_str_start = YYSTATE;
					BEGIN(xus);
					startlit();
				}
} /* <SQL> */

<xb,xh,xq,xe,xn,xus>{quote} {
					/*
					 * When we are scanning a quoted string and see an end
					 * quote, we must look ahead for a possible continuation.
					 * If we don't see one, we know the end quote was in fact
					 * the end of the string.  To reduce the lexer table size,
					 * we use a single "xqs" state to do the lookahead for all
					 * types of strings.
					 */
					state_before_str_stop = YYSTATE;
					BEGIN(xqs);
				}
<xqs>{quotecontinue} {
					/*
					 * Found a quote continuation, so return to the in-quote
					 * state and continue scanning the literal.  Nothing is
					 * added to the literal's contents.
					 */
					BEGIN(state_before_str_stop);
				}
<xqs>{quotecontinuefail} |
<xqs>{other} |
<xqs><<EOF>>	{
					/*
					 * Failed to see a quote continuation.  Throw back
					 * everything after the end quote, and handle the string
					 * according to the state we were in previously.
					 */
					yyless(0);
					BEGIN(state_before_str_start);

					switch (state_before_str_stop)
					{
						case xb:
							if (literalbuf[strspn(literalbuf, "01")] != '\0')
								mmerror(PARSE_ERROR, ET_ERROR, "invalid bit string literal");
							base_yylval.str = psprintf("b'%s'", literalbuf);
							return BCONST;
						case xh:
							if (literalbuf[strspn(literalbuf, "0123456789abcdefABCDEF")] != '\0')
								mmerror(PARSE_ERROR, ET_ERROR, "invalid hexadecimal string literal");
							base_yylval.str = psprintf("x'%s'", literalbuf);
							return XCONST;
						case xq:
							/* fallthrough */
						case xe:
							base_yylval.str = psprintf("E'%s'", literalbuf);
							return SCONST;
						case xn:
							base_yylval.str = psprintf("N'%s'", literalbuf);
							return SCONST;
						case xus:
							base_yylval.str = psprintf("U&'%s'", literalbuf);
							return USCONST;
						default:
							mmfatal(PARSE_ERROR, "unhandled previous state in xqs\n");
					}
				}

<xq,xe,xn,xus>{xqdouble} {
					addlit(yytext, yyleng);
				}
<xq,xn,xus>{xqinside} {
					addlit(yytext, yyleng);
				}
<xe>{xeinside}	{
					addlit(yytext, yyleng);
				}
<xe>{xeunicode}	{
					addlit(yytext, yyleng);
				}
<xe>{xeescape}	{
					addlit(yytext, yyleng);
				}
<xe>{xeoctesc}	{
					addlit(yytext, yyleng);
				}
<xe>{xehexesc}	{
					addlit(yytext, yyleng);
				}
<xe>.			{
					/* This is only needed for \ just before EOF */
					addlitchar(yytext[0]);
				}
<xq,xe,xn,xus><<EOF>>	{
					mmfatal(PARSE_ERROR, "unterminated quoted string");
				}

<SQL>{
{dolqdelim}		{
					token_start = yytext;
					if (dolqstart)
						free(dolqstart);
					dolqstart = mm_strdup(yytext);
					BEGIN(xdolq);
					startlit();
					addlit(yytext, yyleng);
				}
{dolqfailed}	{
					/* throw back all but the initial "$" */
					yyless(1);
					/* and treat it as {other} */
					return yytext[0];
				}
} /* <SQL> */

<xdolq>{dolqdelim}	{
					if (strcmp(yytext, dolqstart) == 0)
					{
						addlit(yytext, yyleng);
						free(dolqstart);
						dolqstart = NULL;
						BEGIN(SQL);
						base_yylval.str = mm_strdup(literalbuf);
						return SCONST;
					}
					else
					{
						/*
						 * When we fail to match $...$ to dolqstart, transfer
						 * the $... part to the output, but put back the final
						 * $ for rescanning.  Consider $delim$...$junk$delim$
						 */
						addlit(yytext, yyleng - 1);
						yyless(yyleng - 1);
					}
				}
<xdolq>{dolqinside} {
					addlit(yytext, yyleng);
				}
<xdolq>{dolqfailed} {
					addlit(yytext, yyleng);
				}
<xdolq>.		{
					/* single quote or dollar sign */
					addlitchar(yytext[0]);
				}
<xdolq><<EOF>>	{
					mmfatal(PARSE_ERROR, "unterminated dollar-quoted string");
				}

<SQL>{
{xdstart}		{
					state_before_str_start = YYSTATE;
					BEGIN(xd);
					startlit();
				}
{xuistart}		{
					state_before_str_start = YYSTATE;
					BEGIN(xui);
					startlit();
				}
} /* <SQL> */

<xd>{xdstop}	{
					BEGIN(state_before_str_start);
					if (literallen == 0)
						mmerror(PARSE_ERROR, ET_ERROR, "zero-length delimited identifier");

					/*
					 * The server will truncate the identifier here.  We do
					 * not, as (1) it does not change the result; (2) we don't
					 * know what NAMEDATALEN the server might use; (3) this
					 * code path is also taken for literal query strings in
					 * PREPARE and EXECUTE IMMEDIATE, which can certainly be
					 * longer than NAMEDATALEN.
					 */
					base_yylval.str = mm_strdup(literalbuf);
					return CSTRING;
				}
<xui>{dquote}	{
					BEGIN(state_before_str_start);
					if (literallen == 0)
						mmerror(PARSE_ERROR, ET_ERROR, "zero-length delimited identifier");

					/*
					 * The backend will truncate the identifier here. We do
					 * not as it does not change the result.
					 */
					base_yylval.str = psprintf("U&\"%s\"", literalbuf);
					return UIDENT;
				}
<xd,xui>{xddouble} {
					addlit(yytext, yyleng);
				}
<xd,xui>{xdinside} {
					addlit(yytext, yyleng);
				}
<xd,xui><<EOF>>	{
					mmfatal(PARSE_ERROR, "unterminated quoted identifier");
				}

<SQL>{
{typecast}		{
					return TYPECAST;
				}

{dot_dot}		{
					return DOT_DOT;
				}

{colon_equals}	{
					return COLON_EQUALS;
				}

{left_brace_minus}	{
					return LEFT_BRACE_MINUS;
				}

{right_minus_brace}	{
					return RIGHT_MINUS_BRACE;
				}

{equals_greater} {
					return EQUALS_GREATER;
				}

{less_equals}	{
					return LESS_EQUALS;
				}

{greater_equals} {
					return GREATER_EQUALS;
				}

{less_greater}	{
					/* We accept both "<>" and "!=" as meaning NOT_EQUALS */
					return NOT_EQUALS;
				}

{not_equals}	{
					/* We accept both "<>" and "!=" as meaning NOT_EQUALS */
					return NOT_EQUALS;
				}

{self}			{
					/*
					 * We may find a ';' inside a structure definition in a
					 * TYPE or VAR statement. This is not an EOL marker.
					 */
					if (yytext[0] == ';' && struct_level == 0)
						BEGIN(C);
					return yytext[0];
				}

{operator}		{
					/*
					 * Check for embedded slash-star or dash-dash; those
					 * are comment starts, so operator must stop there.
					 * Note that slash-star or dash-dash at the first
					 * character will match a prior rule, not this one.
					 */
					int			nchars = yyleng;
					char	   *slashstar = strstr(yytext, "/*");
					char	   *dashdash = strstr(yytext, "--");

					if (slashstar && dashdash)
					{
						/* if both appear, take the first one */
						if (slashstar > dashdash)
							slashstar = dashdash;
					}
					else if (!slashstar)
						slashstar = dashdash;
					if (slashstar)
						nchars = slashstar - yytext;

					/*
					 * For SQL compatibility, '+' and '-' cannot be the
					 * last char of a multi-char operator unless the operator
					 * contains chars that are not in SQL operators.
					 * The idea is to lex '=-' as two operators, but not
					 * to forbid operator names like '?-' that could not be
					 * sequences of SQL operators.
					 */
					if (nchars > 1 &&
						(yytext[nchars - 1] == '+' ||
						 yytext[nchars - 1] == '-'))
					{
						int			ic;

						for (ic = nchars - 2; ic >= 0; ic--)
						{
							char		c = yytext[ic];

							if (c == '~' || c == '!' || c == '@' ||
								c == '#' || c == '^' || c == '&' ||
								c == '|' || c == '`' || c == '?' ||
								c == '%')
								break;
						}
						if (ic < 0)
						{
							/*
							 * didn't find a qualifying character, so remove
							 * all trailing [+-]
							 */
							do
							{
								nchars--;
							} while (nchars > 1 &&
									 (yytext[nchars - 1] == '+' ||
									  yytext[nchars - 1] == '-'));
						}
					}

					if (nchars < yyleng)
					{
						/* Strip the unwanted chars from the token */
						yyless(nchars);

						/*
						 * If what we have left is only one char, and it's
						 * one of the characters matching "self", then
						 * return it as a character token the same way
						 * that the "self" rule would have.
						 */
						if (nchars == 1 &&
							strchr(",()[].;:+-*/%^<>=", yytext[0]))
							return yytext[0];

						/*
						 * Likewise, if what we have left is two chars, and
						 * those match the tokens ">=", "<=", "=>", "<>" or
						 * "!=", then we must return the appropriate token
						 * rather than the generic Op.
						 */
						if (nchars == 2)
						{
							if (yytext[0] == '=' && yytext[1] == '>')
								return EQUALS_GREATER;
							if (yytext[0] == '>' && yytext[1] == '=')
								return GREATER_EQUALS;
							if (yytext[0] == '<' && yytext[1] == '=')
								return LESS_EQUALS;
							if (yytext[0] == '<' && yytext[1] == '>')
								return NOT_EQUALS;
							if (yytext[0] == '!' && yytext[1] == '=')
								return NOT_EQUALS;
						}
					}

					base_yylval.str = mm_strdup(yytext);
					return Op;
				}

{param}			{
					int			val;

					errno = 0;
					val = strtoint(yytext + 1, NULL, 10);
					if (errno == ERANGE)
						mmfatal(PARSE_ERROR, "parameter number too large");
					base_yylval.ival = val;
					return PARAM;
				}
{param_junk}	{
					mmfatal(PARSE_ERROR, "trailing junk after parameter");
				}

}  /* <SQL> */

<C,SQL>{
{decinteger}	{
					return process_integer_literal(yytext, &base_yylval, 10);
				}
{hexinteger}	{
					return process_integer_literal(yytext, &base_yylval, 16);
				}
{numeric}		{
					base_yylval.str = mm_strdup(yytext);
					return FCONST;
				}
{numericfail}	{
					/* throw back the .., and treat as integer */
					yyless(yyleng - 2);
					return process_integer_literal(yytext, &base_yylval, 10);
				}
{real}			{
					base_yylval.str = mm_strdup(yytext);
					return FCONST;
				}
{realfail}		{
					/*
					 * throw back the [Ee][+-], and figure out whether what
					 * remains is an {decinteger} or {numeric}.
					 */
					yyless(yyleng - 2);
					return process_integer_literal(yytext, &base_yylval, 10);
				}
} /* <C,SQL> */

<SQL>{
{octinteger}	{
					return process_integer_literal(yytext, &base_yylval, 8);
				}
{bininteger}	{
					return process_integer_literal(yytext, &base_yylval, 2);
				}

	/*
	 * Note that some trailing junk is valid in C (such as 100LL), so we
	 * contain this to SQL mode.
	 */
{decinteger_junk} {
					mmfatal(PARSE_ERROR, "trailing junk after numeric literal");
				}
{hexinteger_junk} {
					mmfatal(PARSE_ERROR, "trailing junk after numeric literal");
				}
{octinteger_junk} {
					mmfatal(PARSE_ERROR, "trailing junk after numeric literal");
				}
{bininteger_junk} {
					mmfatal(PARSE_ERROR, "trailing junk after numeric literal");
				}
{numeric_junk}	{
					mmfatal(PARSE_ERROR, "trailing junk after numeric literal");
				}
{real_junk}		{
					mmfatal(PARSE_ERROR, "trailing junk after numeric literal");
				}

{identifier}	{
					/*
					 * None of the above.  Return it as an identifier.
					 *
					 * The backend will attempt to truncate and case-fold
					 * the identifier, but I see no good reason for ecpg
					 * to do so; that's just another way that ecpg could
					 * get out of step with the backend.
					 */
					base_yylval.str = mm_strdup(yytext);
					return IDENT;
				}

{other}			{
					return yytext[0];
				}
} /* <SQL> */


<<EOF>>			{
					yyterminate();
				}

<INITIAL>{other}|\n {
					mmfatal(PARSE_ERROR, "internal error: unreachable state; please report this to <%s>", PACKAGE_BUGREPORT);
				}

%%

/* LCOV_EXCL_STOP */

void
lex_init(void)
{
	braces_open = 0;
	parenths_open = 0;
	current_function = NULL;

	yylineno = 1;

	/* initialize literal buffer to a reasonable but expansible size */
	if (literalbuf == NULL)
	{
		literalalloc = 1024;
		literalbuf = (char *) mm_alloc(literalalloc);
	}
	startlit();

	BEGIN(C);
}

static void
addlit(char *ytext, int yleng)
{
	/* enlarge buffer if needed */
	if ((literallen + yleng) >= literalalloc)
	{
		do
			literalalloc *= 2;
		while ((literallen + yleng) >= literalalloc);
		literalbuf = (char *) realloc(literalbuf, literalalloc);
	}
	/* append new data, add trailing null */
	memcpy(literalbuf + literallen, ytext, yleng);
	literallen += yleng;
	literalbuf[literallen] = '\0';
}

static void
addlitchar(unsigned char ychar)
{
	/* enlarge buffer if needed */
	if ((literallen + 1) >= literalalloc)
	{
		literalalloc *= 2;
		literalbuf = (char *) realloc(literalbuf, literalalloc);
	}
	/* append new data, add trailing null */
	literalbuf[literallen] = ychar;
	literallen += 1;
	literalbuf[literallen] = '\0';
}

/*
 * Process {decinteger}, {hexinteger}, etc.  Note this will also do the right
 * thing with {numeric}, ie digits and a decimal point.
 */
static int
process_integer_literal(const char *token, YYSTYPE *lval, int base)
{
	int			val;
	char	   *endptr;

	errno = 0;
	val = strtoint(base == 10 ? token : token + 2, &endptr, base);
	if (*endptr != '\0' || errno == ERANGE)
	{
		/* integer too large (or contains decimal pt), treat it as a float */
		lval->str = mm_strdup(token);
		return FCONST;
	}
	lval->ival = val;
	return ICONST;
}
