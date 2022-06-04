/*
	snd_mem.c

	sound memory management

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2003 Bill Currie <bill@taniwha.org>

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

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"

#include "compat.h"
#include "snd_internal.h"

#define SAMPLE_GAP	4

static uint32_t snd_mem_size;
static cvar_t snd_mem_size_cvar = {
	.name = "snd_mem_size",
	.description =
		"Amount of LOCKED memory to allocate to the sound system in MB. "
		"Defaults to 32MB.",
	.default_value = "32",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_uint, .value = &snd_mem_size },
};

static memzone_t *snd_zone;

void
SND_Memory_Init_Cvars (void)
{
	Cvar_Register (&snd_mem_size_cvar, 0, 0);
}

static void
snd_zone_error (void *data, const char *msg)
{
	Sys_Error ("Sound: %s", msg);
}

static void
snd_memory_shutdown (void *data)
{
	if (snd_zone) {
		size_t      size = snd_mem_size * 1024 * 1024;
		Sys_Free (snd_zone, size);
	}
}

int
SND_Memory_Init (void)
{
	size_t      size = snd_mem_size * 1024 * 1024;

	snd_zone = Sys_Alloc (size);
	if (!snd_zone) {
		Sys_Printf ("Sound: Unable to allocate %uMB buffer\n", snd_mem_size);
		return 0;
	}
	if (!Sys_LockMemory (snd_zone, size)) {
		Sys_Printf ("Sound: Unable to lock %uMB buffer\n", snd_mem_size);
		Sys_Free (snd_zone, size);
		return 0;
	}
	Z_ClearZone (snd_zone, size, 0, 1);
	Z_SetError (snd_zone, snd_zone_error, 0);

	Sys_MaskPrintf (SYS_snd, "Sound: Initialized %uMB buffer\n", snd_mem_size);

	Sys_RegisterShutdown (snd_memory_shutdown, 0);
	return 1;
}

sfxbuffer_t *
SND_Memory_AllocBuffer (unsigned samples)
{
	size_t      size = field_offset (sfxbuffer_t, data[samples]);
	// Z_Malloc (currently) clears memory, don't need that for the whole
	// buffer (just the header), but Z_TagMalloc // does not
	// +4 for sentinel
	sfxbuffer_t *buffer = Z_TagMalloc (snd_zone, size + 4, 1);
	// place a sentinel at the end of the buffer for added safety
	memcpy (&buffer->data[samples], "\xde\xad\xbe\xef", 4);
	// clear buffer header
	memset (buffer, 0, sizeof (sfxbuffer_t));
	return buffer;
}

void
SND_Memory_FreeBuffer (sfxbuffer_t *buffer)
{
	Z_Free (snd_zone, buffer);
}

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
	return sfx->data.block->buffer;
}

sfxbuffer_t *
SND_CacheGetBuffer (sfx_t *sfx)
{
	return sfx->data.block->buffer;
}

sfxbuffer_t *
SND_CacheRetain (sfx_t *sfx)
{
	return sfx->data.block->buffer;
}

void
SND_CacheRelease (sfx_t *sfx)
{
}

sfxbuffer_t *
SND_StreamGetBuffer (sfx_t *sfx)
{
	return sfx->data.stream->buffer;
}

sfxbuffer_t *
SND_StreamRetain (sfx_t *sfx)
{
	return sfx->data.stream->buffer;
}

void
SND_StreamRelease (sfx_t *sfx)
{
}

wavinfo_t *
SND_CacheWavinfo (sfx_t *sfx)
{
	return &sfx->data.block->wavinfo;
}

wavinfo_t *
SND_StreamWavinfo (sfx_t *sfx)
{
	return &sfx->data.stream->wavinfo;
}

static void
read_samples (sfxbuffer_t *buffer, int count)
{

	if (buffer->head + count > buffer->size) {
		count -= buffer->size - buffer->head;
		read_samples (buffer, buffer->size - buffer->head);
		read_samples (buffer, count);
	} else {
		sfx_t      *sfx = buffer->sfx;
		sfxstream_t *stream = sfx->data.stream;
		wavinfo_t  *info = &stream->wavinfo;
		float      *data = buffer->data + buffer->head * info->channels;
		int         c;

		if ((c = stream->read (stream, data, count)) != count)
			Sys_Printf ("%s nr %d %d\n", sfx->name, count, c);

		if (c > 0) {
			buffer->head += count;
			if (buffer->head >= buffer->size)
				buffer->head -= buffer->size;
		}
	}
}

static void
fill_buffer (sfx_t *sfx, sfxstream_t *stream, sfxbuffer_t *buffer,
			 wavinfo_t *info, unsigned int headpos)
{
	unsigned int samples;
	unsigned int loop_samples = 0;

	// find out how many samples can be read into the buffer
	samples = buffer->tail - buffer->head - SAMPLE_GAP;
	if (buffer->tail <= buffer->head)
		samples += buffer->size;

	if (headpos + samples > sfx->length) {
		if (sfx->loopstart == (unsigned int)-1) {
			samples = sfx->length - headpos;
		} else {
			loop_samples = headpos + samples - sfx->length;
			samples -= loop_samples;
		}
	}
	if (samples)
		read_samples (buffer, samples);
	if (loop_samples) {
		stream->seek (stream, info->loopstart);
		read_samples (buffer, loop_samples);
	}
}

void
SND_StreamSetPos (sfxbuffer_t *buffer, unsigned int pos)
{
	float       stepscale;
	sfx_t      *sfx = buffer->sfx;
	sfxstream_t *stream = sfx->data.stream;
	wavinfo_t  *info = &stream->wavinfo;

	stepscale = (float) info->rate / sfx->snd->speed;

	buffer->head = buffer->tail = 0;
	buffer->pos = pos;
	stream->pos = pos;
	stream->seek (stream, buffer->pos * stepscale);
	fill_buffer (sfx, stream, buffer, info, pos);
}

int
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
		return 1;

	stepscale = (float) info->rate / sfx->snd->speed;

	// find out how many samples the buffer currently holds
	samples = buffer->head - buffer->tail;
	if (buffer->head < buffer->tail)
		samples += buffer->size;

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
		stream->seek (stream, buffer->pos * stepscale);
	} else {
		buffer->pos += count;
		if (buffer->pos >= sfx->length) {
			if (sfx->loopstart == (unsigned int)-1) {
				// reset the buffer and fill it in case it's needed again
				headpos = buffer->pos = 0;
				buffer->head = buffer->tail = 0;
				count = 0;
				stream->seek (stream, buffer->pos * stepscale);
			} else {
				buffer->pos -= sfx->length - sfx->loopstart;
			}
			stream->pos = buffer->pos;
		}

		buffer->tail += count;
		if (buffer->tail >= buffer->size)
			buffer->tail -= buffer->size;
	}
	fill_buffer (sfx, stream, buffer, info, headpos);
	return !stream->error;
}

int
SND_Load (sfx_t *sfx)
{
	char       *realname;
	char        buf[4];
	QFile      *file;

	sfx->touch = sfx->retain = snd_fail;
	sfx->release = snd_noop;
	sfx->close = snd_noop;
	sfx->open = snd_open_fail;

	file = QFS_FOpenFile (sfx->name);
	if (!file) {
		Sys_Printf ("Couldn't load %s\n", sfx->name);
		return -1;
	}
	sfx->open = snd_open;
	if (!strequal (qfs_foundfile.realname, sfx->name)) {
		realname = strdup (qfs_foundfile.realname);
	} else {
		realname = (char *) sfx->name;	// won't free if realname == sfx->name
	}
	Qread (file, buf, 4);
	Qseek (file, 0, SEEK_SET);
#ifdef HAVE_VORBIS
	if (strnequal ("OggS", buf, 4)) {
		Sys_MaskPrintf (SYS_dev, "SND_Load: ogg file\n");
		if (SND_LoadOgg (file, sfx, realname) == -1)
			goto bail;
		return 0;
	}
#endif
#ifdef HAVE_FLAC
	if (strnequal ("fLaC", buf, 4)) {
		Sys_MaskPrintf (SYS_dev, "SND_Load: flac file\n");
		if (SND_LoadFLAC (file, sfx, realname) == -1)
			goto bail;
		return 0;
	}
#endif
#ifdef HAVE_WILDMIDI
	if (strnequal ("MThd", buf, 4)) {
		Sys_MaskPrintf (SYS_dev, "SND_Load: midi file\n");
		if (SND_LoadMidi (file, sfx, realname) == -1)
			goto bail;
		return 0;
	}
#endif
	if (strnequal ("RIFF", buf, 4)) {
		Sys_MaskPrintf (SYS_dev, "SND_Load: wav file\n");
		if (SND_LoadWav (file, sfx, realname) == -1)
			goto bail;
		return 0;
	}
bail:
	Qclose (file);
	if (realname != sfx->name)
		free (realname);
	return -1;
}
