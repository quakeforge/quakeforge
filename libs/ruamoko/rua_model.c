/*
	bi_model.c

	Ruamkoko model builtins

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/model.h"
#include "QF/progs.h"

#include "rua_internal.h"

typedef struct rua_model_s {
	struct rua_model_s *next;
	struct rua_model_s **prev;
	model_t    *model;
} rua_model_t;

typedef struct {
	PR_RESMAP (rua_model_t) model_map;
	rua_model_t *handles;
	progs_t    *pr;
} rua_model_resources_t;

static rua_model_t *
rua_model_handle_new (rua_model_resources_t *res)
{
	return PR_RESNEW (res->model_map);
}

static void
rua_model_handle_free (rua_model_resources_t *res, rua_model_t *handle)
{
	PR_RESFREE (res->model_map, handle);
}

static void
rua_model_handle_reset (rua_model_resources_t *res)
{
	PR_RESRESET (res->model_map);
}

static inline rua_model_t * __attribute__((pure))
rua__model_handle_get (rua_model_resources_t *res, int index, const char *name)
{
	rua_model_t *handle = 0;

	handle = PR_RESGET(res->model_map, index);
	if (!handle) {
		PR_RunError (res->pr, "invalid model handle passed to %s", name + 3);
	}
	return handle;
}
#define rua_model_handle_get(res, index) \
	rua__model_handle_get (res, index, __FUNCTION__)

static inline int __attribute__((pure))
rua_model_handle_index (rua_model_resources_t *res, rua_model_t *handle)
{
	return PR_RESINDEX(res->model_map, handle);
}

static void
bi_rua_model_clear (progs_t *pr, void *_res)
{
	rua_model_resources_t *res = (rua_model_resources_t *) _res;
	rua_model_t    *handle;

	for (handle = res->handles; handle; handle = handle->next)
		Mod_UnloadModel (handle->model);
	res->handles = 0;
	rua_model_handle_reset (res);
}

static void
bi_rua_model_destroy (progs_t *pr, void *_res)
{
	rua_model_resources_t *res = _res;
	PR_RESDELMAP (res->model_map);
	free (res);
}

static int
alloc_handle (rua_model_resources_t *res, model_t *model)
{
	rua_model_t    *handle = rua_model_handle_new (res);

	if (!handle)
		return 0;

	handle->next = res->handles;
	handle->prev = &res->handles;
	if (res->handles)
		res->handles->prev = &handle->next;
	res->handles = handle;
	handle->model = model;
	return rua_model_handle_index (res, handle);
}

static void
bi_Model_Load (progs_t *pr, void *_res)
{
	__auto_type res = (rua_model_resources_t *) _res;
	const char *path = P_GSTRING (pr, 0);
	model_t    *model;

	R_INT (pr) = 0;
	if (!(model = Mod_ForName (path, 0)))
		return;
	if (!(R_INT (pr) = alloc_handle (res, model)))
		Mod_UnloadModel (model);
}

model_t *
Model_GetModel (progs_t *pr, int handle)
{
	if (!handle) {
		return 0;
	}
	rua_model_resources_t *res = PR_Resources_Find (pr, "Model");
	rua_model_t    *h = rua_model_handle_get (res, handle);

	return h->model;
}

static void
bi_Model_Unload (progs_t *pr, void *_res)
{
	__auto_type res = (rua_model_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	rua_model_t    *h = rua_model_handle_get (res, handle);

	if (!h)
		PR_RunError (pr, "invalid model handle passed to Qclose");
	Mod_UnloadModel (h->model);
	*h->prev = h->next;
	if (h->next)
		h->next->prev = h->prev;
	rua_model_handle_free (res, h);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(Model_Load,      1, p(string)),
	bi(Model_Unload,    1, p(ptr)),
	{0}
};

void
RUA_Model_Init (progs_t *pr, int secure)
{
	rua_model_resources_t *res = calloc (sizeof (rua_model_resources_t), 1);
	res->pr = pr;

	PR_Resources_Register (pr, "Model", res, bi_rua_model_clear,
						   bi_rua_model_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
}
