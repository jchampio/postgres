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

#if 0
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
#endif

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
				PL		   *right = parenthesized_language(l->node);

				while (left)
				{
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

		case T_RowPatternFactor:
			RowPatternFactor *f = (RowPatternFactor *) n;

			if ((!f->quantifier->min || intValue(f->quantifier->min) != 1)
				|| (!f->quantifier->max || intValue(f->quantifier->max) != 1))
				mmfatal(ET_ERROR, "quantifiers other than 1 not yet supported");

			result = parenthesized_language(f->primary);
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
