/*
	wav.c

	wav file loading

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
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"

#include "snd_render.h"

static byte	   *data_p;
static byte	   *iff_end;
static byte	   *last_chunk;
static byte	   *iff_data;
static int		iff_chunk_len;

static short
SND_GetLittleShort (void)
{
	short		val = 0;

	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	data_p += 2;
	return val;
}

static int
SND_GetLittleLong (void)
{
	int			val = 0;

	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	val = val + (*(data_p + 2) << 16);
	val = val + (*(data_p + 3) << 24);
	data_p += 4;
	return val;
}

static void
SND_FindNexctChunk (const char *name)
{
	while (1) {
		data_p = last_chunk;

		if (data_p >= iff_end) {		// didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = SND_GetLittleLong ();
		if (iff_chunk_len < 0) {
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ((iff_chunk_len + 1) & ~1);
		if (!strncmp (data_p, name, 4))
			return;
	}
}

static void
SND_FindChunk (const char *name)
{
	last_chunk = iff_data;
	SND_FindNexctChunk (name);
}

static wavinfo_t
SND_GetWavinfo (const char *name, byte * wav, int wavlength)
{
	int			format, samples, i;
	wavinfo_t	info;

	memset (&info, 0, sizeof (info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	SND_FindChunk ("RIFF");
	if (!(data_p && !strncmp (data_p + 8, "WAVE", 4))) {
		Sys_Printf ("Missing RIFF/WAVE chunks\n");
		return info;
	}
	// get "fmt " chunk
	iff_data = data_p + 12;
//	SND_DumpChunks ();

	SND_FindChunk ("fmt ");
	if (!data_p) {
		Sys_Printf ("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = SND_GetLittleShort ();
	if (format != 1) {
		Sys_Printf ("Microsoft PCM format only\n");
		return info;
	}

	info.channels = SND_GetLittleShort ();
	info.rate = SND_GetLittleLong ();
	data_p += 4 + 2;
	info.width = SND_GetLittleShort () / 8;

	// get cue chunk
	SND_FindChunk ("cue ");
	if (data_p) {
		data_p += 32;
		info.loopstart = SND_GetLittleLong ();

		// if the next chunk is a LIST chunk, look for a cue length marker
		SND_FindNexctChunk ("LIST");
		if (data_p) {
			if (!strncmp (data_p + 28, "mark", 4)) {
				// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = SND_GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
			}
		}
	} else
		info.loopstart = -1;

	// find data chunk
	SND_FindChunk ("data");
	if (!data_p) {
		Sys_Printf ("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = SND_GetLittleLong () / info.width;

	if (info.samples) {
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	} else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}

void
SND_LoadWav (QFile *file)
{
	int         size;

	size = Qfilelen (file);
	data = Hunk_TempAlloc (size);
	Qread (file, data, size);

	info = SND_GetWavinfo (sfx->name, data, qfs_filesize);
	if (info.channels != 1) {
		Sys_Printf ("%s is a stereo sample\n", sfx->name);
		return NULL;
	}

	stepscale = (float) info.rate / shm->speed;
	len = info.samples / stepscale;

	if (snd_loadas8bit->int_val) {
		len = len * info.channels;
	} else {
		len = len * 2 * info.channels;
	}

	sc = SND_GetCache (info.samples, info.rate, info.width, info.channels,
					   sfx, allocator);
	if (!sc)
		return NULL;

	sfx->loopstart = info.loopstart;

	SND_ResampleSfx (sc, data + info.dataofs);

	return sc;
}
