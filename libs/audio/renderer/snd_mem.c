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

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"

#include "compat.h"
#include "snd_render.h"

#define SAMPLE_GAP	4

typedef struct {
	byte        left;
	byte        right;
} stereo8_t;

typedef struct {
	short       left;
	short       right;
} stereo16_t;

static sfxbuffer_t *
snd_fail (sfx_t *sfx)
{
	return 0;
}

static void
snd_noop (sfx_t *sfx)
{
}

static sfx_t *
snd_open (sfx_t *sfx)
{
	return sfx;
}

sfxbuffer_t *
SND_CacheTouch (sfx_t *sfx)
{
	return Cache_Check (&((sfxblock_t *) sfx->data)->cache);
}

sfxbuffer_t *
SND_CacheRetain (sfx_t *sfx)
{
	return Cache_TryGet (&((sfxblock_t *) sfx->data)->cache);
}

void
SND_CacheRelease (sfx_t *sfx)
{
	Cache_Release (&((sfxblock_t *) sfx->data)->cache);
}

sfxbuffer_t *
SND_StreamRetain (sfx_t *sfx)
{
	return &((sfxstream_t *) sfx->data)->buffer;
}

void
SND_StreamRelease (sfx_t *sfx)
{
}

wavinfo_t *
SND_CacheWavinfo (sfx_t *sfx)
{
	return &((sfxblock_t *) sfx->data)->wavinfo;
}

wavinfo_t *
SND_StreamWavinfo (sfx_t *sfx)
{
	return &((sfxstream_t *) sfx->data)->wavinfo;
}

static void
read_samples (sfxbuffer_t *buffer, int count, void *prev)
{

	if (buffer->head + count > buffer->length) {
		int         s = (buffer->length - 1);

		count -= buffer->length - buffer->head;
		read_samples (buffer, buffer->length - buffer->head, prev);
		prev = buffer->data + s * buffer->bps;
		read_samples (buffer, count, prev);
	} else {
		byte       *data;
		float       stepscale;
		int         samples, size;
		sfx_t      *sfx = buffer->sfx;
		sfxstream_t *stream = (sfxstream_t *) sfx->data;
		wavinfo_t  *info = &stream->wavinfo;

		stepscale = (float) info->rate / shm->speed;	// usually 0.5, 1, or 2

		samples = count * stepscale;
		size = samples * info->width * info->channels;
		data = alloca (size);

		if (stream->resample) {
			if (stream->read (stream->file, data, size, info) != size)
				Sys_Printf ("%s r\n", sfx->name);
			stream->resample (buffer, data, samples, prev);
		} else {
			if (stream->read (stream->file, buffer->data, size, info) != size)
				Sys_Printf ("%s nr\n", sfx->name);
		}
		buffer->head += count;
		if (buffer->head >= buffer->length)
			buffer->head -= buffer->length;
	}
}

static void
fill_buffer (sfx_t *sfx, sfxstream_t *stream, sfxbuffer_t *buffer,
			 wavinfo_t *info, unsigned int headpos)
{
	void       *prev;
	unsigned int samples;
	unsigned int loop_samples = 0;

	// find out how many samples can be read into the buffer
	samples = buffer->tail - buffer->head - SAMPLE_GAP;
	if (buffer->tail <= buffer->head)
		samples += buffer->length;

	if (headpos + samples > sfx->length) {
		if (sfx->loopstart == (unsigned int)-1) {
			samples = sfx->length - headpos;
		} else {
			loop_samples = headpos + samples - sfx->length;
			samples -= loop_samples;
		}
	}
	if (samples) {
		if (buffer->head != buffer->tail) {
			int         s = buffer->head - 1;
			if (!buffer->head)
				s += buffer->length;
			prev = buffer->data + s * buffer->bps;
		} else {
			prev = 0;
		}
		read_samples (buffer, samples, prev);
	}
	if (loop_samples) {
		if (buffer->head != buffer->tail) {
			int         s = buffer->head - 1;
			if (!buffer->head)
				s += buffer->length;
			prev = buffer->data + s * buffer->bps;
		} else {
			prev = 0;
		}
		stream->seek (stream->file, info->loopstart, info);
		read_samples (buffer, loop_samples, prev);
	}
}

void
SND_StreamSetPos (sfxbuffer_t *buffer, unsigned int pos)
{
	float       stepscale;
	sfx_t      *sfx = buffer->sfx;
	sfxstream_t *stream = (sfxstream_t *) sfx->data;
	wavinfo_t  *info = &stream->wavinfo;

	stepscale = (float) info->rate / shm->speed;	// usually 0.5, 1, or 2

	buffer->head = buffer->tail = 0;
	buffer->pos = pos;
	stream->pos = pos;
	stream->seek (stream->file, buffer->pos * stepscale, info);
	fill_buffer (sfx, stream, buffer, info, pos);
}

void
SND_StreamAdvance (sfxbuffer_t *buffer, unsigned int count)
{
	float       stepscale;
	unsigned int headpos, samples;
	sfx_t      *sfx = buffer->sfx;
	sfxstream_t *stream = (sfxstream_t *) sfx->data;
	wavinfo_t  *info = &stream->wavinfo;

	stream->pos += count;
	count = (stream->pos - buffer->pos) & ~255;
	if (!count)
		return;

	stepscale = (float) info->rate / shm->speed;	// usually 0.5, 1, or 2

	// find out how many samples the buffer currently holds
	samples = buffer->head - buffer->tail;
	if (buffer->head < buffer->tail)
		samples += buffer->length;

	// find out where head points to in the stream
	headpos = buffer->pos + samples;
	if (headpos >= sfx->length) {
		if (sfx->loopstart == (unsigned int)-1)
			headpos = sfx->length;
		else
			headpos -= sfx->length - sfx->loopstart;
	}

	if (samples < count) {
		buffer->head = buffer->tail = 0;
		buffer->pos += count;
		if (buffer->pos > sfx->length) {
			if (sfx->loopstart == (unsigned int)-1) {
				// reset the buffer and fill it incase it's needed again
				buffer->pos = 0;
			} else {
				buffer->pos -= sfx->loopstart;
				buffer->pos %= sfx->length - sfx->loopstart;
				buffer->pos += sfx->loopstart;
			}
			stream->pos = buffer->pos;
		}
		headpos = buffer->pos;
		stream->seek (stream->file, buffer->pos * stepscale, info);
	} else {
		buffer->pos += count;
		if (buffer->pos >= sfx->length) {
			if (sfx->loopstart == (unsigned int)-1) {
				// reset the buffer and fill it in case it's needed again
				headpos = buffer->pos = 0;
				buffer->head = buffer->tail = 0;
				count = 0;
				stream->seek (stream->file, buffer->pos * stepscale, info);
			} else {
				buffer->pos -= sfx->length - sfx->loopstart;
			}
			stream->pos = buffer->pos;
		}

		buffer->tail += count;
		if (buffer->tail >= buffer->length)
			buffer->tail -= buffer->length;
	}
	fill_buffer (sfx, stream, buffer, info, headpos);
}

void
SND_Load (sfx_t *sfx)
{
	dstring_t  *foundname = dstring_new ();
	char       *realname;
	char        buf[4];
	QFile      *file;

	sfx->touch = sfx->retain = snd_fail;
	sfx->release = snd_noop;
	sfx->close = snd_noop;
	sfx->open = snd_open;

	_QFS_FOpenFile (sfx->name, &file, foundname, 1);
	if (!file) {
		Sys_Printf ("Couldn't load %s\n", sfx->name);
		dstring_delete (foundname);
		return;
	}
	if (!strequal (foundname->str, sfx->name)) {
		realname = foundname->str;
		free (foundname);
	} else {
		realname = (char *) sfx->name;	// won't free if realname == sfx->name
	}
	Qread (file, buf, 4);
	Qseek (file, 0, SEEK_SET);
#ifdef HAVE_VORBIS
	if (strnequal ("OggS", buf, 4)) {
		Sys_DPrintf ("SND_Load: ogg file\n");
		SND_LoadOgg (file, sfx, realname);
		return;
	}
#endif
#ifdef HAVE_FLAC
	if (strnequal ("fLaC", buf, 4)) {
		Sys_DPrintf ("SND_Load: flac file\n");
		SND_LoadFLAC (file, sfx, realname);
		return;
	}
#endif
#ifdef HAVE_WILDMIDI
	if (strnequal ("MThd", buf, 4)) {
		Sys_DPrintf ("SND_Load: midi file\n");
		SND_LoadMidi (file, sfx, realname);
		return;
	}
#endif
	if (strnequal ("RIFF", buf, 4)) {
		Sys_DPrintf ("SND_Load: wav file\n");
		SND_LoadWav (file, sfx, realname);
		return;
	}
	Qclose (file);
	if (realname != sfx->name)
		free (realname);
}

sfxbuffer_t *
SND_GetCache (long samples, int rate, int inwidth, int channels,
			  sfxblock_t *block, cache_allocator_t allocator)
{
	int         len, size, width;
	float		stepscale;
	sfxbuffer_t *sc;
	sfx_t      *sfx = block->sfx;

	width = snd_loadas8bit->int_val ? 1 : 2;
	stepscale = (float) rate / shm->speed;	// usually 0.5, 1, or 2
	len = size = samples / stepscale;
//	printf ("%ld %d\n", samples, size);
	size *= width * channels;
	sc = allocator (&block->cache, sizeof (sfxbuffer_t) + size, sfx->name);
	if (!sc)
		return 0;
	memset (sc, 0, sizeof (sfxbuffer_t) + size);
	sc->length = len;
	memcpy (sc->data + size, "\xde\xad\xbe\xef", 4);
	return sc;
}

void
SND_ResampleMono (sfxbuffer_t *sc, byte *data, int length, void *prev)
{
	byte       *ib, *ob, *pb;
	int			fracstep, outcount, sample, samplefrac, srcsample, i;
	float		stepscale;
	short	   *is, *os, *ps;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inwidth = info->width;
	int         inrate = info->rate;
	int         outwidth;
	short       zero_s[1];
	byte        zero_b[1];

	is = (short *) data;
	os = (short *) sc->data;
	ib = data;
	ob = sc->data;

	ps = zero_s;
	pb = zero_b;

	if (!prev) {
		zero_s[0] = 0;
		zero_b[0] = 128;
	}

	os += sc->head;
	ob += sc->head;

	stepscale = (float) inrate / shm->speed;	// usually 0.5, 1, or 2

	outcount = length / stepscale;
//	printf ("%d %d\n", length, outcount);

	sc->sfx->length = info->samples / stepscale;
	if (info->loopstart != (unsigned int)-1)
		sc->sfx->loopstart = info->loopstart / stepscale;
	else
		sc->sfx->loopstart = (unsigned int)-1;

	if (snd_loadas8bit->int_val) {
		sc->bps = outwidth = 1;
		sc->paint = SND_PaintChannelFrom8;
		if (prev) {
			zero_s[0] = ((char *)prev)[0];
			zero_b[0] = ((char *)prev)[0] + 128;
		}
	} else {
		sc->bps = outwidth = 2;
		sc->paint = SND_PaintChannelFrom16;
		if (prev) {
			zero_s[0] = ((short *)prev)[0];
			zero_b[0] = (((short *)prev)[0] >> 8) + 128;
		}
	}

	if (!length)
		return;

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++) {
					*ob++ = *ib++ - 128;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++) {
					*os++ = (*ib++ - 128) << 8;
				}
			} else {
				goto general_Mono;
			}
		} else if (inwidth == 2) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++) {
					*ob++ = LittleShort (*is++) >> 8;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++) {
					*os++ = LittleShort (*is++);
				}
			} else {
				goto general_Mono;
			}
		}
	} else {				// general case
general_Mono:
		if (snd_interp->int_val && stepscale < 1) {
			int			j;
			int			points = 1 / stepscale;

			for (i = 0; i < length; i++) {
				int         s1, s2;

				if (inwidth == 2) {
					s1 = LittleShort (*ps);
					s2 = LittleShort (*is);
					ps = is++;
				} else {
					s1 = (*pb - 128) << 8;
					s2 = (*ib - 128) << 8;
					pb = ib++;
				}
				for (j = 0; j < points; j++) {
					sample = s1 + (s2 - s1) * ((float) j) / points;
					if (outwidth == 2) {
						os[j] = sample;
					} else {
						ob[j] = sample >> 8;
					}
				}
				if (outwidth == 2) {
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
					sample = LittleShort (is[srcsample]);
				else
					sample = (ib[srcsample] - 128) << 8;
				if (outwidth == 2)
					os[i] = sample;
				else
					ob[i] = sample >> 8;
			}
		}
	}
	{
		byte       *x = sc->data + sc->length * outwidth;
		if (memcmp (x, "\xde\xad\xbe\xef", 4))
			Sys_Error ("SND_ResampleMono screwed the pooch %02x%02x%02x%02x",
					   x[0], x[1], x[2], x[3]);
	}
}

void
SND_ResampleStereo (sfxbuffer_t *sc, byte *data, int length, void *prev)
{
	int			fracstep, outcount, outwidth, samplefrac, srcsample, sl, sr, i;
	float		stepscale;
	stereo8_t  *ib, *ob, *pb;
	stereo16_t *is, *os, *ps;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inwidth = info->width;
	int         inrate = info->rate;
	stereo16_t  zero_s;
	stereo8_t   zero_b;

	is = (stereo16_t *) data;
	os = (stereo16_t *) sc->data;
	ib = (stereo8_t *) data;
	ob = (stereo8_t *) sc->data;

	ps = &zero_s;
	pb = &zero_b;

	if (!prev) {
		zero_s.left = zero_s.right = 0;
		zero_b.left = zero_b.right = 128;
	}

	os += sc->head;
	ob += sc->head;

	stepscale = (float) inrate / shm->speed;	// usually 0.5, 1, or 2

	outcount = length / stepscale;

	sc->sfx->length = info->samples / stepscale;
	if (info->loopstart != (unsigned int)-1)
		sc->sfx->loopstart = info->loopstart / stepscale;
	else
		sc->sfx->loopstart = (unsigned int)-1;

	if (snd_loadas8bit->int_val) {
		outwidth = 1;
		sc->paint = SND_PaintChannelStereo8;
		sc->bps = 2;
		if (prev) {
			zero_s.left = ((char *)prev)[0];
			zero_s.right = ((char *)prev)[1];
			zero_b.left = ((char *)prev)[0] + 128;
			zero_b.right = ((char *)prev)[1] + 128;
		}
	} else {
		outwidth = 2;
		sc->paint = SND_PaintChannelStereo16;
		sc->bps = 4;
		if (prev) {
			zero_s.left = ((short *)prev)[0];
			zero_s.right = ((short *)prev)[1];
			zero_b.left = (((short *)prev)[0] >> 8) + 128;
			zero_b.right = (((short *)prev)[1] >> 8) + 128;
		}
	}

	if (!length)
		return;

	// resample / decimate to the current source rate
	if (stepscale == 1) {
		if (inwidth == 1) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++, ob++, ib++) {
					ob->left = ib->left - 128;
					ob->right = ib->right - 128;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++, os++, ib++) {
					os->left = (ib->left - 128) << 8;
					os->right = (ib->right - 128) << 8;
				}
			} else {
				goto general_Stereo;
			}
		} else if (inwidth == 2) {
			if (outwidth == 1) {
				for (i = 0; i < outcount; i++, ob++, ib++) {
					ob->left = LittleShort (is->left) >> 8;
					ob->right = LittleShort (is->right) >> 8;
				}
			} else if (outwidth == 2) {
				for (i = 0; i < outcount; i++, os++, is++) {
					os->left = LittleShort (is->left);
					os->right = LittleShort (is->right);
				}
			} else {
				goto general_Stereo;
			}
		}
	} else {				// general case
general_Stereo:
		if (snd_interp->int_val && stepscale < 1) {
			int			j;
			int			points = 1 / stepscale;

			for (i = 0; i < length; i++) {
				int         sl1, sl2;
				int         sr1, sr2;

				if (inwidth == 2) {
					sl1 = LittleShort (ps->left);
					sr1 = LittleShort (ps->right);
					sl2 = LittleShort (is->left);
					sr2 = LittleShort (is->right);
					ps = is++;
				} else {
					sl1 = (pb->left - 128) << 8;
					sr1 = (pb->right - 128) << 8;
					sl2 = (ib->left - 128) << 8;
					sr2 = (ib->right - 128) << 8;
					pb = ib++;
				}
				for (j = 0; j < points; j++) {
					sl = sl1 + (sl2 - sl1) * ((float) j) / points;
					sr = sr1 + (sr2 - sr1) * ((float) j) / points;
					if (outwidth == 2) {
						os[j].left = sl;
						os[j].right = sr;
					} else {
						ob[j].left = sl >> 8;
						ob[j].right = sr >> 8;
					}
				}
				if (outwidth == 2) {
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
				if (inwidth == 2) {
					sl = LittleShort (is[srcsample].left);
					sr = LittleShort (is[srcsample].right);
				} else {
					sl = (ib[srcsample].left - 128) << 8;
					sr = (ib[srcsample].right - 128) << 8;
				}
				if (outwidth == 2) {
					os[i].left = sl;
					os[i].right = sr;
				} else {
					ob[i].left = sl >> 8;
					ob[i].right = sr >> 8;
				}
			}
		}
	}
	{
		byte       *x = sc->data + sc->length * outwidth * 2;
		if (memcmp (x, "\xde\xad\xbe\xef", 4))
			Sys_Error ("SND_ResampleStereo screwed the pooch %02x%02x%02x%02x",
					   x[0], x[1], x[2], x[3]);
	}
}

void
SND_NoResampleStereo (sfxbuffer_t *sc, byte *data, int length, void *prev)
{
	int outcount, i;
	stereo8_t  *ib, *ob;
	stereo16_t *is, *os;
	wavinfo_t  *info = sc->sfx->wavinfo (sc->sfx);
	int         inwidth = info->width;
	int         outwidth;

	is = (stereo16_t *) data;
	os = (stereo16_t *) sc->data;
	ib = (stereo8_t *) data;
	ob = (stereo8_t *) sc->data;

	os += sc->head;
	ob += sc->head;

	outcount = length;

	sc->sfx->length = info->samples;
	if (info->loopstart != (unsigned int)-1)
		sc->sfx->loopstart = info->loopstart;
	else
		sc->sfx->loopstart = (unsigned int)-1;

	if (snd_loadas8bit->int_val) {
		outwidth = 1;
		sc->paint = SND_PaintChannelStereo8;
		sc->bps = 2;
	} else {
		outwidth = 2;
		sc->paint = SND_PaintChannelStereo16;
		sc->bps = 4;
	}

	if (!length)
		return;

	if (inwidth == 1) {
		if (outwidth == 1) {
			for (i = 0; i < outcount; i++, ob++, ib++) {
				ob->left = ib->left - 128;
				ob->right = ib->right - 128;
			}
		} else if (outwidth == 2) {
			for (i = 0; i < outcount; i++, os++, ib++) {
				os->left = (ib->left - 128) << 8;
				os->right = (ib->right - 128) << 8;
			}
		}
	} else if (inwidth == 2) {
		if (outwidth == 1) {
			for (i = 0; i < outcount; i++, ob++, ib++) {
				ob->left = LittleShort (is->left) >> 8;
				ob->right = LittleShort (is->right) >> 8;
			}
		} else if (outwidth == 2) {
			for (i = 0; i < outcount; i++, os++, is++) {
				os->left = LittleShort (is->left);
				os->right = LittleShort (is->right);
			}
		}
	}
	{
		byte       *x = sc->data + sc->length * outwidth * 2;
		if (memcmp (x, "\xde\xad\xbe\xef", 4))
			Sys_Error ("SND_ResampleStereo screwed the pooch %02x%02x%02x%02x",
					   x[0], x[1], x[2], x[3]);
	}
}
