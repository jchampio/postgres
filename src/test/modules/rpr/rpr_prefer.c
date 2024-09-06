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

int			ret_value;

static void
pretty_print(Node *parsed)
{
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
			mmfatal(ET_ERROR, "unknown type %d", parsed->type);
	}
}

int
main()
{
	lex_init();
	if (base_yyparse())
		return 1;

	pretty_print((Node *) parsed);
	printf("\n");

	return 0;
}
