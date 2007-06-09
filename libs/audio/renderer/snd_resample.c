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

static __attribute__ ((used)) const char rcsid[] = 
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

cvar_t         *snd_loadas8bit;
cvar_t         *snd_interp;

static void
check_buffer_integrity (sfxbuffer_t *sc, int width, const char *func)
{
	byte       *x = sc->data + sc->length * width;
	if (memcmp (x, "\xde\xad\xbe\xef", 4))
		Sys_Error ("%s screwed the pooch %02x%02x%02x%02x", func,
				   x[0], x[1], x[2], x[3]);
}

void
SND_ResampleMono (sfxbuffer_t *sc, byte *data, int length, void *prev)
{
	byte       *ib, *ob, *pb;
	int			fracstep, outcount, sample, samplefrac, srcsample, i;
	float		stepscale;
	short	   *is, *os, *ps;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inwidth = info->width;
	int         inrate = info->rate;
	int         outwidth;
	short       zero_s[1];
	byte        zero_b[1];

	is = (short *) data;
	os = (short *) sc->data;
	ib = data;
	ob = sc->data;

	ps = zero_s;
	pb = zero_b;

	if (!prev) {
		zero_s[0] = 0;
		zero_b[0] = 128;
	}

	os += sc->head;
	ob += sc->head;

	stepscale = (float) inrate / snd_shm->speed;

	outcount = length / stepscale;

	sc->sfx->length = info->samples / stepscale;
	//printf ("%s %d %g %d\n", sc->sfx->name, length, stepscale, sc->sfx->length);
	if (info->loopstart != (unsigned int)-1)
		sc->sfx->loopstart = info->loopstart / stepscale;
	else
		sc->sfx->loopstart = (unsigned int)-1;

	if (snd_loadas8bit->int_val) {
		sc->bps = outwidth = 1;
		if (prev) {
			zero_s[0] = ((char *)prev)[0];
			zero_b[0] = ((char *)prev)[0] + 128;
		}
	} else {
		sc->bps = outwidth = 2;
		if (prev) {
			zero_s[0] = ((short *)prev)[0];
			zero_b[0] = (((short *)prev)[0] >> 8) + 128;
		}
	}

	if (!length)
		return;

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++) {
					*ob++ = *ib++ - 128;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++) {
					*os++ = (*ib++ - 128) << 8;
				}
			} else {
				goto general_Mono;
			}
		} else if (inwidth == 2) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++) {
					*ob++ = LittleShort (*is++) >> 8;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++) {
					*os++ = LittleShort (*is++);
				}
			} else {
				goto general_Mono;
			}
		}
	} else {				// general case
general_Mono:
		if (snd_interp->int_val && stepscale < 1) {
			int			j;
			int			points = 1 / stepscale;

			for (i = 0; i < length; i++) {
				int         s1, s2;

				if (inwidth == 2) {
					s1 = LittleShort (*ps);
					s2 = LittleShort (*is);
					ps = is++;
				} else {
					s1 = (*pb - 128) << 8;
					s2 = (*ib - 128) << 8;
					pb = ib++;
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
	check_buffer_integrity (sc, outwidth, __FUNCTION__);
}

void
SND_ResampleStereo (sfxbuffer_t *sc, byte *data, int length, void *prev)
{
	int			fracstep, outcount, outwidth, samplefrac, srcsample, sl, sr, i;
	float		stepscale;
	stereo8_t  *ib, *ob, *pb;
	stereo16_t *is, *os, *ps;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inwidth = info->width;
	int         inrate = info->rate;
	stereo16_t  zero_s;
	stereo8_t   zero_b;

	is = (stereo16_t *) data;
	os = (stereo16_t *) sc->data;
	ib = (stereo8_t *) data;
	ob = (stereo8_t *) sc->data;

	ps = &zero_s;
	pb = &zero_b;

	if (!prev) {
		zero_s.left = zero_s.right = 0;
		zero_b.left = zero_b.right = 128;
	}

	os += sc->head;
	ob += sc->head;

	stepscale = (float) inrate / snd_shm->speed;

	outcount = length / stepscale;

	sc->sfx->length = info->samples / stepscale;
	if (info->loopstart != (unsigned int)-1)
		sc->sfx->loopstart = info->loopstart / stepscale;
	else
		sc->sfx->loopstart = (unsigned int)-1;

	if (snd_loadas8bit->int_val) {
		outwidth = 1;
		sc->bps = 2;
		if (prev) {
			zero_s.left = ((char *)prev)[0];
			zero_s.right = ((char *)prev)[1];
			zero_b.left = ((char *)prev)[0] + 128;
			zero_b.right = ((char *)prev)[1] + 128;
		}
	} else {
		outwidth = 2;
		sc->bps = 4;
		if (prev) {
			zero_s.left = ((short *)prev)[0];
			zero_s.right = ((short *)prev)[1];
			zero_b.left = (((short *)prev)[0] >> 8) + 128;
			zero_b.right = (((short *)prev)[1] >> 8) + 128;
		}
	}

	if (!length)
		return;

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++, ob++, ib++) {
					ob->left = ib->left - 128;
					ob->right = ib->right - 128;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++, os++, ib++) {
					os->left = (ib->left - 128) << 8;
					os->right = (ib->right - 128) << 8;
				}
			} else {
				goto general_Stereo;
			}
		} else if (inwidth == 2) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++, ob++, ib++) {
					ob->left = LittleShort (is->left) >> 8;
					ob->right = LittleShort (is->right) >> 8;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++, os++, is++) {
					os->left = LittleShort (is->left);
					os->right = LittleShort (is->right);
				}
			} else {
				goto general_Stereo;
			}
		}
	} else {				// general case
general_Stereo:
		if (snd_interp->int_val && stepscale < 1) {
			int			j;
			int			points = 1 / stepscale;

			for (i = 0; i < length; i++) {
				int         sl1, sl2;
				int         sr1, sr2;

				if (inwidth == 2) {
					sl1 = LittleShort (ps->left);
					sr1 = LittleShort (ps->right);
					sl2 = LittleShort (is->left);
					sr2 = LittleShort (is->right);
					ps = is++;
				} else {
					sl1 = (pb->left - 128) << 8;
					sr1 = (pb->right - 128) << 8;
					sl2 = (ib->left - 128) << 8;
					sr2 = (ib->right - 128) << 8;
					pb = ib++;
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
	check_buffer_integrity (sc, outwidth * 2, __FUNCTION__);
}
