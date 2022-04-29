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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <inttypes.h>

#include <QF/dstring.h>
#include <QF/mathlib.h>
#include <QF/quakeio.h>
#include <QF/va.h>

#include "qfalloca.h"

#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

#define EX_EXPR(expr) #expr,
const char *expr_names[] =
{
#include "tools/qfcc/include/expr_names.h"
	0
};
#undef EX_EXPR

const char *
get_op_string (int op)
{
	switch (op) {
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
		case MOD:	return "%%";
		case '&':	return "&";
		case '|':	return "|";
		case '^':	return "^";
		case '~':	return "~";
		case '!':	return "!";
		case SHL:	return "<<";
		case SHR:	return ">>";
		case '.':	return ".";
		case 'C':	return "<cast>";
		case CROSS:	return "@cross";
		case DOT:	return "@dot";
		case SCALE:	return "@scale";
		default:
			return "unknown";
	}
}

typedef void (*print_f) (dstring_t *dstr, expr_t *, int, int, expr_t *);
static void _print_expr (dstring_t *dstr, expr_t *e, int level, int id,
						 expr_t *next);

static void
print_error (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"(error)\\n%d\"];\n", indent, "", e,
			   e->line);
}

static void
print_state (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->e.state.frame, level, id, next);
	_print_expr (dstr, e->e.state.think, level, id, next);
	if (e->e.state.step)
		_print_expr (dstr, e->e.state.step, level, id, next);
	dasprintf (dstr, "%*se_%p:f -> \"e_%p\";\n", indent, "", e,
			   e->e.state.frame);
	dasprintf (dstr, "%*se_%p:t -> \"e_%p\";\n", indent, "", e,
			   e->e.state.think);
	if (e->e.state.step)
		dasprintf (dstr, "%*se_%p:s -> e_%p;\n", indent, "", e,
				   e->e.state.step);
	dasprintf (dstr,
			   "%*se_%p [label=\"<f>state|<t>think|<s>step\",shape=record];\n",
			   indent, "", e);
}

static void
print_bool (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	int         i, count;
	int         tl_count = 0, fl_count = 0;
	ex_bool_t  *bool = &e->e.bool;

	dasprintf (dstr, "%*se_%p [shape=none,label=<\n", indent, "", e);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
			   "cellspacing=\"0\">\n",
			   indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"2\">&lt;bool&gt;(%d)</td></tr>\n",
			   indent + 4, "", e->line);
	dasprintf (dstr, "%*s<tr><td>true</td><td>false</td></tr>\n",
			   indent + 4, "");
	if (bool->true_list)
		tl_count = bool->true_list->size;
	if (bool->false_list)
		fl_count = bool->false_list->size;
	count = min (tl_count, fl_count);
	for (i = 0; i < count; i++)
		dasprintf (dstr, "%*s<tr><td port=\"t%d\">t</td>"
				   "<td port=\"f%d\">f</td></tr>\n", indent, "", i, i);
	for ( ; i < tl_count; i++)
		dasprintf (dstr, "%*s<tr><td port=\"t%d\">t</td>%s</tr>\n",
				   indent, "", i,
				   i == count ? va (0, "<td rowspan=\"%d\"></td>",
									bool->true_list->size - count)
							  : "");
	for ( ; i < fl_count; i++)
		dasprintf (dstr, "%*s<tr>%s<td port=\"f%d\">f</td></tr>\n",
				   indent, "",
				   i == count ? va (0, "<td rowspan=\"%d\"></td>",
									bool->false_list->size - count)
							  : "",
				   i);
	dasprintf (dstr, "%*s</table>\n", indent + 2, "");
	dasprintf (dstr, "%*s>];\n", indent, "");
	if (e->next)
		next = e->next;
	_print_expr (dstr, e->e.bool.e, level, id, next);
	for (i = 0; i < tl_count; i++)
		dasprintf (dstr, "%*se_%p:t%d -> e_%p;\n", indent, "", e, i,
				   bool->true_list->e[i]);
	for (i = 0; i < fl_count; i++)
		dasprintf (dstr, "%*se_%p:f%d -> e_%p;\n", indent, "", e, i,
				   bool->false_list->e[i]);
	dasprintf (dstr, "%*se_%p -> e_%p;\n", indent, "", e, e->e.bool.e);
}

static void
print_label (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	if (e->next)
		next = e->next;
	if (next)
		dasprintf (dstr, "%*se_%p -> e_%p [constraint=true,style=dashed];\n",
				   indent, "", e, next);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   e->e.label.name, e->line);
}

static void
print_labelref (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	if (e->next)
		next = e->next;
	if (next)
		dasprintf (dstr, "%*se_%p -> e_%p [constraint=true,style=dashed];\n",
				   indent, "", e, e->next);
	dasprintf (dstr, "%*se_%p [label=\"&%s\\n%d\"];\n", indent, "", e,
			   e->e.label.name, e->line);
}

static void
print_block (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	int         i;
	expr_t     *se;

	dasprintf (dstr, "%*se_%p [shape=none,label=<\n", indent, "", e);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
			   "cellspacing=\"0\">\n", indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"2\">&lt;block&gt;(%d)%s</td>"
			   "</tr>\n", indent + 4, "", e->line,
			   e->e.block.is_call ? "c" : "");
	if (e->e.block.result)
		dasprintf (dstr, "%*s<tr><td colspan=\"2\" port=\"result\">=</td>"
				   "</tr>\n", indent + 4, "");
	for (se = e->e.block.head, i = 0; se; se = se->next, i++)
		dasprintf (dstr, "%*s<tr><td>%d</td><td port=\"b%d\">%s</td></tr>\n",
				   indent + 4, "", se->line, i, expr_names[se->type]);
	dasprintf (dstr, "%*s</table>\n", indent + 2, "");
	dasprintf (dstr, "%*s>];\n", indent, "");

	if (e->e.block.result) {
		_print_expr (dstr, e->e.block.result, level + 1, id, next);
		dasprintf (dstr, "%*se_%p:result -> e_%p;\n", indent, "", e,
				   e->e.block.result);
	}
	if (e->next)
		next = e->next;
	for (se = e->e.block.head, i = 0; se; se = se->next, i++) {
		_print_expr (dstr, se, level + 1, id, next);
		dasprintf (dstr, "%*se_%p:b%d -> e_%p;\n", indent, "", e,
				   i, se);
	}
}

static void
print_subexpr (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->e.expr.e1, level, id, next);
	_print_expr (dstr, e->e.expr.e2, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"l\"];\n", indent, "", e,
			   e->e.expr.e1);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"r\"];\n", indent, "", e,
			   e->e.expr.e2);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   get_op_string (e->e.expr.op), e->line);
}

static void
print_alias (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->e.alias.expr, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"a\"];\n", indent, "", e,
			   e->e.alias.expr);
	if (e->e.alias.offset) {
		_print_expr (dstr, e->e.alias.offset, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"o\"];\n", indent, "", e,
				   e->e.alias.offset);
	}

	dstring_t  *typestr = dstring_newstr();
	print_type_str (typestr, e->e.alias.type);
	dasprintf (dstr, "%*se_%p [label=\"%s (%s)\\n%d\"];\n", indent, "", e,
			   "<alias>", typestr->str, e->line);
	dstring_delete (typestr);
}

static void
print_address (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->e.address.lvalue, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"&\"];\n", indent, "", e,
			   e->e.address.lvalue);
	if (e->e.address.offset) {
		_print_expr (dstr, e->e.address.offset, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"+\"];\n", indent, "", e,
				   e->e.address.offset);
	}

	dstring_t  *typestr = dstring_newstr();
	print_type_str (typestr, e->e.address.type);
	dasprintf (dstr, "%*se_%p [label=\"%s (%s)\\n%d\"];\n", indent, "", e,
			   "&", typestr->str, e->line);
	dstring_delete (typestr);
}

static void
print_assign (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->e.assign.dst, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"lval\"];\n", indent, "", e,
			   e->e.assign.dst);
	_print_expr (dstr, e->e.assign.src, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"rval\"];\n", indent, "", e,
			   e->e.assign.src);

	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "=", e->line);
}

static void
print_conditional (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	static const char *condition[] = {
		"ifz", "ifb", "ifa", 0, "ifnz", "ifnb", "ifna", 0
	};

	_print_expr (dstr, e->e.branch.test, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"t\"];\n", indent, "", e,
			   e->e.branch.test);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"g\"];\n", indent, "", e,
			   e->e.branch.target);
	if (e->next) {
		next = e->next;
	}
	if (next) {
		dasprintf (dstr, "%*se_%p -> e_%p [constraint=true,"
				   "style=dashed];\n", indent, "", e, next);
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   condition [e->e.branch.type], e->line);
}

static void
print_jump (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->e.branch.target, level, id, next);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "jump", e->line);
}

static void
print_call (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	expr_t     *p;
	int         i, count;
	expr_t    **args;

	for (count = 0, p = e->e.branch.args; p; p = p->next)
		count++;
	args = alloca (count * sizeof (expr_t *));
	for (i = 0, p = e->e.branch.args; p; p = p->next, i++)
		args[count - 1 - i] = p;

	_print_expr (dstr, e->e.branch.target, level, id, next);
	dasprintf (dstr, "%*se_%p [label=\"<c>call", indent, "", e);
	for (i = 0; i < count; i++)
		dasprintf (dstr, "|<p%d>p%d", i, i);
	dasprintf (dstr, "\",shape=record];\n");
	for (i = 0; i < count; i++) {
		_print_expr (dstr, args[i], level + 1, id, next);
		dasprintf (dstr, "%*se_%p:p%d -> e_%p;\n", indent + 2, "", e, i,
				   args[i]);
	}
	dasprintf (dstr, "%*se_%p:c -> e_%p;\n", indent, "", e,
			   e->e.branch.target);
}

static void
print_branch (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	switch (e->e.branch.type) {
		case pr_branch_eq:
		case pr_branch_lt:
		case pr_branch_gt:
		case pr_branch_ne:
		case pr_branch_ge:
		case pr_branch_le:
			print_conditional (dstr, e, level, id, next);
			break;
		case pr_branch_jump:
			print_jump (dstr, e, level, id, next);
			break;
		case pr_branch_call:
			print_call (dstr, e, level, id, next);
			break;
	}
}

static void
print_return (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	if (e->e.retrn.ret_val) {
		_print_expr (dstr, e->e.retrn.ret_val, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e,
				   e->e.retrn.ret_val);
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "return", e->line);
}

static void
print_uexpr (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->e.expr.e1, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, e->e.expr.e1);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   get_op_string (e->e.expr.op), e->line);
}

static void
print_def (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"d %s\\n%d\"];\n", indent, "", e,
			   e->e.def->name, e->line);
}

static void
print_symbol (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   e->e.symbol->name, e->line);
}

static void
print_temp (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"tmp_%p\\n%d\"];\n", indent, "", e, e,
			   e->line);
}

static void
print_vector (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	for (expr_t *ele = e->e.vector.list; ele; ele = ele->next) {
		_print_expr (dstr, ele, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, ele);
	}
	dasprintf (dstr, "%*se_%p [label=\"vector %d\"];\n", indent, "", e,
			   e->line);
}

static void
print_selector (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"%s\"];\n", indent, "", e,
			   e->e.selector.sel->name);
}

static void
print_nil (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"nil\\n%d\"];\n", indent, "", e,
			   e->line);
}

static void
print_value (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	const char *label = "?!?";

	label = get_value_string (e->e.value);
	if (is_string (e->e.value->type)) {
		label = quote_string (html_string (label));
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e, label,
			   e->line);
}

static void
print_compound (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	dasprintf (dstr, "%*se_%p [label=\"compound init\"];\n", indent, "", e);
}

static void
print_memset (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	expr_t     *dst = e->e.memset.dst;
	expr_t     *val = e->e.memset.val;
	expr_t     *count = e->e.memset.count;
	_print_expr (dstr, dst, level, id, next);
	_print_expr (dstr, val, level, id, next);
	_print_expr (dstr, count, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, dst);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, val);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, count);
	dasprintf (dstr, "%*se_%p [label=\"memset\"];\n", indent, "", e);
}

static void
print_adjstk (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"adjstk %d:%d\\n%d\"];\n", indent, "", e,
			   e->e.adjstk.mode, e->e.adjstk.offset, e->line);
}

static void
print_with (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;
	expr_t     *with = e->e.with.with;

	_print_expr (dstr, with, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, with);
	dasprintf (dstr, "%*se_%p [label=\"with %d:%d\\n%d\"];\n", indent, "", e,
			   e->e.with.mode, e->e.with.reg, e->line);
}

static void
print_args (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"...\\n%d\"];\n", indent, "", e,
			   e->line);
}

static void
_print_expr (dstring_t *dstr, expr_t *e, int level, int id, expr_t *next)
{
	static print_f print_funcs[ex_count] = {
		[ex_error] = print_error,
		[ex_state] = print_state,
		[ex_bool] = print_bool,
		[ex_label] = print_label,
		[ex_labelref] = print_labelref,
		[ex_block] = print_block,
		[ex_expr] = print_subexpr,
		[ex_uexpr] = print_uexpr,
		[ex_def] = print_def,
		[ex_symbol] = print_symbol,
		[ex_temp] = print_temp,
		[ex_vector] = print_vector,
		[ex_selector] = print_selector,
		[ex_nil] = print_nil,
		[ex_value] = print_value,
		[ex_compound] = print_compound,
		[ex_memset] = print_memset,
		[ex_alias] = print_alias,
		[ex_address] = print_address,
		[ex_assign] = print_assign,
		[ex_branch] = print_branch,
		[ex_return] = print_return,
		[ex_adjstk] = print_adjstk,
		[ex_with] = print_with,
		[ex_args] = print_args,
	};
	int         indent = level * 2 + 2;

	if (!e) {
		dasprintf (dstr, "%*s\"e_%p\" [label=\"(null)\"];\n", indent, "", e);
		return;
	}
	if (e->printid == id)		// already printed this expression
		return;
	e->printid = id;

	if ((int) e->type < 0 || e->type >= ex_count || !print_funcs[e->type]) {
		const char *type = va (0, "%d", e->type);
		if ((unsigned) e->type < ex_count) {
			type = expr_names[e->type];
		}
		dasprintf (dstr, "%*se_%p [label=\"(bad expr type: %s)\\n%d\"];\n",
				   indent, "", e, type, e->line);
		return;
	}
	print_funcs[e->type] (dstr, e, level, id, next);

}

void
dump_dot_expr (void *_e, const char *filename)
{
	static int  id = 0;
	dstring_t  *dstr = dstring_newstr ();
	expr_t     *e = (expr_t *) _e;

	dasprintf (dstr, "digraph expr_%p {\n", e);
	dasprintf (dstr, "  graph [label=\"%s\"];\n", quote_string (filename));
	dasprintf (dstr, "  layout=dot; rankdir=TB; compound=true;\n");
	_print_expr (dstr, e, 0, ++id, 0);
	dasprintf (dstr, "}\n");

	if (filename) {
		QFile      *file;

		file = Qopen (filename, "wt");
		Qwrite (file, dstr->str, dstr->size - 1);
		Qclose (file);
	} else {
		fputs (dstr->str, stdout);
	}
	dstring_delete (dstr);
}

void
print_expr (expr_t *e)
{
	dump_dot_expr (e, 0);
}
