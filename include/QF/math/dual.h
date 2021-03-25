/*
	dual.h

	Dual and dual-quaternion functions

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/5/1

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

#ifndef __QF_math_dual_h
#define __QF_math_dual_h

/** \defgroup mathlib_dual Dual and dual quaternion functions
	\ingroup utils
*/
///@{

#include "QF/qtypes.h"

#define DualAdd(a,b,c) \
	do { \
		(c).r = (a).r + (b).r; \
		(c).e = (a).e + (b).e; \
	} while (0)
#define DualSubtract(a,b,c) \
	do { \
		(c).r = (a).r - (b).r; \
		(c).e = (a).e - (b).e; \
	} while (0)
#define DualNegate(a,b) \
	do { \
		(b).r = -(a).r; \
		(b).e = -(a).e; \
	} while (0)
#define DualConj(a,b) \
	do { \
		(b).r = (a).r; \
		(b).e = -(a).e; \
	} while (0)
#define DualMult(a,b,c) \
	do { \
		(c).e = (a).r * (b).e + (a).e * (b).r; \
		(c).r = (a).r * (b).r; \
	} while (0)
#define DualMultAdd(a,s,b,c) \
	do { \
		(c).r = (a).r + (s) * (b).r; \
		(c).e = (a).e + (s) * (b).e; \
	} while (0)
#define DualMultSub(a,s,b,c) \
	do { \
		(c).r = (a).r - (s) * (b).r; \
		(c).e = (a).e - (s) * (b).e; \
	} while (0)
#define DualNorm(a) ((a).r)
#define DualScale(a,b,c) \
	do { \
		(c).r = (a).r * (b); \
		(c).e = (a).e * (b); \
	} while (0)
#define DualCompCompare(x, op, y) ((x).r op (y).r) && ((x).e op (y).e)
#define DualCompare(x, y) DualCompCompare (x, ==, y)
#define DualIsZero(a) ((a).r == 0 && (a).e == 0)
#define DualIsUnit(a) (((a).r - 1) * ((a).r - 1) < 1e-6 && (a).e * (a).e < 1e-6)
#define DualSet(ar,ae,a) \
	do { \
		(a).ar = r; \
		(a).er = r; \
	} while (0)
#define DualZero(a) \
	do { \
		(a).e = (a).r = 0; \
	} while (0)
#define DualBlend(d1,d2,b,d) \
	do { \
		(d).r = (d1).r * (1 - (b)) + (d2).r * (b); \
		(d).e = (d1).e * (1 - (b)) + (d2).e * (b); \
	} while (0)
#define DualExpand(d) (d).r, (d).e

#define DualQuatAdd(a,b,c) \
	do { \
		QuatAdd ((a).q0.q, (b).q0.q, (c).q0.q); \
		QuatAdd ((a).qe.q, (b).qe.q, (c).qe.q); \
	} while (0)
#define DualQuatSubtract(a,b,c) \
	do { \
		QuatSub ((a).q0.q, (b).q0.q, (c).q0.q); \
		QuatSub ((a).qe.q, (b).qe.q, (c).qe.q); \
	} while (0)
#define DualQuatNegate(a,b) \
	do { \
		QuatNegate ((a).q0.q, (b).q0.q); \
		QuatNegate ((a).qe.q, (b).qe.q); \
	} while (0)
#define DualQuatConjQ(a,b) \
	do { \
		QuatConj ((a).q0.q, (b).q0.q); \
		QuatConj ((a).qe.q, (b).qe.q); \
	} while (0)
#define DualQuatConjE(a,b) \
	do { \
		(b).q0 = (a).q0; \
		QuatNegate ((a).qe.q, (b).qe.q); \
	} while (0)
#define DualQuatConjQE(a,b) \
	do { \
		QuatConj ((a).q0.q, (b).q0.q); \
		(b).qe.sv.s = -(a).qe.sv.s; \
		VectorCopy ((a).qe.sv.v, (b).qe.sv.v); \
	} while (0)
#define DualQuatMult(a,b,c) \
	do { \
		Quat_t      t; \
		QuatMult ((a).q0.q, (b).qe.q, t.q); \
		QuatMult ((a).qe.q, (b).q0.q, (c).qe.q); \
		QuatAdd (t.q, (c).qe.q, (c).qe.q); \
		QuatMult ((a).q0.q, (b).q0.q, (c).q0.q); \
	} while (0);
#define DualQuatMultAdd(a,s,b,c) \
	do { \
		QuatMultAdd ((a).q0.q, s, (b).q0.q, (c).q0.q); \
		QuatMultAdd ((a).qe.q, s, (b).qe.q, (c).qe.q); \
	} while (0)
#define DualQuatMultSub(a,s,b,c) \
	do { \
		QuatMultSub ((a).q0.q, s, (b).q0.q, (c).q0.q); \
		QuatMultSub ((a).qe.q, s, (b).qe.q, (c).qe.q); \
	} while (0)
#define DualQuatNorm(a,b) \
	do { \
		(b).r = QuatLength ((a).q0.q); \
		(b).e = 2 * QDotProduct ((a).q0.q, (a).qe.q); \
	} while (0)
#define DualQuatScale(a,b,c) \
	do { \
		QuatSub ((a).q0.q, (b), (c).q0.q); \
		QuatSub ((a).qe.q, (b), (c).qe.q); \
	} while (0)
#define DualQuatCompCompare(x, op, y) \
	(QuatCompCompare ((x).q0.q, op, (y).q0.q) \
	 &&QuatCompCompare ((x).qe.q, op, (y).qe.q))
#define DualQuatCompare(x, y) DualQuatCompCompare (x, ==, y)
#define DualQuatIsZero(a) (QuatIsZero ((a).q0.q) && QuatIsZero ((a).qe.q))
#define DualQuatSetVect(vec, a) \
	do { \
		(a).q0.sv.s = 1; \
		VectorZero ((a).q0.sv.v); \
		(a).qe.sv.s = 0; \
		VectorCopy (vec, (a).qe.sv.v); \
	} while (0)
#define DualQuatRotTrans(rot, trans, dq) \
	do { \
		QuatCopy (rot, (dq).q0.q); \
		(dq).qe.sv.s = 0; \
		VectorScale (trans, 0.5, (dq).qe.sv.v); \
		QuatMult ((dq).qe.q, (dq).q0.q, (dq).qe.q); \
	} while (0)
#define DualQuatZero(a) \
	do { \
		QuatZero ((a).q0.q); \
		QuatZero ((a).qe.q); \
	} while (0)
#define DualQuatBlend(dq1,dq2,b,dq) \
	do { \
		QuatBlend ((dq1).q0.q, (dq2).q0.q, (b), (dq).q0.q); \
		QuatBlend ((dq1).qe.q, (dq2).qe.q, (b), (dq).qe.q); \
	} while (0)
#define DualQuatExpand(dq) QuatExpand ((dq).q0.q), QuatExpand ((dq).qe.q)

///@}

#endif // __QF_math_dual_h
