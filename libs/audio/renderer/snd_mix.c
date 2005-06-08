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

static __attribute__ ((unused)) const char rcsid[] = 
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

#define	PAINTBUFFER_SIZE	512
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE * 2];
static int  max_overpaint;				// number of extra samples painted
										// due to phase shift
int         snd_scaletable[32][256];
int        *snd_p, snd_linear_count, snd_vol;
short      *snd_out;

void
SND_WriteLinearBlastStereo16 (void)
{
	int			val, i;

	for (i = 0; i < snd_linear_count; i += 2) {
		val = (snd_p[i] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short) 0x8000)
			snd_out[i] = (short) 0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i + 1] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i + 1] = 0x7fff;
		else if (val < (short) 0x8000)
			snd_out[i + 1] = (short) 0x8000;
		else
			snd_out[i + 1] = val;
	}
}

static void
s_xfer_stereo_16 (int endtime)
{
	int			lpaintedtime, lpos;
	unsigned long *pbuf;

	snd_vol = volume->value * 256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;


	pbuf = (unsigned long *) shm->buffer;

	while (lpaintedtime < endtime) {
		// handle recirculating buffer issues
		lpos = lpaintedtime & ((shm->samples >> 1) - 1);

		snd_out = (short *) pbuf + (lpos << 1);

		snd_linear_count = (shm->samples >> 1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		SND_WriteLinearBlastStereo16 ();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}
}

static void
s_xfer_paint_buffer (int endtime)
{
	int			count, out_idx, out_mask, snd_vol, step, val;
	int		   *p;
	unsigned long *pbuf;

	if (shm->samplebits == 16 && shm->channels == 2) {
		s_xfer_stereo_16 (endtime);
		return;
	}

	p = (int *) paintbuffer;
	count = (endtime - paintedtime) * shm->channels;
	out_mask = shm->samples - 1;
	out_idx = paintedtime * shm->channels & out_mask;
	step = 3 - shm->channels;
	snd_vol = volume->value * 256;

	pbuf = (unsigned long *) shm->buffer;

	if (shm->samplebits == 16) {
		short      *out = (short *) pbuf;

		while (count--) {
			val = (*p * snd_vol) >> 8;
			p += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short) 0x8000)
				val = (short) 0x8000;
			out[out_idx] = val;
			out_idx = (out_idx + 1) & out_mask;
		}
	} else if (shm->samplebits == 8) {
		unsigned char *out = (unsigned char *) pbuf;

		while (count--) {
			val = (*p * snd_vol) >> 8;
			p += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < (short) 0x8000)
				val = (short) 0x8000;
			out[out_idx] = (val >> 8) + 128;
			out_idx = (out_idx + 1) & out_mask;
		}
	}
}

/* CHANNEL MIXING */

void
SND_PaintChannels (unsigned int endtime)
{
	unsigned int end, ltime;
	int         i, count;
	channel_t  *ch;
	sfxbuffer_t *sc;

	while (paintedtime < endtime) {
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
//		memset (paintbuffer, 0, (end - paintedtime) *
//				sizeof (portable_samplepair_t));
		max_overpaint = 0;

		// paint in the channels.
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++) {
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			sc = ch->sfx->retain (ch->sfx);
			if (!sc)
				continue;

			ltime = paintedtime;

			while (ltime < end) {		// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0) {
					sc->paint (ch, sc, count);

					if (sc->advance)
						sc->advance (sc, count);

					ltime += count;
				}

				// if at end of loop, restart
				if (ltime >= ch->end) {
					if (ch->sfx->loopstart != (unsigned int) -1) {
						ch->pos = ch->sfx->loopstart;
						ch->end = ltime + ch->sfx->length - ch->pos;
					} else {			// channel just stopped
						ch->sfx->release (ch->sfx);
						ch->sfx->close (ch->sfx);
						ch->sfx = NULL;
						break;
					}
				}
			}

			if (ch->sfx)
				ch->sfx->release (ch->sfx);
		}

		// transfer out according to DMA format
		s_xfer_paint_buffer (end);

		memmove (paintbuffer, paintbuffer + end - paintedtime,
				 max_overpaint * sizeof (paintbuffer[0]));
		memset (paintbuffer + max_overpaint, 0, sizeof (paintbuffer)
				- max_overpaint * sizeof (paintbuffer[0]));

		paintedtime = end;
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
snd_paint_mono_8 (int offs, channel_t *ch, void *bytes, unsigned int count)
{
	unsigned char  *sfx;
	int				data;
	unsigned int    i;
	int		       *lscale, *rscale;
	portable_samplepair_t *pair;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = (unsigned char *) bytes;

	pair = paintbuffer + offs;

	for (i = 0; i < count; i++) {
		data = sfx[i];
		pair[i].left += lscale[data];
		pair[i].right += rscale[data];
	}
}

static void
snd_paint_mono_16 (int offs, channel_t *ch, void *bytes, unsigned int count)
{
	int		leftvol, rightvol;
	unsigned int	left_phase, right_phase;	// Never allowed < 0 anyway
	unsigned int	i = 0;
	signed short   *sfx;
	portable_samplepair_t *pair;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;

	max_overpaint = max (abs (ch->phase),
						 max (abs (ch->oldphase), max_overpaint));

	sfx = (signed short *) bytes;

	pair = paintbuffer + offs;

	if (ch->phase >= 0) {
		left_phase = ch->phase;
		right_phase = 0;
	} else {
		left_phase = 0;
		right_phase = -ch->phase;
	}

	if (ch->oldphase != ch->phase) {
		unsigned int old_phase_left, old_phase_right;
		unsigned int new_phase_left, new_phase_right;
		unsigned int count_left, count_right, c;

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
snd_paint_stereo_8 (int offs, channel_t *ch, void *bytes, unsigned int count)
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
	pair = paintbuffer + offs;
	while (count-- > 0) {
		pair->left += lscale[*samp++];
		pair->right += rscale[*samp++];
		pair++;
	}
}

static void
snd_paint_stereo_16 (int offs, channel_t *ch, void *bytes, unsigned int count)
{
	short      *samp;
	portable_samplepair_t *pair;
	int         leftvol = ch->leftvol;
	int         rightvol = ch->rightvol;

	samp = (short *) bytes;
	pair = paintbuffer + offs;
	while (count-- > 0) {
		pair->left += (*samp++ * leftvol) >> 8;
		pair->right += (*samp++ * rightvol) >> 8;
		pair++;
	}
}

void
SND_PaintChannelFrom8 (channel_t *ch, sfxbuffer_t *sc, int count)
{
	unsigned int pos;
	byte       *samps;

	if (ch->pos < sc->pos)
		sc->setpos (sc, ch->pos);
	pos = (ch->pos - sc->pos + sc->tail) % sc->length;
	samps = sc->data + pos;

	if (pos + count > sc->length) {
		int         sub = sc->length - pos;
		snd_paint_mono_8 (0, ch, samps, sub);
		snd_paint_mono_8 (sub, ch, sc->data, count - sub);
	} else {
		snd_paint_mono_8 (0, ch, samps, count);
	}
	ch->pos += count;
}

void
SND_PaintChannelFrom16 (channel_t *ch, sfxbuffer_t *sc, int count)
{
	unsigned int pos;
	short      *samps;

	if (ch->pos < sc->pos)
		sc->setpos (sc, ch->pos);
	pos = (ch->pos - sc->pos + sc->tail) % sc->length;
	samps = (short *) sc->data + pos;

	if (pos + count > sc->length) {
		unsigned int sub = sc->length - pos;

		snd_paint_mono_16 (0, ch, samps, sub);
		snd_paint_mono_16 (sub, ch, sc->data, count - sub);
	} else {
		snd_paint_mono_16 (0, ch, samps, count);
	}
	ch->pos += count;
}

void
SND_PaintChannelStereo8 (channel_t *ch, sfxbuffer_t *sc, int count)
{
	unsigned int pos;
	short      *samps;

	if (ch->pos < sc->pos)
		sc->setpos (sc, ch->pos);
	pos = (ch->pos - sc->pos + sc->tail) % sc->length;
	samps = (short *) sc->data + pos;

	if (pos + count > sc->length) {
		unsigned int sub = sc->length - pos;

		snd_paint_stereo_8 (0, ch, samps, sub);
		snd_paint_stereo_8 (sub, ch, sc->data, count - sub);
	} else {
		snd_paint_stereo_8 (0, ch, samps, count);
	}
	ch->pos += count;
}

void
SND_PaintChannelStereo16 (channel_t *ch, sfxbuffer_t *sc, int count)
{
	unsigned int pos;
	int        *samps;

	if (ch->pos < sc->pos)
		sc->setpos (sc, ch->pos);
	pos = (ch->pos - sc->pos + sc->tail) % sc->length;
	samps = (int *) sc->data + pos;

	if (pos + count > sc->length) {
		unsigned int sub = sc->length - pos;

		snd_paint_stereo_16 (0, ch, samps, sub);
		snd_paint_stereo_16 (sub, ch, sc->data, count - sub);
	} else {
		snd_paint_stereo_16 (0, ch, samps, count);
	}
	ch->pos += count;
}
