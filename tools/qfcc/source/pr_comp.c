/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QF/va.h>

#include "qfcc.h"


pr_info_t	pr;
def_t		*pr_global_defs[MAX_REGS];	// to find def for a global variable






void
PrecacheSound (def_t *e, int ch)
{
	char	*n;
	int 	i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numsounds; i++) {
		if (!strcmp (n, precache_sounds[i])) {
			return;
		}
	}

	if (numsounds == MAX_SOUNDS)
		Error ("PrecacheSound: numsounds == MAX_SOUNDS");

	strcpy (precache_sounds[i], n);
	if (ch >= '1' && ch <= '9')
		precache_sounds_block[i] = ch - '0';
	else
		precache_sounds_block[i] = 1;

	numsounds++;
}

void
PrecacheModel (def_t *e, int ch)
{
	char	*n;
	int 	i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < nummodels; i++) {
		if (!strcmp (n, precache_models[i])) {
			return;
		}
	}

	if (nummodels == MAX_MODELS)
		Error ("PrecacheModels: nummodels == MAX_MODELS");

	strcpy (precache_models[i], n);
	if (ch >= '1' && ch <= '9')
		precache_models_block[i] = ch - '0';
	else
		precache_models_block[i] = 1;

	nummodels++;
}

void
PrecacheFile (def_t *e, int ch)
{
	char	*n;
	int 	i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numfiles; i++) {
		if (!strcmp (n, precache_files[i])) {
			return;
		}
	}

	if (numfiles == MAX_FILES)
		Error ("PrecacheFile: numfiles == MAX_FILES");

	strcpy (precache_files[i], n);
	if (ch >= '1' && ch <= '9')
		precache_files_block[i] = ch - '0';
	else
		precache_files_block[i] = 1;

	numfiles++;
}
