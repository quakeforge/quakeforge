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

#ifndef __ops_h
#define __ops_h

double OP_Not (double op1, double op2);
double OP_Add (double op1, double op2);
double OP_Sub (double op1, double op2);
double OP_Mult (double op1, double op2);
double OP_Div (double op1, double op2);
double OP_Exp (double op1, double op2);
double OP_Eq (double op1, double op2);
double OP_Neq (double op1, double op2);
double OP_Or (double op1, double op2);
double OP_And (double op1, double op2);
double OP_GreaterThan (double op1, double op2);
double OP_LessThan (double op1, double op2);
double OP_GreaterThanEqual (double op1, double op2);
double OP_LessThanEqual (double op1, double op2);

double Func_Sin (double *oplist, unsigned int numops);
double Func_Cos (double *oplist, unsigned int numops);
double Func_Tan (double *oplist, unsigned int numops);
double Func_Asin (double *oplist, unsigned int numops);
double Func_Acos (double *oplist, unsigned int numops);
double Func_Atan (double *oplist, unsigned int numops);
#endif // __ops_h
