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

int         cache_full_cycle;

sfxcache_t *
SND_GetCache (long samples, int rate, int inwidth, int channels,
			  sfx_t *sfx, cache_allocator_t allocator)
{
	int         size;
	int         width;
	float		stepscale;
	sfxcache_t *sc;

	width = snd_loadas8bit->int_val ? 1 : 2;
	stepscale = (float) rate / shm->speed;	// usually 0.5, 1, or 2
	size = samples / stepscale;
	size *= width * channels;
	sc = allocator (&sfx->cache, size + sizeof (sfxcache_t), sfx->name);
	if (!sc)
		return 0;
	sc->length = samples;
	sc->speed = rate;
	sc->width = inwidth;
	sc->stereo = channels;
	sc->bytes = size;
	memcpy (sc->data + size, "\xde\xad\xbe\xef", 4);
	return sc;
}

void
SND_ResampleSfx (sfxcache_t *sc, byte * data)
{
	unsigned char *ib, *ob;
	int			fracstep, outcount, sample, samplefrac, srcsample, i;
	float		stepscale;
	short	   *is, *os;
	int         inwidth = sc->width;
	int         inrate = sc->speed;

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
			int			j;
			int			points = 1 / stepscale;

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
	if (memcmp (sc->data + sc->bytes, "\xde\xad\xbe\xef", 4))
		Sys_Error ("SND_ResampleSfx screwed the pooch: %02x%02x%02x%02x",
				   sc->data[sc->bytes + 0], sc->data[sc->bytes + 1],
				   sc->data[sc->bytes + 2], sc->data[sc->bytes + 3]);
}

static sfxcache_t *
SND_LoadSound (sfx_t *sfx, cache_allocator_t allocator)
{
	char		namebuffer[256];
	char		foundname[256];
	byte	   *data;
	wavinfo_t	info;
	int			len;
	float		stepscale;
	sfxcache_t *sc;
	byte		stackbuf[1 * 1024];		// avoid dirtying the cache heap
	QFile      *file;

	// load it in
	strcpy (namebuffer, "sound/");
	strncat (namebuffer, sfx->name, sizeof (namebuffer) - strlen (namebuffer));
	_QFS_FOpenFile (namebuffer, &file, foundname, 1);
	if (!file) {
		Sys_Printf ("Couldn't load %s\n", namebuffer);
		return 0;
	}
	if (strcmp (".ogg", QFS_FileExtension (foundname)) == 0) {
		return SND_LoadOgg (file, sfx, allocator);
	}
	Qclose (file); //FIXME this is a dumb way to do this
	data = QFS_LoadStackFile (namebuffer, stackbuf, sizeof (stackbuf));

	if (!data) {
		Sys_Printf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

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

	sc->loopstart = info.loopstart;

	SND_ResampleSfx (sc, data + info.dataofs);

	return sc;
}

void
SND_CallbackLoad (void *object, cache_allocator_t allocator)
{
	SND_LoadSound (object, allocator);
}

/* WAV loading */

byte	   *data_p;
byte	   *iff_end;
byte	   *last_chunk;
byte	   *iff_data;
int			iff_chunk_len;

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
/*
static void
SND_DumpChunks (void)
{
	char		str[5];

	str[4] = 0;
	data_p = iff_data;
	do {
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = SND_GetLittleLong ();
		Sys_Printf ("0x%lx : %s (%d)\n", (long) (data_p - 4), str,
					iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}
*/
wavinfo_t
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
