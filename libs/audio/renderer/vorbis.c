/*
	vorbis.c

	Ogg Vorbis support

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

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#include "QF/cvar.h"
#include "QF/quakefs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "snd_internal.h"

#define FRAMES 1024

typedef struct {
	float      *data;
	OggVorbis_File *vf;
} vorbis_file_t;

static size_t
vorbis_read_func (void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return Qread (datasource, ptr, size * nmemb);
}

static int
vorbis_seek_func (void *datasource, ogg_int64_t offset, int whence)
{
	return Qseek (datasource, offset, whence);
}

static int
vorbis_close_func (void *datasource)
{
	Qclose (datasource);
	return 0;
}

static long
vorbis_tell_func (void *datasource)
{
	return Qtell (datasource);
}

static ov_callbacks callbacks = {
	vorbis_read_func,
	vorbis_seek_func,
	vorbis_close_func,
	vorbis_tell_func,
};

static wavinfo_t
vorbis_get_info (OggVorbis_File *vf)
{
	vorbis_info *vi;
	int         sample_start = -1, sample_count = 0;
	int         samples;
	char      **ptr;
	wavinfo_t   info;

	vi = ov_info (vf, -1);
	samples = ov_pcm_total (vf, -1);

	for (ptr = ov_comment (vf, -1)->user_comments; *ptr; ptr++) {
		Sys_MaskPrintf (SYS_dev, "%s\n", *ptr);
		if (strncmp ("CUEPOINT=", *ptr, 9) == 0) {
			sscanf (*ptr + 9, "%d %d", &sample_start, &sample_count);
		}
	}

	if (sample_start != -1)
		samples = sample_start + sample_count;

	info.rate = vi->rate;
	info.width = sizeof (float);
	info.channels = vi->channels;
	info.loopstart = sample_start;
	info.frames = samples;
	info.dataofs = 0;
	info.datalen = samples * info.channels * info.width;

	Sys_MaskPrintf (SYS_dev, "\nBitstream is %d channel, %dHz\n",
					info.channels, info.rate);
	Sys_MaskPrintf (SYS_dev, "\nDecoded length: %d samples (%d bytes)\n",
					info.frames, info.width);
	Sys_MaskPrintf (SYS_dev, "Encoded by: %s\n\n",
					ov_comment (vf, -1)->vendor);

	return info;
}

static int
vorbis_read (OggVorbis_File *vf, float *buf, int len, wavinfo_t *info)
{
	unsigned    i;
	int         j;
	int         count = 0;
	int         current_section;
	float     **vbuf;

	while (len) {
		int         res = ov_read_float (vf, &vbuf, len, &current_section);

		if (res > 0) {
			for (i = 0; i < info->channels; i++)
				for (j = 0; j < res; j++) {
					buf[j * info->channels + i] = vbuf[i][j];
				}
			count += res;
			len -= res;
			buf += res * info->channels;
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
vorbis_load (OggVorbis_File *vf, sfxblock_t *block, cache_allocator_t allocator)
{
	float      *data;
	sfxbuffer_t *sc = 0;
	sfx_t      *sfx = block->sfx;
	wavinfo_t  *info = &block->wavinfo;

	data = malloc (info->datalen);
	if (!data)
		goto bail;
	sc = SND_GetCache (info->frames, info->rate, info->channels,
					   block, allocator);
	if (!sc)
		goto bail;
	sc->sfx = sfx;
	if (vorbis_read (vf, data, info->frames, info) < 0)
		goto bail;
	SND_SetPaint (sc);
	SND_SetupResampler (sc, 0);
	SND_Resample (sc, data, info->frames);
	sc->head = sc->length;
  bail:
	if (data)
		free (data);
	ov_clear (vf);
	return sc;
}

static void
vorbis_callback_load (void *object, cache_allocator_t allocator)
{
	QFile      *file;
	OggVorbis_File vf;

	sfxblock_t *block = (sfxblock_t *) object;

	file = QFS_FOpenFile (block->file);
	if (!file)
		return; //FIXME Sys_Error?

	if (ov_open_callbacks (file, &vf, 0, 0, callbacks) < 0) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		Qclose (file);
		return; //FIXME Sys_Error?
	}
	vorbis_load (&vf, block, allocator);
}

static void
vorbis_cache (sfx_t *sfx, char *realname, OggVorbis_File *vf, wavinfo_t info)
{
	ov_clear (vf);
	SND_SFX_Cache (sfx, realname, info, vorbis_callback_load);
}

static long
vorbis_stream_read (void *file, float **buf)
{
	sfxstream_t *stream = (sfxstream_t *) file;
	vorbis_file_t *vf = (vorbis_file_t *) stream->file;
	int         res;

	if (!vf->data)
		vf->data = malloc (FRAMES * stream->wavinfo.channels * sizeof (float));
	res = vorbis_read (vf->vf, vf->data, FRAMES, &stream->wavinfo);
	if (res <= 0) {
		stream->error = 1;
		return 0;
	}
	*buf = vf->data;
	return res;
}

static int
vorbis_stream_seek (sfxstream_t *stream, int pos)
{
	vorbis_file_t *vf = (vorbis_file_t *) stream->file;
	return ov_pcm_seek (vf->vf, pos);
}

static void
vorbis_stream_close (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;
	vorbis_file_t *vf = (vorbis_file_t *) stream->file;

	if (vf->data)
		free (vf->data);
	ov_clear (vf->vf);
	free (vf->vf);
	free (vf);
	SND_SFX_StreamClose (sfx);
}

static sfx_t *
vorbis_stream_open (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;
	QFile      *file;
	vorbis_file_t *f;

	file = QFS_FOpenFile (stream->file);
	if (!file)
		return 0;

	f = calloc (sizeof (vorbis_file_t), 1);
	f->vf = malloc (sizeof (OggVorbis_File));
	if (ov_open_callbacks (file, f->vf, 0, 0, callbacks) < 0) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		Qclose (file);
		free (f);
		return 0;
	}

	return SND_SFX_StreamOpen (sfx, f, vorbis_stream_read, vorbis_stream_seek,
							   vorbis_stream_close);
}

static void
vorbis_stream (sfx_t *sfx, char *realname, OggVorbis_File *vf, wavinfo_t info)
{
	ov_clear (vf);
	SND_SFX_Stream (sfx, realname, info, vorbis_stream_open);
}

int
SND_LoadOgg (QFile *file, sfx_t *sfx, char *realname)
{
	OggVorbis_File vf;
	wavinfo_t   info;

	if (ov_open_callbacks (file, &vf, 0, 0, callbacks) < 0) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		free (realname);
		return -1;
	}
	info = vorbis_get_info (&vf);
	if (info.channels < 1 || info.channels > 8) {
		Sys_Printf ("unsupported number of channels");
		return -1;
	}
	if (info.frames / info.rate < 3) {
		Sys_MaskPrintf (SYS_dev, "cache %s\n", realname);
		vorbis_cache (sfx, realname, &vf, info);
	} else {
		Sys_MaskPrintf (SYS_dev, "stream %s\n", realname);
		vorbis_stream (sfx, realname, &vf, info);
	}
	return 0;
}
