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

#ifndef __exp_h
#define __exp_h

typedef enum {TOKEN_GENERIC, TOKEN_NUM, TOKEN_OP, TOKEN_OPAREN, TOKEN_CPAREN} 
token_type;
typedef enum {EXP_E_NORMAL = 0, EXP_E_PARSE, EXP_E_INVOP, EXP_E_PAREN, 
EXP_E_INVPARAM, EXP_E_SYNTAX} exp_error_t;
typedef float (*opfunc) (float op1, float op2);

typedef struct token_generic_s
{
	token_type type;
	union token_u *prev, *next;
} token_generic;

typedef struct token_num_s
{
	token_type type;
	union token_u *prev, *next;
	float value;
} token_num;

typedef struct token_op_s
{
	token_type type;
	union token_u *prev, *next;
	opfunc func;
} token_op;

typedef union token_u
{
	token_generic generic;
	token_num num;
	token_op op;
} token;

typedef struct optable_s
{
	char *str;
	opfunc func;
	unsigned int operands;
} optable_t;

extern exp_error_t EXP_ERROR;

token *EXP_ParseString (char *str);
void EXP_SimplifyTokens (token *chain);
void EXP_RemoveToken (token *tok);
void EXP_DestroyTokens (token *chain);
float EXP_Evaluate (char *str);
void EXP_InsertTokenAfter (token *spot, token *new);
exp_error_t EXP_Validate (token *chain);
void EXP_PrintTokens (token *chain);
#endif // __exp_h
