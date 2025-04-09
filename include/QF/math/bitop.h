/*
	bitop.h

	bit-op functions

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/1/23

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

#ifndef __QF_math_bitop_h
#define __QF_math_bitop_h

/** \defgroup mathlib_bitop Bit-op functions
	\ingroup utils
*/
///@{

#include "QF/qtypes.h"

#define BITOP_RUP1__(x)  (            (x) | (            (x) >>  1))
#define BITOP_RUP2__(x)  (BITOP_RUP1__(x) | (BITOP_RUP1__(x) >>  2))
#define BITOP_RUP4__(x)  (BITOP_RUP2__(x) | (BITOP_RUP2__(x) >>  4))
#define BITOP_RUP8__(x)  (BITOP_RUP4__(x) | (BITOP_RUP4__(x) >>  8))
#define BITOP_RUP16__(x) (BITOP_RUP8__(x) | (BITOP_RUP8__(x) >> 16))
/** Round x up to the next power of two.

	Rounds x up to the next power of two leaving exact powers of two
	untouched.

	\param x	The value to round
	\return		The next higher power of two or x if it already is a power
				of two.
*/
#define BITOP_RUP(x) (BITOP_RUP16__((uint32_t)(x) - 1) + 1)

#define BITOP_LOG2__(x) (((((x) & 0xffff0000) != 0) << 4) \
						|((((x) & 0xff00ff00) != 0) << 3) \
						|((((x) & 0xf0f0f0f0) != 0) << 2) \
						|((((x) & 0xcccccccc) != 0) << 1) \
						|((((x) & 0xaaaaaaaa) != 0) << 0))
/** Log base 2 rounded up.

	Finds the base 2 logarithm of x rounded up (ceil(log2(x))).

	\param x	The value for which to find the base 2 logarithm.
	\return     Log base 2 of x, rounded up (2 -> 1, 3 -> 2, 4 -> 2)
*/
#define BITOP_LOG2(x) BITOP_LOG2__(BITOP_RUP(x))

///@}

#endif // __QF_math_bitop_h
