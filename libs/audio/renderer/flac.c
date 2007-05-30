/*
	flac.c

	FLAC support

	Copyright (C) 2005 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2005/6/15

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

#ifdef HAVE_FLAC

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>
#include <FLAC/export.h>

/* FLAC 1.1.3 has FLAC_API_VERSION_CURRENT == 8 */
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT < 8
#define LEGACY_FLAC
#else
#undef LEGACY_FLAC
#endif

#ifdef LEGACY_FLAC
#include <FLAC/seekable_stream_decoder.h>
#else
#include <FLAC/stream_decoder.h>
#endif
#include <FLAC/metadata.h>

#include "QF/cvar.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "snd_render.h"

typedef struct {
#ifdef LEGACY_FLAC
	FLAC__SeekableStreamDecoder *decoder;
#else
	FLAC__StreamDecoder *decoder;
#endif
	QFile      *file;
	FLAC__StreamMetadata_StreamInfo info;
	FLAC__StreamMetadata *vorbis_info;
	byte       *buffer;
	int         size;
	int         pos;
} flacfile_t;

static void
#ifdef LEGACY_FLAC
error_func (const FLAC__SeekableStreamDecoder *decoder,
#else
error_func (const FLAC__StreamDecoder *decoder,
#endif
			FLAC__StreamDecoderErrorStatus status, void *client_data)
{
}

#ifdef LEGACY_FLAC
static FLAC__SeekableStreamDecoderReadStatus
read_func (const FLAC__SeekableStreamDecoder *decoder, FLAC__byte buffer[],
		   unsigned *bytes, void *client_data)
#else
static FLAC__StreamDecoderReadStatus
read_func (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[],
		   size_t *bytes, void *client_data)
#endif
{
	flacfile_t *ff = (flacfile_t *) client_data;
	*bytes = Qread (ff->file, buffer, *bytes);
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
#endif
}

#ifdef LEGACY_FLAC
static FLAC__SeekableStreamDecoderSeekStatus
seek_func (const FLAC__SeekableStreamDecoder *decoder,
#else
static FLAC__StreamDecoderSeekStatus
seek_func (const FLAC__StreamDecoder *decoder,
#endif
		   FLAC__uint64 absolute_byte_offset, void *client_data)
{
	flacfile_t *ff = (flacfile_t *) client_data;
	Qseek (ff->file, absolute_byte_offset, SEEK_SET);
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
#endif
}

#ifdef LEGACY_FLAC
static FLAC__SeekableStreamDecoderTellStatus
tell_func (const FLAC__SeekableStreamDecoder *decoder,
#else
static FLAC__StreamDecoderTellStatus
tell_func (const FLAC__StreamDecoder *decoder,
#endif
		   FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	flacfile_t *ff = (flacfile_t *) client_data;
	*absolute_byte_offset = Qtell (ff->file);
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
#endif
}

#ifdef LEGACY_FLAC
static FLAC__SeekableStreamDecoderLengthStatus
length_func (const FLAC__SeekableStreamDecoder *decoder,
#else
static FLAC__StreamDecoderLengthStatus
length_func (const FLAC__StreamDecoder *decoder,
#endif
		   FLAC__uint64 *stream_length, void *client_data)
{
	flacfile_t *ff = (flacfile_t *) client_data;
	*stream_length = Qfilesize (ff->file);
#ifdef LEGACY_FLAC
	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
#else
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
#endif
}

static FLAC__bool
#ifdef LEGACY_FLAC
eof_func (const FLAC__SeekableStreamDecoder *decoder, void *client_data)
#else
eof_func (const FLAC__StreamDecoder *decoder, void *client_data)
#endif
{
	flacfile_t *ff = (flacfile_t *) client_data;
	return Qeof (ff->file);
}

static FLAC__StreamDecoderWriteStatus
#ifdef LEGACY_FLAC
write_func (const FLAC__SeekableStreamDecoder *decoder,
#else
write_func (const FLAC__StreamDecoder *decoder,
#endif
			const FLAC__Frame *frame, const FLAC__int32 * const buffer[],
			void *client_data)
{
	flacfile_t *ff = (flacfile_t *) client_data;
	int         bps = ff->info.bits_per_sample / 8;

	if (!ff->buffer)
		ff->buffer = malloc (ff->info.max_blocksize * ff->info.channels * bps);
	if (ff->info.channels == 1) {
		unsigned    i;
		const FLAC__int32 *in = buffer[0];

		if (ff->info.bits_per_sample == 8) {
			byte       *out = ff->buffer;

			for (i = 0; i < frame->header.blocksize; i++)
				*out++ = *in++ + 128;
		} else {
			short      *out = (short *) ff->buffer;

			for (i = 0; i < frame->header.blocksize; i++)
				*out++ = *in++;
		}
	} else {
		unsigned    i;
		const FLAC__int32 *li = buffer[0];
		const FLAC__int32 *ri = buffer[1];

		if (ff->info.bits_per_sample == 8) {
			char       *lo = (char *) ff->buffer + 0;
			char       *ro = (char *) ff->buffer + 1;

			for (i = 0; i < frame->header.blocksize; i++, lo++, ro++) {
				*lo++ = *li++ + 128;
				*ro++ = *ri++ + 128;
			}
		} else {
			short      *lo = (short *) ff->buffer + 0;
			short      *ro = (short *) ff->buffer + 1;

			for (i = 0; i < frame->header.blocksize; i++, lo++, ro++) {
				*lo++ = LittleShort (*li++);
				*ro++ = LittleShort (*ri++);
			}
		}
	}
	ff->size = frame->header.blocksize * bps * ff->info.channels;
	ff->pos = 0;
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
#ifdef LEGACY_FLAC
meta_func (const FLAC__SeekableStreamDecoder *decoder,
#else
meta_func (const FLAC__StreamDecoder *decoder,
#endif
		   const FLAC__StreamMetadata *metadata, void *client_data)
{
	flacfile_t *ff = (flacfile_t *) client_data;
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
		ff->info = metadata->data.stream_info;
	if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
		ff->vorbis_info = FLAC__metadata_object_clone (metadata);
}

static flacfile_t *
open_flac (QFile *file)
{
	flacfile_t *ff = calloc (1, sizeof (flacfile_t));
#ifdef LEGACY_FLAC
	ff->decoder = FLAC__seekable_stream_decoder_new ();
#else
	ff->decoder = FLAC__stream_decoder_new ();
#endif
	ff->file = file;

#ifdef LEGACY_FLAC
	FLAC__seekable_stream_decoder_set_error_callback (ff->decoder, error_func);
	FLAC__seekable_stream_decoder_set_read_callback (ff->decoder, read_func);
	FLAC__seekable_stream_decoder_set_seek_callback (ff->decoder, seek_func);
	FLAC__seekable_stream_decoder_set_tell_callback (ff->decoder, tell_func);
	FLAC__seekable_stream_decoder_set_length_callback (ff->decoder,
													   length_func);
	FLAC__seekable_stream_decoder_set_eof_callback (ff->decoder, eof_func);
	FLAC__seekable_stream_decoder_set_write_callback (ff->decoder, write_func);
	FLAC__seekable_stream_decoder_set_metadata_callback (ff->decoder,
														 meta_func);
	FLAC__seekable_stream_decoder_set_client_data (ff->decoder, ff);
	FLAC__seekable_stream_decoder_set_metadata_respond (ff->decoder,
			FLAC__METADATA_TYPE_VORBIS_COMMENT);

	FLAC__seekable_stream_decoder_init (ff->decoder);
	FLAC__seekable_stream_decoder_process_until_end_of_metadata (ff->decoder);
#else
	FLAC__stream_decoder_set_metadata_respond (ff->decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);
	FLAC__stream_decoder_init_stream(ff->decoder, read_func, seek_func, tell_func, length_func, eof_func, write_func, meta_func, error_func, ff);
	FLAC__stream_decoder_process_until_end_of_metadata (ff->decoder);
#endif
	return ff;
}

static void
close_flac (flacfile_t *ff)
{
#ifdef LEGACY_FLAC
	FLAC__seekable_stream_decoder_finish (ff->decoder);
	FLAC__seekable_stream_decoder_delete (ff->decoder);
#else
	FLAC__stream_decoder_finish (ff->decoder);
	FLAC__stream_decoder_delete (ff->decoder);
#endif

	if (ff->vorbis_info)
		FLAC__metadata_object_delete (ff->vorbis_info);

	if (ff->buffer)
		free (ff->buffer);

	Qclose (ff->file);

	free (ff);
}

static int
flac_read (flacfile_t *ff, byte *buf, int len)
{
	int         count = 0;

	while (len) {
		int         res = 0;
		if (ff->size == ff->pos)
#ifdef LEGACY_FLAC
			FLAC__seekable_stream_decoder_process_single (ff->decoder);
#else
			FLAC__stream_decoder_process_single (ff->decoder);
#endif
		res = ff->size - ff->pos;
		if (res > len)
			res = len;
		if (res > 0) {
			memcpy (buf, ff->buffer + ff->pos, res);
			count += res;
			len -= res;
			buf += res;
			ff->pos += res;
		} else if (res < 0) {
			Sys_Printf ("flac error %d\n", res);
			return -1;
		} else {
			Sys_Printf ("unexpected eof\n");
			break;
		}
	}
	return count;
}

static sfxbuffer_t *
flac_load (flacfile_t *ff, sfxblock_t *block, cache_allocator_t allocator)
{
	byte       *data;
	sfxbuffer_t *sc = 0;
	sfx_t      *sfx = block->sfx;
	void       (*resample)(sfxbuffer_t *, byte *, int, void *);
	wavinfo_t  *info = &block->wavinfo;

	switch (ff->info.channels) {
		case 1:
			resample = SND_ResampleMono;
			break;
		case 2:
			resample = SND_ResampleStereo;
			break;
		default:
			Sys_Printf ("%s: unsupported channel count: %d\n",
						sfx->name, info->channels);
			return 0;
	}

	data = malloc (info->datalen);
	if (!data)
		goto bail;
	sc = SND_GetCache (info->samples, info->rate, info->width, info->channels,
					   block, allocator);
	if (!sc)
		goto bail;
	sc->sfx = sfx;
	if (flac_read (ff, data, info->datalen) < 0)
		goto bail;
	SND_SetPaint (sc);
	resample (sc, data, info->samples, 0);
	sc->head = sc->length;
  bail:
	if (data)
		free (data);
	close_flac (ff);
	return sc;
}

static void
flac_callback_load (void *object, cache_allocator_t allocator)
{
	QFile      *file;
	flacfile_t *ff;

	sfxblock_t *block = (sfxblock_t *) object;
	
	QFS_FOpenFile (block->file, &file);
	if (!file)
		return; //FIXME Sys_Error?

	if (!(ff = open_flac (file))) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		Qclose (file);
		return; //FIXME Sys_Error?
	}
	flac_load (ff, block, allocator);
}

static void
flac_cache (sfx_t *sfx, char *realname, flacfile_t *ff, wavinfo_t info)
{
	close_flac (ff);
	SND_SFX_Cache (sfx, realname, info, flac_callback_load);
}

static int
flac_stream_read (void *file, byte *buf, int count, wavinfo_t *info)
{
	return flac_read (file, buf, count);
}

static int
flac_stream_seek (void *file, int pos, wavinfo_t *info)
{
	flacfile_t *ff = file;

	ff->pos = ff->size = 0;
#ifdef LEGACY_FLAC
	return FLAC__seekable_stream_decoder_seek_absolute (ff->decoder, pos);
#else
	return FLAC__stream_decoder_seek_absolute (ff->decoder, pos);
#endif
}

static void
flac_stream_close (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;

	close_flac (stream->file);
	free (stream);
	free (sfx);
}

static sfx_t *
flac_stream_open (sfx_t *sfx)
{
	sfxstream_t *stream = sfx->data.stream;
	QFile      *file;
	void       *f;

	QFS_FOpenFile (stream->file, &file);
	if (!file)
		return 0;

	f = open_flac (file);
	if (!f) {
		Sys_Printf ("Input does not appear to be a flac bitstream.\n");
		Qclose (file);
		return 0;
	}

	return SND_SFX_StreamOpen (sfx, f, flac_stream_read, flac_stream_seek,
							   flac_stream_close);
}

static void
flac_stream (sfx_t *sfx, char *realname, flacfile_t *ff, wavinfo_t info)
{
	close_flac (ff);
	SND_SFX_Stream (sfx, realname, info, flac_stream_open);
}

static wavinfo_t
get_info (flacfile_t *ff)
{
	int         sample_start = -1, sample_count = 0;
	int         samples;
	wavinfo_t   info;
	FLAC__StreamMetadata_VorbisComment *vc = 0;
	FLAC__StreamMetadata_VorbisComment_Entry *ve;
	FLAC__uint32 i;

	samples = ff->info.total_samples;
	if (ff->vorbis_info) {
		vc = &ff->vorbis_info->data.vorbis_comment;

		for (i = 0, ve = vc->comments; i < vc->num_comments; ve++, i++) {
			Sys_DPrintf ("%.*s\n", ve->length, ve->entry);
			if (strncmp ("CUEPOINT=", (char *) ve->entry, 9) == 0) {
				char       *str = alloca (ve->length + 1);
				strncpy (str, (char *) ve->entry, ve->length);
				str[ve->length] = 0;
				sscanf (str + 9, "%d %d", &sample_start, &sample_count);
			}
		}
	}

	if (sample_start != -1)
		samples = sample_start + sample_count;

	info.rate = ff->info.sample_rate;
	info.width = ff->info.bits_per_sample / 8;
	info.channels = ff->info.channels;
	info.loopstart = sample_start;
	info.samples = samples;
	info.dataofs = 0;
	info.datalen = samples * 2;

	if (developer->int_val) {
		Sys_Printf ("\nBitstream is %d channel, %dHz\n",
					info.channels, info.rate);
		Sys_Printf ("\nDecoded length: %d samples (%d bytes)\n",
					info.samples, info.samples * info.channels * 2);
		if (vc) {
			Sys_Printf ("Encoded by: %.*s\n\n",
						vc->vendor_string.length, vc->vendor_string.entry);
		}
	}

	return info;
}

void
SND_LoadFLAC (QFile *file, sfx_t *sfx, char *realname)
{
	flacfile_t *ff;
	wavinfo_t   info;

	if (!(ff = open_flac (file))) {
		Sys_Printf ("Input does not appear to be an Ogg bitstream.\n");
		Qclose (file);
		return;
	}
	info = get_info (ff);
	if (info.channels < 1 || info.channels > 2) {
		Sys_Printf ("unsupported number of channels");
		return;
	}
	if (info.samples / info.rate < 3) {
		Sys_DPrintf ("cache %s\n", realname);
		flac_cache (sfx, realname, ff, info);
	} else {
		Sys_DPrintf ("stream %s\n", realname);
		flac_stream (sfx, realname, ff, info);
	}
}

#endif//HAVE_FLAC
