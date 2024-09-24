/*-------------------------------------------------------------------------
 *
 * rpr_prefer.c
 *    TODO
 *
 * Copyright (c) 2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *    src/test/modules/rpr/rpr_prefer.c
 *
 * TODO
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>
#include <getopt_long.h>
#include <limits.h>

#include "preproc_extern.h"

/* Kept for calling during a debugger session. */
void pretty_print(Node *parsed);

typedef List PL;
typedef List IDStr;

List *
list_make1(void *datum)
{
	List	   *list = palloc(sizeof(List));
	list->type = T_List;
	list->node = (Node *) datum;
	list->next = NIL;
	list->last = list;
	return list;
}

List *
lappend(List *list, void *datum)
{
	if (list == NIL)
		return list_make1(datum);

	list->last->next = list_make1(datum);
	list->last = list->last->next;

	return list;
}

List *
list_concat(List *list1, List *list2)
{
	if (list1 == NIL)
		return list2;
	else if (list2 == NIL)
		return list1;

	list1->last->next = list2;
	list1->last = list2->last;

	return list1;
}

static List *
list_copy(const List *oldlist)
{
	List	   *l = NIL;

	if (oldlist == NIL)
		return NIL;

	while (oldlist)
	{
		l = lappend(l, oldlist->node);
		oldlist = oldlist->next;
	}

	return l;
}

Node *
newNode(size_t size, NodeTag tag)
{
	Node	   *result;

	result = (Node *) palloc0(size);
	result->type = tag;

	return result;
}

String *
makeString(char *str)
{
	String	   *v = makeNode(String);

	v->sval = str;
	return v;
}

#define intValue(M) (M)->val.ival.ival

int			ret_value;

void
pretty_print(Node *parsed)
{
	if (!parsed)
	{
		printf("( )");
		return;
	}

	switch (parsed->type)
	{
		case T_List:
			List *l = (List *) parsed;

			printf("( ");
			while (l)
			{
				pretty_print(l->node);
				l = l->next;
				printf(" ");
			}
			printf(")");

			break;

		case T_RowPatternAlternation:
			RowPatternAlternation *a = (RowPatternAlternation *) parsed;

			pretty_print((Node *) a->left);
			printf(" | ");
			pretty_print((Node *) a->right);

			break;

		case T_RowPatternExclusion:
			RowPatternExclusion *e = (RowPatternExclusion *) parsed;

			printf("{- ");
			pretty_print(e->pattern);
			printf(" -}");

			break;

		case T_String:
			String *s = (String *) parsed;
			printf("%s", s->sval);
			break;

		case T_RowPatternFactor:
			RowPatternFactor *f = (RowPatternFactor *) parsed;
			pretty_print(f->primary);

			printf("{");
			if (f->quantifier->min)
				printf("%d", f->quantifier->min->val.ival.ival);
			printf(",");
			if (f->quantifier->max)
				printf("%d", f->quantifier->max->val.ival.ival);
			printf("}");

			break;

		case T_RowPatternPermutation:
			RowPatternPermutation *p = (RowPatternPermutation *) parsed;

			printf("PERMUTE(");
			for (List *pl = p->patterns; pl; pl = pl->next)
			{
				pretty_print(pl->node);
				if (pl->next)
					printf(", ");
			}
			printf(")");

			break;

		default:
			mmfatal(ET_ERROR, "pretty_print: unknown type %d", parsed->type);
	}
}

static const char *special_symbols = "()[]$^-";

static int
num_variables(IDStr *id_str)
{
	int			count = 0;

	for (; id_str; id_str = id_str->next)
	{
		String	   *s = (String *) id_str->node;
		const char *str = s->sval;

		Assert(s->type == T_String);

		if (strlen(str) != 1)
			count++;
		else if (!strchr(special_symbols, str[0]))
			count++;
	}

	return count;
}

static bool
has_variable(IDStr *id_str)
{
	for (; id_str; id_str = id_str->next)
	{
		String	   *s = (String *) id_str->node;
		const char *str = s->sval;

		Assert(s->type == T_String);

		if (strlen(str) != 1)
			return true;
		if (!strchr(special_symbols, str[0]))
			return true;
	}

	return false;
}

static void
expand_worker(PL **result, IDStr *prefix, PL *terms, int remaining, bool reluctant, int max_rows)
{
	if (remaining == 0)
	{
		/*
		 * Base case. Expand the provided prefix with terms, allowing the
		 * final term to be empty.
		 */
		for (PL *q = terms; q; q = q->next)
		{
			IDStr	   *qs = (IDStr *) q->node;
			IDStr	   *paren;

			paren = list_make1(makeString("("));
			paren = list_concat(paren, list_copy(prefix));
			paren = list_concat(paren, list_copy(qs));
			paren = lappend(paren, makeString(")"));

			*result = lappend(*result, paren);
		}

		return;
	}

	/*
	 * Order here depends on whether the quantifier is greedy or reluctant --
	 * for the greedy case, superstrings sort before their substrings, and
	 * vice-versa for the reluctant case. Crucially, this is not the same as
	 * sorting by length, which is why it's implemented recursively.
	 */

	for (PL *q = terms; q; q = q->next)
	{
		IDStr	   *qs = (IDStr *) q->node;
		IDStr	   *new;
		IDStr	   *paren;

		new = list_copy(prefix);
		new = list_concat(new, list_copy(qs));

		paren = list_make1(makeString("("));
		paren = list_concat(paren, list_copy(new));
		paren = lappend(paren, makeString(")"));

		if (reluctant)
			*result = lappend(*result, paren);

		/*
		 * Empty matches may not appear in the middle of the identifier string;
		 * skip expansion unless the term had a variable.
		 */
		if (has_variable(qs))
		{
			if (max_rows >= 0)
			{
				int			var_count = num_variables(new);

				if (var_count > max_rows)
					continue; /* impossible to match */
			}

			expand_worker(result, new, terms, remaining - 1, reluctant, max_rows);
		}

		if (!reluctant)
			*result = lappend(*result, paren);
	}
}

static PL *
expand_factor(PL *primary, RowPatternQuantifier *quant, int max_rows)
{
	PL		   *result = NIL;
	PL		   *prefixes;
	int			min = 0;
	int			max = 0;
	int			expansions;

	if (!quant->max && max_rows == -1)
		mmfatal(ET_ERROR, "infinite quantifiers not supported without --max-rows");

	if (quant->min)
		min = intValue(quant->min);
	if (quant->max)
	{
		max = intValue(quant->max);

		if (max == 0)
			mmfatal(ET_ERROR, "maximum must be greater than zero");
		if (max < min)
			mmfatal(ET_ERROR, "maximum may not be less than minimum");
	}

	/*
	 * Build the "prefix" set. All identifier strings that are returned must
	 * start with one of these. The list is generated in preferment order.
	 *
	 * By rule, an empty match -- a string that cannot advance the state
	 * machine, for which has_variable() will return false -- may only appear
	 * in the prefix set before the `min` index, or at the `max` index.
	 *
	 * For min of 0 or 1, our only prefix is the empty set.
	 */
	prefixes = list_make1(NIL);

	for (int i = 1; i < min; i++)
	{
		PL		   *acc = NIL;

		for (PL *p = prefixes; p; p = p->next)
		{
			for (PL *q = primary; q; q = q->next)
			{
				IDStr	   *ps = (IDStr *) p->node;
				IDStr	   *qs = (IDStr *) q->node;
				IDStr	   *new;

				new = list_copy(ps);
				new = list_concat(new, qs);

				acc = lappend(acc, new);
			}
		}

		prefixes = acc;
	}

	/* Now build the full PL. */
	if (max)
	{
		expansions = max - min;
		if (min == 0)
			expansions--; /* we handle the empty set case explicitly */
	}

	if (min == 0 && quant->reluctant)
	{
		IDStr	   *empty = list_make1(makeString("("));
		empty = lappend(empty, makeString(")"));

		result = lappend(result, empty);
	}

	for (PL *p = prefixes; p; p = p->next)
	{
		IDStr	   *ps = (IDStr *) p->node;

		if (max)
			expand_worker(&result, ps, primary, expansions, quant->reluctant, max_rows);
		else
		{
			int			var_count = num_variables(ps);

			if (var_count > max_rows)
				continue; /* impossible to match */

			expand_worker(&result, ps, primary, max_rows - var_count, quant->reluctant, max_rows);
		}
	}

	if (min == 0 && !quant->reluctant)
	{
		IDStr	   *empty = list_make1(makeString("("));
		empty = lappend(empty, makeString(")"));

		result = lappend(result, empty);
	}

	return result;
}

/*
 * Annotates the list with its original indices, so that we can track
 * lexicographic order in next_permutation().
 */
static void
start_permutation(List *sequence)
{
	int			i;

	for (i = 0; sequence; sequence = sequence->next)
		sequence->permutation_index = i++;
}

/*
 * Pandita's algorithm for lexicographic permutation, as described by Wikipedia
 * (apparently via Knuth's TAOCP), adapted for a linked list. Returns NULL when
 * there's nothing more to do.
 */
static List *
next_permutation(List *sequence)
{
	List	   *lk = NIL,
			   *ll = NIL,
			   *s;
	Node	   *tmp;
	int			i, itmp;

	Assert(sequence);

	/*
	 * Find the last node in the sequence that is correctly sorted compared to
	 * its very next node. Call this node lk.
	 */
	for (s = sequence, i = 0; s->next; s = s->next, i++)
	{
		if (s->permutation_index < s->next->permutation_index)
			lk = s;
	}

	if (!lk)
		return NULL; /* we're done; the sequence is fully reversed */

	/*
	 * Find the last node that is still correctly sorted compared with lk. We're
	 * guaranteed at least one (i.e. lk->next, due to the above check) but there
	 * may be more afterwards.
	 */
	for (s = lk->next; s; s = s->next)
	{
		if (lk->permutation_index < s->permutation_index)
			ll = s;
	}

	Assert(ll);

	/* Swap the values of the two nodes we've found. */
	tmp = lk->node;
	lk->node = ll->node;
	ll->node = tmp;

	itmp = lk->permutation_index;
	lk->permutation_index = ll->permutation_index;
	ll->permutation_index = itmp;

	/*
	 * Reverse the order of nodes from index k+1 (that is, lk->next) to the end
	 * of the list.
	 */
	{
		List	   *prev = NIL;

		s = sequence->last = lk->next;

		do
		{
			List	   *ltmp;

			ltmp = s->next;
			s->next = prev;
			prev = s;
			s = ltmp;
		} while (s);

		lk->next = prev;
	}

	return sequence;
}

static PL *
parenthesized_language(Node *n, int max_rows)
{
	PL		   *result = NIL;
	IDStr	   *str = NIL;

	if (!n)
		return list_make1(NIL);

	switch (n->type)
	{
		case T_String:
			str = list_make1(n);
			result = lappend(result, str);
			break;

		case T_List:
			List	   *l = (List *) n;

			result = parenthesized_language(l->node, max_rows);

			l = l->next;
			if (l)
			{
				PL		   *acc = NIL;
				PL		   *left = result;
				PL		   *pl = parenthesized_language(l->node, max_rows);

				while (left)
				{
					PL		   *right = pl;

					while (right)
					{
						IDStr	   *row = NIL;

						row = lappend(row, makeString("("));
						row = list_concat(row, list_copy((List *) left->node));
						row = list_concat(row, list_copy((List *) right->node));
						row = lappend(row, makeString(")"));

						acc = lappend(acc, row);
						right = right->next;
					}

					left = left->next;
				}

				result = acc;
				Assert(!l->next);
			}
			else
			{
				PL		   *acc = NIL;

				for (PL *p = result; p; p = p->next)
				{
					IDStr	   *s = (IDStr *) p->node;
					IDStr	   *row = NIL;

					row = lappend(row, makeString("("));
					row = list_concat(row, list_copy(s));
					row = lappend(row, makeString(")"));

					acc = lappend(acc, row);
				}

				result = acc;
			}

			break;

		case T_RowPatternAlternation:
			RowPatternAlternation *a = (RowPatternAlternation *) n;
			PL		   *left = parenthesized_language((Node *) a->left, max_rows);
			PL		   *right = parenthesized_language((Node *) a->right, max_rows);

			while (left)
			{
				str = list_make1(makeString("("));
				str = list_concat(str, (List *) left->node);
				str = lappend(str, makeString("-"));
				str = lappend(str, makeString(")"));

				result = lappend(result, str);
				left = left->next;
			}

			while (right)
			{
				str = list_make1(makeString("("));
				str = lappend(str, makeString("-"));
				str = list_concat(str, list_copy((List *) right->node));
				str = lappend(str, makeString(")"));

				result = lappend(result, str);
				right = right->next;
			}

			break;

		case T_RowPatternExclusion:
			RowPatternExclusion *e = (RowPatternExclusion *) n;
			PL		   *excluded = parenthesized_language(e->pattern, max_rows);

			for (PL *p = excluded; p; p = p->next)
			{
				str = list_make1(makeString("["));
				str = list_concat(str, list_copy((List *) p->node));
				str = lappend(str, makeString("]"));

				result = lappend(result, str);
			}

			break;

		case T_RowPatternFactor:
			RowPatternFactor *f = (RowPatternFactor *) n;

			result = parenthesized_language(f->primary, max_rows);
			result = expand_factor(result, f->quantifier, max_rows);

			break;

		case T_RowPatternPermutation:
			RowPatternPermutation *p = (RowPatternPermutation *) n;
			List		   *patterns = p->patterns;
			Node		   *translated = NULL;

			/*
			 * Per spec, a PERMUTE(STR1, STR2, ..., STRn ) is equivalent to
			 *
			 *   ( ( STRx1 STRx2 ... STRxn )    \
			 *   | ( STRy1 STRy2 ... STRyn )     )  n! terms total
			 *   | ...                 )        /
			 *
			 * where there's one term for each permutation of the original set
			 * of STRn, ordered lexicographically. So for example
			 *
			 *   PERMUTE(a) -> ( ( a ) )
			 *   PERMUTE(a, b, c) -> ( ( a b c ) | ( a c b ) | ( b a c )
			 *                       | ( b c a ) | ( c a b ) | ( c b a ) )
			 */

			Assert(patterns); /* an empty list is prohibited by the grammar */

			/*
			 * The first term is just a parenthesized concatenation of STRn in
			 * the originally provided order. Then we continue to permute the
			 * patterns.
			 */
			start_permutation(patterns);

			do
			{
				Node		   *current = patterns->node;

				/*
				 * Concatenate all the patterns into a single term, using the
				 * same tree layout generated by the parser.
				 */
				for (List *p = patterns->next; p; p = p->next)
				{
					current = (Node *) list_make1(current);
					current = (Node *) lappend((List *) current, p->node);
				}

				/* Parenthesize. */
				current = (Node *) list_make1(current);

				if (!translated)
					translated = current;
				else
				{
					/* Tack on another alternation. */
					RowPatternAlternation *al = makeNode(RowPatternAlternation);

					al->left = translated;
					al->right = current;

					translated = (Node *) al;
				}
			} while ((patterns = next_permutation(patterns)) != NULL);

			/* Parenthesize the whole alternation. */
			translated = (Node *) list_make1(translated);

			/*
			 * Now that we have an equivalent parse tree, transform it into the
			 * PL.
			 */
			result = parenthesized_language(translated, max_rows);
			break;

		default:
			mmfatal(ET_ERROR, "parenthesized_language: unknown type %d", n->type);
	}

	return result;
}

static void
usage_and_exit(int ec)
{
	fprintf(stderr, "usage: rpr_prefer [--max-rows M] [PATTERN]\n");
	exit(ec);
}

int
main(int argc, char *argv[])
{
	PL		   *pl;

	int			max_rows = -1;
	int			c;
	char	   *cmdl_pattern = NULL;
	static const struct option opts[] =
	{
		{ "max-rows", required_argument, 0, 'm' },
		{ 0 },
	};

	while ((c = getopt_long(argc, argv, "m:", opts, NULL)) >= 0)
	{
		switch (c)
		{
			case 'm':
				max_rows = atoi(optarg);
				break;

			default:
				usage_and_exit(1);
		}
	}

	if (optind < argc - 1)
	{
		/* Too many non-option arguments. */
		usage_and_exit(1);
	}
	else if (optind == argc - 1)
	{
		/* Pattern given on the command line. */
		cmdl_pattern = argv[optind];
	}

	lex_init(cmdl_pattern);
	if (base_yyparse())
		return 1;

	pl = parenthesized_language(parsed_pattern, max_rows);
	for (; pl; pl = pl->next)
	{
		IDStr	   *id_str = (List *) pl->node;

		if (max_rows >= 0 && num_variables(id_str) > max_rows)
			continue;

		while (id_str)
		{
			String	   *s = (String *) id_str->node;
			printf("%s ", s->sval);
			id_str = id_str->next;
		}

		printf("\n");
	}

	return 0;
}
