/*
	midi.c

	midi file loading for use with libWildMidi

	Copyright (C) 2003  Chris Ison

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
#include <stdlib.h>
#include <wildmidi_lib.h>

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/quakefs.h"

#include "snd_internal.h"

#define FRAMES 1024
#define CHANNELS 2
#define WIDTH 2

typedef struct {
	float       data[FRAMES * CHANNELS];
	midi       *handle;
} midi_file_t;

static int midi_intiialized = 0;

static float wildmidi_volume;
static cvar_t wildmidi_volume_cvar = {
	.name = "wildmidi_volume",
	.description =
		"Set the Master Volume",
	.default_value = "100",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &wildmidi_volume },
};
static char *wildmidi_config;
static cvar_t wildmidi_config_cvar = {
	.name = "wildmidi_config",
	.description =
		"path/filename of wildmidi.cfg",
	.default_value = "/etc/wildmidi/wildmidi.cfg",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &wildmidi_config },
};

static int
midi_init (snd_t *snd)
{
	Cvar_Register (&wildmidi_volume_cvar, 0, 0);
	Cvar_Register (&wildmidi_config_cvar, 0, 0);

	if (WildMidi_Init (wildmidi_config, snd->speed, 0) == -1)
		return 1;
	midi_intiialized = 1;
	return 0;
}

static wavinfo_t
midi_get_info (snd_t *snd, void *handle)
{
	wavinfo_t   info;
	struct _WM_Info *wm_info;

	memset (&info, 0, sizeof (info));

	if ((wm_info = WildMidi_GetInfo (handle)) == NULL) {
		Sys_Printf ("Could not obtain midi information\n");
		return info;
	}

	info.rate = snd->speed;
	info.width = 2;
	info.channels = 2;
	info.loopstart = -1;
	info.frames = wm_info->approx_total_samples;
	info.dataofs = 0;
	info.datalen = info.frames * info.channels * info.width;
	return info;
}

static long
midi_stream_read (void *file, float **buf)
{
	sfxstream_t *stream = (sfxstream_t *) file;
	midi_file_t *mf = (midi_file_t *) stream->file;
	int         size = FRAMES * CHANNELS * WIDTH;
	int         res;
	byte       *data = alloca (size);

	res = WildMidi_GetOutput (mf->handle, (int8_t *)data, size);
	if (res <= 0) {
		stream->error = 1;
		return 0;
	}
	res /= CHANNELS * WIDTH;
	SND_Convert (data, mf->data, res, CHANNELS, WIDTH);
	*buf = mf->data;
	return res;
}

static int
midi_stream_seek (sfxstream_t *stream, int pos)
{
	unsigned long int new_pos;
	pos *= stream->wavinfo.width * stream->wavinfo.channels;
	pos += stream->wavinfo.dataofs;
	new_pos = pos;

	return WildMidi_FastSeek(stream->file, &new_pos);
}

static void
midi_stream_close (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;
	midi_file_t *mf = (midi_file_t *) stream->file;

	WildMidi_Close (mf->handle);
	free (mf);
	SND_SFX_StreamClose (sfx);
}

static sfx_t *
midi_stream_open (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;
	QFile	   *file;
	midi	   *handle;
	unsigned char *local_buffer;
	unsigned long int local_buffer_size;
	midi_file_t *mf;

	file = QFS_FOpenFile (stream->file);

	local_buffer_size = Qfilesize (file);

	local_buffer = malloc (local_buffer_size);
	Qread (file, local_buffer, local_buffer_size);
	Qclose (file);

	handle = WildMidi_OpenBuffer(local_buffer, local_buffer_size);

	if (handle == NULL)
		return NULL;

	mf = calloc (sizeof (midi_file_t), 1);
	mf->handle = handle;

	return SND_SFX_StreamOpen (sfx, mf, midi_stream_read, midi_stream_seek,
							   midi_stream_close);
}

int
SND_LoadMidi (QFile *file, sfx_t *sfx, char *realname)
{
	snd_t      *snd = sfx->snd;
	wavinfo_t   info;
	midi *handle;
	unsigned char *local_buffer;
	unsigned long int local_buffer_size = Qfilesize (file);

	if (!midi_intiialized) {
		if (midi_init (snd)) {
			return -1;
		}
	}


	local_buffer = malloc (local_buffer_size);
	Qread (file, local_buffer, local_buffer_size);
	Qclose (file);

	// WildMidi takes ownership, so be damned if you touch it
	handle = WildMidi_OpenBuffer (local_buffer, local_buffer_size);

	if (handle == NULL)
		return -1;

	info = midi_get_info (snd, handle);

	WildMidi_Close (handle);

	Sys_MaskPrintf (SYS_dev, "stream %s\n", realname);

	// we init stream here cause we will only ever stream
	SND_SFX_Stream (sfx, realname, info, midi_stream_open);
	return 0;
}
