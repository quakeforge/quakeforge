#include <stdlib.h>

#include "qfcc.h"
#include "expr.h"

static etype_t qc_types[] = {
	ev_void,
	ev_void,
	ev_void,
	ev_void,	// FIXME ex_int
	ev_float,	// ex_float
	ev_string,	// ex_string
	ev_vector,	// ex_vector
	ev_void,	// FIXME ex_quaternion
};

static type_t *types[] = {
	&type_void,
	&type_string,
	&type_float,
	&type_vector,
	&type_entity,
	&type_field,
	&type_function,
	&type_pointer,
};

expr_t *
new_expr ()
{
	return calloc (1, sizeof (expr_t));
}

static expr_t *
binary_const (int op, expr_t *e1, expr_t *e2)
{
	return 0;
}

static etype_t
get_type (expr_t *e)
{
	switch (e->type) {
		case ex_statement:
		case ex_quaternion: //FIXME
			return ev_void;
		case ex_expr:
			return e->e.expr.type->type;
		case ex_def:
			return e->e.def->type->type;
		case ex_int: //FIXME?
			e->type = ex_float;
			e->e.float_val = e->e.int_val;
			return ev_float;
		case ex_float:
		case ex_string:
		case ex_vector:
		//FIXME case ex_quaternion:
			return qc_types[e->type];
	}
	return ev_void;
}

expr_t *
binary_expr (int op, expr_t *e1, expr_t *e2)
{
	etype_t t1, t2;

	if (e1->type >= ex_int && e2->type >= ex_int)
		return binary_const (op, e1, e2);

	t1 = get_type (e1);
	t2 = get_type (e2);
	if (t1 == ev_void || t2 == ev_void)
		return 0;

	if (t1 == t2) {
		expr_t *e = new_expr ();
		e->type = ex_expr;
		e->e.expr.op = op;
		e->e.expr.type = types[t1];
		e->e.expr.e1 = e1;
		e->e.expr.e2 = e2;
		return e;
	} else {
		return 0;
	}
}

expr_t *
unary_expr (int op, expr_t *e)
{
	return 0;
}
