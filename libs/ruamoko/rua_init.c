/*
	bi_init.c

	CSQC builtins init

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include "QF/progs.h"
#include "QF/ruamoko.h"

#include "rua_internal.h"

static void (*init_funcs[])(progs_t *, int) = {
	RUA_Obj_Init,			// done early (for the heck of it at this stage)

	RUA_Cbuf_Init,
	RUA_Cmd_Init,
	RUA_Cvar_Init,
	RUA_Dirent_Init,
	RUA_Hash_Init,
	RUA_Math_Init,
	RUA_MsgBuf_Init,
	RUA_Plist_Init,
	RUA_QFile_Init,
	RUA_QFS_Init,
	RUA_Runtime_Init,
	RUA_Script_Init,
	RUA_Set_Init,
	RUA_Stdlib_Init,
	RUA_String_Init,
};

void
RUA_Secured (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	PR_RunError (pr, "Secured function called");
}

typedef struct rua_system_s {
	wssched_t  *sched;
} rua_system_t;

static void
rua_cleanup (progs_t *pr, void *_res)
{
}

static void
rua_destroy (progs_t *pr, void *_res)
{
	rua_system_t *res = _res;
	free (res);
}

VISIBLE void
RUA_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);

	for (size_t i = 0; i < countof (init_funcs); i++) {
		init_funcs[i] (pr, secure);
	}

	rua_system_t *res = malloc (sizeof (rua_system_t));
	*res = (rua_system_t) {
	};
	PR_Resources_Register (pr, "RUA_system", res, rua_cleanup, rua_destroy);
}

VISIBLE void
RUA_SetSched (progs_t *pr, wssched_t *sched)
{
	qfZoneScoped (true);
	rua_system_t *res = PR_Resources_Find (pr, "RUA_system");
	res->sched = sched;
}

wssched_t *
RUA_GetSched (progs_t *pr)
{
	qfZoneScoped (true);
	rua_system_t *res = PR_Resources_Find (pr, "RUA_system");
	return res->sched;
}
