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

#include <math.h>

float OP_Not (float op1, float op2)
{
	return !op1;
}

float OP_Add (float op1, float op2)
{
	return op1 + op2;
}

float OP_Sub (float op1, float op2)
{
	return op1 - op2;
}

float OP_Mult (float op1, float op2)
{
	return op1 * op2;
}

float OP_Div (float op1, float op2)
{
	return op1 / op2;
}

float OP_Exp (float op1, float op2)
{
	return pow(op1, op2);
}

float OP_Eq (float op1, float op2)
{
	return op1 == op2;
}

float OP_Neq (float op1, float op2)
{
	return op1 != op2;
}

float OP_Or (float op1, float op2)
{
	return op1 || op2;
}

float OP_And (float op1, float op2)
{
	return op1 && op2;
}
float OP_GreaterThan (float op1, float op2)
{
	return op1 > op2;
}

float OP_LessThan (float op1, float op2)
{
	return op1 < op2;
}
float OP_GreaterThanEqual (float op1, float op2)
{
	return op1 >= op2;
}
float OP_LessThanEqual (float op1, float op2)
{
	return op1 <= op2;
}
