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
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_VORBIS

#include <stdlib.h>
#include <vorbis/vorbisfile.h>

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vfile.h"

//FIXME should be in header
void SND_ResampleSfx (sfxcache_t *sc, int inrate, int inwidth, byte * data);

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
SND_LoadOgg (VFile *file, sfx_t *sfx, cache_allocator_t allocator)
{
	OggVorbis_File vf;
	vorbis_info *vi;
	long        samples, size;
	void       *data;
	int         current_section;
	sfxcache_t *sc = 0;

	if (ov_open_callbacks (file, &vf, 0, 0, callbacks) < 0) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		return 0;
	}
	vi = ov_info (&vf, -1);
	samples = ov_pcm_total (&vf, -1);
	if (developer->int_val) {
		char      **ptr = ov_comment (&vf, -1)->user_comments;

		while (*ptr)
			Sys_Printf ("%s\n", *ptr++);
		Sys_Printf ("\nBitstream is %d channel, %ldHz\n",
					vi->channels, vi->rate);
		Sys_Printf ("\nDecoded length: %ld samples\n", samples);
		Sys_Printf ("Encoded by: %s\n\n", ov_comment (&vf, -1)->vendor);
	}
	size = samples * vi->channels * 2;
	data = malloc (size);
	if (!data)
		goto bail;
	sc = allocator (&sfx->cache, size + sizeof (sfxcache_t), sfx->name);
	if (!sc)
		goto bail;
	ov_read (&vf, data, size, 0, 2, 1, &current_section);
	sc->length = samples;
	sc->loopstart = 0;
	sc->speed = vi->rate;
	sc->width = 16;
	sc->stereo = vi->channels;
	SND_ResampleSfx (sc, sc->speed, sc->width, data);
  bail:
	if (data)
		free (data);
	ov_clear (&vf);
	return sc;
}

#else//HAVE_VORBIS

#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vfile.h"

sfxcache_t *
SND_LoadOgg (VFile *file, sfx_t *sfx, cache_allocator_t allocator)
{
	Sys_Printf ("Ogg/Vorbis support not available, sorry.\n");
	Qclose (file);
	return 0;
}
#endif//HAVE_VORBIS
