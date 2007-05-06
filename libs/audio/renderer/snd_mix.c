/*
	snd_mix.c

	portable code to mix sounds for snd_dma.c

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

#include "winquake.h"

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "compat.h"
#include "snd_render.h"

cvar_t     *snd_volume;

unsigned    snd_paintedtime;				// sample PAIRS

portable_samplepair_t snd_paintbuffer[PAINTBUFFER_SIZE * 2];
static int  max_overpaint;				// number of extra samples painted
										// due to phase shift
static int         snd_scaletable[32][256];

/* CHANNEL MIXING */

static inline int
check_channel_end (channel_t *ch, sfx_t *sfx, int count, unsigned ltime)
{
	if (count <= 0 || ltime >= ch->end) {
		if (sfx->loopstart != (unsigned) -1) {
			ch->pos = sfx->loopstart;
			ch->end = ltime + sfx->length - ch->pos;
		} else {			// channel just stopped
			ch->done = 1;
			return 1;
		}
	}
	return 0;
}

static inline void
snd_paint_channel (channel_t *ch, sfxbuffer_t *sc, int count)
{
	unsigned    pos;
	int         offs = 0;
	byte       *samps;

	if ((int) ch->pos < 0) {
		ch->pos += count;
		if ((int) ch->pos <= 0)
			return;
		offs = count - ch->pos;
		count -= offs;
		ch->pos = 0;
	}
	if (ch->pos < sc->pos || ch->pos - sc->pos >= sc->length)
		sc->setpos (sc, ch->pos);
	pos = (ch->pos - sc->pos + sc->tail) % sc->length;
	samps = sc->data + pos * sc->bps;

	if (pos + count > sc->length) {
		unsigned    sub = sc->length - pos;
		sc->paint (offs, ch, samps, sub);
		sc->paint (offs + sub, ch, sc->data, count - sub);
	} else {
		sc->paint (offs, ch, samps, count);
	}
	ch->pos += count;
}

void
SND_PaintChannels (unsigned endtime)
{
	unsigned    end, ltime;
	int         i, count;
	channel_t  *ch;
	sfx_t      *sfx;
	sfxbuffer_t *sc;

	// clear the paint buffer
	memset (snd_paintbuffer, 0, sizeof (snd_paintbuffer));

	while (snd_paintedtime < endtime) {
		// if snd_paintbuffer is smaller than DMA buffer
		end = endtime;
		if (end - snd_paintedtime > PAINTBUFFER_SIZE)
			end = snd_paintedtime + PAINTBUFFER_SIZE;

		max_overpaint = 0;

		// paint in the channels.
		ch = snd_channels;
		for (i = 0; i < snd_total_channels; i++, ch++) {
			if (!(sfx = ch->sfx))
				continue;
			if (ch->stop || ch->done) {
				ch->done = 1;		// acknowledge stopped signal
				continue;
			}
			if (ch->pause)
				continue;
			sc = sfx->getbuffer (sfx);
			if (!sc) {				// something went wrong with the sfx
				printf ("XXXX sfx blew up!!!!\n");
				continue;
			}

			if (!ch->end)
				ch->end = snd_paintedtime + sfx->length - ch->pos;

			ltime = snd_paintedtime;

			while (ltime < end) {		// paint up to end
				count = ((ch->end < end) ? ch->end : end) - ltime;
				if (count > 0) {
					if (ch->leftvol || ch->rightvol) {
						snd_paint_channel (ch, sc, count);
						if (sc->advance)
							sc->advance (sc, count);
					}
					ltime += count;
				}

				if (check_channel_end (ch, sfx, count, ltime))
					break;
			}
		}

		// transfer out according to DMA format
		snd_shm->xfer (end);

		memmove (snd_paintbuffer, snd_paintbuffer + end - snd_paintedtime,
				 max_overpaint * sizeof (snd_paintbuffer[0]));
		memset (snd_paintbuffer + max_overpaint, 0, sizeof (snd_paintbuffer)
				- max_overpaint * sizeof (snd_paintbuffer[0]));

		snd_paintedtime = end;
	}
}

void
SND_InitScaletable (void)
{
	int			i, j;

	for (i = 0; i < 32; i++)
		for (j = 0; j < 256; j++)
			snd_scaletable[i][j] = ((signed char) j) * i * 8;
}

static void
snd_paint_mono_8 (int offs, channel_t *ch, void *bytes, unsigned count)
{
	byte       *sfx;
	int         data;
	unsigned    i;
	int        *lscale, *rscale;
	portable_samplepair_t *pair;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = (byte *) bytes;

	pair = snd_paintbuffer + offs;

	for (i = 0; i < count; i++) {
		data = sfx[i];
		pair[i].left += lscale[data];
		pair[i].right += rscale[data];
	}
}

static void
snd_paint_mono_16 (int offs, channel_t *ch, void *bytes, unsigned count)
{
	int         leftvol, rightvol;
	unsigned    left_phase, right_phase;	// Never allowed < 0 anyway
	unsigned    i = 0;
	short      *sfx;
	portable_samplepair_t *pair;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;

	max_overpaint = max (abs (ch->phase),
						 max (abs (ch->oldphase), max_overpaint));

	sfx = (signed short *) bytes;

	pair = snd_paintbuffer + offs;

	if (ch->phase >= 0) {
		left_phase = ch->phase;
		right_phase = 0;
	} else {
		left_phase = 0;
		right_phase = -ch->phase;
	}

	if (ch->oldphase != ch->phase) {
		unsigned    old_phase_left, old_phase_right;
		unsigned    new_phase_left, new_phase_right;
		unsigned    count_left, count_right, c;

		if (ch->oldphase >= 0) {
			old_phase_left = ch->oldphase;
			old_phase_right = 0;
		} else {
			old_phase_left = 0;
			old_phase_right = -ch->oldphase;
		}
		new_phase_left = left_phase;
		new_phase_right = right_phase;

		if (new_phase_left > old_phase_left)
			count_left = 2 * (new_phase_left - old_phase_left);
		else
			count_left = old_phase_left - new_phase_left;

		if (new_phase_right > old_phase_right)
			count_right = 2 * (new_phase_right - old_phase_right);
		else
			count_right = old_phase_right - new_phase_right;

		c = min (count, max (count_right, count_left));
		count -= c;
		while (c) {
			int         left = (sfx[i] * leftvol) >> 8;
			int         right = (sfx[i] * rightvol) >> 8;

			if (new_phase_left < old_phase_left) {
				if (!(count_left & 1)) {
					pair[i + old_phase_left].left += left;
					old_phase_left--;
				}
				count_left--;
			} else {
				if (new_phase_left > old_phase_left) {
					pair[i + old_phase_left].left += left;
					old_phase_left++;
				}
				pair[i + old_phase_left].left += left;
			}

			if (new_phase_right < old_phase_right) {
				if (!(count_right & 1)) {
					pair[i + old_phase_right].right += right;
					old_phase_right--;
				}
				count_right--;
			} else {
				if (new_phase_right > old_phase_right) {
					pair[i + old_phase_right].right += right;
					old_phase_right++;
				}
				pair[i + old_phase_right].right += right;
			}

			c--;
			i++;
		}
	}

	for (i = 0; i < count; i++) {
		pair[i + left_phase].left += (sfx[i] * leftvol) >> 8;
		pair[i + right_phase].right += (sfx[i] * rightvol) >> 8;
	}
}

static void
snd_paint_stereo_8 (int offs, channel_t *ch, void *bytes, unsigned count)
{
	byte       *samp;
	portable_samplepair_t *pair;
	int		   *lscale, *rscale;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];

	samp = bytes;
	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		pair->left += lscale[*samp++];
		pair->right += rscale[*samp++];
		pair++;
	}
}

static void
snd_paint_stereo_16 (int offs, channel_t *ch, void *bytes, unsigned count)
{
	short      *samp;
	portable_samplepair_t *pair;
	int         leftvol = ch->leftvol;
	int         rightvol = ch->rightvol;

	samp = (short *) bytes;
	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		pair->left += (*samp++ * leftvol) >> 8;
		pair->right += (*samp++ * rightvol) >> 8;
		pair++;
	}
}

void
SND_SetPaint (sfxbuffer_t *sc)
{
	wavinfo_t *info = sc->sfx->wavinfo (sc->sfx);
	if (snd_loadas8bit->int_val) {
		if (info->channels == 2)
			sc->paint = snd_paint_stereo_8;
		else
			sc->paint = snd_paint_mono_8;
	} else {
		if (info->channels == 2)
			sc->paint = snd_paint_stereo_16;
		else
			sc->paint = snd_paint_mono_16;
	}
}
