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

typedef struct {
	byte        left;
	byte        right;
} stereo8_t;

typedef struct {
	short       left;
	short       right;
} stereo16_t;

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
SND_CacheTouch (sfx_t *sfx)
{
	return Cache_Check (&((sfxblock_t *) sfx->data)->cache);
}

sfxbuffer_t *
SND_CacheRetain (sfx_t *sfx)
{
	return Cache_TryGet (&((sfxblock_t *) sfx->data)->cache);
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

wavinfo_t *
SND_CacheWavinfo (sfx_t *sfx)
{
	return &((sfxblock_t *) sfx->data)->wavinfo;
}

wavinfo_t *
SND_StreamWavinfo (sfx_t *sfx)
{
	return &((sfxstream_t *) sfx->data)->wavinfo;
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
#ifdef HAVE_VORBIS
	if (strnequal ("OggS", buf, 4)) {
		Sys_DPrintf ("SND_Load: ogg file\n");
		SND_LoadOgg (file, sfx, realname);
		return;
	}
#endif
	if (strnequal ("RIFF", buf, 4)) {
		Sys_DPrintf ("SND_Load: wav file\n");
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
//printf ("%ld %d\n", samples, size);
	size *= width * channels;
	sc = allocator (&block->cache, sizeof (sfxbuffer_t) + size, sfx->name);
	if (!sc)
		return 0;
	memcpy (sc->data + size, "\xde\xad\xbe\xef", 4);
	return sc;
}

void
SND_ResampleMono (sfxbuffer_t *sc, byte *data, int length)
{
	unsigned char *ib, *ob;
	int			fracstep, outcount, sample, samplefrac, srcsample, i;
	float		stepscale;
	short	   *is, *os;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inwidth = info->width;
	int         inrate = info->rate;
	int         outwidth;

	is = (short *) data;
	os = (short *) sc->data;
	ib = data;
	ob = sc->data;

	os += sc->head;
	ob += sc->head;

	stepscale = (float) inrate / shm->speed;	// usually 0.5, 1, or 2

	outcount = length / stepscale;
//printf ("%d %d\n", length, outcount);

	sc->sfx->length = info->samples / stepscale;
	if (info->loopstart != -1)
		sc->sfx->loopstart = info->loopstart / stepscale;
	else
		sc->sfx->loopstart = -1;

	if (snd_loadas8bit->int_val) {
		outwidth = 1;
		sc->paint = SND_PaintChannelFrom8;
	} else {
		outwidth = 2;
		sc->paint = SND_PaintChannelFrom16;
	}

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1 && outwidth == 1) {
			for (i = 0; i < outcount; i++) {
				*ob++ = *ib++ - 128;
			}
		} else if (inwidth == 1 && outwidth == 2) {
			for (i = 0; i < outcount; i++) {
				*os++ = (*ib++ - 128) << 8;
			}
		} else if (inwidth == 2 && outwidth == 1) {
			for (i = 0; i < outcount; i++) {
				*ob++ = LittleShort (*is++) >> 8;
			}
		} else if (inwidth == 2 && outwidth == 2) {
			for (i = 0; i < outcount; i++) {
				*os++ = LittleShort (*is++);
			}
		}
	} else {
		// general case
		if (snd_interp->int_val && stepscale < 1) {
			int			j;
			int			points = 1 / stepscale;

			for (i = 0; i < length; i++) {
				int         s1, s2;

				if (inwidth == 2) {
					s2 = s1 = LittleShort (is[0]);
					if (i < length - 1)
						s2 = LittleShort (is[1]);
					is++;
				} else {
					s2 = s1 = (ib[0] - 128) << 8;
					if (i < length - 1)
						s2 = (ib[1] - 128) << 8;
					ib++;
				}
				for (j = 0; j < points; j++) {
					sample = s1 + (s2 - s1) * ((float) j) / points;
					if (outwidth == 2) {
						os[j] = sample;
					} else {
						ob[j] = sample >> 8;
					}
				}
				if (outwidth == 2) {
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
					sample = LittleShort (is[srcsample]);
				else
					sample = (ib[srcsample] - 128) << 8;
				if (outwidth == 2)
					os[i] = sample;
				else
					ob[i] = sample >> 8;
			}
		}
	}
	{
		byte       *x = sc->data + sc->length * outwidth;
		if (memcmp (x, "\xde\xad\xbe\xef", 4))
			Sys_Error ("SND_ResampleMono screwed the pooch %02x%02x%02x%02x",
					   x[0], x[1], x[2], x[3]);
	}
}

void
SND_ResampleStereo (sfxbuffer_t *sc, byte *data, int length)
{
	int			fracstep, outcount, sl, sr, samplefrac, srcsample, i;
	float		stepscale;
	stereo8_t  *ib, *ob;
	stereo16_t *is, *os;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inwidth = info->width;
	int         inrate = info->rate;
	int         outwidth;

	is = (stereo16_t *) data;
	os = (stereo16_t *) sc->data;
	ib = (stereo8_t *) data;
	ob = (stereo8_t *) sc->data;

	os += sc->head;
	ob += sc->head;

	stepscale = (float) inrate / shm->speed;	// usually 0.5, 1, or 2

	outcount = length / stepscale;

	sc->sfx->length = info->samples / stepscale;
	if (info->loopstart != -1)
		sc->sfx->loopstart = info->loopstart / stepscale;
	else
		sc->sfx->loopstart = -1;

	if (snd_loadas8bit->int_val) {
		outwidth = 1;
		sc->paint = SND_PaintChannelStereo8;
	} else {
		outwidth = 2;
		sc->paint = SND_PaintChannelStereo16;
	}

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1 && outwidth == 1) {
			for (i = 0; i < outcount; i++, ob++, ib++) {
				ob->left = ib->left - 128;
				ob->right = ib->right - 128;
			}
		} else if (inwidth == 1 && outwidth == 2) {
			for (i = 0; i < outcount; i++, os++, ib++) {
				os->left = (ib->left - 128) << 8;
				os->right = (ib->right - 128) << 8;
			}
		} else if (inwidth == 2 && outwidth == 1) {
			for (i = 0; i < outcount; i++, ob++, ib++) {
				ob->left = LittleShort (is->left) >> 8;
				ob->right = LittleShort (is->right) >> 8;
			}
		} else if (inwidth == 2 && outwidth == 2) {
			for (i = 0; i < outcount; i++, os++, is++) {
				os->left = LittleShort (is->left);
				os->right = LittleShort (is->right);
			}
		}
	} else {
		// general case
		if (snd_interp->int_val && stepscale < 1) {
			int			j;
			int			points = 1 / stepscale;

			for (i = 0; i < length; i++) {
				int         sl1, sl2;
				int         sr1, sr2;

				if (inwidth == 2) {
					sl2 = sl1 = LittleShort (is[0].left);
					sr2 = sr1 = LittleShort (is[0].right);
					if (i < length - 1) {
						sl2 = LittleShort (is[1].left);
						sr2 = LittleShort (is[1].right);
					}
					is++;
				} else {
					sl2 = sl1 = (ib[0].left - 128) << 8;
					sr2 = sr1 = (ib[0].right - 128) << 8;
					if (i < length - 1) {
						sl2 = (ib[1].left - 128) << 8;
						sr2 = (ib[1].right - 128) << 8;
					}
					ib++;
				}
				for (j = 0; j < points; j++) {
					sl = sl1 + (sl2 - sl1) * ((float) j) / points;
					sr = sr1 + (sr2 - sr1) * ((float) j) / points;
					if (outwidth == 2) {
						os[j].left = sl;
						os[j].right = sr;
					} else {
						ob[j].left = sl >> 8;
						ob[j].right = sr >> 8;
					}
				}
				if (outwidth == 2) {
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
				if (inwidth == 2) {
					sl = LittleShort (is[srcsample].left);
					sr = LittleShort (is[srcsample].right);
				} else {
					sl = (ib[srcsample].left - 128) << 8;
					sr = (ib[srcsample].right - 128) << 8;
				}
				if (outwidth == 2) {
					os[i].left = sl;
					os[i].right = sr;
				} else {
					ob[i].left = sl >> 8;
					ob[i].right = sr >> 8;
				}
			}
		}
	}
}
