/*
	bi_keys.c

	CSQC key-api builtins

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/keys.h"
#include "QF/progs.h"
#include "QF/zone.h"

/*
    bi_Key_SetBinding
    
    QC-Function for set a binding
*/
static void
bi_Key_SetBinding (progs_t *pr)
{
	int	        target  = G_INT (pr, OFS_PARM0);
	int         keynum  = G_INT (pr, OFS_PARM1);
	const char *binding = G_STRING (pr, OFS_PARM2);

	if(strlen(binding) == 0 || binding[0] == '\0') {
		binding = NULL;	/* unbind a binding */
	}

	Key_SetBinding (target, keynum, binding);
}

static void
bi_Key_LookupBinding (progs_t *pr)
{
	int	        target  = G_INT (pr, OFS_PARM0);
	const char *binding = G_STRING (pr, OFS_PARM1);
	int i;
	knum_t keynum = -1;
	const char *keybind = NULL;

	for (i = 0; i < QFK_LAST; i++) {
		keybind = keybindings[target][i];
		if(keybind == NULL) { continue; }
		if(strcmp(keybind, binding) == 0) {
			keynum = i;
		}
	}

	G_INT (pr, OFS_RETURN) = keynum;	
};

static void
bi_Key_KeynumToString (progs_t *pr)
{
	int	        keynum  = G_INT (pr, OFS_PARM0);

	RETURN_STRING (pr, Key_KeynumToString (keynum));
};


void
Key_Progs_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "Key_SetBinding", bi_Key_SetBinding, -1);
	PR_AddBuiltin (pr, "Key_LookupBinding", bi_Key_LookupBinding, -1);
	PR_AddBuiltin (pr, "Key_KeynumToString", bi_Key_KeynumToString, -1);
// NEED THIS ?//	PR_AddBuiltin (pr, "Key_StringToKeynum", bi_Key_KeynumToString, -1);
}
