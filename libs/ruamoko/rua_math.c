/*
	rua_math.c

	Floating point math api for ruamoko

	Copyright (C) 2003 Robert F Merrill

	Author: Robert F Merrill
	Date: 2003-01-22

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#include <stdlib.h>
#include <math.h>	

#include "QF/cmd.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "rua_internal.h"

static void
bi_sin (progs_t *pr)
{
	R_FLOAT (pr) = sinf (P_FLOAT (pr, 0));
}

static void
bi_cos (progs_t *pr)
{
	R_FLOAT (pr) = cosf (P_FLOAT (pr, 0));
}

static void
bi_tan (progs_t *pr)
{
	R_FLOAT (pr) = tanf (P_FLOAT (pr, 0));
}

static void
bi_asin (progs_t *pr)
{
	R_FLOAT (pr) = asinf (P_FLOAT (pr, 0));
}

static void
bi_acos (progs_t *pr)
{
	R_FLOAT (pr) = acosf (P_FLOAT (pr, 0));
}

static void
bi_atan (progs_t *pr)
{
	R_FLOAT (pr) = atanf (P_FLOAT (pr, 0));
}

static void
bi_atan2 (progs_t *pr)
{
	R_FLOAT (pr) = atan2f (P_FLOAT (pr, 0), P_FLOAT (pr, 1));
}

static void
bi_log (progs_t *pr)
{
	R_FLOAT (pr) = logf (P_FLOAT (pr, 0));
}

static void
bi_log2 (progs_t *pr)
{
#ifdef HAVE_LOG2F
	R_FLOAT (pr) = log2f (P_FLOAT (pr, 0));
#else
	R_FLOAT (pr) = logf (P_FLOAT (pr, 0)) / M_LOG2E;
#endif
}

static void
bi_log10 (progs_t *pr)
{
	R_FLOAT (pr) = log10f (P_FLOAT (pr, 0));
}

static void
bi_pow (progs_t *pr)
{
	R_FLOAT (pr) = powf (P_FLOAT (pr, 0), P_FLOAT (pr, 1));
}

static void
bi_sqrt (progs_t *pr)
{
	R_FLOAT (pr) = sqrtf (P_FLOAT (pr, 0));
}

static void
bi_cbrt (progs_t *pr)
{
	R_FLOAT (pr) = cbrtf (P_FLOAT (pr, 0));
}

static void
bi_hypot (progs_t *pr)
{
	R_FLOAT (pr) = hypotf (P_FLOAT (pr, 0), P_FLOAT (pr, 1));
}

static void
bi_sinh (progs_t *pr)
{
	R_FLOAT (pr) = sinhf (P_FLOAT (pr, 0));
}

static void
bi_cosh (progs_t *pr)
{
	R_FLOAT (pr) = coshf (P_FLOAT (pr, 0));
}

static void
bi_tanh (progs_t *pr)
{
	R_FLOAT (pr) = tanhf (P_FLOAT (pr, 0));
}

static void
bi_asinh (progs_t *pr)
{
	double      y = P_FLOAT (pr, 0);
	R_FLOAT (pr) = logf (y + sqrtf (y * y + 1));
}

static void
bi_acosh (progs_t *pr)
{
	double      y = P_FLOAT (pr, 0);
	R_FLOAT (pr) = logf (y + sqrtf (y * y - 1));
}

static void
bi_atanh (progs_t *pr)
{
	double      y = P_FLOAT (pr, 0);
	R_FLOAT (pr) = logf ((1 + y) / (1 - y)) / 2;
}

static builtin_t builtins[] = {
	{"sin",		bi_sin,		-1},
	{"cos",		bi_cos,		-1},
	{"tan",		bi_tan,		-1},
	{"asin",	bi_asin,	-1},
	{"acos",	bi_acos,	-1},
	{"atan",	bi_atan,	-1},
	{"atan2", 	bi_atan2, 	-1},
	{"log",		bi_log,		-1},
	{"log2",	bi_log2,	-1},
	{"log10",	bi_log10,	-1},
	{"pow",		bi_pow,		-1},
	{"sqrt",	bi_sqrt,	-1},
	{"cbrt",	bi_cbrt,	-1},
	{"hypot",	bi_hypot,	-1},
	{"sinh",	bi_sinh,	-1},
	{"cosh",	bi_cosh,	-1},
	{"tanh",	bi_tanh,	-1},
	{"asinh",	bi_asinh,	-1},
	{"acosh",	bi_acosh,	-1},
	{"atanh",	bi_atanh,	-1},
	{0}
};

void
RUA_Math_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
