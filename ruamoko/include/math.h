/*
	math.h

	Built-in math function definitions

	Copyright (C) 2002 Bill Currie <taniwha@quakeforge.net>
	Copyright (C) 2002 Jeff Teunissen <deek@quakeforge.net>

	This file is part of the Ruamoko Standard Library.

	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.

	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifndef __ruamoko_math_h
#define __ruamoko_math_h

/*
	random

	Generate a random number such that 0 <= num <= 1 (0 to 1 inclusive)
*/
@extern float () random;

/*
	ftoi

	Returns the integer component of f
*/
@extern integer (float f) ftoi;

/*
	itof

	Returns the float representation of i
*/
@extern float (integer i) itof;

/*
	rint

	Rounds v to the nearest integer value and returns it.
	rint() does not change the type.
*/
@extern float (float v) rint;

/*
	floor

	Returns v, rounded down to the next lower integer
*/
@extern float (float v) floor;

/*
	ceil

	Returns v, rounded up to the next highest integer
*/
@extern float (float v) ceil;

/*
	fabs

	Returns the absolute value of v
*/
@extern float (float f) fabs;

/****************************************************************************
 *									VECTORS									*
 ****************************************************************************/

@extern vector v_forward, v_up, v_right;

/*
	normalize

	Transform vector v into a unit vector (a vector with a length of 1).
	The direction is not changed, except for (possible) roundoff errors.
*/
@extern vector (vector v) normalize;

/*
	vlen

	Return the length of vector v
*/
@extern float (vector v) vlen;

/*
	vectoyaw

	Returns the yaw angle ("bearing"), in degrees, associated with vector v.
*/
@extern float (vector v) vectoyaw;

/*
	vectoangles

	Returns a vector 'pitch yaw 0' corresponding to vector v.
*/
@extern vector (vector v) vectoangles;

@extern float (float x) sin;
@extern float (float x) cos;
@extern float (float x) tan;
@extern float (float x) asin;
@extern float (float x) acos;
@extern float (float x) atan;
@extern float (float y, float x) atan2;
@extern float (float x) log;
@extern float (float x) log10;
@extern float (float x, float y) pow;
@extern float (float x) sinh;
@extern float (float x) cosh;
@extern float (float x) tanh;
@extern float (float x) asinh;
@extern float (float x) acosh;
@extern float (float x) atanh;

#endif //__ruamoko_math_h
