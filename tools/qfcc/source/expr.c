#include <stdlib.h>

#include <QF/mathlib.h>
#include <QF/va.h>

#include "qfcc.h"
#include "scope.h"
#include "qc-parse.h"

void yyerror (const char*);
extern function_t *current_func;

static etype_t qc_types[] = {
	ev_void,	// ex_label
	ev_void,	// ex_block
	ev_void,	// ex_expr
	ev_void,	// ex_uexpr
	ev_void,	// ex_def
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

static expr_type expr_types[] = {
	ex_label,		// ev_void (ick)
	ex_string,		// ev_string
	ex_float,		// ev_float
	ex_vector,		// ev_vector
	ex_label,		// ev_entity (ick)
	ex_label,		// ev_field (ick)
	ex_label,		// ev_func (ick)
	ex_label,		// ev_pointer (ick)
};

static etype_t
get_type (expr_t *e)
{
	switch (e->type) {
		case ex_label:
		case ex_block:
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
	expr_t *e = calloc (1, sizeof (expr_t));
	e->line = pr_source_line;
	e->file = s_file;
	return e;
}

expr_t *
new_label_expr (void)
{
	static int label = 0;
	int lnum = ++label;
	const char *fname = current_func->def->name;

	expr_t *l = new_expr ();
	l->type = ex_label;
	l->e.label.name = malloc (1 + strlen (fname) + 1 + ceil (log10 (lnum)) + 1);
	sprintf (l->e.label.name, "$%s_%d", fname, lnum);
	return l;
}

expr_t *
new_block_expr (void)
{
	expr_t *b = new_expr ();

	b->type = ex_block;
	b->e.block.head = 0;
	b->e.block.tail = &b->e.block.head;
	return b;
}

expr_t *
new_binary_expr (int op, expr_t *e1, expr_t *e2)
{
	expr_t *e = new_expr ();

	e->type = ex_expr;
	e->e.expr.op = op;
	e->e.expr.e1 = e1;
	e->e.expr.e2 = e2;
	return e;
}

expr_t *
new_unary_expr (int op, expr_t *e1)
{
	expr_t *e = new_expr ();

	e->type = ex_uexpr;
	e->e.expr.op = op;
	e->e.expr.e1 = e1;
	return e;
}

expr_t *
append_expr (expr_t *block, expr_t *e)
{
	if (block->type != ex_block)
		abort ();

	if (!e)
		return block;

	*block->e.block.tail = e;
	block->e.block.tail = &e->next;

	return block;
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
		case ex_label:
			printf ("%s", e->e.label.name);
			break;
		case ex_block:
			printf ("{\n");
			for (e = e->e.block.head; e; e = e->next) {
				print_expr (e);
				puts("");
			}
			printf ("}");
			break;
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
			printf ("\"%s\"", e->e.string_val);
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

	s1 = e1->e.string_val;
	s2 = e2->e.string_val;
	
	switch (op) {
		case '+':
			len = strlen (s1) + strlen (s2) + 1;
			buf = malloc (len);
			strcpy (buf, s1);
			strcat (buf, s2);
			e1->e.string_val = buf;
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

	e = new_binary_expr ('.', e1, e2);
	e->e.expr.type = (e2->type == ex_def)
					 ? e2->e.def->type->aux_type
					 : e2->e.expr.type;
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
		expr_t *e = new_binary_expr (op, e1, e2);
		if ((op >= OR && op <= GT) || (op == '*' && t1 == ev_vector))
			e->e.expr.type = &type_float;
		else
			e->e.expr.type = types[t1];
		if (op == '=' && e1->type == ex_expr && e1->e.expr.op == '.') {
			e1->e.expr.type = &type_pointer;
		}
		return e;
	} else {
		switch (t1) {
			case ev_float:
				if (t2 == ev_vector) {
					expr_t *e = new_binary_expr (op, e1, e2);
					e->e.expr.type = &type_vector;
					return e;
				} else {
					goto type_mismatch;
				}
			case ev_vector:
				if (t2 == ev_float) {
					expr_t *e = new_binary_expr (op, e1, e2);
					e->e.expr.type = &type_vector;
					return e;
				} else {
					goto type_mismatch;
				}
			case ev_field:
				if (e1->e.expr.type->aux_type->type == t2) {
					expr_t *e = new_binary_expr (op, e1, e2);
					e->e.expr.type = e->e.expr.type->aux_type;
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
				case ex_label:
				case ex_block:
					abort ();
				case ex_uexpr:
					if (e->e.expr.op == '-')
						return e->e.expr.e1;
				case ex_expr:
				case ex_def:
					{
						expr_t *n = new_unary_expr (op, e);
						n->e.expr.type = (e->type == ex_def)
										 ? e->e.def->type
										 : e->e.expr.type;
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
				case ex_label:
				case ex_block:
					abort ();
				case ex_uexpr:
				case ex_expr:
				case ex_def:
					{
						expr_t *n = new_unary_expr (op, e);
						n->e.expr.type = &type_float;
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
					e->e.int_val = !e->e.string_val || !e->e.string_val[0];
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
	e = new_binary_expr ('c', e1, e2);
	e->e.expr.type = ftype->aux_type;
	return e;
}

def_t *
emit_statement (opcode_t *op, def_t *var_a, def_t *var_b, def_t *var_c)
{
	dstatement_t    *statement;
	def_t			*ret;

	statement = &statements[numstatements];
	numstatements++;
	statement_linenums[statement - statements] = pr_source_line;
	statement->op = op->opcode;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (op->type_c == &def_void || op->right_associative) {
		// ifs, gotos, and assignments don't need vars allocated
		var_c = NULL;
		statement->c = 0;
		ret = var_a;
	} else {	// allocate result space
		if (!var_c)
			var_c = PR_GetTempDef (op->type_c->type, pr_scope);
		statement->c = var_c->ofs;
		ret = var_c;
	}
	PR_AddStatementRef (var_a, statement, 0);
	PR_AddStatementRef (var_b, statement, 1);
	PR_AddStatementRef (var_c, statement, 2);

	if (op->right_associative)
		return var_a;
	return var_c;
}

def_t *emit_sub_expr (expr_t *e, def_t *dest);

void
emit_branch (opcode_t *op, expr_t *e, expr_t *l)
{
	dstatement_t *st;
	statref_t *ref;
	def_t *def = 0;

	if (e)
		def = emit_sub_expr (e, 0);
	st = &statements[numstatements];
	emit_statement (op, def, 0, 0);
	if (l->e.label.statement) {
		if (op == op_goto)
			st->a = l->e.label.statement - st;
		else
			st->b = l->e.label.statement - st;
	} else {
		ref = PR_NewStatref (st, op != op_goto);
		ref->next = l->e.label.refs;
		l->e.label.refs = ref;
	}
}

def_t *
emit_function_call (expr_t *e, def_t *dest)
{
	def_t *func = emit_sub_expr (e->e.expr.e1, 0);
	def_t *arg;
	expr_t *earg;
	opcode_t *op;
	int count = 0, ind;

	for (earg = e->e.expr.e2; earg; earg = earg->next)
		count++;
	ind = count;
	for (earg = e->e.expr.e2; earg; earg = earg->next) {
		ind--;
		arg = emit_sub_expr (earg, 0);
		def_parms[ind].type = arg->type;
		op = PR_Opcode_Find ("=", 5, arg, &def_parms[ind], &def_parms[ind]);
		emit_statement (op, arg, &def_parms[ind], 0);
	}
	op = PR_Opcode_Find (va ("<CALL%d>", count), -1, &def_function,  &def_void, &def_void);
	emit_statement (op, func, 0, dest);

	def_ret.type = func->type->aux_type;
	return &def_ret;
}

def_t *
emit_assign_expr (expr_t *e)
{
	def_t	*def_a, *def_b;
	opcode_t *op;
	def_a = emit_sub_expr (e->e.expr.e1, 0);
	if (def_a->type == &type_pointer) {
		def_b = emit_sub_expr (e->e.expr.e2, 0);
		op = PR_Opcode_Find ("=", 5, def_a, def_b, def_b);
		emit_statement (op, def_b, def_a, 0);
	} else {
		def_b = emit_sub_expr (e->e.expr.e2, def_a);
	}
	return def_b;
}

def_t *
emit_sub_expr (expr_t *e, def_t *dest)
{
	opcode_t *op;
	char *operator;
	def_t *def_a, *def_b;
	int priority;

	switch (e->type) {
		default:
		case ex_label:
		case ex_block:
			abort ();
		case ex_expr:
			if (e->e.expr.op == 'c')
				return emit_function_call (e, dest);
			if (e->e.expr.op == '=')
				return emit_assign_expr (e);
			def_a = emit_sub_expr (e->e.expr.e1, 0);
			def_b = emit_sub_expr (e->e.expr.e2, 0);
			switch (e->e.expr.op) {
				case AND:
					operator = "&&";
					priority = 6;
					break;
				case OR:
					operator = "||";
					priority = 6;
					break;
				case EQ:
					operator = "==";
					priority = 4;
					break;
				case NE:
					operator = "!=";
					priority = 4;
					break;
				case LE:
					operator = "<=";
					priority = 4;
					break;
				case GE:
					operator = ">=";
					priority = 4;
					break;
				case LT:
					operator = "<";
					priority = 4;
					break;
				case GT:
					operator = ">";
					priority = 4;
					break;
				case '+':
					operator = "+";
					priority = 3;
					break;
				case '-':
					operator = "-";
					priority = 3;
					break;
				case '*':
					operator = "*";
					priority = 2;
					break;
				case '/':
					operator = "/";
					priority = 2;
					break;
				case '&':
					operator = "&";
					priority = 2;
					break;
				case '|':
					operator = "|";
					priority = 2;
					break;
				case '.':
					operator = ".";
					priority = 1;
					break;
				default:
					abort ();
			}
			if (!dest)
				dest = PR_GetTempDef (e->e.expr.type, pr_scope);
			op = PR_Opcode_Find (operator, priority, def_a, def_b, dest);
			return emit_statement (op, def_a, def_b, dest);
		case ex_uexpr:
			if (e->e.expr.op == '!') {
				operator = "!";
				priority = -1;
				def_a = emit_sub_expr (e->e.expr.e1, 0);
				def_b = &def_void;
			} else if (e->e.expr.op == '-') {
				static expr_t zero;

				zero.type = expr_types[get_type (e->e.expr.e1)];

				operator = "-";
				priority = 3;
				def_a = PR_ReuseConstant (&zero, 0);
				def_b = emit_sub_expr (e->e.expr.e1, 0);
			} else {
				abort ();
			}
			op = PR_Opcode_Find (operator, priority, def_a, def_b, dest);
			return emit_statement (op, def_a, def_b, dest);
		case ex_def:
			return e->e.def;
		case ex_int:
		case ex_float:
		case ex_string:
		case ex_vector:
		case ex_quaternion:
			return PR_ReuseConstant (e, 0);
	}
}

void
emit_expr (expr_t *e)
{
	def_t *def;
	def_t *def_a;
	def_t *def_b;
	statref_t *ref;
	label_t *label;
	//opcode_t *op;

	switch (e->type) {
		case ex_label:
			label = &e->e.label;
			label->statement = &statements[numstatements];
			for (ref = label->refs; ref; ref = ref->next) {
				printf ("%d\n", ref->statement->op);
				switch (ref->field) {
					case 0:
						ref->statement->a = label->statement - ref->statement;
						break;
					case 1:
						ref->statement->b = label->statement - ref->statement;
						break;
					case 2:
						ref->statement->c = label->statement - ref->statement;
						break;
					default:
						abort();
				}
			}
			break;
		case ex_block:
			for (e = e->e.block.head; e; e = e->next)
				emit_expr (e);
			break;
		case ex_expr:
			switch (e->e.expr.op) {
				case '=':
					emit_assign_expr (e);
					break;
				case 'n':
					emit_branch (op_ifnot, e->e.expr.e1, e->e.expr.e2);
					break;
				case 'i':
					emit_branch (op_if, e->e.expr.e1, e->e.expr.e2);
					break;
				case 'c':
					emit_function_call (e, 0);
					break;
				case 's':
					def_a = emit_sub_expr (e->e.expr.e1, 0);
					def_b = emit_sub_expr (e->e.expr.e2, 0);
					emit_statement (op_state, def_a, def_b, 0);
					break;
				default:
					fprintf (stderr,
							 "%s:%d: warning: unused expression ignored\n",
							 strings + e->file, e->line);
			}
			break;
		case ex_uexpr:
			switch (e->e.expr.op) {
				case 'r':
					def = 0;
					if (e->e.expr.e1)
						def = emit_sub_expr (e->e.expr.e1, 0);
					PR_Statement (op_return, def, 0);
					return;
				case 'g':
					emit_branch (op_goto, 0, e);
					return;
				default:
					fprintf (stderr,
							 "%s:%d: warning: unused expression ignored\n",
							 strings + e->file, e->line);
					emit_expr (e->e.expr.e1);
					break;
			}
		case ex_def:
		case ex_int:
		case ex_float:
		case ex_string:
		case ex_vector:
		case ex_quaternion:
			fprintf (stderr, "%s:%d: warning: unused expression ignored\n",
					 strings + e->file, e->line);
			break;
	}
}
