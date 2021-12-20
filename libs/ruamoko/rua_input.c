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

#include "QF/input.h"
#include "QF/progs.h"

#include "rua_internal.h"

//typedef struct input_resources_s {
//} input_resources_t;

static void
bi_IN_FindDeviceId (progs_t *pr)
{
	const char *id = P_GSTRING (pr, 0);

	R_INT (pr) = IN_FindDeviceId (id);
}

static void
bi_IN_GetDeviceName (progs_t *pr)
{
	int	        devid  = P_INT (pr, 0);

	RETURN_STRING (pr, IN_GetDeviceName (devid));
}

static void
bi_IN_GetDeviceId (progs_t *pr)
{
	int	        devid  = P_INT (pr, 0);

	RETURN_STRING (pr, IN_GetDeviceId (devid));
}

static void
bi_IN_AxisInfo (progs_t *pr)
{
}

static void
bi_IN_ButtonInfo (progs_t *pr)
{
}

static void
bi_IN_GetAxisName (progs_t *pr)
{
	int	        devid  = P_INT (pr, 0);
	int	        axis  = P_INT (pr, 1);

	RETURN_STRING (pr, IN_GetAxisName (devid, axis));
}

static void
bi_IN_GetButtonName (progs_t *pr)
{
	int	        devid  = P_INT (pr, 0);
	int	        button  = P_INT (pr, 1);

	RETURN_STRING (pr, IN_GetButtonName (devid, button));
}

static void
bi_IN_GetAxisNumber (progs_t *pr)
{
	int	        devid  = P_INT (pr, 0);
	const char *axis_name = P_GSTRING (pr, 1);

	R_INT (pr) = IN_GetAxisNumber (devid, axis_name);
}

static void
bi_IN_GetButtonNumber (progs_t *pr)
{
	int	        devid  = P_INT (pr, 0);
	const char *button_name = P_GSTRING (pr, 1);

	R_INT (pr) = IN_GetButtonNumber (devid, button_name);
}

static void
bi_IN_ProcessEvents (progs_t *pr)
{
	IN_ProcessEvents ();
}

static void
bi_IN_ClearStates (progs_t *pr)
{
	IN_ClearStates ();
}

static void
bi_IN_CreateButton (progs_t *pr)
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
bi_IN_CreateAxis (progs_t *pr)
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
secured (progs_t *pr)
{
	PR_RunError (pr, "Secured function called");
}

#define bi(x) {#x, secured, -1}
static builtin_t secure_builtins[] = {
	bi(IN_CreateButton),
	bi(IN_CreateAxis),
	{0}
};

#undef bi
#define bi(x) {#x, bi_##x, -1}
static builtin_t insecure_builtins[] = {
	bi(IN_CreateButton),
	bi(IN_CreateAxis),
	{0}
};
static builtin_t builtins[] = {
	bi(IN_FindDeviceId),
	bi(IN_GetDeviceName),
	bi(IN_GetDeviceId),
	bi(IN_AxisInfo),
	bi(IN_ButtonInfo),
	bi(IN_GetAxisName),
	bi(IN_GetButtonName),
	bi(IN_GetAxisNumber),
	bi(IN_GetButtonNumber),
	bi(IN_ProcessEvents),
	bi(IN_ClearStates),
	{0}
};

//static void
//bi_input_clear (progs_t *pr, void *_res)
//{
//}

void
RUA_Input_Init (progs_t *pr, int secure)
{
	//input_resources_t *res = calloc (sizeof (input_resources_t), 1);
	//PR_Resources_Register (pr, "input", res, bi_input_clear);

	if (secure & 2) {
		PR_RegisterBuiltins (pr, secure_builtins);
	} else {
		PR_RegisterBuiltins (pr, insecure_builtins);
	}
	PR_RegisterBuiltins (pr, builtins);
}
