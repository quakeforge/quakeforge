/*
	rua_keys.c

	Ruamoko key-api builtins

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/csqc.h"
#include "QF/keys.h"
#include "QF/progs.h"
#include "QF/zone.h"

static void
bi_Key_keydown (progs_t *pr)
{
	int         keynum  = P_INT (pr, 0);
	R_INT (pr) = keydown[keynum];
}

/*
    bi_Key_SetBinding

    QC-Function for set a binding
*/
static void
bi_Key_SetBinding (progs_t *pr)
{
	const char *imt_name  = P_GSTRING (pr, 0);
	int         keynum  = P_INT (pr, 1);
	const char *binding = P_GSTRING (pr, 2);
	imt_t      *imt;

	if (binding && !binding[0]) {
		binding = NULL;	/* unbind a binding */
	}

	imt = Key_FindIMT (imt_name);
	if (imt) {
		Key_SetBinding (imt, keynum, binding);
	}
}

/*
    bi_Key_LookupBinding

    Perform a reverse-binding-lookup
*/
static void
bi_Key_LookupBinding (progs_t *pr)
{
	const char *imt_name = P_GSTRING (pr, 0);
	int	        bindnum = P_INT (pr, 1);
	const char *binding = P_GSTRING (pr, 2);
	imt_t      *imt;
	int i;
	knum_t keynum = -1;
	const char *keybind = NULL;

	imt = Key_FindIMT (imt_name);
	if (imt) {
		for (i = 0; i < QFK_LAST; i++) {
			keybind = imt->bindings[i].str;
			if (keybind == NULL) {
			  continue;
			}
			if (strcmp (keybind, binding) == 0) {
				bindnum--;
				if (bindnum == 0) {
					keynum = i;
					break;
				}
			}
		}
	}

	R_INT (pr) = keynum;
};

/*
    bi_Key_CountBinding

    Counts how often a binding is assigned to a key
*/
static void
bi_Key_CountBinding (progs_t *pr)
{
	const char *imt_name = P_GSTRING (pr, 0);
	const char *binding = P_GSTRING (pr, 1);
	int         i, res = 0;
	const char *keybind = NULL;
	imt_t      *imt;

	imt = Key_FindIMT (imt_name);
	if (imt) {
		for (i = 0; i < QFK_LAST; i++) {
			keybind = imt->bindings[i].str;
			if (keybind == NULL) {
			  continue;
			}
			if (strcmp (keybind, binding) == 0) {
				res++;
			}
		}
	}

	R_INT (pr) = res;
};


/*
    bi_Key_LookupBinding

    Convertes a keynum to a string
*/
static void
bi_Key_KeynumToString (progs_t *pr)
{
	int	        keynum  = P_INT (pr, 0);

	RETURN_STRING (pr, Key_KeynumToString (keynum));
};

static void
bi_Key_StringToKeynum (progs_t *pr)
{
	const char *keyname = P_GSTRING (pr, 0);
	R_INT (pr) = Key_StringToKeynum (keyname);
}

static builtin_t builtins[] = {
	{"Key_keydown",			bi_Key_keydown,		-1},
	{"Key_SetBinding",		bi_Key_SetBinding,		-1},
	{"Key_LookupBinding",	bi_Key_LookupBinding,	-1},
	{"Key_CountBinding",	bi_Key_CountBinding,	-1},
	{"Key_KeynumToString",	bi_Key_KeynumToString,	-1},
	{"Key_StringToKeynum",	bi_Key_StringToKeynum,	-1},
	{0}
};

void
RUA_Key_Init (progs_t *pr)
{
	PR_RegisterBuiltins (pr, builtins);
}
