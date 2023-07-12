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

*/
#ifndef __ruamoko_math_h
#define __ruamoko_math_h

/**
	\defgroup math Math Definitions
	\{
*/

/**
	Generate a random number such that 0 <= n <= 1 (0 to 1 inclusive)
*/
@extern float random (void);

///\name Conversions
//\{
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
@extern @overload float floor (float f);
@extern @overload double floor (double f);

/**
	Returns \a f, rounded up to the next highest integer
*/
@extern @overload float ceil (float f);
@extern @overload double ceil (double f);

/**
	Returns the absolute value of \a f
*/
@extern @overload float fabs (float f);
@extern @overload double fabs (double f);
//\}

///\name Exponentials and Logarithms
//\{
/**
	Returns e to the power of \a x.
*/
@extern @overload float exp (float x);
@extern @overload double exp (double x);

/**
	Returns the natural log of \a x.
*/
@extern @overload float log (float x);
@extern @overload double log (double x);

/**
	Returns the base-2 log of \a x.
*/
@extern @overload float log2 (float x);
@extern @overload double log2 (double x);

/**
	Returns the base-10 log of \a x.
*/
@extern @overload float log10 (float x);
@extern @overload double log10 (double x);

/**
	Returns \a x to the \a y power
*/
@extern @overload float pow (float x, float y);
@extern @overload double pow (double x, double y);

/**
	Returns the square root of \a x
*/
@extern @overload float sqrt (float x);
@extern @overload double sqrt (double x);

/**
	Returns the cube root of \a x
*/
@extern @overload float cbrt (float x);
@extern @overload double cbrt (double x);
//\}

///\name Trigonometric functions
//\{

/**
	Returns the sine of \a x.
*/
@extern @overload float sin (float x);
@extern @overload double sin (double x);

/**
	Returns the cosine of \a x.
*/
@extern @overload float cos (float x);
@extern @overload double cos (double x);

/**
	Returns the tangent of \a x.
*/
@extern @overload float tan (float x);
@extern @overload double tan (double x);

/**
	Returns the arcsine of \a x.
*/
@extern @overload float asin (float x);
@extern @overload double asin (double x);

/**
	Returns the arccosine of \a x.
*/
@extern @overload float acos (float x);
@extern @overload double acos (double x);

/**
	Returns the arctangent of \a x.
*/
@extern @overload float atan (float x);
@extern @overload double atan (double x);

/**
	Returns the arctangent of \a y / \a x preserving the quadrant.
*/
@extern @overload float atan2 (float y, float x);
@extern @overload double atan2 (double y, double x);

/**
	Returns the length of the hypotenuse of a right triangle with sides \a x
	and \a y. That is, this function returns
	<code>sqrt (\a x*\a x + \a y*\a y)</code>.
*/
@extern @overload float hypot (float x, float y);
@extern @overload double hypot (double x, double y);
//\}

///\name Hyperbolic functions
//\{
/**
	Returns the hyperbolic sine of \a x
*/
@extern @overload float sinh (float x);
@extern @overload double sinh (double x);

/**
	Returns the hyperbolic cosine of \a x
*/
@extern @overload float cosh (float x);
@extern @overload double cosh (double x);

/**
	Returns the hyperbolic tangent of \a x
*/
@extern @overload float tanh (float x);
@extern @overload double tanh (double x);

/**
	Returns the area hyperbolic sine of \a x
*/
@extern @overload float asinh (float x);
@extern @overload double asinh (double x);

/**
	Returns the area hyperbolic cosine of \a x
*/
@extern @overload float acosh (float x);
@extern @overload double acosh (double x);

/**
	Returns the area hyperbolic tangent of \a x
*/
@extern @overload float atanh (float x);
@extern @overload double atanh (double x);
//\}

///\name Vector Functions
//\{
/**
	Transform vector \a v into a unit vector (a vector with a length of 1).
	The direction is not changed, except for (possible) rounding errors.
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
//\}

/**
	\name Constants
	Constants for speeding up math calculations.
	These constants are defined to replace some common math functions. Since
	these are the same values that would be returned by any math functions or
	floating-point calculations, these allow you to get the results without
	actually calling them.
	\{
*/
/**
	Positive infinity. This is a special value replaced by the compiler with
	the actual floating-point value for a positive infinity. To get a negative
	infinity, just use -INFINITY.
*/
# define INFINITY	__INFINITY__
/**
	Euler's number \em e, the irrational base of the natural logarithm and a
	really neat thing
*/
# define M_E		2.7182818284590452354
# define M_LOG2E	1.4426950408889634074	///< The log, base 2, of \em e: <code>log2 (\ref M_E)</code>
# define M_LOG10E	0.43429448190325182765	///< The log, base 10, of \em e: <code>log10 (\ref M_E)</code>
# define M_LN2		0.69314718055994530942	///< The natural log evaluated at 2: <code>log (2)</code>
# define M_LN10		2.30258509299404568402	///< The natural log evaluated at 10: <code>log (10)</code>
# define M_PI		3.14159265358979323846	///< The most famous irrational number, and the sixteenth letter of the Greek alphabet
# define M_PI_2		1.57079632679489661923	///< Half of pi, (\ref M_PI/2)
# define M_PI_4		0.78539816339744830962	///< One quarter of pi, (\ref M_PI/4)
# define M_PI_6		0.52359877559829887308	///< One sixth of pi, (\ref M_PI/6)
# define M_1_PI		0.31830988618379067154	///< The reciprocal of pi, (1/\ref M_PI)
# define M_2_PI		0.63661977236758134308	///< Twice the reciprocal of pi, (2/\ref M_PI)
# define M_2_SQRTPI	1.12837916709551257390	///< Twice the reciprocal of the square root of pi, 2/\ref sqrt(\ref M_PI)
# define M_SQRT2	1.41421356237309504880	///< The square root of 2
# define M_SQRT1_2	0.70710678118654752440	///< 1/sqrt(2)
# define M_SQRT3	1.73205080756887729353	///< The square root of 3
# define M_SQRT1_3	0.57735026918962576451	///< 1/sqrt(3)
/**
	\}	Constants
	\}	Math Definitions
*/
#endif //__ruamoko_math_h
