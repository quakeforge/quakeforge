/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "snd_internal.h"

#define MAX_SFX		512
static sfx_t    snd_sfx[MAX_SFX];
static int      snd_num_sfx;
static hashtab_t *snd_sfx_hash;

static const char *
snd_sfx_getkey (const void *sfx, void *unused)
{
	return ((sfx_t *) sfx)->name;
}

static void
snd_sfx_free (void *_sfx, void *unused)
{
	sfx_t      *sfx = (sfx_t *) _sfx;
	free ((char *) sfx->name);
	sfx->name = 0;
}

static void
snd_block_close (sfxbuffer_t *buffer)
{
	SND_Memory_Release (buffer);
}

void
SND_SFX_Block (sfx_t *sfx, char *realname, wavinfo_t info,
			   sfxbuffer_t *(*load) (sfxblock_t *block))
{
	sfxblock_t *block = calloc (1, sizeof (sfxblock_t));

	sfx->block = block;
	sfx->wavinfo = SND_BlockWavinfo;
	sfx->loopstart = SND_ResamplerFrames (sfx, info.loopstart);
	sfx->length = SND_ResamplerFrames (sfx, info.frames);

	block->sfx = sfx;
	block->file = realname;
	block->wavinfo = info;
	block->buffer = load (block);
	SND_Memory_Retain (block->buffer);
	block->buffer->close = snd_block_close;
}

void
SND_SFX_Stream (sfx_t *sfx, char *realname, wavinfo_t info,
				sfxbuffer_t *(*open) (sfx_t *sfx))
{
	sfxstream_t *stream = calloc (1, sizeof (sfxstream_t));
	sfx->open = open;
	sfx->wavinfo = SND_StreamWavinfo;
	sfx->stream = stream;
	sfx->loopstart = SND_ResamplerFrames (sfx, info.loopstart);
	sfx->length = SND_ResamplerFrames (sfx, info.frames);

	stream->file = realname;
	stream->wavinfo = info;
}

sfxbuffer_t *
SND_SFX_StreamOpen (sfx_t *sfx, void *file,
					long (*read)(void *, float **),
					int (*seek)(sfxstream_t *, int),
					void (*close) (sfxbuffer_t *))
{
	snd_t      *snd = sfx->snd;
	sfxstream_t *stream = sfx->stream;
	wavinfo_t  *info = &stream->wavinfo;
	int         frames;

	// if the speed is 0, there is no sound driver (probably failed to connect
	// to jackd)
	if (!snd->speed)
		return 0;

	frames = snd->speed * 0.3;
	frames = (frames + 255) & ~255;

	stream = calloc (1, sizeof (sfxstream_t));
	stream->buffer = SND_Memory_AllocBuffer (frames * info->channels);
	if (!stream->buffer) {
		free (stream);
		return 0;
	}

	stream->file = file;
	stream->sfx = sfx;
	stream->ll_read = read;
	stream->ll_seek = seek;

	stream->wavinfo = *sfx->wavinfo (sfx);

	stream->buffer->stream = stream;
	stream->buffer->size = frames;
	stream->buffer->advance = SND_StreamAdvance;
	stream->buffer->setpos = SND_StreamSetPos;
	stream->buffer->sfx_length = info->frames;
	stream->buffer->channels = info->channels;
	stream->buffer->close = close;
	SND_SetPaint (stream->buffer);

	SND_SetupResampler (stream->buffer, 1);			// get sfx setup properly
	stream->buffer->setpos (stream->buffer, 0);		// pre-fill the buffer

	return stream->buffer;
}

void
SND_SFX_StreamClose (sfxstream_t *stream)
{
	SND_PulldownResampler (stream);
	SND_Memory_Free (stream->buffer);
	free (stream);
}

sfx_t *
SND_LoadSound (snd_t *snd, const char *name)
{
	sfx_t      *sfx;

	if (!snd_sfx_hash)
		return 0;
	if ((sfx = (sfx_t *) Hash_Find (snd_sfx_hash, name)))
		return sfx;

	if (snd_num_sfx == MAX_SFX)
		Sys_Error ("s_load_sound: out of sfx_t");

	sfx = &snd_sfx[snd_num_sfx++];
	sfx->snd = snd;
	sfx->name = strdup (name);
	if (SND_Load (sfx) == -1) {
		snd_num_sfx--;
		return 0;
	}
	Hash_Add (snd_sfx_hash, sfx);
	return sfx;
}

sfx_t *
SND_PrecacheSound (snd_t *snd, const char *name)
{
	sfx_t      *sfx;

	if (!name)
		Sys_Error ("SND_PrecacheSound: NULL");

	sfx = SND_LoadSound (snd, va (0, "sound/%s", name));
	return sfx;
}

static void
s_gamedir (int phase, void *data)
{
	snd_num_sfx = 0;
}

static void
s_soundlist_f (void)
{
	int			total, i;
	sfx_t	   *sfx;

	total = 0;
	for (sfx = snd_sfx, i = 0; i < snd_num_sfx; i++, sfx++) {
		total += sfx->length;
		Sys_Printf ("%6d %6d %s\n", sfx->loopstart, sfx->length, sfx->name);
	}
	Sys_Printf ("Total resident: %i\n", total);
}

void
SND_SFX_Init (snd_t *snd)
{
	snd_sfx_hash = Hash_NewTable (511, snd_sfx_getkey, snd_sfx_free, 0, 0);

	QFS_GamedirCallback (s_gamedir, 0);

	Cmd_AddCommand ("soundlist", s_soundlist_f,
					"Reports a list of loaded sounds");
}
