/*
	bi_stdlib.c

	QuakeC stdlib api

	Copyright (C) 2021 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/5/31

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include "compat.h"
#include "qfalloca.h"

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "QF/fbsearch.h"
#include "QF/progs.h"

#include "rua_internal.h"

typedef struct {
	progs_t    *pr;
	pr_func_t   func;
} function_t;

static int
int_compare (const void *_a, const void *_b)
{
	qfZoneScoped (true);
	const int  *a = _a;
	const int  *b = _b;
	return *a - *b;
}

static int
rua_compare (const void *a, const void *b, void *_f)
{
	qfZoneScoped (true);
	function_t *f = _f;

	PR_PushFrame (f->pr);
	PR_RESET_PARAMS (f->pr);
	P_POINTER (f->pr, 0) = PR_SetPointer (f->pr, a);
	P_POINTER (f->pr, 1) = PR_SetPointer (f->pr, b);
	f->pr->pr_argc = 2;
	PR_ExecuteProgram (f->pr, f->func);
	int         cmp = R_INT (f->pr);
	PR_PopFrame (f->pr);
	return cmp;
}

static void
bi_bsearch (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const void *key = P_GPOINTER (pr, 0);
	const void *array = P_GPOINTER (pr, 1);
	size_t      nmemb = P_INT (pr, 2);
	size_t      size = P_INT (pr, 3) * sizeof (pr_int_t);
	pr_func_t   cmp = P_FUNCTION (pr, 4);
	void       *p = 0;

	if (!cmp) {
		p = bsearch (key, array, nmemb, size, int_compare);
	} else {
		function_t  func = { pr, cmp };
		p = bsearch_r (key, array, nmemb, size, rua_compare, &func);
	}
	RETURN_POINTER (pr, p);
}

static void
bi_fbsearch (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const void *key = P_GPOINTER (pr, 0);
	const void *array = P_GPOINTER (pr, 1);
	size_t      nmemb = P_INT (pr, 2);
	size_t      size = P_INT (pr, 3) * sizeof (pr_int_t);
	pr_func_t   cmp = P_FUNCTION (pr, 4);
	void       *p = 0;

	if (!cmp) {
		p = fbsearch (key, array, nmemb, size, int_compare);
	} else {
		function_t  func = { pr, cmp };
		p = fbsearch_r (key, array, nmemb, size, rua_compare, &func);
	}
	RETURN_POINTER (pr, p);
}

static void
bi_qsort (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	void       *array = P_GPOINTER (pr, 0);
	size_t      nmemb = P_INT (pr, 1);
	size_t      size = P_INT (pr, 2) * sizeof (pr_int_t);
	pr_func_t   cmp = P_FUNCTION (pr, 3);

	if (!cmp) {
		qsort (array, nmemb, size, int_compare);
	} else {
		function_t  func = { pr, cmp };
		qsort_r (array, nmemb, size, rua_compare, &func);
	}
}

static void
bi_prefixsumi (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	int        *array = (int *) P_GPOINTER (pr, 0);
	int         count = P_INT (pr, 1);

	for (int i = 1; i < count; i++) {
		array[i] += array[i - 1];
	}
}

static void
bi_prefixsumf (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	float      *array = (float *) P_GPOINTER (pr, 0);
	int         count = P_INT (pr, 1);

	for (int i = 1; i < count; i++) {
		array[i] += array[i - 1];
	}
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(bsearch,  4, p(ptr), p(ptr), p(int), p(int), p(func)),
	bi(fbsearch, 4, p(ptr), p(ptr), p(int), p(int), p(func)),
	bi(qsort,    3, p(ptr), p(int), p(int), p(func)),
	{"prefixsum|^ii",	bi_prefixsumi,	-1, 2, {p(ptr), p(int)}},
	{"prefixsum|^fi",	bi_prefixsumf,	-1, 2, {p(ptr), p(int)}},
	{0}
};

void
RUA_Stdlib_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	PR_RegisterBuiltins (pr, builtins, 0);
}
