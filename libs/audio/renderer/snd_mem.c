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

#ifdef HAVE_MALLOC_H
#include <malloc.h>
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

volatile dma_t  *snd_shm;
snd_render_data_t snd_render_data = {
	0,
	0,
	0,
	0,
	&snd_paintedtime,
	0,
};

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

static sfx_t *
snd_open_fail (sfx_t *sfx)
{
	return 0;
}

sfxbuffer_t *
SND_CacheTouch (sfx_t *sfx)
{
	return Cache_Check (&sfx->data.block->cache);
}

sfxbuffer_t *
SND_CacheGetBuffer (sfx_t *sfx)
{
	return sfx->data.block->buffer;
}

sfxbuffer_t *
SND_CacheRetain (sfx_t *sfx)
{
	sfxblock_t *block = sfx->data.block;
	block->buffer = Cache_TryGet (&block->cache);
	if (!block->buffer)
		Sys_Printf ("failed to cache sound!\n");
	return block->buffer;
}

void
SND_CacheRelease (sfx_t *sfx)
{
	sfxblock_t *block = sfx->data.block;
	// due to the possibly asynchronous nature of the mixer, the cache
	// may have been flushed behind our backs
	if (block->cache.data) {
		if (!Cache_ReadLock (&block->cache)) {
			Sys_Printf ("WARNING: taniwha screwed up in the sound engine: %s\n",
						sfx->name);
			return;
		}
		Cache_Release (&block->cache);
		if (!Cache_ReadLock (&block->cache))
			block->buffer = 0;
	}
}

sfxbuffer_t *
SND_StreamGetBuffer (sfx_t *sfx)
{
	return &sfx->data.stream->buffer;
}

sfxbuffer_t *
SND_StreamRetain (sfx_t *sfx)
{
	return &sfx->data.stream->buffer;
}

void
SND_StreamRelease (sfx_t *sfx)
{
}

wavinfo_t *
SND_CacheWavinfo (sfx_t *sfx)
{
	return &sfx->data.stream->wavinfo;
}

wavinfo_t *
SND_StreamWavinfo (sfx_t *sfx)
{
	return &sfx->data.stream->wavinfo;
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
		float       stepscale;
		int         samples, size;
		sfx_t      *sfx = buffer->sfx;
		sfxstream_t *stream = sfx->data.stream;
		wavinfo_t  *info = &stream->wavinfo;

		stepscale = (float) info->rate / snd_shm->speed;

		samples = count * stepscale;
		size = samples * info->width * info->channels;

		if (stream->resample) {
			byte       *data = alloca (size);
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
	sfxstream_t *stream = sfx->data.stream;
	wavinfo_t  *info = &stream->wavinfo;

	stepscale = (float) info->rate / snd_shm->speed;

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
	sfxstream_t *stream = sfx->data.stream;
	wavinfo_t  *info = &stream->wavinfo;

	stream->pos += count;
	count = (stream->pos - buffer->pos) & ~255;
	if (!count)
		return;

	stepscale = (float) info->rate / snd_shm->speed;

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
	sfx->open = snd_open_fail;

	_QFS_FOpenFile (sfx->name, &file, foundname, 1);
	if (!file) {
		Sys_Printf ("Couldn't load %s\n", sfx->name);
		dstring_delete (foundname);
		return;
	}
	sfx->open = snd_open;
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
	stepscale = (float) rate / snd_shm->speed;
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
