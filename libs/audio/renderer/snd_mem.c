/*
	snd_mem.c

	sound caching

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
#include "QF/dstring.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"

#include "compat.h"
#include "snd_render.h"

static sfxbuffer_t *
snd_fail (sfx_t *sfx)
{
	return 0;
}

static void
snd_noop (sfx_t *sfx)
{
}

sfxbuffer_t *
SND_CacheRetain (sfx_t *sfx)
{
	return Cache_TryGet (&((sfxblock_t *) sfx)->cache);
}

void
SND_CacheRelease (sfx_t *sfx)
{
	Cache_Release (&((sfxblock_t *) sfx->data)->cache);
}

sfxbuffer_t *
SND_StreamRetain (sfx_t *sfx)
{
	return &((sfxstream_t *) sfx->data)->buffer;
}

void
SND_StreamRelease (sfx_t *sfx)
{
}

void
SND_Load (sfx_t *sfx)
{
	dstring_t  *name = dstring_new ();
	dstring_t  *foundname = dstring_new ();
	char       *realname;
	char        buf[4];
	QFile      *file;

	sfx->touch = sfx->retain = snd_fail;
	sfx->release = snd_noop;

	dsprintf (name, "sound/%s", sfx->name);
	_QFS_FOpenFile (name->str, &file, foundname, 1);
	if (!file) {
		Sys_Printf ("Couldn't load %s\n", name->str);
		dstring_delete (name);
		dstring_delete (foundname);
		return;
	}
	if (!strequal (foundname->str, name->str)) {
		realname = foundname->str;
		dstring_delete (name);
		free (foundname);
	} else {
		realname = name->str;
		free (name);
		dstring_delete (foundname);
	}
	Qread (file, buf, 4);
	Qseek (file, 0, SEEK_SET);
	if (strnequal ("OggS", buf, 4)) {
		free (name);
		SND_LoadOgg (file, sfx, realname);
		return;
	}
	if (strnequal ("RIFF", buf, 4)) {
		free (name);
		SND_LoadWav (file, sfx, realname);
		return;
	}
	free (realname);
}

sfxbuffer_t *
SND_GetCache (long samples, int rate, int inwidth, int channels,
			  sfxblock_t *block, cache_allocator_t allocator)
{
	int         size;
	int         width;
	float		stepscale;
	sfxbuffer_t *sc;
	sfx_t      *sfx = block->sfx;

	width = snd_loadas8bit->int_val ? 1 : 2;
	stepscale = (float) rate / shm->speed;	// usually 0.5, 1, or 2
	size = samples / stepscale;
	size *= width * channels;
	sc = allocator (&block->cache, sizeof (sfxbuffer_t) + size, sfx->name);
	if (!sc)
		return 0;
	sfx->length = samples;
	sfx->speed = rate;
	sfx->width = inwidth;
	sfx->channels = channels;
	block->bytes = size;
	return sc;
}

void
SND_ResampleSfx (sfx_t *sfx, sfxbuffer_t *sc, byte *data)
{
	unsigned char *ib, *ob;
	int			fracstep, outcount, sample, samplefrac, srcsample, i;
	float		stepscale;
	short	   *is, *os;
	int         inwidth = sfx->width;
	int         inrate = sfx->speed;

	is = (short *) data;
	os = (short *) sc->data;
	ib = data;
	ob = sc->data;

	stepscale = (float) inrate / shm->speed;	// usually 0.5, 1, or 2

	outcount = sfx->length / stepscale;

	sfx->speed = shm->speed;
	if (snd_loadas8bit->int_val)
		sfx->width = 1;
	else
		sfx->width = 2;
	sfx->channels = 1;

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1 && sfx->width == 1) {
			for (i = 0; i < outcount; i++) {
				*ob++ = *ib++ - 128;
			}
		} else if (inwidth == 1 && sfx->width == 2) {
			for (i = 0; i < outcount; i++) {
				*os++ = (*ib++ - 128) << 8;
			}
		} else if (inwidth == 2 && sfx->width == 1) {
			for (i = 0; i < outcount; i++) {
				*ob++ = LittleShort (*is++) >> 8;
			}
		} else if (inwidth == 2 && sfx->width == 2) {
			for (i = 0; i < outcount; i++) {
				*os++ = LittleShort (*is++);
			}
		}
	} else {
		// general case
		if (snd_interp->int_val && stepscale < 1) {
			int			j;
			int			points = 1 / stepscale;

			for (i = 0; i < sfx->length; i++) {
				int         s1, s2;

				if (inwidth == 2) {
					s2 = s1 = LittleShort (is[0]);
					if (i < sfx->length - 1)
						s2 = LittleShort (is[1]);
					is++;
				} else {
					s2 = s1 = (ib[0] - 128) << 8;
					if (i < sfx->length - 1)
						s2 = (ib[1] - 128) << 8;
					ib++;
				}
				for (j = 0; j < points; j++) {
					sample = s1 + (s2 - s1) * ((float) j) / points;
					if (sfx->width == 2) {
						os[j] = sample;
					} else {
						ob[j] = sample >> 8;
					}
				}
				if (sfx->width == 2) {
					os += points;
				} else {
					ob += points;
				}
			}
		} else {
			samplefrac = 0;
			fracstep = stepscale * 256;
			for (i = 0; i < outcount; i++) {
				srcsample = samplefrac >> 8;
				samplefrac += fracstep;
				if (inwidth == 2)
					sample = LittleShort (((short *) data)[srcsample]);
				else
					sample =
						(int) ((unsigned char) (data[srcsample]) - 128) << 8;
				if (sfx->width == 2)
					((short *) sc->data)[i] = sample;
				else
					((signed char *) sc->data)[i] = sample >> 8;
			}
		}
	}

	sfx->length = outcount;
	if (sfx->loopstart != -1)
		sfx->loopstart = sfx->loopstart / stepscale;
}
