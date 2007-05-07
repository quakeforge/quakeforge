/*
	snd_progs.c

	CSQC sound builtins

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id: rua_qfs.c 11332 2006-12-20 12:08:57Z taniwha $";

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/progs.h"
#include "QF/sound.h"

static void
bi_S_LocalSound (progs_t *pr)
{
	const char *sound = P_GSTRING (pr, 0);

	S_LocalSound (sound);
}

static builtin_t builtins[] = {
	{"S_LocalSound",		bi_S_LocalSound,		-1},
	{0}
};

VISIBLE void
S_Progs_Init (progs_t *pr)
{
	PR_RegisterBuiltins (pr, builtins);
}
