/*
	vorbis.c

	ogg/vorbis support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/6/14

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#ifdef HAVE_VORBIS

#include <stdlib.h>
#include <vorbis/vorbisfile.h>

#include "QF/cvar.h"
#include "QF/quakeio.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "snd_render.h"

static size_t
read_func (void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return Qread (datasource, ptr, size * nmemb);
}

static int
seek_func (void *datasource, ogg_int64_t offset, int whence)
{
	return Qseek (datasource, offset, whence);
}

static int
close_func (void *datasource)
{
	Qclose (datasource);
	return 0;
}

static long
tell_func (void *datasource)
{
	return Qtell (datasource);
}

static ov_callbacks callbacks = {
	read_func,
	seek_func,
	close_func,
	tell_func,
};

sfxcache_t *
SND_LoadOgg (QFile *file, sfx_t *sfx, cache_allocator_t allocator)
{
	OggVorbis_File vf;
	vorbis_info *vi;
	long        samples, size;
	byte       *data, *d;
	int         current_section;
	int         sample_start = -1, sample_count = 0;
	sfxcache_t *sc = 0;
	char      **ptr;

	if (ov_open_callbacks (file, &vf, 0, 0, callbacks) < 0) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		Qclose (file);
		return 0;
	}
	vi = ov_info (&vf, -1);
	samples = ov_pcm_total (&vf, -1);

	for (ptr = ov_comment (&vf, -1)->user_comments; *ptr; ptr++) {
		Sys_DPrintf ("%s\n", *ptr);
		if (strncmp ("CUEPOINT=", *ptr, 9) == 0) {
			sscanf (*ptr + 9, "%d %d", &sample_start, &sample_count);
		} else {
			sample_count = samples;
		}
	}
	if (sample_start != -1)
		samples = sample_start + sample_count;
	size = samples * vi->channels * 2;
	if (developer->int_val) {
		Sys_Printf ("\nBitstream is %d channel, %ldHz\n",
					vi->channels, vi->rate);
		Sys_Printf ("\nDecoded length: %ld samples (%ld bytes)\n",
					samples, size);
		Sys_Printf ("Encoded by: %s\n\n", ov_comment (&vf, -1)->vendor);
	}
	data = malloc (size);
	if (!data)
		goto bail;
	sc = SND_GetCache (samples, vi->rate, 2, vi->channels, sfx, allocator);
	if (!sc)
		goto bail;
	d = data;
	while (size) {
		int         res = ov_read (&vf, d, size, 0, 2, 1, &current_section);
		if (res > 0) {
			size -= res;
			d += res;
		} else if (res < 0) {
			Sys_Printf ("vorbis error %d\n", res);
			goto bail;
		} else {
			Sys_Printf ("unexpected eof\n");
			break;
		}
	}
	sc->loopstart = sample_start;
	SND_ResampleSfx (sc, data);
  bail:
	if (data)
		free (data);
	ov_clear (&vf);
	return sc;
}

#else//HAVE_VORBIS

#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/quakeio.h"

sfxcache_t *
SND_LoadOgg (QFile *file, sfx_t *sfx, cache_allocator_t allocator)
{
	Sys_Printf ("Ogg/Vorbis support not available, sorry.\n");
	Qclose (file);
	return 0;
}
#endif//HAVE_VORBIS
