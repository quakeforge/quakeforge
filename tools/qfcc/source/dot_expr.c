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

#include "tools/qfcc/include/algebra.h"
#include "tools/qfcc/include/attribute.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#define EX_EXPR(expr) #expr,
const char *expr_names[] =
{
#include "tools/qfcc/include/expr_names.h"
	0
};

const char *
get_op_string (int op)
{
	switch (op) {
		case QC_OR:			return "||";
		case QC_AND:		return "&&";
		case QC_EQ:			return "==";
		case QC_NE:			return "!=";
		case QC_LE:			return "<=";
		case QC_GE:			return ">=";
		case QC_LT:			return "<";
		case QC_GT:			return ">";
		case '=':			return "=";
		case '+':			return "+";
		case '-':			return "-";
		case '*':			return "*";
		case '/':			return "/";
		case '%':			return "%";
		case QC_MOD:		return "%%";
		case '&':			return "&";
		case '|':			return "|";
		case '^':			return "^";
		case '~':			return "~";
		case '!':			return "!";
		case QC_SHL:		return "<<";
		case QC_SHR:		return ">>";
		case '.':			return ".";
		case 'C':			return "<cast>";
		case 'S':			return "<sizeof>";
		case QC_SCALE:		return "@scale";
		case QC_GEOMETRIC:	return "@geometric";
		case QC_QMUL:		return "@qmul";
		case QC_QVMUL:		return "@qvmul";
		case QC_VQMUL:		return "@vqmul";
		case QC_HADAMARD:	return "@hadamard";
		case QC_CROSS:		return "@cross";
		case QC_DOT:		return "@dot";
		case QC_OUTER:		return "@outer";
		case QC_WEDGE:		return "@wedge";
		case QC_REGRESSIVE:	return "@regressive";
		case QC_REVERSE:	return "@reverse";
		case QC_DUAL:		return "@dual";
		case QC_UNDUAL:		return "@undual";
		case QC_ATTRIBUTE:  return ".property";
		case QC_AT_FUNCTION:return "@function";
		case QC_AT_FIELD:	return "@field";
		case QC_AT_POINTER:	return "@pointer";
		case QC_AT_ARRAY:	return "@array";
		case QC_AT_BASE:	return "@base";
		case QC_AT_WIDTH:	return "@width";
		case QC_AT_VECTOR:	return "@vector";
		case QC_AT_ROWS:	return "@rows";
		case QC_AT_COLS:	return "@cols";
		case QC_AT_MATRIX:	return "@matrix";
		case QC_AT_INT:		return "@int";
		case QC_AT_UINT:	return "@uint";
		case QC_AT_BOOL:	return "@bool";
		case QC_AT_FLOAT:	return "@float";
		default:
			return "unknown";
	}
}

typedef void print_f (dstring_t *dstr, const expr_t *, int, int,
						 const expr_t *);
static print_f _print_expr;

static void
print_error (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"(error)\\n%d\"];\n", indent, "", e,
			   e->loc.line);
}

static void
print_state (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->state.frame, level, id, next);
	_print_expr (dstr, e->state.think, level, id, next);
	if (e->state.step)
		_print_expr (dstr, e->state.step, level, id, next);
	dasprintf (dstr, "%*se_%p:f -> \"e_%p\";\n", indent, "", e,
			   e->state.frame);
	dasprintf (dstr, "%*se_%p:t -> \"e_%p\";\n", indent, "", e,
			   e->state.think);
	if (e->state.step)
		dasprintf (dstr, "%*se_%p:s -> e_%p;\n", indent, "", e,
				   e->state.step);
	dasprintf (dstr,
			   "%*se_%p [label=\"<f>state|<t>think|<s>step\",shape=record];\n",
			   indent, "", e);
}

static void
print_bool (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	int         i, count;
	int         tl_count = 0, fl_count = 0;
	const ex_bool_t *boolean = &e->boolean;

	dasprintf (dstr, "%*se_%p [shape=none,label=<\n", indent, "", e);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
			   "cellspacing=\"0\">\n",
			   indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"2\">&lt;boolean&gt;(%d)</td></tr>\n",
			   indent + 4, "", e->loc.line);
	dasprintf (dstr, "%*s<tr><td>true</td><td>false</td></tr>\n",
			   indent + 4, "");
	if (boolean->true_list)
		tl_count = boolean->true_list->size;
	if (boolean->false_list)
		fl_count = boolean->false_list->size;
	count = min (tl_count, fl_count);
	for (i = 0; i < count; i++)
		dasprintf (dstr, "%*s<tr><td port=\"t%d\">t</td>"
				   "<td port=\"f%d\">f</td></tr>\n", indent, "", i, i);
	for ( ; i < tl_count; i++)
		dasprintf (dstr, "%*s<tr><td port=\"t%d\">t</td>%s</tr>\n",
				   indent, "", i,
				   i == count ? va ("<td rowspan=\"%d\"></td>",
									boolean->true_list->size - count)
							  : "");
	for ( ; i < fl_count; i++)
		dasprintf (dstr, "%*s<tr>%s<td port=\"f%d\">f</td></tr>\n",
				   indent, "",
				   i == count ? va ("<td rowspan=\"%d\"></td>",
									boolean->false_list->size - count)
							  : "",
				   i);
	dasprintf (dstr, "%*s</table>\n", indent + 2, "");
	dasprintf (dstr, "%*s>];\n", indent, "");
	_print_expr (dstr, e->boolean.e, level, id, next);
	for (i = 0; i < tl_count; i++)
		dasprintf (dstr, "%*se_%p:t%d -> e_%p;\n", indent, "", e, i,
				   boolean->true_list->e[i]);
	for (i = 0; i < fl_count; i++)
		dasprintf (dstr, "%*se_%p:f%d -> e_%p;\n", indent, "", e, i,
				   boolean->false_list->e[i]);
	dasprintf (dstr, "%*se_%p -> e_%p;\n", indent, "", e, e->boolean.e);
}

static void
print_label (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	if (next)
		dasprintf (dstr, "%*se_%p -> e_%p [constraint=true,style=dashed];\n",
				   indent, "", e, next);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   e->label.name, e->loc.line);
}

static void
print_labelref (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	if (next)
		dasprintf (dstr, "%*se_%p -> e_%p [constraint=true,style=dashed];\n",
				   indent, "", e, next);
	dasprintf (dstr, "%*se_%p [label=\"&%s\\n%d\"];\n", indent, "", e,
			   e->label.name, e->loc.line);
}

static void
print_block (dstring_t *dstr, const expr_t *e, int level, int id,
			 const expr_t *next)
{
	int         indent = level * 2 + 2;
	int         num_exprs = list_count (&e->block.list);
	const expr_t *exprs[num_exprs + 1];

	list_scatter (&e->block.list, exprs);
	exprs[num_exprs] = next;

	int colspan = num_exprs ? num_exprs : 1;

	dasprintf (dstr, "%*se_%p [shape=none,label=<\n", indent, "", e);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
			   "cellspacing=\"0\">\n", indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"%d\">&lt;block&gt;(%d)%s</td>"
			   "</tr>\n", indent + 4, "", colspan, e->loc.line,
			   e->block.is_call ? "c" : "");
	if (e->block.result)
		dasprintf (dstr, "%*s<tr><td colspan=\"%d\" port=\"result\">=</td>"
				   "</tr>\n", indent + 4, "", colspan);
	if (num_exprs) {
		dasprintf (dstr, "%*s<tr>\n", indent + 4, "");
		for (int i = 0; i < num_exprs; i++) {
			dasprintf (dstr, "%*s<td>%d</td>\n", indent + 8, "",
					   exprs[i]->loc.line);
		}
		dasprintf (dstr, "%*s</tr>\n", indent + 4, "");
		dasprintf (dstr, "%*s<tr>\n", indent + 4, "");
		for (int i = 0; i < num_exprs; i++) {
			dasprintf (dstr, "%*s<td port=\"b%d\">%s</td>\n", indent + 8, "",
					   i, expr_names[exprs[i]->type]);
		}
		dasprintf (dstr, "%*s</tr>\n", indent + 4, "");
	}
	dasprintf (dstr, "%*s</table>\n", indent + 2, "");
	dasprintf (dstr, "%*s>];\n", indent, "");

	if (e->block.result) {
		_print_expr (dstr, e->block.result, level + 1, id, next);
		dasprintf (dstr, "%*se_%p:result -> e_%p;\n", indent, "", e,
				   e->block.result);
	}
	// print any label expressions first to ensure they get the correct
	// next pointer instead of pointing to themselves.
	for (int i = 0; i < num_exprs; i++) {
		if (exprs[i]->type == ex_label) {
			_print_expr (dstr, exprs[i], level + 1, id, exprs[i + 1]);
		}
	}
	for (int i = 0; i < num_exprs; i++) {
		_print_expr (dstr, exprs[i], level + 1, id, exprs[i + 1]);
		dasprintf (dstr, "%*se_%p:b%d -> e_%p;\n", indent, "", e,
				   i, exprs[i]);
	}
}

static void
print_list (dstring_t *dstr, const expr_t *e, int level, int id,
			const expr_t *next)
{
	int         indent = level * 2 + 2;
	int         num_exprs = list_count (&e->list);
	const expr_t *exprs[num_exprs + 1];

	list_scatter (&e->list, exprs);
	exprs[num_exprs] = 0;

	int colspan = num_exprs ? num_exprs : 1;

	dasprintf (dstr, "%*se_%p [shape=none,label=<\n", indent, "", e);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
			   "cellspacing=\"0\">\n", indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"%d\">&lt;list&gt;(%d)</td>"
			   "</tr>\n", indent + 4, "", colspan, e->loc.line);
	if (num_exprs) {
		dasprintf (dstr, "%*s<tr>\n", indent + 4, "");
		for (int i = 0; i < num_exprs; i++) {
			dasprintf (dstr, "%*s<td>%d</td>\n", indent + 8, "",
					   exprs[i]->loc.line);
		}
		dasprintf (dstr, "%*s</tr>\n", indent + 4, "");
		dasprintf (dstr, "%*s<tr>\n", indent + 4, "");
		for (int i = 0; i < num_exprs; i++) {
			dasprintf (dstr, "%*s<td port=\"b%d\">%s</td>\n", indent + 8, "",
					   i, expr_names[exprs[i]->type]);
		}
		dasprintf (dstr, "%*s</tr>\n", indent + 4, "");
	}
	dasprintf (dstr, "%*s</table>\n", indent + 2, "");
	dasprintf (dstr, "%*s>];\n", indent, "");

	for (int i = 0; i < num_exprs; i++) {
		_print_expr (dstr, exprs[i], level + 1, id, exprs[i + 1]);
		dasprintf (dstr, "%*se_%p:b%d -> e_%p;\n", indent, "", e,
				   i, exprs[i]);
	}
}

static void
print_type_expr (dstring_t *dstr, const expr_t *e, int level, int id,
				 const expr_t *next)
{
	int         indent = level * 2 + 2;
	const char *str = "(null)";
	if (e->typ.op) {
		_print_expr (dstr, e->typ.params, level + 1, id, nullptr);
		dasprintf (dstr, "%*se_%p -> e_%p;\n", indent, "", e, e->typ.params);
		str = get_op_string (e->typ.op);
	} else if (e->typ.type) {
		str = e->typ.type->encoding;
		if (!str) {
		   str = type_get_encoding (e->typ.type);
		}
	} else if (e->typ.property) {
		auto property = e->typ.property;
		if (property->params) {
			_print_expr (dstr, property->params, level + 1, id, nullptr);
			dasprintf (dstr, "%*se_%p -> e_%p;\n", indent, "", e,
					   property->params);
		}
		str = property->name;
	} else if (e->typ.sym) {
	   str = e->typ.sym->name;
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   str, e->loc.line);
}

static void
print_incop (dstring_t *dstr, const expr_t *e, int level, int id,
			 const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"m\"];\n", indent, "", e,
			   e->incop.expr);
	_print_expr (dstr, e->incop.expr, level, id, next);
	const char *b = e->incop.postop ? "Q" : "";
	const char *a = e->incop.postop ? "" : "Q";
	int op = e->incop.op;
	dasprintf (dstr, "%*se_%p [label=\"%s%c%c%s\\n%d\"];\n", indent, "", e,
			   b, op, op, a, e->loc.line);
}

static void
print_cond (dstring_t *dstr, const expr_t *e, int level, int id,
			const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->cond.test, level, id, next);
	_print_expr (dstr, e->cond.true_expr, level, id, next);
	_print_expr (dstr, e->cond.false_expr, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"?\"];\n", indent, "", e,
			   e->cond.test);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"t\"];\n", indent, "", e,
			   e->cond.true_expr);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"f\"];\n", indent, "", e,
			   e->cond.false_expr);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "?:", e->loc.line);
}

static void
print_field (dstring_t *dstr, const expr_t *e, int level, int id,
			 const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->field.object, level, id, next);
	_print_expr (dstr, e->field.member, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"o\"];\n", indent, "", e,
			   e->field.object);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"m\"];\n", indent, "", e,
			   e->field.member);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   ".", e->loc.line);
}

static void
print_array (dstring_t *dstr, const expr_t *e, int level, int id,
			 const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->array.base, level, id, next);
	_print_expr (dstr, e->array.index, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"b\"];\n", indent, "", e,
			   e->array.base);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"i\"];\n", indent, "", e,
			   e->array.index);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "[]", e->loc.line);
}

static void
print_decl (dstring_t *dstr, const expr_t *e, int level, int id,
			const expr_t *next)
{
	int         indent = level * 2 + 2;

	for (auto l = e->decl.list.head; l; l = l->next) {
		_print_expr (dstr, l->expr, level, id, next);
	}
	for (auto l = e->decl.list.head; l; l = l->next) {
		dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"b\"];\n", indent, "", e,
				   l->expr);
	}
	dasprintf (dstr, "%*se_%p [label=\"decl:%s\\n%d\"];\n", indent, "", e,
			   get_type_string (e->decl.spec.type), e->loc.line);
}

static void
print_select (dstring_t *dstr, const expr_t *e, int level, int id,
			  const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->select.test, level, id, next);
	_print_expr (dstr, e->select.true_body, level, id, next);
	_print_expr (dstr, e->select.false_body, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"\"];\n", indent, "", e,
			   e->select.test);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"t\"];\n", indent, "", e,
			   e->select.true_body);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"f\"];\n", indent, "", e,
			   e->select.false_body);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "select", e->loc.line);
}

static void
print_intrinsic (dstring_t *dstr, const expr_t *e, int level, int id,
				 const expr_t *next)
{
	int         indent = level * 2 + 2;

	if (e->intrinsic.extra) {
		_print_expr (dstr, e->intrinsic.extra, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"extra\"];\n",
				   indent, "", e, e->intrinsic.extra);
	}
	_print_expr (dstr, e->intrinsic.opcode, level, id, next);
	for (auto l = e->intrinsic.operands.head; l; l = l->next) {
		_print_expr (dstr, l->expr, level, id, next);
	}
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"op\"];\n", indent, "", e,
			   e->intrinsic.opcode);
	for (auto l = e->intrinsic.operands.head; l; l = l->next) {
		dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e,
				   l->expr);
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "@intrinsic", e->loc.line);
}

static void
print_xvalue (dstring_t *dstr, const expr_t *e, int level, int id,
			  const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->xvalue.expr, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, e->xvalue.expr);
	const char *b = e->xvalue.lvalue ? "Q" : "";
	const char *a = e->xvalue.lvalue ? "" : "Q";
	dasprintf (dstr, "%*se_%p [label=\"%s%c%s\\n%d\"];\n", indent, "", e,
			   b, '=', a, e->loc.line);
}

static void
print_process (dstring_t *dstr, const expr_t *e, int level, int id,
			   const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->process.expr, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, e->process.expr);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "<process>", e->loc.line);
}

static void
print_subexpr (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->expr.e1, level, id, next);
	_print_expr (dstr, e->expr.e2, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"l\"];\n", indent, "", e,
			   e->expr.e1);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"r\"];\n", indent, "", e,
			   e->expr.e2);
	const char *o = e->paren ? "(" : "";
	const char *c = e->paren ? ")" : "";
	dasprintf (dstr, "%*se_%p [label=\"%s%s%s\\n%d\"];\n", indent, "", e,
			   o, get_op_string (e->expr.op), c, e->loc.line);
}

static void
print_alias (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->alias.expr, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"a\"];\n", indent, "", e,
			   e->alias.expr);
	if (e->alias.offset) {
		_print_expr (dstr, e->alias.offset, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"o\"];\n", indent, "", e,
				   e->alias.offset);
	}

	dstring_t  *typestr = dstring_newstr();
	print_type_str (typestr, e->alias.type);
	dasprintf (dstr, "%*se_%p [label=\"%s (%s)\\n%d\"];\n", indent, "", e,
			   "<alias>", typestr->str, e->loc.line);
	dstring_delete (typestr);
}

static void
print_address (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->address.lvalue, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"&\"];\n", indent, "", e,
			   e->address.lvalue);
	if (e->address.offset) {
		_print_expr (dstr, e->address.offset, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"+\"];\n", indent, "", e,
				   e->address.offset);
	}

	dstring_t  *typestr = dstring_newstr();
	print_type_str (typestr, e->address.type);
	dasprintf (dstr, "%*se_%p [label=\"%s (%s)\\n%d\"];\n", indent, "", e,
			   "&", typestr->str, e->loc.line);
	dstring_delete (typestr);
}

static void
print_assign (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->assign.dst, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"lval\"];\n", indent, "", e,
			   e->assign.dst);
	_print_expr (dstr, e->assign.src, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"rval\"];\n", indent, "", e,
			   e->assign.src);

	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "=", e->loc.line);
}

static void
print_conditional (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	static const char *condition[] = {
		"ifz", "ifb", "ifa", 0, "ifnz", "ifnb", "ifna", 0
	};

	_print_expr (dstr, e->branch.test, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"t\"];\n", indent, "", e,
			   e->branch.test);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"g\"];\n", indent, "", e,
			   e->branch.target);
	if (next) {
		dasprintf (dstr, "%*se_%p -> e_%p [constraint=true,"
				   "style=dashed];\n", indent, "", e, next);
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   condition [e->branch.type], e->loc.line);
}

static void
print_jump (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->branch.target, level, id, next);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "jump", e->loc.line);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e,
			   e->branch.target);
}

static void
print_call (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	int         count = list_count (&e->branch.args->list);
	const expr_t *args[count];
	list_scatter_rev (&e->branch.args->list, args);

	_print_expr (dstr, e->branch.target, level, id, next);
	dasprintf (dstr, "%*se_%p [label=\"<c>call", indent, "", e);
	for (int i = 0; i < count; i++)
		dasprintf (dstr, "|<p%d>p%d", i, i);
	dasprintf (dstr, "\",shape=record];\n");
	for (int i = 0; i < count; i++) {
		_print_expr (dstr, args[i], level + 1, id, next);
		dasprintf (dstr, "%*se_%p:p%d -> e_%p;\n", indent + 2, "", e, i,
				   args[i]);
	}
	dasprintf (dstr, "%*se_%p:c -> e_%p;\n", indent, "", e,
			   e->branch.target);
}

static void
print_branch (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	switch (e->branch.type) {
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
print_inout (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->inout.in, level, id, next);
	_print_expr (dstr, e->inout.out, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"in\"];\n", indent, "",
			   e, e->inout.in);
	dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"out\"];\n", indent, "",
			   e, e->inout.out);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "inout", e->loc.line);
}

static void
print_return (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	if (e->retrn.ret_val) {
		_print_expr (dstr, e->retrn.ret_val, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e,
				   e->retrn.ret_val);
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   "return", e->loc.line);
}

static void
print_uexpr (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->expr.e1, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, e->expr.e1);
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   get_op_string (e->expr.op), e->loc.line);
}

static void
print_def (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"d %s\\n%d\"];\n", indent, "", e,
			   e->def->name, e->loc.line);
}

static void
print_symbol (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e,
			   e->symbol->name, e->loc.line);
}

static void
print_temp (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"tmp_%p\\n%d\"];\n", indent, "", e, e,
			   e->loc.line);
}

static void
print_vector (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	for (auto li = e->vector.list.head; li; li = li->next) {
		auto ele = li->expr;
		_print_expr (dstr, ele, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, ele);
	}
	dasprintf (dstr, "%*se_%p [label=\"vector %d\"];\n", indent, "", e,
			   e->loc.line);
}

static void
print_selector (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"%s\"];\n", indent, "", e,
			   e->selector.sel->name);
}

static void
print_message (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->message.receiver, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e,
			   e->message.receiver);
	for (auto kw = e->message.message; kw; kw = kw->next) {
		_print_expr (dstr, kw->expr, level, id, next);
		dasprintf (dstr, "%*se_%p -> \"e_%p\" [label=\"%s\"];\n", indent, "", e,
				   kw->expr, kw->selector);
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\"];\n", indent, "", e,
			   "[message]");
}

static void
print_nil (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"nil\\n%d\"];\n", indent, "", e,
			   e->loc.line);
}

static void
print_value (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	const char *label = "?!?";

	label = get_value_string (e->value);
	if (is_string (e->value->type)) {
		label = quote_string (html_string (label));
	}
	dasprintf (dstr, "%*se_%p [label=\"%s\\n%d\"];\n", indent, "", e, label,
			   e->loc.line);
}

static void
print_compound (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	for (auto ele = e->compound.head; ele; ele = ele->next) {
		_print_expr (dstr, ele->expr, level, id, next);
	}
	for (auto ele = e->compound.head; ele; ele = ele->next) {
		dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e,
				   ele->expr);
	}
	dasprintf (dstr, "%*se_%p [label=\"= { }\"];\n", indent, "", e);
}

static void
print_memset (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	const expr_t *dst = e->memset.dst;
	const expr_t *val = e->memset.val;
	const expr_t *count = e->memset.count;
	_print_expr (dstr, dst, level, id, next);
	_print_expr (dstr, val, level, id, next);
	_print_expr (dstr, count, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, dst);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, val);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, count);
	dasprintf (dstr, "%*se_%p [label=\"memset\"];\n", indent, "", e);
}

static void
print_adjstk (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"adjstk %d:%d\\n%d\"];\n", indent, "", e,
			   e->adjstk.mode, e->adjstk.offset, e->loc.line);
}

static void
print_with (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	const expr_t *with = e->with.with;

	_print_expr (dstr, with, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, with);
	dasprintf (dstr, "%*se_%p [label=\"with %d:%d\\n%d\"];\n", indent, "", e,
			   e->with.mode, e->with.reg, e->loc.line);
}

static void
print_args (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	dasprintf (dstr, "%*se_%p [label=\"...\\n%d\"];\n", indent, "", e,
			   e->loc.line);
}

static void
print_horizontal (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;

	_print_expr (dstr, e->hop.vec, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, e->hop.vec);
	dasprintf (dstr, "%*se_%p [label=\"hop %s\\n%d\"];\n", indent, "", e,
			   get_op_string (e->hop.op), e->loc.line);
}

static void
print_swizzle (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	static char swizzle_components[] = "xyzw";
	int         indent = level * 2 + 2;
	ex_swizzle_t swiz = e->swizzle;
	int         count = type_width (swiz.type);
	const char *swizzle = "";

	for (int i = 0; i < count; i++) {
		if (swiz.zero & (1 << i)) {
			swizzle = va ("%s0", swizzle);
		} else {
			swizzle = va ("%s%s%c", swizzle,
						  swiz.neg & (1 << i) ? "-" : "",
						  swizzle_components[swiz.source[i]]);
		}
	}

	_print_expr (dstr, swiz.src, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, swiz.src);
	dasprintf (dstr, "%*se_%p [label=\"swizzle %s\\n%d\"];\n", indent, "", e,
			   swizzle, e->loc.line);
}

static void
print_extend (dstring_t *dstr, const expr_t *e, int level, int id, const expr_t *next)
{
	int         indent = level * 2 + 2;
	ex_extend_t extend = e->extend;
	int         ext = extend.extend;

	if (ext < 0 || ext > 3) {
		ext = 4;
	}
	_print_expr (dstr, extend.src, level, id, next);
	dasprintf (dstr, "%*se_%p -> \"e_%p\";\n", indent, "", e, extend.src);
	dasprintf (dstr, "%*se_%p [label=\"extend [%d>%d%s:%s]\\n%d\"];\n",
			   indent, "", e,
			   type_width (get_type (extend.src)), type_width (extend.type),
			   extend.reverse ? ":r" : "",
			   ((const char *[]){"0", "1", "c", "-1", "?"})[ext],
			   e->loc.line);
}

static void
print_multivec (dstring_t *dstr, const expr_t *e, int level, int id,
				const expr_t *next)
{
	int         indent = level * 2 + 2;
	auto algebra = e->multivec.algebra;
	int         count = list_count (&e->multivec.components);
	const expr_t *components[count];
	list_scatter (&e->multivec.components, components);

	for (int i = 0; i < count; i++) {
		_print_expr (dstr, components[i], level, id, 0);
		dasprintf (dstr, "%*se_%p:m%d -> e_%p;\n", indent, "", e, i,
				   components[i]);
	}

	int colspan = count ? count : 1;

	dasprintf (dstr, "%*se_%p [shape=none,label=<\n", indent, "", e);
	dasprintf (dstr, "%*s<table border=\"0\" cellborder=\"1\" "
			   "cellspacing=\"0\">\n", indent + 2, "");
	dasprintf (dstr, "%*s<tr><td colspan=\"%d\">&lt;multivec&gt;(%d)</td>"
			   "</tr>\n", indent + 4, "", colspan, e->loc.line);
	dstring_t  *typestr = dstring_newstr();
	print_type_str (typestr, e->multivec.type);
	dasprintf (dstr, "%*s<tr><td colspan=\"%d\">%s</td></tr>\n",
			   indent + 4, "", colspan, typestr->str);
	dstring_delete (typestr);

	if (count) {
		dasprintf (dstr, "%*s<tr>\n", indent + 4, "");
		for (int i = 0; i < count; i++) {
			auto mask = get_group_mask (get_type (components[i]), algebra);
			dasprintf (dstr, "%*s<td port=\"m%d\">%04x</td>\n", indent + 8, "",
					   i, mask);
		}
		dasprintf (dstr, "%*s</tr>\n", indent + 4, "");
	}
	dasprintf (dstr, "%*s</table>\n", indent + 2, "");
	dasprintf (dstr, "%*s>];\n", indent, "");
}

static void
_print_expr (dstring_t *dstr, const expr_t *e, int level, int id,
			 const expr_t *next)
{
	static print_f *print_funcs[ex_count] = {
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
		[ex_message] = print_message,
		[ex_nil] = print_nil,
		[ex_value] = print_value,
		[ex_compound] = print_compound,
		[ex_memset] = print_memset,
		[ex_alias] = print_alias,
		[ex_address] = print_address,
		[ex_assign] = print_assign,
		[ex_branch] = print_branch,
		[ex_inout] = print_inout,
		[ex_return] = print_return,
		[ex_adjstk] = print_adjstk,
		[ex_with] = print_with,
		[ex_args] = print_args,
		[ex_horizontal] = print_horizontal,
		[ex_swizzle] = print_swizzle,
		[ex_extend] = print_extend,
		[ex_multivec] = print_multivec,
		[ex_list] = print_list,
		[ex_type] = print_type_expr,
		[ex_incop] = print_incop,
		[ex_cond] = print_cond,
		[ex_field] = print_field,
		[ex_array] = print_array,
		[ex_decl] = print_decl,
		[ex_loop] = nullptr,
		[ex_select] = print_select,
		[ex_intrinsic] = print_intrinsic,
		[ex_xvalue] = print_xvalue,
		[ex_process] = print_process,
	};
	int         indent = level * 2 + 2;

	if (!e) {
		dasprintf (dstr, "%*s\"e_%p\" [label=\"(null)\"];\n", indent, "", e);
		return;
	}
	if (e->printid == id)		// already printed this expression
		return;
	((expr_t *) e)->printid = id;

	if ((int) e->type < 0 || e->type >= ex_count || !print_funcs[e->type]) {
		const char *type = va ("%d", e->type);
		if ((unsigned) e->type < ex_count) {
			type = expr_names[e->type];
		}
		dasprintf (dstr, "%*se_%p [label=\"(bad expr type: %s)\\n%d\"];\n",
				   indent, "", e, type, e->loc.line);
		return;
	}
	print_funcs[e->type] (dstr, e, level, id, next);

}

void
dump_dot_expr (const void *_e, const char *filename)
{
	static int  id = 0;
	dstring_t  *dstr = dstring_newstr ();
	const expr_t *e = _e;

	dasprintf (dstr, "digraph expr_%p {\n", e);
	dasprintf (dstr, "  graph [label=\"%s\"];\n",
			   filename ? quote_string (filename) : "");
	dasprintf (dstr, "  layout=dot; rankdir=TB; compound=true;\n");
	_print_expr (dstr, e, 0, ++id, nullptr);
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
print_expr (const expr_t *e)
{
	dump_dot_expr (e, 0);
}
