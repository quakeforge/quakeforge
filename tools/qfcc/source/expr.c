#include <stdlib.h>

#include <QF/mathlib.h>

#include "qfcc.h"
#include "expr.h"
#include "scope.h"
#include "qc-parse.h"

void yyerror (const char*);

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

static etype_t
get_type (expr_t *e)
{
	switch (e->type) {
		case ex_quaternion: //FIXME
			return ev_void;
		case ex_expr:
		case ex_uexpr:
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
new_expr (void)
{
	return calloc (1, sizeof (expr_t));
}

expr_t *
label_expr (void)
{
	static int label = 0;

	expr_t *l = new_expr ();
	l->type = ex_uexpr;
	l->e.expr.op = 'l';
	l->e.expr.e1 = new_expr ();
	l->e.expr.e1->type = ex_int;
	l->e.expr.e1->e.int_val = label++;
	return l;
}

void
print_expr (expr_t *e)
{
	printf (" ");
	if (!e) {
		printf ("(nil)");
		return;
	}
	switch (e->type) {
		case ex_expr:
			print_expr (e->e.expr.e1);
			if (e->e.expr.op == 'c') {
				expr_t *p = e->e.expr.e2;
				printf ("(");
				while (p) {
					print_expr (p);
					if (p->next)
						printf (",");
					p = p->next;
				}
				printf (")");
			} else {
				print_expr (e->e.expr.e2);
				if (isprint (e->e.expr.op)) {
					printf (" %c", e->e.expr.op);
				} else {
					printf (" %d", e->e.expr.op);
				}
			}
			break;
		case ex_uexpr:
			print_expr (e->e.expr.e1);
			if (isprint (e->e.expr.op)) {
				printf (" u%c", e->e.expr.op);
			} else {
				printf (" u%d", e->e.expr.op);
			}
			break;
		case ex_def:
			printf ("%s", e->e.def->name);
			break;
		case ex_int:
			printf ("%d", e->e.int_val);
			break;
		case ex_float:
			printf ("%g", e->e.float_val);
			break;
		case ex_string:
			printf ("\"%s\"", strings + e->e.string_val);
			break;
		case ex_vector:
			printf ("'%g", e->e.vector_val[0]);
			printf ( " %g", e->e.vector_val[1]);
			printf ( " %g'", e->e.vector_val[2]);
			break;
		case ex_quaternion:
			printf ("'%g", e->e.quaternion_val[0]);
			printf (" %g", e->e.quaternion_val[1]);
			printf (" %g", e->e.quaternion_val[2]);
			printf (" %g'", e->e.quaternion_val[3]);
			break;
	}
}

static expr_t *
do_op_string (int op, expr_t *e1, expr_t *e2)
{
	int len;
	char *buf;
	char *s1, *s2;

	s1 = strings + e1->e.string_val;
	s2 = strings + e2->e.string_val;
	
	switch (op) {
		case '+':
			len = strlen (s1) + strlen (s2) + 1;
			buf = alloca (len);
			strcpy (buf, s1);
			strcat (buf, s2);
			e1->e.string_val = ReuseString (buf);
			break;
		case LT:
			e1->type = ex_int;
			e1->e.int_val = strcmp (s1, s2) < 0;
			break;
		case GT:
			e1->type = ex_int;
			e1->e.int_val = strcmp (s1, s2) > 0;
			break;
		case LE:
			e1->type = ex_int;
			e1->e.int_val = strcmp (s1, s2) <= 0;
			break;
		case GE:
			e1->type = ex_int;
			e1->e.int_val = strcmp (s1, s2) >= 0;
			break;
		case EQ:
			e1->type = ex_int;
			e1->e.int_val = strcmp (s1, s2) == 0;
			break;
		case NE:
			e1->type = ex_int;
			e1->e.int_val = strcmp (s1, s2) != 0;
			break;
		default:
			yyerror ("invalid operand for string");
			return 0;
	}
	return e1;
}

static expr_t *
do_op_float (int op, expr_t *e1, expr_t *e2)
{
	float f1, f2;

	f1 = e1->e.float_val;
	f2 = e2->e.float_val;
	
	switch (op) {
		case '+':
			e1->e.float_val += f2;
			break;
		case '-':
			e1->e.float_val -= f2;
			break;
		case '*':
			e1->e.float_val *= f2;
			break;
		case '/':
			e1->e.float_val /= f2;
			break;
		case '&':
			e1->e.float_val = (int)f1 & (int)f2;
			break;
		case '|':
			e1->e.float_val += (int)f1 | (int)f2;
			break;
		case AND:
			e1->e.float_val = f1 && f2;
			break;
		case OR:
			e1->e.float_val += f1 || f2;
			break;
		case LT:
			e1->type = ex_int;
			e1->e.int_val = f1 < f2;
			break;
		case GT:
			e1->type = ex_int;
			e1->e.int_val = f1 > f2;
			break;
		case LE:
			e1->type = ex_int;
			e1->e.int_val = f1 <= f2;
			break;
		case GE:
			e1->type = ex_int;
			e1->e.int_val = f1 >= f2;
			break;
		case EQ:
			e1->type = ex_int;
			e1->e.int_val = f1 == f2;
			break;
		case NE:
			e1->type = ex_int;
			e1->e.int_val = f1 != f2;
			break;
		default:
			yyerror ("invalid operand for string");
			return 0;
	}
	return e1;
}

static expr_t *
do_op_vector (int op, expr_t *e1, expr_t *e2)
{
	float *v1, *v2;

	v1 = e1->e.vector_val;
	v2 = e2->e.vector_val;
	
	switch (op) {
		case '+':
			VectorAdd (v1, v2, v1);
			break;
		case '-':
			VectorSubtract (v1, v2, v1);
			break;
		case '*':
			e1->type = ex_float;
			e1->e.float_val = DotProduct (v1, v2);
			break;
		case EQ:
			e1->type = ex_int;
			e1->e.int_val = (v1[0] == v2[0])
							&& (v1[1] == v2[1])
							&& (v1[2] == v2[2]);
			break;
		case NE:
			e1->type = ex_int;
			e1->e.int_val = (v1[0] == v2[0])
							|| (v1[1] != v2[1])
							|| (v1[2] != v2[2]);
			break;
		default:
			yyerror ("invalid operand for string");
			return 0;
	}
	return e1;
}

static expr_t *
do_op_huh (int op, expr_t *e1, expr_t *e2)
{
	yyerror ("funny constant");
	return 0;
}

static expr_t *(*do_op[]) (int op, expr_t *e1, expr_t *e2) = {
	do_op_huh,
	do_op_string,
	do_op_float,
	do_op_vector,
	do_op_huh,
	do_op_huh,
	do_op_huh,
	do_op_huh,
};

static expr_t *
binary_const (int op, expr_t *e1, expr_t *e2)
{
	etype_t t1, t2;
	//expr_t *e;

	t1 = get_type (e1);
	t2 = get_type (e2);

	if (t1 == t2) {
		return do_op[t1](op, e1, e2);
	} else {
		yyerror ("type missmatch for");
		fprintf (stderr, "%d (%c)\n", op, (op > ' ' && op < 127) ? op : ' ');
		return 0;
	}
	return 0;
}

static expr_t *
field_expr (expr_t *e1, expr_t *e2)
{
	etype_t t1, t2;
	expr_t *e;

	t1 = get_type (e1);
	t2 = get_type (e2);

	if (t1 != ev_entity && t2 != ev_field) {
		yyerror ("type missmatch for .");
		return 0;
	}

	e = new_expr ();
	e->type = ex_expr;
	e->e.expr.op = '.';
	e->e.expr.type = (e2->type == ex_def)
					 ? e2->e.def->type->aux_type
					 : e2->e.expr.type;
	e->e.expr.e1 = e1;
	e->e.expr.e2 = e2;
	return e;
}

expr_t *
binary_expr (int op, expr_t *e1, expr_t *e2)
{
	etype_t t1, t2;

	if (op == '.')
		return field_expr (e1, e2);

	if (e1->type >= ex_int && e2->type >= ex_int)
		return binary_const (op, e1, e2);

	t1 = get_type (e1);
	t2 = get_type (e2);
	if (t1 == ev_void || t2 == ev_void) {
		yyerror ("internal error");
		abort ();
	}

	if (t1 == t2) {
		expr_t *e = new_expr ();
		e->type = ex_expr;
		e->e.expr.op = op;
		if (op >= OR && op <= GT)
			e->e.expr.type = &type_float;
		else
			e->e.expr.type = types[t1];
		e->e.expr.e1 = e1;
		e->e.expr.e2 = e2;
		return e;
	} else {
		switch (t1) {
			case ev_float:
				if (t2 == ev_vector) {
					expr_t *e = new_expr ();
					e->type = ex_expr;
					e->e.expr.op = op;
					e->e.expr.type = &type_vector;
					e->e.expr.e1 = e1;
					e->e.expr.e2 = e2;
					return e;
				} else {
					goto type_mismatch;
				}
			case ev_vector:
				if (t2 == ev_float) {
					expr_t *e = new_expr ();
					e->type = ex_expr;
					e->e.expr.op = op;
					e->e.expr.type = &type_vector;
					e->e.expr.e1 = e1;
					e->e.expr.e2 = e2;
					return e;
				} else {
					goto type_mismatch;
				}
			case ev_field:
				if (e1->e.expr.type->aux_type->type == t2) {
					expr_t *e = new_expr ();
					e->type = ex_expr;
					e->e.expr.op = op;
					e->e.expr.type = e->e.expr.type->aux_type;
					e->e.expr.e1 = e1;
					e->e.expr.e2 = e2;
					return e;
				} else {
					goto type_mismatch;
				}
			default:
type_mismatch:
				yyerror ("type missmatch");
				fprintf (stderr, "%d %d\n", t1, t2);
				return 0;
		}
	}
}

expr_t *
unary_expr (int op, expr_t *e)
{
	switch (op) {
		case '-':
			switch (e->type) {
				case ex_uexpr:
					if (e->e.expr.op == '-')
						return e->e.expr.e1;
				case ex_expr:
				case ex_def:
					{
						expr_t *n = new_expr ();
						n->type = ex_uexpr;
						n->e.expr.op = op;
						n->e.expr.type = (e->type == ex_def)
										 ? e->e.def->type
										 : e->e.expr.type;
						n->e.expr.e1 = e;
						return n;
					}
				case ex_int:
					e->e.int_val *= -1;
					return e;
				case ex_float:
					e->e.float_val *= -1;
					return e;
				case ex_string:
					return 0;		// FIXME
				case ex_vector:
					e->e.vector_val[0] *= -1;
					e->e.vector_val[1] *= -1;
					e->e.vector_val[2] *= -1;
					return e;
				case ex_quaternion:
					e->e.quaternion_val[0] *= -1;
					e->e.quaternion_val[1] *= -1;
					e->e.quaternion_val[2] *= -1;
					e->e.quaternion_val[3] *= -1;
					return e;
			}
			break;
		case '!':
			switch (e->type) {
				case ex_uexpr:
				case ex_expr:
				case ex_def:
					{
						expr_t *n = new_expr ();
						n->type = ex_uexpr;
						n->e.expr.op = op;
						n->e.expr.type = &type_float;
						n->e.expr.e1 = e;
						return n;
					}
				case ex_int:
					e->e.int_val = !e->e.int_val;
					return e;
				case ex_float:
					e->e.int_val = !e->e.float_val;
					e->type = ex_int;
					return e;
				case ex_string:
					e->e.int_val = !e->e.string_val
									|| !G_STRING(e->e.string_val)[0];
					e->type = ex_int;
					return e;
				case ex_vector:
					e->e.int_val = !e->e.vector_val[0]
									&& !e->e.vector_val[1]
									&& !e->e.vector_val[2];
					e->type = ex_int;
					return e;
				case ex_quaternion:
					e->e.int_val = !e->e.quaternion_val[0]
									&& !e->e.quaternion_val[1]
									&& !e->e.quaternion_val[2]
									&& !e->e.quaternion_val[3];
					e->type = ex_int;
					return e;
			}
			break;
		default:
			abort ();
	}
	yyerror ("internal error");
	abort ();
}

expr_t *
function_expr (expr_t *e1, expr_t *e2)
{
	etype_t t1;
	expr_t *e;
	int parm_count = 0;
	type_t *ftype;

	t1 = get_type (e1);

	if (t1 != ev_func) {
		yyerror ("called object is not a function");
		fprintf (stderr, "%d\n", t1);
		return 0;
	}

	ftype = e1->type == ex_def
			? e1->e.def->type
			: e1->e.expr.type;

	for (e = e2; e; e = e->next)
		parm_count++;
	if (parm_count > 8) {
		yyerror ("more than 8 paramters");
		return 0;
	}
	if (ftype->num_parms != -1) {
		if (parm_count > ftype->num_parms) {
			yyerror ("too many arguments");
			return 0;
		} else if (parm_count < ftype->num_parms) {
			yyerror ("too few arguments");
			return 0;
		}
	}
	e = new_expr ();
	e->type = ex_expr;
	e->e.expr.op = 'c';
	e->e.expr.type = ftype->aux_type;
	e->e.expr.e1 = e1;
	e->e.expr.e2 = e2;
	return e;
}
