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

#include "QF/exp.h"
#include "QF/ops.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

exp_error_t EXP_ERROR;

optable_t optable[] = 
{
	{"**", OP_Exp},
	{"*", OP_Mult},
	{"/", OP_Div},
	{"+", OP_Add},
	{"-", OP_Sub},
	{"==", OP_Eq},
	{"!=", OP_Neq},
	{">=", OP_GreaterThanEqual},
	{">", OP_GreaterThan},
	{"<=", OP_LessThanEqual},
	{"<", OP_LessThan},
	{"||", OP_Or},
	{"&&", OP_And},
	{"", 0}
};

token *EXP_NewToken (void)
{
	token *new;

	new = malloc(sizeof(token));

	if (!new)
		return 0;
	new->generic.type = TOKEN_GENERIC;
	return new;
}

token *EXP_ParseString (char *str)
{
	char buf[256];
	
	token *chain, *new, *cur;
	int i,n,m;
	
	cur = chain = EXP_NewToken();
	chain->generic.type = TOKEN_OPAREN;
	chain->generic.prev = 0;
	chain->generic.next = 0;

	for (i = 0; i < strlen(str); i++)
	{
		m = 0;
		while(str[i] == ' ')
			i++;
		if (isdigit(str[i]) || str[i] == '.')
		{
			while ((isdigit(str[i]) || str[i] == '.') && i < strlen(str) && m < 256)
				buf[m++] = str[i++];
			buf[m] = 0;
			new = EXP_NewToken();
			new->generic.type = TOKEN_NUM;
			new->num.value = atof(buf);
			new->generic.prev = cur;
			new->generic.next = 0;
			cur->generic.next = new;
			cur = new;
			i--;
		}
		else if (str[i] == '(')
		{
			new = EXP_NewToken();
			new->generic.type = TOKEN_OPAREN;
			new->generic.prev = cur;
			new->generic.next = 0;
			cur->generic.next = new;
			cur = new;
		}
		else if (str[i] == ')')
		{
			new = EXP_NewToken();
			new->generic.type = TOKEN_CPAREN;
			new->generic.prev = cur;
			new->generic.next = 0;
			cur->generic.next = new;
			cur = new;
		}
		else
		{
			while(!(isdigit(str[i])) && str[i] != '.' && str[i] != '(' && str[i] != ')' && m < 256)
				buf[m++] = str[i++];
			buf[m] = 0;
			if (m)
			{
				for (n = 0; optable[n].func; n++)
				{
					if (!(strncmp(optable[n].str, buf, strlen(optable[n].str))))
					{
						i -= (m - strlen(optable[n].str) + 1);
						new = EXP_NewToken();
						new->generic.type = TOKEN_OP;
						new->op.func = optable[n].func;
						new->generic.prev = cur;
						new->generic.next = 0;
						cur->generic.next = new;
						cur = new;
						break;
					}
				}
				if (!(optable[n].func)) {
					EXP_DestroyTokens (chain);
					return 0;
				}
			}
		}
	}
	new = EXP_NewToken();
	new->generic.type = TOKEN_CPAREN;
	new->generic.prev = cur;
	new->generic.next = 0;
	cur->generic.next = new;
	return chain;
}

void EXP_SimplifyTokens (token *chain)
{
	int i;
	token *cur;
	token *temp;

	/* First, get rid of parentheses */

	for (cur = chain->generic.next; cur->generic.type != TOKEN_CPAREN; cur = cur->generic.next)
	{
		if (cur->generic.type == TOKEN_OPAREN)
		{
			EXP_SimplifyTokens(cur); /* Call ourself to simplify parentheses content */
			temp = cur;
			cur = cur->generic.next;
			EXP_RemoveToken(temp); /* Remove parentheses, leaving value behind */
			EXP_RemoveToken(cur->generic.next);
		}
	}

	/* Next, evaluate all operators in order of operations */

	for (i = 0; optable[i].func; i++)
	{
		for (cur = chain->generic.next; cur->generic.type != TOKEN_CPAREN; cur = cur->generic.next)
		{
			if (cur->generic.type == TOKEN_OP && cur->op.func == optable[i].func && cur->generic.next)
				if (cur->generic.prev->generic.type == TOKEN_NUM && cur->generic.next->generic.type == TOKEN_NUM)
				{
					cur->generic.prev->num.value = optable[i].func(cur->generic.prev->num.value, cur->generic.next->num.value);
					temp = cur;
					cur = cur->generic.prev;
					EXP_RemoveToken(temp->generic.next);
					EXP_RemoveToken(temp);
				}
		}
	}
}

void EXP_RemoveToken (token *tok)
{
	tok->generic.prev->generic.next = tok->generic.next;
	tok->generic.next->generic.prev = tok->generic.prev;
	free(tok);
}

void EXP_DestroyTokens (token *chain)
{
	token *temp;
	for (;chain; chain = temp)
	{
		temp = chain->generic.next;
		free(chain);
	}
}

float EXP_Evaluate (char *str)
{
	token *chain;
	float res;	

	EXP_ERROR = EXP_E_NORMAL;

	if (!(chain = EXP_ParseString (str)))
	{
		EXP_ERROR = EXP_E_PARSE;
		return 0;
	}
	res = EXP_Validate (chain);
	
	if (res)
	{
		EXP_DestroyTokens (chain);
		EXP_ERROR = res;
		return 0;
	}
	
	EXP_SimplifyTokens (chain);
	res = chain->generic.next->num.value;
	
	EXP_DestroyTokens (chain);
	return res;
}

void EXP_InsertTokenAfter (token *spot, token *new)
{
	spot->generic.next->generic.prev = new;
	new->generic.next = spot->generic.next;
	new->generic.prev = spot;
	spot->generic.next = new;
}


exp_error_t EXP_Validate (token *chain)
{
	token *cur, *new;
	int paren = 0;
	
	for (cur = chain; cur->generic.next; cur = cur->generic.next)
	{
		if (cur->generic.type == TOKEN_OPAREN)
			paren++;
		if (cur->generic.type == TOKEN_CPAREN)
			paren--;
		/* Implied multiplication */
		if ((cur->generic.type == TOKEN_NUM && cur->generic.next->generic.type == TOKEN_OPAREN) || /* 5(1+1) */
			(cur->generic.type == TOKEN_CPAREN && cur->generic.next->generic.type == TOKEN_NUM) || /* (1+1)5 */
			(cur->generic.type == TOKEN_CPAREN && cur->generic.next->generic.type == TOKEN_OPAREN)) /* (1+1)(1+1) */
		{
			new = EXP_NewToken ();
			new->generic.type = TOKEN_OP;
			new->op.func = OP_Mult;
			EXP_InsertTokenAfter (cur, new);
		}

		if ((cur->generic.type == TOKEN_OP || cur->generic.type == TOKEN_OPAREN) && cur->generic.next->generic.type == TOKEN_OP)
		{
			if (cur->generic.next->op.func == OP_Sub) /* Stupid hack for negation */
			{
				cur = cur->generic.next;
				cur->generic.type = TOKEN_NUM;
				cur->num.value = -1.0;
				new = EXP_NewToken();
				new->generic.type = TOKEN_OP;
				new->op.func = OP_Mult;
				EXP_InsertTokenAfter (cur, new);
			}
			else
				EXP_DestroyTokens (chain);
				return EXP_E_SYNTAX; /* Operator misuse */
		}
		else if (cur->generic.type == TOKEN_NUM && cur->generic.next->generic.type == TOKEN_NUM)
			return EXP_E_SYNTAX; /* Double number error */
		else if (cur->generic.type == TOKEN_OPAREN && cur->generic.next->generic.type == TOKEN_CPAREN)
			return EXP_E_PAREN; /* Pointless parentheses */

	}

	paren--;
	if (paren)
		return EXP_E_PAREN; /* Paren mismatch */
	return 0;
}

void EXP_PrintTokens (token *chain)
{
	int m;
	for (; chain; chain = chain->generic.next)
		switch (chain->generic.type)
		{
			case TOKEN_OPAREN:
				printf("(");
				break;
			case TOKEN_CPAREN:
				printf(")");
				break;
			case TOKEN_NUM:
				printf("%f", chain->num.value);
				break;
			case TOKEN_OP:
				for (m = 0; optable[m].func != chain->op.func; m++);
				printf("%s", optable[m].str);
			case TOKEN_GENERIC:
				break;
		}
}
