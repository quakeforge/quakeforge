/*
	snd_render.h

	Sound renderer plugin stuff

	Copyright (C) 2002 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: Jan 31 2003

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

	$Id$
*/

#ifndef __snd_render_h
#define __snd_render_h

#include "QF/zone.h"

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct portable_samplepair_s {
	int         left;
	int         right;
} portable_samplepair_t;

typedef struct wavinfo_s {
	unsigned    rate;
	unsigned    width;
	unsigned    channels;
	unsigned    loopstart;
	unsigned    samples;
	unsigned    dataofs;		// chunk starts this many bytes from file start
	unsigned    datalen;		// chunk bytes
} wavinfo_t;

typedef struct channel_s channel_t;
typedef struct sfxbuffer_s sfxbuffer_t;
struct sfxbuffer_s {
	unsigned    head;			// ring buffer head position in sampels
	unsigned    tail;			// ring buffer tail position in sampels
	unsigned    length;			// length of buffer in samples
	unsigned    pos;			// position of tail within full stream
	unsigned    bps;			// bytes per sample: 1 2 4 usually
	void        (*paint) (channel_t *ch, sfxbuffer_t *buffer, int count);
	void        (*advance) (sfxbuffer_t *buffer, unsigned int count);
	void        (*setpos) (sfxbuffer_t *buffer, unsigned int pos);
	sfx_t      *sfx;
	byte        data[4];
};

typedef struct sfxstream_s {
	sfx_t      *sfx;
	void       *file;
	wavinfo_t   wavinfo;
	int         pos;
	void        (*resample)(sfxbuffer_t *, byte *, int, void *);
	int         (*read)(void *file, byte *data, int bytes, wavinfo_t *info);
	int         (*seek)(void *file, int pos, wavinfo_t *info);
	sfxbuffer_t buffer;
} sfxstream_t;

typedef struct sfxblock_s {
	sfx_t      *sfx;
	void       *file;
	wavinfo_t   wavinfo;
	cache_user_t cache;
} sfxblock_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
struct channel_s {
	sfx_t      *sfx;			// sfx number
	int         leftvol;		// 0-255 volume
	int         rightvol;		// 0-255 volume
	unsigned    end;			// end time in global paintsamples
	unsigned    pos;			// sample position in sfx
	unsigned    looping;		// where to loop, -1 = no looping
	int         entnum;			// to allow overriding a specific sound
	int         entchannel;		//
	vec3_t      origin;			// origin of sound effect
	vec_t       dist_mult;		// distance multiplier (attenuation/clipK)
	int         master_vol;		// 0-255 master volume
	int         phase;			// phase shift between l-r in samples
	int         oldphase;		// phase shift between l-r in samples
};

void SND_PaintChannels(unsigned int endtime);

void SND_ResampleMono (sfxbuffer_t *sc, byte *data, int length, void *prev);
void SND_ResampleStereo (sfxbuffer_t *sc, byte *data, int length, void *prev);
void SND_NoResampleStereo (sfxbuffer_t *sc, byte *data, int length, void *prev);
sfxbuffer_t *SND_GetCache (long samples, int rate, int inwidth, int channels,
						   sfxblock_t *block, cache_allocator_t allocator);

void SND_InitScaletable (void);

void SND_Load (sfx_t *sfx);
void SND_CallbackLoad (void *object, cache_allocator_t allocator);
void SND_LoadOgg (QFile *file, sfx_t *sfx, char *realname);
void SND_LoadWav (QFile *file, sfx_t *sfx, char *realname);
void SND_LoadMidi (QFile *file, sfx_t *sfx, char *realname);

wavinfo_t *SND_CacheWavinfo (sfx_t *sfx);
wavinfo_t *SND_StreamWavinfo (sfx_t *sfx);
sfxbuffer_t *SND_CacheTouch (sfx_t *sfx);
sfxbuffer_t *SND_CacheRetain (sfx_t *sfx);
void SND_CacheRelease (sfx_t *sfx);
sfxbuffer_t *SND_StreamRetain (sfx_t *sfx);
void SND_StreamRelease (sfx_t *sfx);
void SND_StreamAdvance (sfxbuffer_t *buffer, unsigned int count);
void SND_StreamSetPos (sfxbuffer_t *buffer, unsigned int pos);

void SND_WriteLinearBlastStereo16 (void);
void SND_PaintChannelFrom8 (channel_t *ch, sfxbuffer_t *sc, int count);
void SND_PaintChannelFrom16 (channel_t *ch, sfxbuffer_t *sc, int count);
void SND_PaintChannelStereo8 (channel_t *ch, sfxbuffer_t *sc, int count);
void SND_PaintChannelStereo16 (channel_t *ch, sfxbuffer_t *sc, int count);

extern volatile dma_t *shm;
extern	channel_t   channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds
extern	int			total_channels;

#endif//__snd_render_h
