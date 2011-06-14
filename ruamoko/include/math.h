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

/**
	\defgroup math Math Functions
	\{
*/

/**
	Generate a random number such that 0 <= n <= 1 (0 to 1 inclusive)
*/
@extern float random (void);

/**
	Returns the integer component of \a f
*/
@extern int ftoi (float f);

/**
	Returns the float representation of \a i
*/
@extern float itof (int i);

/**
	Rounds \a f to the nearest integer value and returns it. Does not change the type.
*/
@extern float rint (float f);

/**
	Returns \a f, rounded down to the next lower integer
*/
@extern float floor (float f);

/**
	Returns \a f, rounded up to the next highest integer
*/
@extern float ceil (float f);

/**
	Returns the absolute value of \a f
*/
@extern float fabs (float f);

/****************************************************************************
 *									VECTORS									*
 ****************************************************************************/

/**
	Transform vector \a v into a unit vector (a vector with a length of 1).
	The direction is not changed, except for (possible) roundoff errors.
*/
@extern vector normalize (vector v);

/**
	Return the length of vector \a v
*/
@extern float vlen (vector v);

/**
	Returns the yaw angle ("bearing"), in degrees, associated with vector \a v.
*/
@extern float vectoyaw (vector v);

/**
	Returns a vector 'pitch yaw 0' corresponding to vector \a v.
*/
@extern vector vectoangles (vector v);

/**
	Returns the sine of \a x.
*/
@extern float sin (float x);

/**
	Returns the cosine of \a x.
*/
@extern float cos (float x);

/**
	Returns the tangent of \a x.
*/
@extern float tan (float x);

/**
	Returns the arcsine of \a x.
*/
@extern float asin (float x);

/**
	Returns the arccosine of \a x.
*/
@extern float acos (float x);

/**
	Returns the arctangent of \a x.
*/
@extern float atan (float x);
@extern float atan2 (float y, float x);

/**
	Returns the natural log of \a x.
*/
@extern float log (float x);

/**
	Returns the base-10 log of \a x.
*/
@extern float log10 (float x);

/**
	Returns \a x to the \a y power
*/
@extern float pow (float x, float y);

@extern float sinh (float x);
@extern float cosh (float x);
@extern float tanh (float x);
@extern float asinh (float x);
@extern float acosh (float x);
@extern float atanh (float x);

//\}

#endif //__ruamoko_math_h
