/*
	r_tent.c

	rendering entities

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
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>
#include <stdlib.h>

#include "QF/console.h"
#include "QF/model.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "r_dynamic.h"

#define     MAX_VISEDICTS   256
int         r_numvisedicts;
entity_t   *r_visedicts[MAX_VISEDICTS];


void
R_ClearEnts (void)
{
	r_numvisedicts = 0;
}

entity_t **
R_NewEntity (void)
{

	if (r_numvisedicts == MAX_VISEDICTS)
		return NULL;
	return &r_visedicts[r_numvisedicts++];
}
