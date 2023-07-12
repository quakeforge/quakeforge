/*

	EXP equation evaluation routines
	Copyright (C) 2000, 2001  Brian Koropoff

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, US

*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "QF/qtypes.h"
#include "QF/va.h"

#include "exp.h"
#include "ops.h"

exp_error_t EXP_ERROR;
char       *exp_error_msg = 0;

optable_t   optable[] = {
	{"~", OP_BitInv, 1},
	{"!", OP_Not, 1},
	{"&", OP_BitAnd, 2},
	{"|", OP_BitOr, 2},
	{"^", OP_BitXor, 2},
	{"**", OP_Exp, 2},
	{"/", OP_Div, 2},
	{"neg", OP_Negate, 1},
	{"*", OP_Mult, 2},
	{"+", OP_Add, 2},
	{"-", OP_Sub, 2},
	{"==", OP_Eq, 2},
	{"!=", OP_Neq, 2},
	{">=", OP_GreaterThanEqual, 2},
	{">", OP_GreaterThan, 2},
	{"<=", OP_LessThanEqual, 2},
	{"<", OP_LessThan, 2},
	{"||", OP_Or, 2},
	{"or", OP_Or, 2},
	{"&&", OP_And, 2},
	{"and", OP_And, 2},
	{"", 0, 0}
};

functable_t functable[] = {
	{"sqrt", Func_Sqrt, 1},
	{"abs", Func_Abs, 1},
	{"sin", Func_Sin, 1},
	{"cos", Func_Cos, 1},
	{"tan", Func_Tan, 1},
	{"asin", Func_Asin, 1},
	{"acos", Func_Acos, 1},
	{"atan", Func_Atan, 1},
	{"rand", Func_Rand, 2},
	{"trunc", Func_Trunc, 1},
	{"", 0, 0}
};

// Error handling
static      exp_error_t
EXP_Error (exp_error_t err, const char *msg)
{
	EXP_ERROR = err;
	if (exp_error_msg)
		free (exp_error_msg);
	exp_error_msg = strdup (msg);
	return err;
}

const char *
EXP_GetErrorMsg (void)
{
	return exp_error_msg;
}

static token *
EXP_NewToken (void)
{
	token      *new;

	new = malloc (sizeof (token));

	if (!new)
		return 0;
	new->generic.type = TOKEN_GENERIC;
	return new;
}

/*
int
EXP_FindIndexByFunc (opfunc func)
{
	int i;
	for (i = 0; optable[i].func; i++)
		if (func == optable[i].func)
			return i;
	return -1;
}
*/

static optable_t *
EXP_FindOpByStr (const char *str)
{
	size_t      i, len;
	int         fi;

	for (i = 0, len = 0, fi = -1; optable[i].func; i++)
		if (!strncmp (str, optable[i].str, strlen (optable[i].str))
			&& strlen (optable[i].str) > len) {
			len = strlen (optable[i].str);
			fi = i;
		}
	if (fi >= 0)
		return optable + fi;
	else
		return 0;
}

static functable_t *
EXP_FindFuncByStr (const char *str)
{
	size_t      i, len;
	int         fi;

	for (i = 0, len = 0, fi = -1; functable[i].func; i++)
		if (!strncmp (str, functable[i].str, strlen (functable[i].str))
			&& strlen (functable[i].str) > len) {
			len = strlen (functable[i].str);
			fi = i;
		}
	if (fi >= 0)
		return functable + fi;
	else
		return 0;
}

static __attribute__((pure)) int
EXP_ContainsCommas (token * chain)
{
	token      *cur;
	int         paren = 0;

	for (cur = chain; cur; cur = cur->generic.next) {
		if (cur->generic.type == TOKEN_OPAREN)
			paren++;
		if (cur->generic.type == TOKEN_CPAREN)
			paren--;
		if (!paren)
			return 0;
		if (cur->generic.type == TOKEN_COMMA)
			return 1;
	}
	return -1;							// We should never get here
}

static int
EXP_DoFunction (token * chain)
{
	token      *cur, *temp;
	double     *oplist = 0;
	double      value;
	unsigned int numops = 0;


	for (cur = chain->generic.next; cur; cur = temp) {
		if (cur->generic.type != TOKEN_CPAREN)
			temp = cur->generic.next;
		else
			temp = 0;
		if (cur->generic.type == TOKEN_NUM) {
			numops++;
			oplist = realloc (oplist, sizeof (double) * numops);
			oplist[numops - 1] = cur->num.value;
		}
		EXP_RemoveToken (cur);
	}
	if (numops == chain->func.func->operands) {
		value = chain->func.func->func (oplist, numops);	// Heh
		chain->generic.type = TOKEN_NUM;
		chain->num.value = value;
		if (oplist)
			free (oplist);
		return 0;
	} else {
		if (oplist)
			free (oplist);
		return -1;
	}
	return -2;							// We shouldn't get here
}

static int
EXP_DoUnary (token * chain)
{
	if (chain->generic.next->generic.type == TOKEN_OP)
		EXP_DoUnary (chain->generic.next);
	if (chain->generic.next->generic.type != TOKEN_NUM)
		return -1;						// In theory, this should never happen
	chain->generic.next->num.value =
		chain->op.op->func (chain->generic.next->num.value, 0);
	EXP_RemoveToken (chain);
	return 0;
}

token *
EXP_ParseString (char *str)
{
	char        buf[257];

	token      *chain, *new, *cur;
	size_t      i, m;
	optable_t  *op;
	functable_t *func;

	cur = chain = EXP_NewToken ();
	chain->generic.type = TOKEN_OPAREN;
	chain->generic.prev = 0;
	chain->generic.next = 0;

	for (i = 0; i < strlen (str); i++) {
		m = 0;
		while (isspace ((byte) str[i]))
			i++;
		if (!str[i])
			break;
		if (isdigit ((byte) str[i]) || str[i] == '.') {
			while ((isdigit ((byte) str[i])	// A number
					|| str[i] == '.'	// A decimal point
					|| str[i] == 'e'	// An exponent
					|| ((str[i] == '-' || str[i] == '+')
						&& str[i - 1] == 'e'))	// A + or - after an exponent
				   && i < strlen (str)	// We are within the string
				   && m < 256)			// And there is space in the buffer
				buf[m++] = str[i++];
			buf[m] = 0;
			new = EXP_NewToken ();
			new->generic.type = TOKEN_NUM;
			new->num.value = atof (buf);
			new->generic.prev = cur;
			new->generic.next = 0;
			cur->generic.next = new;
			cur = new;
			i--;
		} else if (str[i] == ',') {
			new = EXP_NewToken ();
			new->generic.type = TOKEN_COMMA;
			new->generic.prev = cur;
			new->generic.next = 0;
			cur->generic.next = new;
			cur = new;
		} else if (str[i] == '(') {
			new = EXP_NewToken ();
			new->generic.type = TOKEN_OPAREN;
			new->generic.prev = cur;
			new->generic.next = 0;
			cur->generic.next = new;
			cur = new;
		} else if (str[i] == ')') {
			new = EXP_NewToken ();
			new->generic.type = TOKEN_CPAREN;
			new->generic.prev = cur;
			new->generic.next = 0;
			cur->generic.next = new;
			cur = new;
		} else {
			while (!(isdigit ((byte) str[i])) && !isspace ((byte) str[i])
				   && str[i] != '.' && str[i] != '(' && str[i] != ')'
				   && str[i] != ',' && m < 256) {
				buf[m++] = str[i++];
			}
			buf[m] = 0;
			if (m) {
				if ((op = EXP_FindOpByStr (buf))) {
					i -= (m - strlen (op->str) + 1);
					new = EXP_NewToken ();
					new->generic.type = TOKEN_OP;
					new->op.op = op;
					new->generic.prev = cur;
					new->generic.next = 0;
					cur->generic.next = new;
					cur = new;
				} else if ((func = EXP_FindFuncByStr (buf))) {
					i -= (m - strlen (func->str) + 1);
					new = EXP_NewToken ();
					new->generic.type = TOKEN_FUNC;
					new->func.func = func;
					new->generic.prev = cur;
					new->generic.next = 0;
					cur->generic.next = new;
					cur = new;
				} else {
					EXP_DestroyTokens (chain);
					EXP_Error (EXP_E_INVOP,
							   va (0, "Unknown operator or function '%s'.", buf));
					return 0;
				}
			}
		}
	}
	new = EXP_NewToken ();
	new->generic.type = TOKEN_CPAREN;
	new->generic.prev = cur;
	new->generic.next = 0;
	cur->generic.next = new;
	return chain;
}

exp_error_t
EXP_SimplifyTokens (token * chain)
{
	exp_error_t res;
	int         i;
	token      *cur;
	token      *temp;

	/* First, get rid of parentheses */

	for (cur = chain->generic.next; cur->generic.type != TOKEN_CPAREN;
		 cur = cur->generic.next) {
		if (cur->generic.type == TOKEN_OPAREN) {
			res = EXP_SimplifyTokens (cur);	/* Call ourself to simplify
											   parentheses content */
			if (res)
				return res;
			if (cur->generic.prev->generic.type == TOKEN_FUNC) {
				// These are arguments to a function
				cur = cur->generic.prev;
				if (EXP_DoFunction (cur))
					return EXP_Error (EXP_E_SYNTAX,
									  va (0, "Invalid number of arguments to function '%s'.",
									   cur->func.func->str));
			} else {
				if (EXP_ContainsCommas (cur))
					return EXP_Error (EXP_E_SYNTAX,
									  "Comma used outside of a function argument list.");
				temp = cur;
				cur = cur->generic.next;
				EXP_RemoveToken (temp);	/* Remove parentheses, leaving value
										   behind */
				EXP_RemoveToken (cur->generic.next);
			}
		}
	}

	/* Next, evaluate all operators in order of operations */

	for (i = 0; optable[i].func; i++) {
		for (cur = chain->generic.next; cur->generic.type != TOKEN_CPAREN;
			 cur = cur->generic.next) {
			if (cur->generic.type == TOKEN_OP && cur->op.op == optable + i
				&& cur->generic.next) {
				// If a unary operator is in our way, it gets evaluated early
				if (cur->generic.next->generic.type == TOKEN_OP)
					if (EXP_DoUnary (cur->generic.next))
						return EXP_Error (EXP_E_SYNTAX,
										  va (0, "Unary operator '%s' not followed by a unary operator or numerical value.",
										   cur->generic.next->op.op->str));
				if (optable[i].operands == 1
					&& cur->generic.next->generic.type == TOKEN_NUM) {
					cur->generic.next->num.value =
						optable[i].func (cur->generic.next->num.value, 0);
					temp = cur;
					cur = cur->generic.next;
					EXP_RemoveToken (temp);
				} else if (cur->generic.prev->generic.type == TOKEN_NUM
						   && cur->generic.next->generic.type == TOKEN_NUM) {
					cur->generic.prev->num.value =
						optable[i].func (cur->generic.prev->num.value,
										 cur->generic.next->num.value);
					temp = cur;
					cur = cur->generic.prev;
					EXP_RemoveToken (temp->generic.next);
					EXP_RemoveToken (temp);
				}
			}
		}
	}
	return EXP_E_NORMAL;
}

void
EXP_RemoveToken (token * tok)
{
	tok->generic.prev->generic.next = tok->generic.next;
	tok->generic.next->generic.prev = tok->generic.prev;
	free (tok);
}

void
EXP_DestroyTokens (token * chain)
{
	token      *temp;

	for (; chain; chain = temp) {
		temp = chain->generic.next;
		free (chain);
	}
}

double
EXP_Evaluate (char *str)
{
	token      *chain;
	double      res;

	EXP_ERROR = EXP_E_NORMAL;

	if (!(chain = EXP_ParseString (str)))
		return 0;
	if (EXP_Validate (chain)) {
		EXP_DestroyTokens (chain);
		return 0;
	}
	if (EXP_SimplifyTokens (chain)) {
		EXP_DestroyTokens (chain);
		return 0;
	}
	res = chain->generic.next->num.value;
	EXP_DestroyTokens (chain);
	return res;
}

void
EXP_InsertTokenAfter (token * spot, token * new)
{
	spot->generic.next->generic.prev = new;
	new->generic.next = spot->generic.next;
	new->generic.prev = spot;
	spot->generic.next = new;
}


exp_error_t
EXP_Validate (token * chain)
{
	token      *cur, *new;
	int         paren = 0;

	for (cur = chain; cur->generic.next; cur = cur->generic.next) {
		if (cur->generic.type == TOKEN_OPAREN)
			paren++;
		if (cur->generic.type == TOKEN_CPAREN)
			paren--;
		/* Implied multiplication */
		if ((cur->generic.type == TOKEN_NUM && (cur->generic.next->generic.type == TOKEN_OPAREN ||	// 5(1+1)
												cur->generic.next->generic.type == TOKEN_FUNC ||	// 5 sin (1+1)
												(cur->generic.next->generic.type == TOKEN_OP && cur->generic.next->op.op->operands == 1))) ||	// 5!(1+1)
			(cur->generic.type == TOKEN_CPAREN && (cur->generic.next->generic.type == TOKEN_NUM ||	// (1+1)5
												   cur->generic.next->generic.type == TOKEN_OPAREN)))	// (1+1)(1+1)
		{
			new = EXP_NewToken ();
			new->generic.type = TOKEN_OP;
			new->op.op = EXP_FindOpByStr ("*");
			EXP_InsertTokenAfter (cur, new);
		} else if ((cur->generic.type == TOKEN_OP
				 || cur->generic.type == TOKEN_OPAREN)
				&& cur->generic.next->generic.type == TOKEN_OP) {
			if (cur->generic.next->op.op->func == OP_Sub)	/* Stupid hack for
															   negation */
				cur->generic.next->op.op = EXP_FindOpByStr ("neg");
			else if (cur->generic.next->op.op->operands == 2)
				return EXP_Error (EXP_E_SYNTAX,
								  va (0, "Operator '%s' does not follow a number or numerical value.",
								   cur->generic.next->op.op->str));
		} else if (cur->generic.type == TOKEN_FUNC
				   && cur->generic.next->generic.type != TOKEN_OPAREN)
			return EXP_Error (EXP_E_SYNTAX,
							  va (0, "Function '%s' called without an argument list.",
							   cur->func.func->str));
		else if (cur->generic.type == TOKEN_COMMA
				 &&
				 ((cur->generic.prev->generic.type != TOKEN_CPAREN
				   && cur->generic.prev->generic.type != TOKEN_NUM)
				  || paren <= 1))
			return EXP_Error (EXP_E_SYNTAX,
							  "Comma used outside of a function or after a non-number.");
		else if (cur->generic.type == TOKEN_OP
				 && cur->generic.next->generic.type == TOKEN_CPAREN)
			return EXP_Error (EXP_E_SYNTAX,
							  va (0, "Operator '%s' is missing an operand.",
								  cur->op.op->str));
		else if (cur->generic.type == TOKEN_NUM
				 && cur->generic.next->generic.type == TOKEN_NUM)
			return EXP_Error (EXP_E_SYNTAX,
							  "Double number error (two numbers next two each other without an operator).");
		else if (cur->generic.type == TOKEN_OPAREN
				 && cur->generic.next->generic.type == TOKEN_CPAREN)
			return EXP_Error (EXP_E_PAREN, "Empty parentheses found.");
	}

	paren--;
	if (paren)
		return EXP_Error (EXP_E_PAREN, "Parenthesis mismatch.");
	return 0;
}

void
EXP_PrintTokens (token * chain)
{
	for (; chain; chain = chain->generic.next)
		switch (chain->generic.type) {
			case TOKEN_OPAREN:
				printf ("(");
				break;
			case TOKEN_CPAREN:
				printf (")");
				break;
			case TOKEN_COMMA:
				printf (",");
				break;
			case TOKEN_NUM:
				printf ("%f", chain->num.value);
				break;
			case TOKEN_OP:
				printf ("%s", chain->op.op->str);
				break;
			case TOKEN_FUNC:
				printf ("%s", chain->func.func->str);
				break;
			case TOKEN_GENERIC:
				break;
		}
	printf ("\n");
}
