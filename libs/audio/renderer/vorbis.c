/*
	vorbis.c

	ogg/vorbis support

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/6/14

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

#ifdef HAVE_VORBIS

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>
#include <vorbis/vorbisfile.h>

#include "QF/cvar.h"
#include "QF/quakefs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "snd_render.h"

static size_t
read_func (void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return Qread (datasource, ptr, size * nmemb);
}

static int
seek_func (void *datasource, ogg_int64_t offset, int whence)
{
	return Qseek (datasource, offset, whence);
}

static int
close_func (void *datasource)
{
	Qclose (datasource);
	return 0;
}

static long
tell_func (void *datasource)
{
	return Qtell (datasource);
}

static ov_callbacks callbacks = {
	read_func,
	seek_func,
	close_func,
	tell_func,
};

static void
get_info (OggVorbis_File *vf, sfx_t *sfx)
{
	vorbis_info *vi;
	int         sample_start = -1, sample_count = 0;
	char      **ptr;

	vi = ov_info (vf, -1);
	sfx->length = ov_pcm_total (vf, -1);
	sfx->channels = vi->channels;
	sfx->speed = vi->rate;

	for (ptr = ov_comment (vf, -1)->user_comments; *ptr; ptr++) {
		Sys_DPrintf ("%s\n", *ptr);
		if (strncmp ("CUEPOINT=", *ptr, 9) == 0) {
			sscanf (*ptr + 9, "%d %d", &sample_start, &sample_count);
			break;
		}
	}

	if (developer->int_val) {
		Sys_Printf ("\nBitstream is %d channel, %dHz\n",
					sfx->channels, sfx->speed);
		Sys_Printf ("\nDecoded length: %d samples (%d bytes)\n",
					sfx->length, sfx->length * sfx->channels * 2);
		Sys_Printf ("Encoded by: %s\n\n", ov_comment (vf, -1)->vendor);
	}

	if (sample_start != -1)
		sfx->length = sample_start + sample_count;
	sfx->loopstart = sample_start;
}

static int
read_ogg (OggVorbis_File *vf, byte *buf, int len)
{
	int         count = 0;
	int         current_section;

	while (len) {
		int         res = ov_read (vf, buf, len, 0, 2, 1, &current_section);
		if (res > 0) {
			count += res;
			len -= res;
			buf += res;
		} else if (res < 0) {
			Sys_Printf ("vorbis error %d\n", res);
			return -1;
		} else {
			Sys_Printf ("unexpected eof\n");
			break;
		}
	}
	return count;
}

static sfxbuffer_t *
load_ogg (OggVorbis_File *vf, sfxblock_t *block, cache_allocator_t allocator)
{
	long        size;
	byte       *data;
	sfxbuffer_t *sc = 0;
	sfx_t      *sfx = block->sfx;
	int        channels;
	void        (*paint) (channel_t *ch, sfxbuffer_t *buffer, int count);

	get_info (vf, sfx);

	switch (sfx->channels) {
		case 1:
			paint = SND_PaintChannelFrom16;
			break;
		case 2:
			paint = SND_PaintChannelStereo16;
			break;
		default:
			Sys_Printf ("%s: unsupported channel count: %d\n",
						sfx->name, sfx->channels);
			return 0;
	}

	channels = sfx->channels;

	size = sfx->length * channels * 2;

	data = malloc (size);
	if (!data)
		goto bail;
	sc = SND_GetCache (sfx->length, sfx->speed, 2, channels, block, allocator);
	if (!sc)
		goto bail;
	if (read_ogg (vf, data, size) < 0)
		goto bail;
	SND_ResampleSfx (sfx, sc, data);
	sc->paint = paint;
	sc->length = sc->head = sfx->length;
  bail:
	if (data)
		free (data);
	ov_clear (vf);
	return sc;
}

static void
ogg_callback_load (void *object, cache_allocator_t allocator)
{
	QFile      *file;
	OggVorbis_File vf;

	sfxblock_t *block = (sfxblock_t *) object;
	
	QFS_FOpenFile (block->file, &file);
	if (!file)
		return; //FIXME Sys_Error?

	if (ov_open_callbacks (file, &vf, 0, 0, callbacks) < 0) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		Qclose (file);
		return; //FIXME Sys_Error?
	}
	load_ogg (&vf, block, allocator);
}

static void
vorbis_advance (sfxbuffer_t *buffer, int count)
{
}

static void
stream_ogg (sfx_t *sfx, OggVorbis_File *vf)
{
	sfxstream_t *stream;
	int         length, bytes;
	void        (*paint) (channel_t *ch, sfxbuffer_t *buffer, int count);


	get_info (vf, sfx);

	switch (sfx->channels) {
		case 1:
			paint = SND_PaintChannelFrom16;
			break;
		case 2:
			paint = SND_PaintChannelStereo16;
			break;
		default:
			Sys_Printf ("%s: unsupported channel count: %d\n",
						sfx->name, sfx->channels);
			return;
	}

	length = shm->speed * snd_mixahead->value * 3;
	bytes = sfx->channels * 2 * length;

	stream = calloc (1, sizeof (sfxstream_t) + bytes);

	stream->file = malloc (sizeof (OggVorbis_File));
	memcpy (stream->file, vf, sizeof (OggVorbis_File));

	stream->buffer.length = length;
	stream->buffer.paint = paint;
	stream->buffer.advance = vorbis_advance;

	sfx->data = stream;
}

void
SND_LoadOgg (QFile *file, sfx_t *sfx, char *realname)
{
	OggVorbis_File vf;

	if (ov_open_callbacks (file, &vf, 0, 0, callbacks) < 0) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		Qclose (file);
		free (realname);
		return;
	}
	if (ov_pcm_total (&vf, -1) < 3 * shm->speed) {
		sfxblock_t *block = calloc (1, sizeof (sfxblock_t));
		ov_clear (&vf);
		sfx->data = block;
		sfx->retain = SND_CacheRetain;
		sfx->release = SND_CacheRelease;
		block->sfx = sfx;
		block->file = realname;
		Cache_Add (&block->cache, block, ogg_callback_load);
	} else {
		sfx->retain = SND_StreamRetain;
		sfx->release = SND_StreamRelease;
		free (realname);
		stream_ogg (sfx, &vf);
	}
}

#endif//HAVE_VORBIS
