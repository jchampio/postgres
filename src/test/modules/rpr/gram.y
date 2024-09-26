/* header */
/* src/interfaces/ecpg/preproc/ecpg.header */

/* Copyright comment */
%{
#include "postgres_fe.h"

#include "preproc_extern.h"
#include "gram.h"
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

%}

%expect 0
%name-prefix="base_yy"
%locations

/* tokens */
%token CSTRING

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

%type <str> unreserved_keyword

%type <node>	row_pattern opt_row_pattern row_pattern_term
				row_pattern_alternation row_pattern_factor row_pattern_primary
				opt_quantifier opt_iconst
%type <list>	permute_list
%type <boolean>	opt_reluctant

/* orig_tokens */
 %token IDENT UIDENT FCONST SCONST USCONST BCONST XCONST Op
 %token ICONST PARAM
 %token TYPECAST DOT_DOT COLON_EQUALS EQUALS_GREATER
 %token LEFT_BRACE_MINUS RIGHT_MINUS_BRACE
 %token LESS_EQUALS GREATER_EQUALS NOT_EQUALS

 %token PERMUTE

 %nonassoc '<' '>' '=' LESS_EQUALS GREATER_EQUALS NOT_EQUALS

 %nonassoc IDENT
%nonassoc CSTRING PERMUTE
 %left Op OPERATOR
 %left '+' '-'
 %left '*' '/' '%'
 %left '^'

 %right UMINUS
 %left '[' ']'
 %left '(' ')'
 %left '.'

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
			| PERMUTE '(' permute_list ')'
				{
					RowPatternPermutation *p = makeNode(RowPatternPermutation);
					p->patterns = $3;
					$$ = (Node *) p;
				}
		;

opt_row_pattern:
			row_pattern								{ $$ = $1; }
			| /* EMPTY */							{ $$ = NULL; }
		;

permute_list:
			row_pattern								{ $$ = list_make1($1); }
			| permute_list ',' row_pattern			{ $$ = lappend($1, $3); }
		;


 Iconst:
 ICONST
	{
		$$ = $1;
	}
;


/* trailer */
/* src/interfaces/ecpg/preproc/ecpg.trailer */


/*
 * Name classification hierarchy.
 */

/* Column identifier --- names that can be column, table, etc names.
 */
ColId: IDENT							{ $$ = $1; }
	| unreserved_keyword				{ $$ = $1; }
	;

/* "Unreserved" keywords --- available for use as any kind of name.
 */

unreserved_keyword:
 PERMUTE
 { 
 $$ = mm_strdup("permute");
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
