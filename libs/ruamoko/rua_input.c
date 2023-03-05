/*
	bi_input.c

	CSQC file builtins

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/progs.h"

#include "rua_internal.h"

typedef struct rua_in_cookie_s {
	size_t      users;
	progs_t    *pr;
	pr_func_t   func;
	pr_ptr_t    data;
} rua_in_cookie_t;

typedef struct input_resources_s {
	hashctx_t  *hashctx;
	hashtab_t  *cookies;
	memsuper_t *cookie_super;
} input_resources_t;

static void
bi_IN_FindDeviceId (progs_t *pr, void *_res)
{
	const char *id = P_GSTRING (pr, 0);

	R_INT (pr) = IN_FindDeviceId (id);
}

static void
bi_IN_GetDeviceName (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);

	RETURN_STRING (pr, IN_GetDeviceName (devid));
}

static void
bi_IN_GetDeviceId (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);

	RETURN_STRING (pr, IN_GetDeviceId (devid));
}

static void
bi_IN_AxisInfo (progs_t *pr, void *_res)
{
}

static void
bi_IN_ButtonInfo (progs_t *pr, void *_res)
{
}

static void
bi_IN_GetAxisName (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);
	int	        axis  = P_INT (pr, 1);

	RETURN_STRING (pr, IN_GetAxisName (devid, axis));
}

static void
bi_IN_GetButtonName (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);
	int	        button  = P_INT (pr, 1);

	RETURN_STRING (pr, IN_GetButtonName (devid, button));
}

static void
bi_IN_GetAxisNumber (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);
	const char *axis_name = P_GSTRING (pr, 1);

	R_INT (pr) = IN_GetAxisNumber (devid, axis_name);
}

static void
bi_IN_GetButtonNumber (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);
	const char *button_name = P_GSTRING (pr, 1);

	R_INT (pr) = IN_GetButtonNumber (devid, button_name);
}

static void
bi_IN_ProcessEvents (progs_t *pr, void *_res)
{
	IN_ProcessEvents ();
}

static void
bi_IN_ClearStates (progs_t *pr, void *_res)
{
	IN_ClearStates ();
}

static void
bi_IN_CreateButton (progs_t *pr, void *_res)
{
	const char *name = P_GSTRING (pr, 0);
	const char *desc = P_GSTRING (pr, 1);
	in_button_t *button = PR_Zone_Malloc (pr, sizeof (in_button_t));
	button->name = name;
	button->description = desc;
	IN_RegisterButton (button);
	RETURN_POINTER (pr, button);
}

static void
bi_IN_CreateAxis (progs_t *pr, void *_res)
{
	const char *name = P_GSTRING (pr, 0);
	const char *desc = P_GSTRING (pr, 1);
	in_axis_t  *axis = PR_Zone_Malloc (pr, sizeof (in_axis_t));
	axis->name = name;
	axis->description = desc;
	IN_RegisterAxis (axis);
	RETURN_POINTER (pr, axis);
}

static void
bi_IN_GetAxisInfo (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);
	int	        axis  = P_INT (pr, 1);

	R_INT (pr) = 0;

	in_axisinfo_t info;
	if (IN_GetAxisInfo (devid, axis, &info)) {
		P_STRUCT (pr, in_axisinfo_t, 2) = info;
		R_INT (pr) = 1;
	}
}

static void
bi_IN_GetButtonInfo (progs_t *pr, void *_res)
{
	int	        devid  = P_INT (pr, 0);
	int	        button  = P_INT (pr, 1);

	R_INT (pr) = 0;

	in_buttoninfo_t info;
	if (IN_GetButtonInfo (devid, button, &info)) {
		P_STRUCT (pr, in_buttoninfo_t, 2) = info;
		R_INT (pr) = 1;
	}
}

static rua_in_cookie_t *
make_cookie (progs_t *pr, input_resources_t *res, pr_func_t func, pr_ptr_t data)
{
	rua_in_cookie_t search = {
		.func = func,
		.data = data,
	};
	rua_in_cookie_t *cookie = Hash_FindElement (res->cookies, &search);
	if (!cookie) {
		cookie = cmemalloc (res->cookie_super, sizeof (rua_in_cookie_t));
		*cookie = search;
		cookie->pr = pr;
		Hash_AddElement (res->cookies, cookie);
	}
	cookie->users++;
	return cookie;
}

static rua_in_cookie_t *
find_cookie (progs_t *pr, input_resources_t *res, pr_func_t func, pr_ptr_t data)
{
	rua_in_cookie_t search = {
		.func = func,
		.data = data,
	};
	return Hash_FindElement (res->cookies, &search);
}

static void
release_cookie (progs_t *pr, input_resources_t *res, rua_in_cookie_t *cookie)
{
	if (!--cookie->users) {
		Hash_DelElement (res->cookies, cookie);
		Hash_Free (res->cookies, cookie);
	}
}

static void
rua_add_axis_listener (progs_t *pr, input_resources_t *res,
					   axis_listener_t listener)
{
	in_axis_t  *axis = &P_STRUCT (pr, in_axis_t, 0);
	pr_func_t   func = P_FUNCTION (pr, 1);
	pr_func_t   data = P_POINTER (pr, 2);
	rua_in_cookie_t *cookie = make_cookie (pr, res, func, data);
	IN_AxisAddListener (axis, listener, cookie);
}

static void
rua_remove_axis_listener (progs_t *pr, input_resources_t *res,
						  axis_listener_t listener)
{
	in_axis_t  *axis = &P_STRUCT (pr, in_axis_t, 0);
	pr_func_t   func = P_FUNCTION (pr, 1);
	pr_func_t   data = P_POINTER (pr, 2);
	rua_in_cookie_t *cookie = find_cookie (pr, res, func, data);
	if (cookie) {
		IN_AxisRemoveListener (axis, listener, cookie);
		release_cookie (pr, res, cookie);
	}
}

static void
rua_add_button_listener (progs_t *pr, input_resources_t *res,
						 button_listener_t listener)
{
	in_button_t *button = &P_STRUCT (pr, in_button_t, 0);
	pr_func_t   func = P_FUNCTION (pr, 1);
	pr_func_t   data = P_POINTER (pr, 2);
	rua_in_cookie_t *cookie = make_cookie (pr, res, func, data);
	IN_ButtonAddListener (button, listener, cookie);
}

static void
rua_remove_button_listener (progs_t *pr, input_resources_t *res,
							button_listener_t listener)
{
	in_button_t *button = &P_STRUCT (pr, in_button_t, 0);
	pr_func_t   func = P_FUNCTION (pr, 1);
	pr_func_t   data = P_POINTER (pr, 2);
	rua_in_cookie_t *cookie = find_cookie (pr, res, func, data);
	if (cookie) {
		IN_ButtonRemoveListener (button, listener, cookie);
		release_cookie (pr, res, cookie);
	}
}

static void
rua_listener_func (rua_in_cookie_t *cookie, const void *input)
{
	progs_t    *pr = cookie->pr;
	PR_PushFrame (pr);
	PR_RESET_PARAMS (pr);
	P_POINTER (pr, 0) = cookie->data;
	P_POINTER (pr, 1) = PR_SetPointer (pr, input);//FIXME check input
	pr->pr_argc = 2;
	PR_ExecuteProgram (pr, cookie->func);
	PR_PopFrame (pr);
}

static void
rua_listener_method (rua_in_cookie_t *cookie, const void *input)
{
	progs_t    *pr = cookie->pr;
	PR_PushFrame (pr);
	PR_RESET_PARAMS (pr);
	P_POINTER (pr, 0) = cookie->data;
	P_POINTER (pr, 1) = 0;	// don't know the method name (selector)
	P_POINTER (pr, 2) = PR_SetPointer (pr, input);//FIXME check input
	pr->pr_argc = 3;
	PR_ExecuteProgram (pr, cookie->func);
	PR_PopFrame (pr);
}

static void
rua_axis_listener_func (void *data, const in_axis_t *axis)
{
	rua_listener_func (data, axis);
}

static void
rua_axis_listener_method (void *data, const in_axis_t *axis)
{
	rua_listener_method (data, axis);
}

static void
rua_button_listener_func (void *data, const in_button_t *button)
{
	rua_listener_func (data, button);
}

static void
rua_button_listener_method (void *data, const in_button_t *button)
{
	rua_listener_method (data, button);
}

static void
rua_IN_ButtonAddListener_func (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_add_button_listener (pr, res, rua_button_listener_func);
}

static void
rua_IN_ButtonRemoveListener_func (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_remove_button_listener (pr, res, rua_button_listener_func);
}

static void
rua_IN_AxisAddListener_func (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_add_axis_listener (pr, res, rua_axis_listener_func);
}

static void
rua_IN_AxisRemoveListener_func (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_remove_axis_listener (pr, res, rua_axis_listener_func);
}

static void
rua_IN_ButtonAddListener_method (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_add_button_listener (pr, res, rua_button_listener_method);
}

static void
rua_IN_ButtonRemoveListener_method (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_remove_button_listener (pr, res, rua_button_listener_method);
}

static void
rua_IN_AxisAddListener_method (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_add_axis_listener (pr, res, rua_axis_listener_method);
}

static void
rua_IN_AxisRemoveListener_method (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	rua_remove_axis_listener (pr, res, rua_axis_listener_method);
}

static void
bi_IN_LoadConfig (progs_t *pr, void *_res)
{
	IN_LoadConfig (Plist_GetItem (pr, P_INT (pr, 0)));
}

static void
bi_IMT_CreateContext (progs_t *pr, void *_res)
{
	const char *name = P_GSTRING (pr, 0);
	R_INT (pr) = IMT_CreateContext (name);
}

static void
bi_IMT_GetContext (progs_t *pr, void *_res)
{
	R_INT (pr) = IMT_GetContext ();
}

static void
bi_IMT_SetContext (progs_t *pr, void *_res)
{
	IMT_SetContext (P_INT (pr, 0));
}

static void
secured (progs_t *pr, void *_res)
{
	PR_RunError (pr, "Secured function called");
}

#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
#define bi(x,np,params...) {#x, secured, -1, np, {params}}
static builtin_t secure_builtins[] = {
	bi(IN_CreateButton, 2, p(string), p(string)),
	bi(IN_CreateAxis,   2, p(string), p(string)),
	bi(IN_LoadConfig,   1, p(ptr)),
	{0}
};

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
static builtin_t insecure_builtins[] = {
	bi(IN_CreateButton, 2, p(string), p(string)),
	bi(IN_CreateAxis,   2, p(string), p(string)),
	bi(IN_LoadConfig,   1, p(ptr)),
	{0}
};
static builtin_t builtins[] = {
	bi(IN_FindDeviceId,     1, p(string)),
	bi(IN_GetDeviceName,    1, p(int)),
	bi(IN_GetDeviceId,      1, p(int)),
	bi(IN_AxisInfo,         0), //FIXME
	bi(IN_ButtonInfo,       0), //FIXME
	bi(IN_GetAxisName,      2, p(int), p(int)),
	bi(IN_GetButtonName,    2, p(int), p(int)),
	bi(IN_GetAxisNumber,    2, p(int), p(string)),
	bi(IN_GetButtonNumber,  2, p(int), p(string)),
	bi(IN_ProcessEvents,    0),
	bi(IN_ClearStates,      0),
	bi(IN_GetAxisInfo,      3, p(int), p(int), p(ptr)),
	bi(IN_GetButtonInfo,    3, p(int), p(int), p(ptr)),
	{"IN_ButtonAddListener|^{tag in_button_s=}^(v^v^{tag in_button_s=})^v",
		rua_IN_ButtonAddListener_func, -1,      3, {p(ptr), p(func), p(ptr)}},
	{"IN_ButtonRemoveListener|^{tag in_button_s=}^(v^v^{tag in_button_s=})^v",
		rua_IN_ButtonRemoveListener_func, -1,   3, {p(ptr), p(func), p(ptr)}},
	{"IN_AxisAddListener|^{tag in_axis_s=}^(v^v^{tag in_axis_s=})^v",
		rua_IN_AxisAddListener_func, -1,        3, {p(ptr), p(func), p(ptr)}},
	{"IN_AxisRemoveListener|^{tag in_axis_s=}^(v^v^{tag in_axis_s=})^v",
		rua_IN_AxisRemoveListener_func, -1,     3, {p(ptr), p(func), p(ptr)}},
	{"IN_ButtonAddListener|^{tag in_button_s=}(@@:.)@",
		rua_IN_ButtonAddListener_method, -1,    3, {p(ptr), p(func), p(ptr)}},
	{"IN_ButtonRemoveListener|^{tag in_button_s=}(@@:.)@",
		rua_IN_ButtonRemoveListener_method, -1, 3, {p(ptr), p(func), p(ptr)}},
	{"IN_AxisAddListener|^{tag in_axis_s=}(@@:.)@",
		rua_IN_AxisAddListener_method, -1,      3, {p(ptr), p(func), p(ptr)}},
	{"IN_AxisRemoveListener|^{tag in_axis_s=}(@@:.)@",
		rua_IN_AxisRemoveListener_method, -1,   3, {p(ptr), p(func), p(ptr)}},

	bi(IMT_CreateContext,   1, p(string)),
	bi(IMT_GetContext,      0),
	bi(IMT_SetContext,      1, p(int)),

	{0}
};

static void
bi_input_clear (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	Hash_FlushTable (res->cookies);
}

static void
bi_input_destroy (progs_t *pr, void *_res)
{
	input_resources_t *res = _res;
	Hash_DelTable (res->cookies);
	delete_memsuper (res->cookie_super);

	free (res);
}

static uintptr_t
rua_in_hash_cookie (const void *_cookie, void *_res)
{
	const rua_in_cookie_t *cookie = _cookie;
	return cookie->func + cookie->data;
}

static int
rua_in_cmp_cookies (const void *_a, const void *_b, void *_res)
{
	const rua_in_cookie_t *a = _a;
	const rua_in_cookie_t *b = _b;
	return a->func == b->func && a->data == b->data;
}

static void
rua_in_free_cookie (void *_cookie, void *_res)
{
	input_resources_t *res = _res;
	rua_in_cookie_t *cookie = _cookie;
	cmemfree (res->cookie_super, cookie);
}

void
RUA_Input_Init (progs_t *pr, int secure)
{
	input_resources_t *res = calloc (sizeof (input_resources_t), 1);

	res->cookie_super = new_memsuper ();
	res->cookies = Hash_NewTable (251, 0, rua_in_free_cookie, res, pr->hashctx);
	Hash_SetHashCompare (res->cookies, rua_in_hash_cookie, rua_in_cmp_cookies);

	PR_Resources_Register (pr, "input", res, bi_input_clear, bi_input_destroy);
	if (secure & 2) {
		PR_RegisterBuiltins (pr, secure_builtins, res);
	} else {
		PR_RegisterBuiltins (pr, insecure_builtins, res);
	}
	PR_RegisterBuiltins (pr, builtins, res);
}
