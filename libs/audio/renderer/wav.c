/*
	wav.c

	wav file loading

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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/riff.h"

#include "snd_render.h"


void
SND_LoadWav (QFile *file, sfx_t *sfx, char *realname)
{
	riff_t     *riff;
	riff_d_chunk_t **ck;

	riff_format_t *fmt;
	riff_d_format_t *dfmt = 0;

	riff_data_t *data = 0;

	riff_cue_t *cue;
	riff_d_cue_t *dcue;
	riff_d_cue_point_t *cp = 0;

	riff_list_t *list;
	riff_d_chunk_t **lck;

	riff_ltxt_t *ltxt;
	riff_d_ltxt_t *dltxt = 0;

	int         loop_start = -1, sample_count = -1;

	if (!(riff = riff_read (file))) {
		Sys_Printf ("bad riff file\n");
		return;
	}

	for (ck = riff->chunks; *ck; ck++) {
		RIFF_SWITCH ((*ck)->name) {
			case RIFF_CASE ('f','m','t',' '):
				fmt = (riff_format_t *) *ck;
				dfmt = (riff_d_format_t *) fmt->fdata;
				break;
			case RIFF_CASE ('d','a','t','a'):
				data = (riff_data_t *) *ck;
				break;
			case RIFF_CASE ('c','u','e',' '):
				cue = (riff_cue_t *) *ck;
				dcue = cue->cue;
				if (dcue->count)
					cp = &dcue->cue_points[dcue->count - 1];
				break;
			case RIFF_CASE ('L','I','S','T'):
				list = (riff_list_t *) *ck;
				RIFF_SWITCH (list->name) {
					case RIFF_CASE ('a','d','t','l'):
						for (lck = list->chunks; *lck; lck++) {
							RIFF_SWITCH ((*lck)->name) {
								case RIFF_CASE ('l','t','x','t'):
									ltxt = (riff_ltxt_t *) *ck;
									dltxt = &ltxt->ltxt;
									break;
							}
						}
						break;
				}
				break;
		}
	}
	if (!dfmt) {
		Sys_Printf ("missing format chunk\n");
		goto bail;
	}
	if (!data) {
		Sys_Printf ("missing data chunk\n");
		goto bail;
	}
	if (dltxt)
		sample_count = dltxt->len;
	if (cp)
		loop_start = cp->sample_offset;
bail:
	Qclose (file);
	riff_free (riff);
}
