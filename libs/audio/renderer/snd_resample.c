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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <samplerate.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"

#include "compat.h"
#include "snd_internal.h"

typedef struct {
	float      *data;
	int         size;
	int         pos;
} snd_null_state_t;

static void
check_buffer_integrity (sfxbuffer_t *sb, int width, const char *func)
{
	byte       *x = (byte *) sb->data + sb->size * width;
	if (memcmp (x, "\xde\xad\xbe\xef", 4))
		Sys_Error ("%s screwed the pooch %02x%02x%02x%02x", func,
				   x[0], x[1], x[2], x[3]);
}

unsigned
SND_ResamplerFrames (const sfx_t *sfx, unsigned frames)
{
	if (frames == ~0u) {
		return frames;
	}
	wavinfo_t  *info = sfx->wavinfo (sfx);
	snd_t      *snd = sfx->snd;
	int         inrate = info->rate;
	double      stepscale = (double) snd->speed / inrate;
	return frames * stepscale;
}

void
SND_Resample (sfxbuffer_t *sb, float *data, int length)
{
	int			outcount;
	double		stepscale;
	wavinfo_t  *info = (*sb->sfx)->wavinfo (*sb->sfx);
	snd_t      *snd = (*sb->sfx)->snd;
	int         inrate = info->rate;
	int         outwidth;
	SRC_DATA    src_data;

	stepscale = (double) snd->speed / inrate;
	outcount = length * stepscale;

	src_data.data_in = data;
	src_data.data_out = sb->data + sb->head * sb->channels;
	src_data.input_frames = length;
	src_data.output_frames = outcount;
	src_data.src_ratio = stepscale;

	src_simple (&src_data, SRC_LINEAR, sb->channels);

	outwidth = info->channels * sizeof (float);
	check_buffer_integrity (sb, outwidth, __FUNCTION__);
}

static int
snd_read (sfxstream_t *stream, float *data, int frames)
{
	snd_null_state_t *state = (snd_null_state_t *) stream->state;
	int         channels = stream->buffer->channels;
	int         framesize = channels * sizeof (float);
	int         count;
	int         read = 0;

	while (frames) {
		if (state->pos == state->size) {
			state->size = stream->ll_read (stream, &state->data);
			if (state->size <= 0)
				return state->size;
			state->pos = 0;
		}
		count = frames;
		if (count > state->size - state->pos)
			count = state->size - state->pos;
		memcpy (data, state->data + state->pos * channels, count * framesize);
		state->pos += count;
		frames -= count;
		read += count;
		data += count * channels;
	}
	return read;
}

static int
snd_resample_read (sfxstream_t *stream, float *data, int frames)
{
	int         inrate = stream->wavinfo.rate;
	double ratio = (double) stream->sfx->snd->speed / inrate;

	return src_callback_read (stream->state, ratio, frames, data);
}

static int
snd_seek (sfxstream_t *stream, int pos)
{
	int res = stream->ll_seek (stream, pos);
	if (stream->read == snd_resample_read) {
		src_reset (stream->state);
	} else {
		snd_null_state_t *state = (snd_null_state_t *) stream->state;
		state->size = 0;
		state->pos = 0;
	}
	return res;
}

void
SND_SetupResampler (sfxbuffer_t *sb, int streamed)
{
	double		stepscale;
	wavinfo_t  *info = (*sb->sfx)->wavinfo (*sb->sfx);
	snd_t      *snd = (*sb->sfx)->snd;
	int         inrate = info->rate;

	stepscale = (double) snd->speed / inrate;

	sb->sfx_length = info->frames * stepscale;

	sb->channels = info->channels;

	if (streamed) {
		int         err;
		sfxstream_t *stream = sb->stream;

		if (snd->speed == inrate) {
			stream->state = calloc (sizeof (snd_null_state_t), 1);
			stream->read = snd_read;
		} else {
			stream->state = src_callback_new (stream->ll_read,
											  SRC_LINEAR, info->channels,
											  &err, stream);
			stream->read = snd_resample_read;
		}
		stream->seek = snd_seek;
	}
}

void
SND_PulldownResampler (sfxstream_t *stream)
{
	if (stream->read == snd_resample_read) {
		src_delete (stream->state);
	} else {
		free (stream->state);
	}
}

void
SND_Convert (byte *idata, float *fdata, int frames, int channels, int width)
{
	int         i;

	if (width == 1) {
		for (i = 0; i < frames * channels; i++)
			*fdata++ = (*idata++ - 0x80) / 128.0;
	} else if (width == 2) {
		short      *id = (short *) idata;
		for (i = 0; i < frames * channels; i++)
			*fdata++ = *id++ / 32768.0;
	}
}
