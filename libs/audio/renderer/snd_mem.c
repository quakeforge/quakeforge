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

	$Id$
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

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qendian.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vfs.h"

int         cache_full_cycle;

byte       *SND_Alloc (int size);
wavinfo_t   SND_GetWavinfo (const char *name, byte * wav, int wavlength);
sfxcache_t *SND_LoadSound (cache_user_t *cache, const char *name, cache_allocator_t allocator);


void
SND_ResampleSfx (sfxcache_t *sc, int inrate, int inwidth, byte * data)
{
	int         outcount;
	int         srcsample;
	float       stepscale;
	int         i;
	int         sample, samplefrac, fracstep;
	short      *is, *os;
	unsigned char *ib, *ob;

	is = (short *) data;
	os = (short *) sc->data;
	ib = data;
	ob = sc->data;

	stepscale = (float) inrate / shm->speed;	// usually 0.5, 1, or 2

	outcount = sc->length / stepscale;

	sc->speed = shm->speed;
	if (snd_loadas8bit->int_val)
		sc->width = 1;
	else
		sc->width = 2;
	sc->stereo = 0;

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1 && sc->width == 1) {
			for (i = 0; i < outcount; i++) {
				*ob++ = *ib++ - 128;
			}
		} else if (inwidth == 1 && sc->width == 2) {
			for (i = 0; i < outcount; i++) {
				*os++ = (*ib++ - 128) << 8;
			}
		} else if (inwidth == 2 && sc->width == 1) {
			for (i = 0; i < outcount; i++) {
				*ob++ = LittleShort (*is++) >> 8;
			}
		} else if (inwidth == 2 && sc->width == 2) {
			for (i = 0; i < outcount; i++) {
				*os++ = LittleShort (*is++);
			}
		}
	} else {
		// general case
		if (snd_interp->int_val && stepscale < 1) {
			int         points = 1 / stepscale;
			int         j;

			for (i = 0; i < sc->length; i++) {
				int         s1, s2;

				if (inwidth == 2) {
					s2 = s1 = LittleShort (is[0]);
					if (i < sc->length - 1)
						s2 = LittleShort (is[1]);
					is++;
				} else {
					s2 = s1 = (ib[0] - 128) << 8;
					if (i < sc->length - 1)
						s2 = (ib[1] - 128) << 8;
					ib++;
				}
				for (j = 0; j < points; j++) {
					sample = s1 + (s2 - s1) * ((float) j) / points;
					if (sc->width == 2) {
						os[j] = sample;
					} else {
						ob[j] = sample >> 8;
					}
				}
				if (sc->width == 2) {
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
				if (sc->width == 2)
					((short *) sc->data)[i] = sample;
				else
					((signed char *) sc->data)[i] = sample >> 8;
			}
		}
	}

	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;
}

//=============================================================================

sfxcache_t *
SND_LoadSound (cache_user_t *cache, const char *name, cache_allocator_t allocator)
{
	char		namebuffer[256];
	byte	   *data;
	wavinfo_t	info;
	int			len;
	float		stepscale;
	sfxcache_t *sc;
	byte		stackbuf[1 * 1024];		// avoid dirtying the cache heap

	// load it in
	strcpy (namebuffer, "sound/");
	strncat (namebuffer, name, sizeof (namebuffer) - strlen (namebuffer));

	data = COM_LoadStackFile (namebuffer, stackbuf, sizeof (stackbuf));

	if (!data) {
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = SND_GetWavinfo (name, data, com_filesize);
	if (info.channels != 1) {
		Con_Printf ("%s is a stereo sample\n", name);
		return NULL;
	}

	stepscale = (float) info.rate / shm->speed;
	len = info.samples / stepscale;

	if (snd_loadas8bit->int_val) {
		len = len * info.channels;
	} else {
		len = len * 2 * info.channels;
	}

	sc = allocator (cache, len + sizeof (sfxcache_t), name);

	if (!sc)
		return NULL;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	SND_ResampleSfx (sc, sc->speed, sc->width, data + info.dataofs);

	return sc;
}

void
SND_CallbackLoad (struct cache_user_s *cache, cache_allocator_t allocator)
{
	SND_LoadSound (cache, cache->filename, allocator);
}

/* WAV loading */

byte       *data_p;
byte       *iff_end;
byte       *last_chunk;
byte       *iff_data;
int         iff_chunk_len;

short
SND_GetLittleShort (void)
{
	short       val = 0;

	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	data_p += 2;
	return val;
}

int
SND_GetLittleLong (void)
{
	int         val = 0;

	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	val = val + (*(data_p + 2) << 16);
	val = val + (*(data_p + 3) << 24);
	data_p += 4;
	return val;
}

void
SND_FindNexctChunk (char *name)
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

void
SND_FindChunk (char *name)
{
	last_chunk = iff_data;
	SND_FindNexctChunk (name);
}

void
SND_DumpChunks (void)
{
	char        str[5];

	str[4] = 0;
	data_p = iff_data;
	do {
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = SND_GetLittleLong ();
		Con_Printf ("0x%x : %s (%d)\n", (int) (data_p - 4), str,
					iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

wavinfo_t
SND_GetWavinfo (const char *name, byte * wav, int wavlength)
{
	wavinfo_t   info;
	int         i;
	int         format;
	int         samples;

	memset (&info, 0, sizeof (info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	SND_FindChunk ("RIFF");
	if (!(data_p && !strncmp (data_p + 8, "WAVE", 4))) {
		Con_Printf ("Missing RIFF/WAVE chunks\n");
		return info;
	}
	// get "fmt " chunk
	iff_data = data_p + 12;
//	SND_DumpChunks ();

	SND_FindChunk ("fmt ");
	if (!data_p) {
		Con_Printf ("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = SND_GetLittleShort ();
	if (format != 1) {
		Con_Printf ("Microsoft PCM format only\n");
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
		Con_Printf ("Missing data chunk\n");
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
