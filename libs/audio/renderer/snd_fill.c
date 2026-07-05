/*
	snd_fill.c

	Sound buffer filling

	Copyright (C) 2026 Bill Currie <bill@tanwiah.org>

	Author: Bill Currie <bill@tanwiah.org>
	Date: 2026/07/05

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
#include <pthread.h>
#include <unistd.h>

#include "QF/darray.h"
#include "QF/ringbuffer.h"
#include "QF/sys.h"

#include "snd_internal.h"

#define SAMPLE_GAP	4

typedef struct sfx_bind_s {
	sfx_t      *sfx;
	channel_t  *channel;
} sfx_bind_t;

typedef struct sfx_fill_s {
	sfxbuffer_t *buffer;
	unsigned    headpos;
} sfx_fill_t;

typedef struct sfx_seek_s {
	sfxstream_t *stream;
	unsigned    pos;
} sfx_seek_t;

typedef struct sfx_load_s {
	sfxblock_t *block;
	sfxbuffer_t *(*load) (sfxblock_t *block);
} sfx_load_t;

static RING_BUFFER_ATOMIC(sfx_bind_t, 512) bind_queue;

static RING_BUFFER_ATOMIC(sfx_fill_t, 512) fill_queue;

static RING_BUFFER_ATOMIC(sfx_seek_t, 512) seek_queue;

static RING_BUFFER_ATOMIC(sfx_load_t, 512) load_queue;

static void
read_samples (sfxbuffer_t *buffer, int count)
{
	qfZoneScoped (true);

	if (buffer->head + count > buffer->size) {
		count -= buffer->size - buffer->head;
		read_samples (buffer, buffer->size - buffer->head);
		read_samples (buffer, count);
	} else {
		sfxstream_t *stream = buffer->stream;
		const sfx_t *sfx = stream->base.sfx;
		wavinfo_t  *info = &stream->base.wavinfo;
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
fill_buffer (sfxbuffer_t *buffer, unsigned headpos)
{
	qfZoneScoped (true);
	unsigned    samples;
	unsigned    loop_samples = 0;
	const sfx_t *sfx = *buffer->sfx;
	sfxstream_t *stream = buffer->stream;
	wavinfo_t  *info = &stream->base.wavinfo;

	// find out how many samples can be read into the buffer
	samples = buffer->tail - buffer->head - SAMPLE_GAP;
	if (buffer->tail <= buffer->head)
		samples += buffer->size;

	if (headpos + samples > buffer->sfx_length) {
		if (sfx->loopstart == (unsigned)-1) {
			samples = buffer->sfx_length - headpos;
		} else {
			loop_samples = headpos + samples - buffer->sfx_length;
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

static void
queue_fill (sfxbuffer_t *buffer, unsigned headpos)
{
	qfZoneScoped (true);
	sfx_fill_t  fill = {
		.buffer = buffer,
		.headpos = headpos,
	};
	while (!RB_SPACE_AVAILABLE (fill_queue)) continue;
	RB_WRITE_DATA (fill_queue, &fill, 1);
}

static void
queue_seek (sfxstream_t *stream, unsigned pos)
{
	qfZoneScoped (true);
	sfx_seek_t  seek = {
		.stream = stream,
		.pos = pos,
	};
	while (!RB_SPACE_AVAILABLE (seek_queue)) continue;
	RB_WRITE_DATA (seek_queue, &seek, 1);
}

static bool
run_fill_queue (void)
{
	qfZoneScoped (true);
	if (RB_DATA_AVAILABLE (fill_queue)) {
		sfx_fill_t  fill;
		RB_READ_DATA (fill_queue, &fill, 1);

		fill_buffer (fill.buffer, fill.headpos);
		return true;
	}
	return false;
}

static bool
run_seek_queue (void)
{
	qfZoneScoped (true);
	if (RB_DATA_AVAILABLE (seek_queue)) {
		sfx_seek_t  seek;
		RB_READ_DATA (seek_queue, &seek, 1);

		seek.stream->seek (seek.stream, seek.pos);
		fill_buffer (seek.stream->buffer, seek.pos);
		return true;
	}
	return false;
}

static bool
run_bind_queue (void)
{
	qfZoneScoped (true);
	if (RB_DATA_AVAILABLE (bind_queue)) {
		sfx_bind_t  bind;
		RB_READ_DATA (bind_queue, &bind, 1);

		auto buffer = bind.sfx->open (bind.sfx);
		if (buffer) {
			// start the channel in the mixer
			bind.channel->buffer = buffer;
		} else {
			// the channel can't be used any more, so let it get cleaned up
			bind.channel->done = true;
		}
		return true;
	}
	return false;
}

static sfxbuffer_t *
snd_block_open (sfx_t *sfx)
{
	qfZoneScoped (true);
	sfxbuffer_t *buffer = sfx->block->buffer;
	SND_Memory_Retain (buffer);
	return buffer;
}

static void
snd_block_close (sfxbuffer_t *buffer)
{
	qfZoneScoped (true);
	SND_Memory_Release (buffer);
}

static bool
run_load_queue (void)
{
	qfZoneScoped (true);
	if (RB_DATA_AVAILABLE (load_queue)) {
		sfx_load_t  load;
		RB_READ_DATA (load_queue, &load, 1);

		auto buffer = load.load (load.block);
		if (buffer) {
			SND_Memory_Retain (buffer);
			load.block->buffer = buffer;
			load.block->base.sfx->open = snd_block_open;
			buffer->close = snd_block_close;
		};
		return true;
	}
	return false;
}

void
SND_StreamSetPos (sfxbuffer_t *buffer, unsigned pos)
{
	qfZoneScoped (true);
	float       stepscale;
	sfxstream_t *stream = buffer->stream;
	const sfx_t *sfx = stream->base.sfx;
	wavinfo_t  *info = &stream->base.wavinfo;

	stepscale = (float) info->rate / sfx->snd->speed;

	buffer->head = buffer->tail = 0;
	buffer->pos = pos;
	stream->pos = pos;
	queue_seek (stream, buffer->pos * stepscale);
}

void
SND_StreamAdvance (sfxbuffer_t *buffer, unsigned count)
{
	qfZoneScoped (true);
	float       stepscale;
	unsigned    headpos, samples;
	sfxstream_t *stream = buffer->stream;
	const sfx_t *sfx = stream->base.sfx;
	wavinfo_t  *info = &stream->base.wavinfo;

	stream->pos += count;
	// update the stream buffers in chunks
	count = (stream->pos - buffer->pos) & ~(STREAM_CHUNK - 1);
	if (!count) {
		return;
	}

	stepscale = (float) info->rate / sfx->snd->speed;

	// find out how many samples the buffer currently holds
	samples = buffer->head - buffer->tail;
	if (buffer->head < buffer->tail)
		samples += buffer->size;

	// find out where head points to in the stream
	headpos = buffer->pos + samples;
	if (headpos >= buffer->sfx_length) {
		if (sfx->loopstart == (unsigned)-1)
			headpos = buffer->sfx_length;
		else
			headpos -= buffer->sfx_length - sfx->loopstart;
	}

	bool seek = false;
	if (samples < count) {
		buffer->head = buffer->tail = 0;
		buffer->pos += count;
		if (buffer->pos > buffer->sfx_length) {
			if (sfx->loopstart == (unsigned)-1) {
				// reset the buffer and fill it incase it's needed again
				buffer->pos = 0;
			} else {
				buffer->pos -= sfx->loopstart;
				buffer->pos %= buffer->sfx_length - sfx->loopstart;
				buffer->pos += sfx->loopstart;
			}
			stream->pos = buffer->pos;
		}
		headpos = buffer->pos;
		seek = true;
	} else {
		buffer->pos += count;
		if (buffer->pos >= buffer->sfx_length) {
			if (sfx->loopstart == (unsigned)-1) {
				// reset the buffer and fill it in case it's needed again
				headpos = buffer->pos = 0;
				buffer->head = buffer->tail = 0;
				count = 0;
				seek = true;
			} else {
				buffer->pos -= buffer->sfx_length - sfx->loopstart;
			}
			stream->pos = buffer->pos;
		}

		buffer->tail += count;
		if (buffer->tail >= buffer->size)
			buffer->tail -= buffer->size;
	}
	if (seek) {
		// seek will fill the buffer
		queue_seek (stream, buffer->pos * stepscale);
	} else {
		queue_fill (buffer, headpos);
	}
}

void
SND_Queue_Bind (sfx_t *sfx, channel_t *channel)
{
	qfZoneScoped (true);
	sfx_bind_t bind = {
		.sfx = sfx,
		.channel = channel,
	};
	while (!RB_SPACE_AVAILABLE (bind_queue)) continue;
	RB_WRITE_DATA (bind_queue, &bind, 1);
}

void
SND_Queue_Load (sfxblock_t *block, sfx_load_f loadf)
{
	qfZoneScoped (true);
	sfx_load_t load = {
		.block = block,
		.load = loadf,
	};
	while (!RB_SPACE_AVAILABLE (load_queue)) continue;
	RB_WRITE_DATA (load_queue, &load, 1);
}

static _Atomic bool snd_fill_stop;

static void *
fill_thread (void *data)
{
	qfZoneScoped (true);
	while (!snd_fill_stop) {
		bool did_something = false;
		did_something |= run_fill_queue ();
		did_something |= run_seek_queue ();
		did_something |= run_bind_queue ();
		did_something |= run_load_queue ();
		if (!did_something) {
			usleep (500);
		}
	}
	return nullptr;
}

static pthread_t fill_thread_id;

bool
SND_Fill_Init (void)
{
	qfZoneScoped (true);
	if (pthread_create (&fill_thread_id, nullptr, fill_thread, nullptr) < 0) {
		memset (&fill_thread_id, 0, sizeof (fill_thread_id));
		Sys_Printf (RED"could not create thread"DFL"\n");
		return false;
	}
	return true;
}

void
SND_Fill_Shutdown (void)
{
	qfZoneScoped (true);
	snd_fill_stop = true;
	void *ret;
	pthread_join (fill_thread_id, &ret);
}
