/*
	dot_expr.c

	"emit" expressions to dot (graphvis).

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/20

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include <QF/dstring.h>
#include <QF/va.h>

#include "expr.h"
#include "symtab.h"
#include "type.h"
#include "qc-parse.h"

const char *
get_op_string (int op)
{
	switch (op) {
		case PAS:	return ".=";
		case OR:	return "||";
		case AND:	return "&&";
		case EQ:	return "==";
		case NE:	return "!=";
		case LE:	return "<=";
		case GE:	return ">=";
		case LT:	return "<";
		case GT:	return ">";
		case '=':	return "=";
		case '+':	return "+";
		case '-':	return "-";
		case '*':	return "*";
		case '/':	return "/";
		case '%':	return "%";
		case '&':	return "&";
		case '|':	return "|";
		case '^':	return "^";
		case '~':	return "~";
		case '!':	return "!";
		case SHL:	return "<<";
		case SHR:	return ">>";
		case '.':	return ".";
		case 'i':	return "<if>";
		case 'n':	return "<ifnot>";
		case IFBE:	return "<ifbe>";
		case IFB:	return "<ifb>";
		case IFAE:	return "<ifae>";
		case IFA:	return "<ifa>";
		case 'g':	return "<goto>";
		case 'r':	return "<return>";
		case 's':	return "<state>";
		case 'c':	return "<call>";
		case 'C':	return "<cast>";
		case 'M':	return "<move>";
		default:
			return "unknown";
	}
}

typedef void (*print_f) (expr_t *, int, int);
static void _print_expr (expr_t *e, int level, int id);

static void
print_error (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	printf ("%*s\"e_%p\" [label=\"(error)\\n%d\"];\n", indent, "", e, e->line);
}

static void
print_state (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	_print_expr (e->e.state.frame, level, id);
	_print_expr (e->e.state.think, level, id);
	if (e->e.state.step)
		_print_expr (e->e.state.step, level, id);
	printf ("%*s\"e_%p\":f -> \"e_%p\";\n", indent, "", e, e->e.state.frame);
	printf ("%*s\"e_%p\":t -> \"e_%p\";\n", indent, "", e, e->e.state.think);
	if (e->e.state.step)
		printf ("%*s\"e_%p\":s -> \"e_%p\";\n", indent, "", e,
				e->e.state.step);
	printf ("%*s\"e_%p\" [label=\"<f>state|<t>think|<s>step\","
			"shape=record];\n", indent, "", e);
}

static void
print_bool (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	_print_expr (e->e.bool.e, level, id);
	if (e->e.bool.e->type == ex_block && e->e.bool.e->e.block.head) {
		expr_t     *se;

		printf ("%*s\"e_%p\" -> \"e_%p\";\n", indent, "", e, e->e.bool.e);
		se = (expr_t *) e->e.bool.e->e.block.tail;
		if (se && se->type == ex_label && e->next)
			printf ("%*s\"e_%p\" -> \"e_%p\" "
					"[constraint=false,style=dashed];\n", indent, "",
					se, e->next);
	} else {
		printf ("%*s\"e_%p\" -> \"e_%p\";\n", indent, "", e, e->e.bool.e);
	}
	printf ("%*s\"e_%p\" [label=\"<bool>\\n%d\"];\n", indent, "", e, e->line);
}

static void
print_label (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	if (e->next)
		printf ("%*s\"e_%p\" -> \"e_%p\" "
				"[constraint=false,style=dashed];\n", indent, "",
				e, e->next);
	printf ("%*s\"e_%p\" [label=\"%s\\n%d\"];\n", indent, "", e,
			e->e.label.name, e->line);
}

static void
print_block (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;
	expr_t     *se;

	if (e->e.block.result) {
		_print_expr (e->e.block.result, level + 1, id);
		printf ("%*s\"e_%p\" -> \"e_%p\";\n", indent, "",
				e, e->e.block.result);
	}
	printf ("%*s\"e_%p\" -> \"e_%p\" [style=dashed];\n", indent, "",
			e, e->e.block.head);
	//printf ("%*ssubgraph cluster_%p {\n", indent, "", e);
	for (se = e->e.block.head; se; se = se->next) {
		_print_expr (se, level + 1, id);
	}
	for (se = e->e.block.head; se && se->next; se = se->next) {
		if ((se->type == ex_uexpr && se->e.expr.op == 'g')
			|| se->type == ex_label || se->type == ex_bool)
			continue;
		printf ("%*s\"e_%p\" -> \"e_%p\" [constraint=false,style=dashed];\n",
				indent, "", se, se->next);
	}
	if (se && se->type == ex_label && e->next)
		printf ("%*s\"e_%p\" -> \"e_%p\" [constraint=false,style=dashed];\n",
				indent, "", se, e->next);
	//printf ("%*s}\n", indent, "");
	printf ("%*s\"e_%p\" [label=\"<block>\\n%d\"];\n", indent, "", e, e->line);
}

static void
print_call (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;
	expr_t     *p;
	int         i, count;
	expr_t    **args;

	for (count = 0, p = e->e.expr.e2; p; p = p->next)
		count++;
	args = alloca (count * sizeof (expr_t *));
	for (i = 0, p = e->e.expr.e2; p; p = p->next, i++)
		args[count - 1 - i] = p;

	_print_expr (e->e.expr.e1, level, id);
	printf ("%*s\"e_%p\" [label=\"<c>call", indent, "", e);
	for (i = 0; i < count; i++)
		printf ("|<p%d>p%d", i, i);
	printf ("\",shape=record];\n");
	for (i = 0; i < count; i++) {
		_print_expr (args[i], level + 1, id);
		printf ("%*s\"e_%p\":p%d -> \"e_%p\";\n", indent + 2, "", e, i,
				args[i]);
	}
	printf ("%*s\"e_%p\":c -> \"e_%p\";\n", indent, "", e, e->e.expr.e1);
}

static void
print_subexpr (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	if (e->e.expr.op == 'c') {
		print_call (e, level, id);
		return;
	} else if (e->e.expr.op == 'i' || e->e.expr.op == 'n'
			   || e->e.expr.op == IFB || e->e.expr.op ==IFBE
			   || e->e.expr.op == IFA || e->e.expr.op ==IFAE) {
		_print_expr (e->e.expr.e1, level, id);
		printf ("%*s\"e_%p\" -> \"e_%p\" [label=\"t\"];\n", indent, "",
				e, e->e.expr.e1);
		printf ("%*s\"e_%p\" -> \"e_%p\" [label=\"g\"];\n", indent, "",
				e, e->e.expr.e2);
	} else {
		_print_expr (e->e.expr.e1, level, id);
		_print_expr (e->e.expr.e2, level, id);
		printf ("%*s\"e_%p\" -> \"e_%p\" [label=\"l\"];\n", indent, "",
				e, e->e.expr.e1);
		printf ("%*s\"e_%p\" -> \"e_%p\" [label=\"r\"];\n", indent, "",
				e, e->e.expr.e2);
	}
	printf ("%*s\"e_%p\" [label=\"%s\\n%d\"];\n", indent, "", e,
			get_op_string (e->e.expr.op), e->line);
}

static void
print_uexpr (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	if (e->e.expr.op != 'g')
		_print_expr (e->e.expr.e1, level, id);
	printf ("%*s\"e_%p\" -> \"e_%p\";\n", indent, "", e, e->e.expr.e1);
	printf ("%*s\"e_%p\" [label=\"%s\\n%d\"];\n", indent, "", e,
			get_op_string (e->e.expr.op), e->line);
}

static void
print_symbol (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	printf ("%*s\"e_%p\" [label=\"%s\\n%d\"];\n", indent, "", e,
			e->e.symbol->name, e->line);
}

static void
print_temp (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	printf ("%*s\"e_%p\" [label=\"tmp_%p\\n%d\"];\n", indent, "", e, e,
			e->line);
}

static void
print_nil (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;

	printf ("%*s\"e_%p\" [label=\"nil\\n%d\"];\n", indent, "", e, e->line);
}

static void
print_value (expr_t *e, int level, int id)
{
	int         indent = level * 2 + 2;
	type_t     *type;
	const char *label = "?!?";

	switch (e->e.value.type) {
		case ev_string:
			label = va ("\\\"%s\\\"", e->e.value.v.string_val);
			break;
		case ev_float:
			label = va ("%g", e->e.value.v.float_val);
			break;
		case ev_vector:
			label = va ("'%g %g %g'",
						e->e.value.v.vector_val[0],
						e->e.value.v.vector_val[1],
						e->e.value.v.vector_val[2]);
			break;
		case ev_quat:
			label = va ("'%g %g %g %g'",
						e->e.value.v.quaternion_val[0],
						e->e.value.v.quaternion_val[1],
						e->e.value.v.quaternion_val[2],
						e->e.value.v.quaternion_val[3]);
			break;
		case ev_pointer:
			type = e->e.value.v.pointer.type;
			if (e->e.value.v.pointer.def)
				label = va ("(%s)[%d]<%s>",
							type ? pr_type_name[type->type] : "???",
							e->e.value.v.pointer.val,
							e->e.value.v.pointer.def->name);
			else
				label = va ("(%s)[%d]",
							type ? pr_type_name[type->type] : "???",
							e->e.value.v.pointer.val);
			break;
		case ev_field:
			label = va ("field %d", e->e.value.v.pointer.val);
			break;
		case ev_entity:
			label = va ("ent %d", e->e.value.v.integer_val);
			break;
		case ev_func:
			label = va ("func %d", e->e.value.v.integer_val);
			break;
		case ev_integer:
			label = va ("%d", e->e.value.v.integer_val);
			break;
		case ev_short:
			label = va ("%d", e->e.value.v.short_val);
			break;
		case ev_void:
			label = "<void>";
			break;
		case ev_invalid:
			label = "<invalid>";
			break;
		case ev_type_count:
			label = "<type_count>";
			break;
	}
	printf ("%*s\"e_%p\" [label=\"%s\\n%d\"];\n", indent, "", e, label,
			e->line);
}

static void
_print_expr (expr_t *e, int level, int id)
{
	static print_f print_funcs[] = {
		print_error,
		print_state,
		print_bool,
		print_label,
		print_block,
		print_subexpr,
		print_uexpr,
		print_symbol,
		print_temp,
		print_nil,
		print_value,
	};
	int         indent = level * 2 + 2;

	if (!e) {
		printf ("%*s\"e_%p\" [label=\"(null)\"];\n", indent, "", e);
		return;
	}
	if (e->printid == id)		// already printed this expression
		return;
	e->printid = id;

	if (e->type < 0 || e->type > ex_value) {
		printf ("%*s\"e_%p\" [label=\"(bad expr type)\\n%d\"];\n", indent, "",
				e, e->line);
		return;
	}
	print_funcs [e->type] (e, level, id);

}

void
print_expr (expr_t *e)
{
	static int id = 0;
	printf ("digraph expr_%p {\n", e);
	printf ("  layout=dot; rankdir=TB; compound=true;\n");
	_print_expr (e, 0, ++id);
	printf ("}\n");
}
