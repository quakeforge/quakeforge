#include <stdlib.h>

#include <QF/mathlib.h>
#include <QF/va.h>

#include "qfcc.h"
#include "scope.h"
#include "qc-parse.h"

def_t *
emit_statement (int sline, opcode_t *op, def_t *var_a, def_t *var_b, def_t *var_c)
{
	dstatement_t    *statement;
	def_t			*ret;

	if (!op) {
		expr_t e;
		e.line = sline;
		e.file = s_file;
		error (&e, "ice ice baby\n");
		abort ();
	}
	if (options.debug) {
		int				line = sline - lineno_base;

		if (line != linenos[num_linenos - 1].line) {
			pr_lineno_t		*lineno = new_lineno ();
			lineno->line = line;
			lineno->fa.addr = numstatements;
		}
	}
	statement = &statements[numstatements];
	numstatements++;
	statement_linenums[statement - statements] = pr_source_line;
	statement->op = op->opcode;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (op->type_c == ev_void || op->right_associative) {
		// ifs, gotos, and assignments don't need vars allocated
		var_c = NULL;
		statement->c = 0;
		ret = var_a;
	} else {	// allocate result space
		if (!var_c)
			var_c = PR_GetTempDef (types[op->type_c], pr_scope);
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
emit_branch (int line, opcode_t *op, expr_t *e, expr_t *l)
{
	dstatement_t *st;
	statref_t *ref;
	def_t *def = 0;

	if (e)
		def = emit_sub_expr (e, 0);
	st = &statements[numstatements];
	emit_statement (line, op, def, 0, 0);
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
	def_t parm;
	def_t *arg;
	expr_t *earg;
	opcode_t *op;
	int count = 0, ind;
	def_t *args[MAX_PARMS];

	for (earg = e->e.expr.e2; earg; earg = earg->next)
		count++;
	ind = count;
	for (earg = e->e.expr.e2; earg; earg = earg->next) {
		ind--;
		args[ind] = PR_GetTempDef (types[get_type (earg)], pr_scope);
		arg = emit_sub_expr (earg, args[ind]);
		if (earg->type != ex_expr && earg->type != ex_uexpr) {
			op = PR_Opcode_Find ("=", 5, arg, args[ind], args[ind]);
			emit_statement (e->line, op, arg, args[ind], 0);
		}
	}
	ind = count;
	while (ind-- > 0) {
		arg = args[ind];
		parm = def_parms[ind];
		parm.type = arg->type;
		op = PR_Opcode_Find ("=", 5, arg, &parm, &parm);
		emit_statement (e->line, op, arg, &parm, 0);
	}
	op = PR_Opcode_Find (va ("<CALL%d>", count), -1, &def_function,  &def_void, &def_void);
	emit_statement (e->line, op, func, 0, 0);

	def_ret.type = func->type->aux_type;
	if (def_ret.type->type != ev_void) {
		if (!dest)
			dest = PR_GetTempDef (def_ret.type, pr_scope);
		op = PR_Opcode_Find ("=", 5, dest, &def_ret, &def_ret);
		emit_statement (e->line, op, &def_ret, dest, 0);
		return dest;
	} else
		return &def_ret;
}

def_t *
emit_assign_expr (expr_t *e)
{
	def_t	*def_a, *def_b;
	opcode_t *op;
	expr_t *e1 = e->e.expr.e1;
	expr_t *e2 = e->e.expr.e2;

	def_a = emit_sub_expr (e1, 0);
	if (def_a->type == &type_pointer) {
		def_b = emit_sub_expr (e2, 0);
		op = PR_Opcode_Find ("=", 5, def_a, def_b, def_b);
		emit_statement (e->line, op, def_b, def_a, 0);
	} else {
		if (def_a->initialized) {
			if (options.cow) {
				int size = type_size [def_a->type->type];
				int ofs = PR_NewLocation (def_a->type);
				memcpy (pr_globals + ofs, pr_globals + def_a->ofs, size);
				def_a->ofs = ofs;
				def_a->initialized = 0;
				warning (e1, "assignment to constant %s", def_a->name);
			} else {
				error (e1, "assignment to constant %s", def_a->name);
			}
		}
		def_b = emit_sub_expr (e2, def_a);
		if (def_b != def_a) {
			op = PR_Opcode_Find ("=", 5, def_a, def_b, def_b);
			emit_statement (e->line, op, def_b, def_a, 0);
		}
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
		case ex_label:
		case ex_block:
			error (e, "internal error");
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
				case '^':
					operator = "^";
					priority = 2;
					break;
				case '|':
					operator = "|";
					priority = 2;
					break;
				case '%':
					operator = "%";
					priority = 2;
					break;
				case SHL:
					operator = "<<";
					priority = 2;
					break;
				case SHR:
					operator = ">>";
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
			return emit_statement (e->line, op, def_a, def_b, dest);
		case ex_uexpr:
			if (e->e.expr.op == '!') {
				operator = "!";
				priority = -1;
				def_a = emit_sub_expr (e->e.expr.e1, 0);
				def_b = &def_void;
			} else if (e->e.expr.op == '~') {
				operator = "~";
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
				if (!dest)
					dest = PR_GetTempDef (e->e.expr.type, pr_scope);
			} else {
				abort ();
			}
			op = PR_Opcode_Find (operator, priority, def_a, def_b, dest);
			return emit_statement (e->line, op, def_a, def_b, dest);
		case ex_def:
			return e->e.def;
		case ex_string:
		case ex_float:
		case ex_vector:
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_pointer:
		case ex_quaternion:
		case ex_integer:
			return PR_ReuseConstant (e, 0);
	}
	return 0;
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
					emit_branch (e->line, op_ifnot, e->e.expr.e1, e->e.expr.e2);
					break;
				case 'i':
					emit_branch (e->line, op_if, e->e.expr.e1, e->e.expr.e2);
					break;
				case 'c':
					emit_function_call (e, 0);
					break;
				case 's':
					def_a = emit_sub_expr (e->e.expr.e1, 0);
					def_b = emit_sub_expr (e->e.expr.e2, 0);
					emit_statement (e->line, op_state, def_a, def_b, 0);
					break;
				default:
					warning (e, "Ignoring useless expression");
					break;
			}
			break;
		case ex_uexpr:
			switch (e->e.expr.op) {
				case 'r':
					def = 0;
					if (e->e.expr.e1)
						def = emit_sub_expr (e->e.expr.e1, 0);
					emit_statement (e->line, op_return, def, 0, 0);
					break;
				case 'g':
					emit_branch (e->line, op_goto, 0, e->e.expr.e1);
					break;
				default:
					warning (e, "Ignoring useless expression");
					emit_expr (e->e.expr.e1);
					break;
			}
			break;
		case ex_def:
		case ex_string:
		case ex_float:
		case ex_vector:
		case ex_entity:
		case ex_field:
		case ex_func:
		case ex_pointer:
		case ex_quaternion:
		case ex_integer:
			warning (e, "Ignoring useless expression");
			break;
	}
	PR_FreeTempDefs ();
}
