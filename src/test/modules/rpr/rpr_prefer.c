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
#include <limits.h>

#include "preproc_extern.h"

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

static List *
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

static void
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

		default:
			mmfatal(ET_ERROR, "pretty_print: unknown type %d", parsed->type);
	}
}

static bool
has_variable(List *id_str)
{
	static const char *special_symbols = "()[]$^-";

	while (id_str)
	{
		String	   *s = (String *) id_str->node;
		const char *str = s->sval;

		Assert(s->type == T_String);

		if (strlen(str) != 1)
			return true;
		if (!strchr(special_symbols, str[0]))
			return true;

		id_str = id_str->next;
	}

	return false;
}

static void
expand_worker(PL **result, IDStr *prefix, PL *terms, int remaining, bool reluctant)
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
			expand_worker(result, new, terms, remaining - 1, reluctant);

		if (!reluctant)
			*result = lappend(*result, paren);
	}
}

static PL *
expand_factor(PL *primary, RowPatternQuantifier *quant)
{
	PL		   *result = NIL;
	PL		   *prefixes;
	int			min = 0;
	int			max;
	int			expansions;

	if (!quant->max)
		mmfatal(ET_ERROR, "infinite quantifiers not yet supported");

	if (quant->min)
		min = intValue(quant->min);
	if (quant->max)
		max = intValue(quant->max);

	if (max == 0)
		mmfatal(ET_ERROR, "maximum must be greater than zero");
	if (max < min)
		mmfatal(ET_ERROR, "maximum may not be less than minimum");

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
	expansions = max - min;
	if (min == 0)
		expansions--; /* we handle the empty set case explicitly */

	for (PL *p = prefixes; p; p = p->next)
	{
		IDStr	   *ps = (IDStr *) p->node;

		if (min == 0 && quant->reluctant)
		{
			IDStr	   *empty = list_make1(makeString("("));
			empty = lappend(empty, makeString(")"));

			result = lappend(result, empty);
		}

		expand_worker(&result, ps, primary, expansions, quant->reluctant);

		if (min == 0 && !quant->reluctant)
		{
			IDStr	   *empty = list_make1(makeString("("));
			empty = lappend(empty, makeString(")"));

			result = lappend(result, empty);
		}
	}

	return result;
}

static PL *
parenthesized_language(Node *n)
{
	PL		   *result = NIL;
	IDStr	   *str = NIL;

	if (!n)
	{
		str = lappend(str, makeString("("));
		str = lappend(str, makeString(")"));
		result = lappend(result, str);

		return result;
	}

	switch (n->type)
	{
		case T_String:
			str = list_make1(n);
			result = lappend(result, str);
			break;

		case T_List:
			List	   *l = (List *) n;

			result = parenthesized_language(l->node);
			l = l->next;

			while (l)
			{
				PL		   *acc = NIL;
				PL		   *left = result;
				PL		   *pl = parenthesized_language(l->node);

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
				l = l->next;
			}

			break;

		case T_RowPatternAlternation:
			RowPatternAlternation *a = (RowPatternAlternation *) n;
			PL		   *left = parenthesized_language((Node *) a->left);
			PL		   *right = parenthesized_language((Node *) a->right);

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

		case T_RowPatternFactor:
			RowPatternFactor *f = (RowPatternFactor *) n;

			result = parenthesized_language(f->primary);

			if (!f->quantifier->min
				|| intValue(f->quantifier->min) != 1
				|| !f->quantifier->max
				|| intValue(f->quantifier->max) != 1)
				result = expand_factor(result, f->quantifier);

			break;

		default:
			mmfatal(ET_ERROR, "parenthesized_language: unknown type %d", n->type);
	}

	return result;
}

int
main()
{
	PL	   *pl;

	lex_init();
	if (base_yyparse())
		return 1;

	pretty_print((Node *) parsed);
	printf("\n\n");

	pl = parenthesized_language((Node *) parsed);
	while (pl)
	{
		IDStr	   *id_str = (List *) pl->node;

		while (id_str)
		{
			String	   *s = (String *) id_str->node;
			printf("%s ", s->sval);
			id_str = id_str->next;
		}

		printf("\n");
		pl = pl->next;
	}

	return 0;
}
