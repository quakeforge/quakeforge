/*
	nonintel.c

	code for non-Intel processors only

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

#ifdef PIC
#undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifndef USE_INTEL_ASM
#include "r_local.h"

int         r_bmodelactive;

void
R_SurfPatch (void)
{
	// we patch code only on Intel
}

void
R_SurfacePatch (void)
{
	// we patch code only on Intel
}
#endif // !USE_INTEL_ASM
