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

#include <samplerate.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"

#include "compat.h"
#include "snd_render.h"

static void
check_buffer_integrity (sfxbuffer_t *sc, int width, const char *func)
{
	byte       *x = (byte *) sc->data + sc->length * width;
	if (memcmp (x, "\xde\xad\xbe\xef", 4))
		Sys_Error ("%s screwed the pooch %02x%02x%02x%02x", func,
				   x[0], x[1], x[2], x[3]);
}

void
SND_Resample (sfxbuffer_t *sc, float *data, int length)
{
	int			outcount;
	double		stepscale;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inrate = info->rate;
	int         outwidth;
	SRC_DATA    src_data;

	stepscale = (double) snd_shm->speed / inrate;
	outcount = length * stepscale;

	src_data.data_in = data;
	src_data.data_out = sc->data + sc->head * sc->channels;
	src_data.input_frames = length;
	src_data.output_frames = outcount;
	src_data.src_ratio = stepscale;

	src_simple (&src_data, SRC_LINEAR, sc->channels);

	outwidth = info->channels * sizeof (float);
	check_buffer_integrity (sc, outwidth, __FUNCTION__);
}

static void
SND_ResampleStream (sfxbuffer_t *sc, float *data, int length)
{
	SRC_DATA    src_data;
	SRC_STATE  *state = (SRC_STATE *) sc->state;

	int			outcount;
	double		stepscale;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inrate = info->rate;

	stepscale = (double) snd_shm->speed / inrate;
	outcount = length * stepscale;

	src_data.data_in = data;
	src_data.data_out = sc->data + sc->head * sc->channels;
	src_data.input_frames = length;
	src_data.output_frames = outcount;
	src_data.src_ratio = stepscale;
	src_data.end_of_input = 0; //XXX

	src_process (state, &src_data);
}

void
SND_SetupResampler (sfxbuffer_t *sc, int streamed)
{
	double		stepscale;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inrate = info->rate;

	stepscale = (double) snd_shm->speed / inrate;

	sc->sfx->length = info->frames * stepscale;
	if (info->loopstart != (unsigned int)-1)
		sc->sfx->loopstart = info->loopstart * stepscale;
	else
		sc->sfx->loopstart = (unsigned int)-1;

	sc->channels = info->channels;

	if (streamed) {
		int         err;
		sfxstream_t *stream = sc->sfx->data.stream;

		if (snd_shm->speed == inrate) {
			sc->state = 0;
			stream->resample = 0;
		} else {
			sc->state = src_new (SRC_LINEAR, info->channels, &err);
			stream->resample = SND_ResampleStream;
		}
	} else {
		sc->state = 0;
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
