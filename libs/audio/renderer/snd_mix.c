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
#include "snd_internal.h"

#define VOLSCALE 0.5				// so mixing is less likely to overflow

portable_samplepair_t snd_paintbuffer[PAINTBUFFER_SIZE * 2];
static int  max_overpaint;				// number of extra samples painted
										// due to phase shift

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
snd_paint_channel (channel_t *ch, sfxbuffer_t *sb, int count)
{
	unsigned    pos;
	int         offs = 0;
	float      *samps;

	if ((int) ch->pos < 0) {
		ch->pos += count;
		if ((int) ch->pos <= 0)
			return;
		offs = count - ch->pos;
		count -= offs;
		ch->pos = 0;
	}
	if (ch->pos < sb->pos || ch->pos - sb->pos >= sb->length)
		sb->setpos (sb, ch->pos);
	pos = (ch->pos - sb->pos + sb->tail) % sb->length;
	samps = sb->data + pos * sb->channels;

	if (pos + count > sb->length) {
		unsigned    sub = sb->length - pos;
		sb->paint (offs, ch, samps, sub);
		sb->paint (offs + sub, ch, sb->data, count - sub);
	} else {
		sb->paint (offs, ch, samps, count);
	}
	ch->pos += count;
}

void
SND_PaintChannels (snd_t *snd, unsigned endtime)
{
	unsigned    end, ltime;
	int         i, count;
	channel_t  *ch;
	sfx_t      *sfx;
	sfxbuffer_t *sb;

	// clear the paint buffer
	for (i = 0; i < PAINTBUFFER_SIZE * 2; i++) {
		snd_paintbuffer[i].left = 0;
		snd_paintbuffer[i].right = 0;
	}

	while (snd->paintedtime < endtime) {
		// if snd_paintbuffer is smaller than DMA buffer
		end = endtime;
		if (end - snd->paintedtime > PAINTBUFFER_SIZE)
			end = snd->paintedtime + PAINTBUFFER_SIZE;

		max_overpaint = 0;

		// paint in the channels.
		ch = snd_channels;
		for (i = 0; i < snd_total_channels; i++, ch++) {
			if (!(sfx = ch->sfx)) {
				// channel is inactive
				continue;
			}
			if (ch->stop || ch->done) {
				ch->done = 1;		// acknowledge stopped signal
				continue;
			}
			if (ch->pause)
				continue;
			sb = sfx->getbuffer (sfx);
			if (!sb) {				// something went wrong with the sfx
				printf ("XXXX sfx blew up!!!!\n");
				continue;
			}

			if (!ch->end)
				ch->end = snd->paintedtime + sfx->length - ch->pos;

			ltime = snd->paintedtime;

			while (ltime < end) {		// paint up to end
				count = ((ch->end < end) ? ch->end : end) - ltime;
				if (count > 0) {
					if (ch->leftvol || ch->rightvol) {
						snd_paint_channel (ch, sb, count);
						if (sb->advance) {
							if (!sb->advance (sb, count)) {
								// this channel can no longer be used as its
								// source has died.
								ch->done = 1;
								break;
							}
						}
					}
					ltime += count;
				}

				if (check_channel_end (ch, sfx, count, ltime))
					break;
			}
		}

		// transfer out according to DMA format
		snd->xfer (snd, snd_paintbuffer, end - snd->paintedtime,
				   snd_volume);

		memmove (snd_paintbuffer, snd_paintbuffer + end - snd->paintedtime,
				 max_overpaint * sizeof (snd_paintbuffer[0]));
		memset (snd_paintbuffer + max_overpaint, 0, sizeof (snd_paintbuffer)
				- max_overpaint * sizeof (snd_paintbuffer[0]));

		snd->paintedtime = end;
	}
}

static inline void
snd_mix_single (portable_samplepair_t *pair, float **samp,
				float lvol, float rvol)
{
	float       single = *(*samp)++;

	pair->left += single * lvol;
	pair->right += single * rvol;
}

static inline void
snd_mix_pair (portable_samplepair_t *pair, float **samp,
			  float lvol, float rvol)
{
	float       left = *(*samp)++;
	float       right = *(*samp)++;

	pair->left += left * lvol;
	pair->right += right * rvol;
}

static inline void
snd_mix_triple (portable_samplepair_t *pair, float **samp,
				 float lvol, float rvol)
{
	float       left = *(*samp)++;
	float       center = *(*samp)++;
	float       right = *(*samp)++;

	pair->left += left * lvol;
	pair->left += center * lvol;
	pair->right += center * rvol;
	pair->right += right * rvol;
}

/*	mono
		center
	Spacializes the sound.
*/
static void
snd_paint_mono (int offs, channel_t *ch, float *sfx, unsigned count)
{
	float       leftvol, rightvol;
	unsigned    left_phase, right_phase;	// Never allowed < 0 anyway
	unsigned    i = 0;
	portable_samplepair_t *pair;

	leftvol = ch->leftvol * VOLSCALE;
	rightvol = ch->rightvol * VOLSCALE;

	max_overpaint = max (abs (ch->phase),
						 max (abs (ch->oldphase), max_overpaint));

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
			float       left = sfx[i] * leftvol;
			float       right = sfx[i] * rightvol;

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
		pair[i + left_phase].left += sfx[i] * leftvol;
		pair[i + right_phase].right += sfx[i] * rightvol;
	}
}

/*	stereo
		left, right
	Does not spacialize the sound.
*/
static void
snd_paint_stereo (int offs, channel_t *ch, float *samp, unsigned count)
{
	portable_samplepair_t *pair;
	float       leftvol = ch->leftvol * VOLSCALE;
	float       rightvol = ch->rightvol * VOLSCALE;

	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		pair++;
	}
}

/*	1d surround
		left, center, right
	Does not spacialize the sound.
*/
static void
snd_paint_3 (int offs, channel_t *ch, float *samp, unsigned count)
{
	portable_samplepair_t *pair;
	float       leftvol = ch->leftvol * VOLSCALE;
	float       rightvol = ch->rightvol * VOLSCALE;

	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		snd_mix_triple (pair, &samp, leftvol, rightvol);
		pair++;
	}
}

/*	quadraphonic surround
		front (left, right),
		rear (left, right)
	Does not spacialize the sound.
*/
static void
snd_paint_4 (int offs, channel_t *ch, float *samp, unsigned count)
{
	portable_samplepair_t *pair;
	float       leftvol = ch->leftvol * VOLSCALE;
	float       rightvol = ch->rightvol * VOLSCALE;

	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		pair++;
	}
}

/*	five-channel surround
		front (left, center, right),
		rear (left, right)
	Does not spacialize the sound.
*/
static void
snd_paint_5 (int offs, channel_t *ch, float *samp, unsigned count)
{
	portable_samplepair_t *pair;
	float       leftvol = ch->leftvol * VOLSCALE;
	float       rightvol = ch->rightvol * VOLSCALE;

	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		snd_mix_triple (pair, &samp, leftvol, rightvol);
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		pair++;
	}
}

/*	5.1 surround
		front (left, center, right),
		rear (left, right),
		lfe
	Does not spacialize the sound.
*/
static void
snd_paint_6 (int offs, channel_t *ch, float *samp, unsigned count)
{
	portable_samplepair_t *pair;
	float       leftvol = ch->leftvol * VOLSCALE;
	float       rightvol = ch->rightvol * VOLSCALE;

	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		snd_mix_triple (pair, &samp, leftvol, rightvol);
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		snd_mix_single (pair, &samp, leftvol, rightvol);
		pair++;
	}
}

/*	6.1 surround
		front (left, center, right),
		side (left, right),
		rear (center),
		lfe
	Does not spacialize the sound.
*/
static void
snd_paint_7 (int offs, channel_t *ch, float *samp, unsigned count)
{
	portable_samplepair_t *pair;
	float       leftvol = ch->leftvol * VOLSCALE;
	float       rightvol = ch->rightvol * VOLSCALE;

	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		snd_mix_triple (pair, &samp, leftvol, rightvol);
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		snd_mix_single (pair, &samp, leftvol, rightvol);
		snd_mix_single (pair, &samp, leftvol, rightvol);
		pair++;
	}
}

/*	7.1 surround
		front (left, center, right),
		side (left, right),
		rear (left, right),
		lfe
	Does not spacialize the sound.
*/
static void
snd_paint_8 (int offs, channel_t *ch, float *samp, unsigned count)
{
	portable_samplepair_t *pair;
	float       leftvol = ch->leftvol * VOLSCALE;
	float       rightvol = ch->rightvol * VOLSCALE;

	pair = snd_paintbuffer + offs;
	while (count-- > 0) {
		snd_mix_triple (pair, &samp, leftvol, rightvol);
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		snd_mix_pair (pair, &samp, leftvol, rightvol);
		snd_mix_single (pair, &samp, leftvol, rightvol);
		pair++;
	}
}

void
SND_SetPaint (sfxbuffer_t *sb)
{
	static sfxpaint_t *painters[] = {
		0,
		snd_paint_mono,
		snd_paint_stereo,
		snd_paint_3,
		snd_paint_4,
		snd_paint_5,
		snd_paint_6,
		snd_paint_7,
		snd_paint_8,
	};

	wavinfo_t *info = sb->sfx->wavinfo (sb->sfx);
	if (info->channels > 8)
		Sys_Error ("illegal channel count %d", info->channels);
	sb->paint = painters[info->channels];
}
