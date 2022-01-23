/*
	bi_runtime.c

	QuakeC runtime api

	Copyright (C) 2020 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/4/1

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "qfalloca.h"

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include "QF/progs.h"

#include "rua_internal.h"

static void
bi_va_copy (progs_t *pr)
{
	__auto_type src_args = (pr_va_list_t *) &P_POINTER (pr, 0);
	__auto_type src_list = &G_STRUCT (pr, pr_type_t, src_args->list);
	size_t      parm_size = pr->pr_param_size * sizeof(pr_type_t);
	size_t      size = src_args->count * parm_size;
	pr_string_t dst_list_block = 0;
	pr_type_t  *dst_list = 0;

	if (size) {
		dst_list_block = PR_AllocTempBlock (pr, size);
		dst_list = (pr_type_t *) PR_GetString (pr, dst_list_block);
	}

	memcpy (dst_list, src_list, size);
	R_PACKED (pr, pr_va_list_t).count = src_args->count;
	R_PACKED (pr, pr_va_list_t).list = PR_SetPointer (pr, dst_list);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(va_copy, 1, P(1, 2)),
	{0}
};

void
RUA_Runtime_Init (progs_t *pr, int secure)
{
	PR_RegisterBuiltins (pr, builtins);
}
