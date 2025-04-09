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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <math.h>

#include "QF/cmd.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/sys.h"

#include "rua_internal.h"

static void
bi_sinf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = sinf (P_FLOAT (pr, 0));
}

static void
bi_sincosf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float      *vec = &R_var (pr, float);
	float       x = P_FLOAT (pr, 0);
	sincosf (x, &vec[0], &vec[1]);
}

static void
bi_cosf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = cosf (P_FLOAT (pr, 0));
}

static void
bi_tanf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = tanf (P_FLOAT (pr, 0));
}

static void
bi_asinf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = asinf (P_FLOAT (pr, 0));
}

static void
bi_acosf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = acosf (P_FLOAT (pr, 0));
}

static void
bi_atanf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = atanf (P_FLOAT (pr, 0));
}

static void
bi_atan2f (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = atan2f (P_FLOAT (pr, 0), P_FLOAT (pr, 1));
}

static void
bi_expf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = expf (P_FLOAT (pr, 0));
}

static void
bi_logf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = logf (P_FLOAT (pr, 0));
}

static void
bi_log2f (progs_t *pr, void *data)
{
	qfZoneScoped (true);
#ifdef HAVE_LOG2F
	R_FLOAT (pr) = log2f (P_FLOAT (pr, 0));
#else
	R_FLOAT (pr) = logf (P_FLOAT (pr, 0)) / M_LOG2E;
#endif
}

static void
bi_log10f (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = log10f (P_FLOAT (pr, 0));
}

static void
bi_powf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = powf (P_FLOAT (pr, 0), P_FLOAT (pr, 1));
}

static void
bi_sqrtf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = sqrtf (P_FLOAT (pr, 0));
}

static void
bi_cbrtf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = cbrtf (P_FLOAT (pr, 0));
}

static void
bi_hypotf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = hypotf (P_FLOAT (pr, 0), P_FLOAT (pr, 1));
}

static void
bi_sinhf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = sinhf (P_FLOAT (pr, 0));
}

static void
bi_sincoshf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float      *vec = &R_var (pr, float);
	float       x = P_FLOAT (pr, 0);
	vec[0] = sinhf (x);
	vec[1] = coshf (x);
}

static void
bi_coshf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = coshf (P_FLOAT (pr, 0));
}

static void
bi_tanhf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_FLOAT (pr) = tanhf (P_FLOAT (pr, 0));
}

static void
bi_asinhf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	double      y = P_FLOAT (pr, 0);
	R_FLOAT (pr) = logf (y + sqrtf (y * y + 1));
}

static void
bi_acoshf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	double      y = P_FLOAT (pr, 0);
	R_FLOAT (pr) = logf (y + sqrtf (y * y - 1));
}

static void
bi_atanhf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	double      y = P_FLOAT (pr, 0);
	R_FLOAT (pr) = logf ((1 + y) / (1 - y)) / 2;
}

static void
bi_floor (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = floor (P_DOUBLE (pr, 0));
}

static void
bi_ceil (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = ceil (P_DOUBLE (pr, 0));
}

static void
bi_fabs (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = fabs (P_DOUBLE (pr, 0));
}

static void
bi_sin (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = sin (P_DOUBLE (pr, 0));
}

static void
bi_sincos (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	double     *vec = &R_var (pr, double);
	float       x = P_FLOAT (pr, 0);
	sincos (x, &vec[0], &vec[1]);
}

static void
bi_cos (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = cos (P_DOUBLE (pr, 0));
}

static void
bi_tan (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = tan (P_DOUBLE (pr, 0));
}

static void
bi_asin (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = asin (P_DOUBLE (pr, 0));
}

static void
bi_acos (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = acos (P_DOUBLE (pr, 0));
}

static void
bi_atan (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = atan (P_DOUBLE (pr, 0));
}

static void
bi_atan2 (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = atan2 (P_DOUBLE (pr, 0), P_DOUBLE (pr, 1));
}

static void
bi_exp (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = exp (P_DOUBLE (pr, 0));
}

static void
bi_log (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = log (P_DOUBLE (pr, 0));
}

static void
bi_log2 (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = log (P_DOUBLE (pr, 0)) / M_LOG2E;
}

static void
bi_log10 (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = log10 (P_DOUBLE (pr, 0));
}

static void
bi_pow (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = pow (P_DOUBLE (pr, 0), P_DOUBLE (pr, 1));
}

static void
bi_sqrt (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = sqrt (P_DOUBLE (pr, 0));
}

static void
bi_cbrt (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = cbrt (P_DOUBLE (pr, 0));
}

static void
bi_hypot (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = hypot (P_DOUBLE (pr, 0), P_DOUBLE (pr, 1));
}

static void
bi_sinh (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = sinh (P_DOUBLE (pr, 0));
}

static void
bi_sincosh (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float      *vec = &R_var (pr, float);
	float       x = P_FLOAT (pr, 0);
	vec[0] = sinh (x);
	vec[1] = cosh (x);
}

static void
bi_cosh (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = cosh (P_DOUBLE (pr, 0));
}

static void
bi_tanh (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	R_DOUBLE (pr) = tanh (P_DOUBLE (pr, 0));
}

static void
bi_asinh (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	double      y = P_DOUBLE (pr, 0);
	R_DOUBLE (pr) = log (y + sqrt (y * y + 1));
}

static void
bi_acosh (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	double      y = P_DOUBLE (pr, 0);
	R_DOUBLE (pr) = log (y + sqrt (y * y - 1));
}

static void
bi_atanh (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	double      y = P_DOUBLE (pr, 0);
	R_DOUBLE (pr) = log ((1 + y) / (1 - y)) / 2;
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	{"sin|f",	bi_sinf,	-1, 1, {p(float)}},
	{"sincos|f",bi_sincosf,	-1, 1, {p(float)}},
	{"cos|f",	bi_cosf,	-1, 1, {p(float)}},
	{"tan|f",	bi_tanf,	-1, 1, {p(float)}},
	{"asin|f",	bi_asinf,	-1, 1, {p(float)}},
	{"acos|f",	bi_acosf,	-1, 1, {p(float)}},
	{"atan|f",	bi_atanf,	-1, 1, {p(float)}},
	{"atan2|ff",bi_atan2f, 	-1, 2, {p(float), p(float)}},
	{"exp|f",	bi_expf,	-1, 1, {p(float)}},
	{"log|f",	bi_logf,	-1, 1, {p(float)}},
	{"log2|f",	bi_log2f,	-1, 1, {p(float)}},
	{"log10|f",	bi_log10f,	-1, 1, {p(float)}},
	{"pow|ff",	bi_powf,	-1, 2, {p(float), p(float)}},
	{"sqrt|f",	bi_sqrtf,	-1, 1, {p(float)}},
	{"cbrt|f",	bi_cbrtf,	-1, 1, {p(float)}},
	{"hypot|ff",bi_hypotf,	-1, 2, {p(float), p(float)}},
	{"sinh|f",	bi_sinhf,	-1, 1, {p(float)}},
	{"sincosh|f",bi_sincoshf,-1, 1, {p(float)}},
	{"cosh|f",	bi_coshf,	-1, 1, {p(float)}},
	{"tanh|f",	bi_tanhf,	-1, 1, {p(float)}},
	{"asinh|f",	bi_asinhf,	-1, 1, {p(float)}},
	{"acosh|f",	bi_acoshf,	-1, 1, {p(float)}},
	{"atanh|f",	bi_atanhf,	-1, 1, {p(float)}},
	{"floor|d",	bi_floor,	-1, 1, {p(double)}},	// float version in pr_cmds
	{"ceil|d",	bi_ceil,	-1, 1, {p(double)}},	// float version in pr_cmds
	{"fabs|d",	bi_fabs,	-1, 1, {p(double)}},	// float version in pr_cmds
	{"sin|d",	bi_sin,		-1, 1, {p(double)}},
	{"sincos|d",bi_sincos,	-1, 1, {p(double)}},
	{"cos|d",	bi_cos,		-1, 1, {p(double)}},
	{"tan|d",	bi_tan,		-1, 1, {p(double)}},
	{"asin|d",	bi_asin,	-1, 1, {p(double)}},
	{"acos|d",	bi_acos,	-1, 1, {p(double)}},
	{"atan|d",	bi_atan,	-1, 1, {p(double)}},
	{"atan2|dd",bi_atan2, 	-1, 2, {p(double), p(double)}},
	{"exp|d",	bi_exp,		-1, 1, {p(double)}},
	{"log|d",	bi_log,		-1, 1, {p(double)}},
	{"log2|d",	bi_log2,	-1, 1, {p(double)}},
	{"log10|d",	bi_log10,	-1, 1, {p(double)}},
	{"pow|dd",	bi_pow,		-1, 2, {p(double), p(double)}},
	{"sqrt|d",	bi_sqrt,	-1, 1, {p(double)}},
	{"cbrt|d",	bi_cbrt,	-1, 1, {p(double)}},
	{"hypot|dd",bi_hypot,	-1, 2, {p(double), p(double)}},
	{"sinh|d",	bi_sinh,	-1, 1, {p(double)}},
	{"sincosh|d",bi_sincosh,-1, 1, {p(double)}},
	{"cosh|d",	bi_cosh,	-1, 1, {p(double)}},
	{"tanh|d",	bi_tanh,	-1, 1, {p(double)}},
	{"asinh|d",	bi_asinh,	-1, 1, {p(double)}},
	{"acosh|d",	bi_acosh,	-1, 1, {p(double)}},
	{"atanh|d",	bi_atanh,	-1, 1, {p(double)}},
	{0}
};

void
RUA_Math_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	PR_RegisterBuiltins (pr, builtins, 0);
}
