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
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sound.h"
#include "QF/sys.h"

int         cache_full_cycle;

byte       *I_S_Alloc (int size);

/*
	ResampleSfx
*/
void
ResampleSfx (sfx_t *sfx, int inrate, int inwidth, byte * data)
{
	int         outcount;
	int         srcsample;
	float       stepscale;
	int         i;
	int         sample, samplefrac, fracstep;
	sfxcache_t *sc;
	short      *is, *os;
	unsigned char *ib, *ob;

	sc = Cache_Check (&sfx->cache);
	if (!sc)
		return;

	is = (short *) data;
	os = (short *) sc->data;
	ib = data;
	ob = sc->data;

	stepscale = (float) inrate / shm->speed;	// this is usually 0.5, 1, or 
												// 
	// 2

	outcount = sc->length / stepscale;

	sc->speed = shm->speed;
	if (loadas8bit->int_val)
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

/*
	I_S_LoadSound
*/
sfxcache_t *
I_S_LoadSound (sfx_t *s)
{
	char        namebuffer[256];
	byte       *data;
	wavinfo_t   info;
	int         len;
	float       stepscale;
	sfxcache_t *sc;
	byte        stackbuf[1 * 1024];		// avoid dirtying the cache heap

// see if still in memory
	sc = Cache_Check (&s->cache);
	if (sc)
		return sc;

//Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
// load it in
	strcpy (namebuffer, "sound/");
	strncat (namebuffer, s->name, sizeof (namebuffer) - strlen (namebuffer));

//  Con_Printf ("loading %s\n",namebuffer);

	data = COM_LoadStackFile (namebuffer, stackbuf, sizeof (stackbuf));

	if (!data) {
		Con_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo (s->name, data, com_filesize);
	if (info.channels != 1) {
		Con_Printf ("%s is a stereo sample\n", s->name);
		return NULL;
	}

	stepscale = (float) info.rate / shm->speed;
	len = info.samples / stepscale;

	if (loadas8bit->int_val) {
		len = len * info.channels;
	} else {
		len = len * 2 * info.channels;
	}

	sc = Cache_Alloc (&s->cache, len + sizeof (sfxcache_t), s->name);

	if (!sc)
		return NULL;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	return sc;
}



/*
	WAV loading
*/


byte       *data_p;
byte       *iff_end;
byte       *last_chunk;
byte       *iff_data;
int         iff_chunk_len;


short
GetLittleShort (void)
{
	short       val = 0;

	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	data_p += 2;
	return val;
}

int
GetLittleLong (void)
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
FindNextChunk (char *name)
{
	while (1) {
		data_p = last_chunk;

		if (data_p >= iff_end) {		// didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong ();
		if (iff_chunk_len < 0) {
			data_p = NULL;
			return;
		}
//      if (iff_chunk_len > 1024*1024)
//          Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		last_chunk = data_p + 8 + ((iff_chunk_len + 1) & ~1);
		if (!strncmp (data_p, name, 4))
			return;
	}
}

void
FindChunk (char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


void
DumpChunks (void)
{
	char        str[5];

	str[4] = 0;
	data_p = iff_data;
	do {
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong ();
		Con_Printf ("0x%x : %s (%d)\n", (int) (data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

/*
	GetWavinfo
*/
wavinfo_t
GetWavinfo (char *name, byte * wav, int wavlength)
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
	FindChunk ("RIFF");
	if (!(data_p && !strncmp (data_p + 8, "WAVE", 4))) {
		Con_Printf ("Missing RIFF/WAVE chunks\n");
		return info;
	}
// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks ();

	FindChunk ("fmt ");
	if (!data_p) {
		Con_Printf ("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort ();
	if (format != 1) {
		Con_Printf ("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort ();
	info.rate = GetLittleLong ();
	data_p += 4 + 2;
	info.width = GetLittleShort () / 8;

// get cue chunk
	FindChunk ("cue ");
	if (data_p) {
		data_p += 32;
		info.loopstart = GetLittleLong ();
//      Con_Printf("loopstart=%d\n", sfx->loopstart);

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p) {
			if (!strncmp (data_p + 28, "mark", 4)) {	// this is not a
				// proper parse, but
				// it works with
				// cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
//              Con_Printf("looped length: %i\n", i);
			}
		}
	} else
		info.loopstart = -1;

// find data chunk
	FindChunk ("data");
	if (!data_p) {
		Con_Printf ("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width;

	if (info.samples) {
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	} else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}
