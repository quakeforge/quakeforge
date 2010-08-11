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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/riff.h"

#include "snd_render.h"

static void
wav_callback_load (void *object, cache_allocator_t allocator)
{
	sfxblock_t *block = (sfxblock_t *) object;
	sfx_t      *sfx = block->sfx;
	const char *name = (const char *) block->file;
	QFile      *file;
	int         len;
	byte       *data;
	float      *fdata;
	sfxbuffer_t *buffer;
	wavinfo_t  *info = &block->wavinfo;

	QFS_FOpenFile (name, &file);
	if (!file)
		return; //FIXME Sys_Error?

	Qseek (file, info->dataofs, SEEK_SET);
	len = info->datalen + info->frames * info->channels * sizeof (float);
	data = malloc (len);
	fdata = (float *) (data + info->datalen);
	Qread (file, data, info->datalen);
	Qclose (file);

	SND_Convert (data, fdata, info->frames, info->channels, info->width);

	buffer = SND_GetCache (info->frames, info->rate,
						   info->channels, block, allocator);
	buffer->sfx = sfx;
	SND_SetPaint (buffer);
	SND_SetupResampler (buffer, 0);
	SND_Resample (buffer, fdata, info->frames);
	buffer->head = buffer->length;
	free (data);
}

static void
wav_cache (sfx_t *sfx, char *realname, void *file, wavinfo_t info)
{
	Qclose (file);
	SND_SFX_Cache (sfx, realname, info, wav_callback_load);
}

static int
wav_stream_read (void *file, float *buf, int count, wavinfo_t *info)
{
	int         res;
	int         len = count * info->channels * info->width;
	byte       *data = alloca (len);

	res = Qread (file, data, len);
	if (res > 0) {
		res /= (info->channels * info->width);
		SND_Convert (data, buf, res, info->channels, info->width);
	}
	return res;
}

static int
wav_stream_seek (void *file, int pos, wavinfo_t *info)
{
	pos *= info->width * info->channels;
	pos += info->dataofs;
	return Qseek (file, pos, SEEK_SET);
}

static void
wav_stream_close (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;

	Qclose (stream->file);
	free (stream);
	free (sfx);
}

static sfx_t *
wav_stream_open (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;
	QFile      *file;

	QFS_FOpenFile (stream->file, &file);
	if (!file)
		return 0;

	return SND_SFX_StreamOpen (sfx, file, wav_stream_read, wav_stream_seek,
							   wav_stream_close);
}

static void
wav_stream (sfx_t *sfx, char *realname, void *file, wavinfo_t info)
{
	Qclose (file);
	SND_SFX_Stream (sfx, realname, info, wav_stream_open);
}

static wavinfo_t
wav_get_info (QFile *file)
{
	riff_t     *riff;
	riff_d_chunk_t **ck;

	riff_format_t *fmt;
	riff_d_format_t *dfmt = 0;

	riff_data_t *data = 0;

	riff_cue_t *cue;
	riff_d_cue_t *dcue;
	riff_d_cue_point_t *cp = 0;

	riff_list_t *list;
	riff_d_chunk_t **lck;

	riff_ltxt_t *ltxt;
	riff_d_ltxt_t *dltxt = 0;

	wavinfo_t   info;

	memset (&info, 0, sizeof (info));

	if (!(riff = riff_read (file))) {
		Sys_Printf ("bad riff file\n");
		return info;
	}

	for (ck = riff->chunks; *ck; ck++) {
		RIFF_SWITCH ((*ck)->name) {
			case RIFF_CASE ('f','m','t',' '):
				fmt = (riff_format_t *) *ck;
				dfmt = (riff_d_format_t *) fmt->fdata;
				break;
			case RIFF_CASE ('d','a','t','a'):
				data = (riff_data_t *) *ck;
				break;
			case RIFF_CASE ('c','u','e',' '):
				cue = (riff_cue_t *) *ck;
				dcue = cue->cue;
				if (dcue->count)
					cp = &dcue->cue_points[dcue->count - 1];
				break;
			case RIFF_CASE ('L','I','S','T'):
				list = (riff_list_t *) *ck;
				RIFF_SWITCH (list->name) {
					case RIFF_CASE ('a','d','t','l'):
						for (lck = list->chunks; *lck; lck++) {
							RIFF_SWITCH ((*lck)->name) {
								case RIFF_CASE ('l','t','x','t'):
									ltxt = (riff_ltxt_t *) *lck;
									dltxt = &ltxt->ltxt;
									break;
							}
						}
						break;
				}
				break;
		}
	}
	if (!dfmt) {
		Sys_Printf ("missing format chunk\n");
		goto bail;
	}
	if (!data) {
		Sys_Printf ("missing data chunk\n");
		goto bail;
	}
	if (dfmt->format_tag != 1) {
		Sys_Printf ("not Microsfot PCM\n");
		goto bail;
	}
	if (dfmt->channels < 1 || dfmt->channels > 2) {
		Sys_Printf ("unsupported channel count\n");
		goto bail;
	}
	
	info.rate = dfmt->samples_per_sec;
	info.width = dfmt->bits_per_sample / 8;
	info.channels = dfmt->channels;
	info.frames = 0;
	if (cp) {
		info.loopstart = cp->sample_offset;
		if (dltxt)
			info.frames = info.loopstart + dltxt->len;
	} else {
		info.loopstart = -1;
	}
	if (!info.frames)
		info.frames = data->ck.len / (info.width * info.channels);
	info.dataofs = *(int *)data->data;
	info.datalen = data->ck.len;

bail:
	riff_free (riff);
	return info;
}

int
SND_LoadWav (QFile *file, sfx_t *sfx, char *realname)
{
	wavinfo_t   info;

	info = wav_get_info (file);
	if (!info.rate) {
		return -1;
	}

	if (info.frames / info.rate < 3) {
		Sys_DPrintf ("cache %s\n", realname);
		wav_cache (sfx, realname, file, info);
	} else {
		Sys_DPrintf ("stream %s\n", realname);
		wav_stream (sfx, realname, file, info);
	}
	return 0;
}
