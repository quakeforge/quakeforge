/*
	bi_inputline.c

	CSQC inputline builtins

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/console.h"
#include "QF/progs.h"
#include "QF/zone.h"

static void
bi_InputLine_Create (progs_t *pr)
{
}

static void
bi_InputLine_Destroy (progs_t *pr)
{
}

static void
bi_InputLine_Clear (progs_t *pr)
{
}

static void
bi_InputLine_Process (progs_t *pr)
{
}

static void
bi_InputLine_Draw (progs_t *pr)
{
}

void
InputLine_Progs_Init (progs_t *pr)
{
	PR_AddBuiltin (pr, "InputLine_Create", bi_InputLine_Create, -1);
	PR_AddBuiltin (pr, "InputLine_Destroy", bi_InputLine_Destroy, -1);
	PR_AddBuiltin (pr, "InputLine_Clear", bi_InputLine_Clear, -1);
	PR_AddBuiltin (pr, "InputLine_Process", bi_InputLine_Process, -1);
	PR_AddBuiltin (pr, "InputLine_Draw", bi_InputLine_Draw, -1);
}
