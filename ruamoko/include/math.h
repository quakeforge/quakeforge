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
@extern float random (void);

/*
	ftoi

	Returns the integer component of f
*/
@extern integer ftoi (float f);

/*
	itof

	Returns the float representation of i
*/
@extern float itof (integer i);

/*
	rint

	Rounds v to the nearest integer value and returns it.
	rint() does not change the type.
*/
@extern float rint (float v);

/*
	floor

	Returns v, rounded down to the next lower integer
*/
@extern float floor (float v);

/*
	ceil

	Returns v, rounded up to the next highest integer
*/
@extern float ceil (float v);

/*
	fabs

	Returns the absolute value of v
*/
@extern float fabs (float f);

/****************************************************************************
 *									VECTORS									*
 ****************************************************************************/

@extern vector v_forward, v_up, v_right;

/*
	normalize

	Transform vector v into a unit vector (a vector with a length of 1).
	The direction is not changed, except for (possible) roundoff errors.
*/
@extern vector normalize (vector v);

/*
	vlen

	Return the length of vector v
*/
@extern float vlen (vector v);

/*
	vectoyaw

	Returns the yaw angle ("bearing"), in degrees, associated with vector v.
*/
@extern float vectoyaw (vector v);

/*
	vectoangles

	Returns a vector 'pitch yaw 0' corresponding to vector v.
*/
@extern vector vectoangles (vector v);

@extern float sin (float x);
@extern float cos (float x);
@extern float tan (float x);
@extern float asin (float x);
@extern float acos (float x);
@extern float atan (float x);
@extern float atan2 (float y, float x);
@extern float log (float x);
@extern float log10 (float x);
@extern float pow (float x, float y);
@extern float sinh (float x);
@extern float cosh (float x);
@extern float tanh (float x);
@extern float asinh (float x);
@extern float acosh (float x);
@extern float atanh (float x);

#endif //__ruamoko_math_h
