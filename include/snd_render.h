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


/** \defgroup sound_render Sound rendering sub-system.
	\ingroup sound
*/
//@{

#include "QF/plugin/snd_render.h"
#include "QF/quakeio.h"
#include "QF/sound.h"
#include "QF/zone.h"

typedef struct portable_samplepair_s portable_samplepair_t;
typedef struct dma_s dma_t;
typedef struct wavinfo_s wavinfo_t;
typedef struct channel_s channel_t;
typedef struct sfxbuffer_s sfxbuffer_t;
typedef struct sfxblock_s sfxblock_t;
typedef struct sfxstream_s sfxstream_t;

/** Represent a sound sample in the mixer.
*/
struct portable_samplepair_s {
	int         left;						//!< left sample
	int         right;						//!< right sample
};

/** communication structure between output drivers and renderer
*/
struct dma_s {
	int				speed;					//!< sample rate
	int				samplebits;				//!< bits per sample
	int				channels;				//!< number of output channels
	int				samples;				//!< mono samples in buffer
	int				submission_chunk;		//!< don't mix less than this #
	int				samplepos;				//!< in mono samples
	unsigned char	*buffer;				//!< destination for mixed sound
	/** Transfer mixed samples to the output.
		\param endtime	sample end time (count = endtime - snd_paintedtime)
	*/
	void            (*xfer) (int endtime);
};

/** Describes the sound data.
	For looped data (loopstart >= 0), play starts at sample 0, goes to the end,
	then loops back to loopstart. This allows an optional lead in section
	followed by a continuously looped (until the sound is stopped) section.
*/
struct wavinfo_s {
	unsigned    rate;			//!< sample rate
	unsigned    width;			//!< bits per sample
	unsigned    channels;		//!< number of channels
	unsigned    loopstart;		//!< start sample of loop. -1 = no loop
	unsigned    samples;		//!< size of sound in samples
	unsigned    dataofs;		//!< chunk starts this many bytes from BOF
	unsigned    datalen;		//!< chunk bytes
};

/** Buffer for storing sound samples in memory. For cached sounds, acts as
	an ordinary buffer. For streamed sounds, acts as a ring buffer.
*/
struct sfxbuffer_s {
	unsigned    head;			//!< ring buffer head position in sampels
	unsigned    tail;			//!< ring buffer tail position in sampels
	unsigned    length;			//!< length of buffer in samples
	unsigned    pos;			//!< position of tail within full stream
	unsigned    bps;			//!< bytes per sample: 1 2 4 usually
	/** paint samples into the mix buffer
		\param offset   offset into the mix buffer at which to start mixing
						the channel
		\param ch		sound channel
		\param buffer	sound data
		\param count	number of samples to paint
	*/
	void        (*paint) (int offset, channel_t *ch, void *buffer,
						  unsigned count);
	/** Advance the position with the stream, updating the ring buffer as
		necessary. Null for chached sounds.
		\param buffer	"this"
		\param count	number of samples to advance
	*/
	void        (*advance) (sfxbuffer_t *buffer, unsigned int count);
	/** Seek to an absolute position within the stream, resetting the ring
		buffer.
		\param buffer	"this"
		\param pos		sample position with the stream
	*/
	void        (*setpos) (sfxbuffer_t *buffer, unsigned int pos);
	sfx_t      *sfx;			//!< owning sfx_t instance
	byte        data[4];		//!< sample data
};

/** Representation of sound loaded that is streamed in as needed.
*/
struct sfxstream_s {
	sfx_t      *sfx;			//!< owning sfx_t instance
	void       *file;			//!< handle for "file" representing the stream
	wavinfo_t   wavinfo;		//!< description of sound data
	unsigned    pos;			//!< position of next sample within full stream
	/** Resample raw data into internal format.
		\param sc		buffer to write resampled sound (sfxstream_s::buffer)
		\param data		raw sample data
		\param length	number of raw samples to resample
		\param prev		pointer to end of last resample for smoothing
	*/
	void        (*resample)(sfxbuffer_t *, byte *, int, void *);
	/** Read data from the stream.
		\param file		handle for "file" representing the stream
						(sfxstream_s::file)
		\param data		destination of read data
		\param bytes	number of bytes to read from stream
		\param info		description of sound data (sfxstream_s::wavinfo)
		\return			number of bytes read from stream
	*/
	int         (*read)(void *file, byte *data, int bytes, wavinfo_t *info);
	/** Seek to an absolute position within the stream.
		\param file		handle for "file" representing the stream
						(sfxstream_s::file)
		\param pos		sample position with the stream
		\param info		description of sound data (sfxstream_s::wavinfo)
	*/
	int         (*seek)(void *file, int pos, wavinfo_t *info);
	sfxbuffer_t buffer;			//<! stream's ring buffer
};

/** Representation of sound loaded into memory as a full block.
*/
struct sfxblock_s {
	sfx_t      *sfx;			//!< owning sfx_t instance
	void       *file;			//!< handle for "file" representing the block
	wavinfo_t   wavinfo;		//!< description of sound data
	cache_user_t cache;			//!< cached sound buffer (::sfxbuffer_s)
	sfxbuffer_t *buffer;		//!< pointer to cached buffer
};

/** Representation of a sound being played.
*/
struct channel_s {
	struct channel_s *next;		//!< next channel in "free" list
	sfx_t      *sfx;			//!< sound played by this channel
	int         leftvol;		//!< 0-255 volume
	int         rightvol;		//!< 0-255 volume
	unsigned    end;			//!< end time in global paintsamples
	unsigned    pos;			//!< sample position in sfx
	unsigned    looping;		//!< where to loop, -1 = no looping
	int         entnum;			//!< to allow overriding a specific sound
	int         entchannel;		//
	vec3_t      origin;			//!< origin of sound effect
	vec_t       dist_mult;		//!< distance multiplier (attenuation/clip)
	int         pause;			//!< don't update the channel at all
	int         master_vol;		//!< 0-255 master volume
	int         phase;			//!< phase shift between l-r in samples
	int         oldphase;		//!< phase shift between l-r in samples
	/** signal between main program and mixer thread that the channel is to be
		stopped.
		- both \c stop and \c done are zero: normal operation
		- \c stop is non-zero: main program is done with channel. must wait
		  for mixer to finish with channel before re-using.
		- \c done is non-zero: mixer is done with channel. can be reused at
		  any time.
		- both \c stop and \c done are non-zero: both are done with channel.
		  can be reused at any time.
	*/
	//@{
	int			stop;
	int         done;
	//@}
};

extern struct cvar_s *snd_loadas8bit;
extern struct cvar_s *snd_volume;

extern struct cvar_s *snd_interp;
extern struct cvar_s *snd_stereo_phase_separation;

extern volatile dma_t *snd_shm;

extern snd_render_data_t snd_render_data;
#define PAINTBUFFER_SIZE    512
extern portable_samplepair_t snd_paintbuffer[PAINTBUFFER_SIZE * 2];

//@}

/** \defgroup sound_render_sfx Sound sfx
	\ingroup sound_render_mix
*/
//@{
/** Cache sound data. Initializes caching fields of sfx.
	\param sfx
	\param realname
	\param info
	\param loader
*/
void SND_SFX_Cache (sfx_t *sfx, char *realname, wavinfo_t info, 
		            cache_loader_t loader);

/** Stream sound data. Initializes streaming fields of sfx.
	\param sfx
	\param realname
	\param info
	\param open
*/
void SND_SFX_Stream (sfx_t *sfx, char *realname, wavinfo_t info, 
					 sfx_t *(*open) (sfx_t *sfx));

/** Open a stream for playback.
	\param sfx
	\param file
	\param read
	\param seek
	\param close
*/
sfx_t *SND_SFX_StreamOpen (sfx_t *sfx, void *file,
						   int (*read)(void *, byte *, int, wavinfo_t *),
						   int (*seek)(void *, int, wavinfo_t *),
						   void (*close) (sfx_t *));

/** Pre-load a sound into the cache.
	\param sample	name of sound to precache
*/
sfx_t *SND_PrecacheSound (const char *sample);

/** Tag a cached sound to prevent it being flushed unnecessarily.
	\param sample	name of sound touch
	\todo check that Cache_TryGet() does the right thing
*/
void SND_TouchSound (const char *sample);

/** Pre-load a sound.
	\param name		name of sound to load
*/
sfx_t *SND_LoadSound (const char *name);

/** Initialize the sfx sub-subsystem
*/
void SND_SFX_Init (void);

//@}

/** \defgroup sound_render_mix_channels Sound channels
	\ingroup sound_render_mix
	Output sound is mixed from an number of active sound channels.

	- 0 to MAX_DYNAMIC_CHANNELS-1 <br>
		normal entity sounds
	- MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 <br>
		water, etc
	- MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels - 1 <br>
		static sounds
*/
//@{
#define	MAX_CHANNELS			512	//!< number of available mixing channels
#define	MAX_DYNAMIC_CHANNELS	8	//!< number of dynamic channels
#define MAX_STATIC_CHANNELS		256	//!< number of static channels
extern	channel_t   snd_channels[MAX_CHANNELS];	//!< pool of available channels
extern	int			snd_total_channels;	//!< number of active channels

/** Allocate a sound channel that can be used for playing sounds.
*/
struct channel_s *SND_AllocChannel (void);

/** Stop a channel from playing.
	\param chan the channel to stop
*/
void SND_ChannelStop (channel_t *chan);

/** Scan channels looking for stopped channels.
*/
void SND_ScanChannels (void);

/** Disable ambient sounds.
	\todo not used, remove?
*/
void SND_AmbientOff (void);

/** Enable ambient sounds.
	\todo not used, remove?
*/
void SND_AmbientOn (void);

/** Update the sound engine with the client's position and orientation and
	render some sound.
	\param origin	3d coords of the client
	\param v_forward 3d vector of the client's facing direction
	\param v_right	3d vector of the client's rightward direction
	\param v_up		3d vector of the client's upward direction
*/
void SND_SetListener (const vec3_t origin, const vec3_t v_forward,
					  const vec3_t v_right, const vec3_t v_up);

/** Stop all sounds from playing.
*/
void SND_StopAllSounds(void);

/** Initialize the channels sub-subsystem
*/
void SND_Channels_Init (void);

/** Start a sound playing.
	\param entnum	index of entity the sound is associated with.
	\param entchannel 0-7
		- 0 auto (never willingly overrides)
		- 1 weapon
		- 2 voice
		- 3 item
		- 4 body
	\param sfx		sound to play
	\param origin	3d coords of where the sound originates
	\param fvol		absolute volume of the sound
	\param attenuation rate of volume dropoff vs distance
*/
void SND_StartSound (int entnum, int entchannel, sfx_t *sfx, const vec3_t origin,
				   float fvol,  float attenuation);

/** Create a sound generated by the world.
	\param sfx		sound to play
	\param origin	3d coords of where the sound originates
	\param vol		absolute volume of the sound
	\param attenuation rate of volume dropoff vs distance
*/
void SND_StaticSound (sfx_t *sfx, const vec3_t origin, float vol,
					float attenuation);
/** Stop an entity's sound.
	\param entnum	index of entity the sound is associated with.
	\param entchannel channel to silence
*/
void SND_StopSound (int entnum, int entchannel);

/** Start a sound local to the client view.
	\param s name of sound to play
*/
void SND_LocalSound (const char *s);
//@}


/** \defgroup sound_render_mix Mixer engine.
	\ingroup sound_render
*/
//@{
/** sound clock in samples
*/
extern unsigned snd_paintedtime;

/** Mix all active channels into the output buffer.
	\param endtime	sample time until when to mix
*/
void SND_PaintChannels(unsigned int endtime);

/** Initialize the scale table for painting of 8 bit samples.
*/
void SND_InitScaletable (void);

/** Set the paint function of the sfxbuffer
	\param sc		sfxbuffer to set.
*/
void SND_SetPaint (sfxbuffer_t *sc);
//@}


/** \defgroup sound_render_resample Resampling functios
	\ingroup sound_render
*/
//@{
/** Copy/resample mono data into sample buffer, resampling as necessary.
	\param sc		buffer to write resampled sound
	\param data		raw sample data
	\param length	number of raw samples to resample
	\param prev		pointer to end of last resample for smoothing
*/
void SND_ResampleMono (sfxbuffer_t *sc, byte *data, int length, void *prev);

/** Copy/resample stereo data into sample buffer, resampling as necessary.
	\param sc		buffer to write resampled sound
	\param data		raw sample data
	\param length	number of raw samples to resample
	\param prev		pointer to end of last resample for smoothing
*/
void SND_ResampleStereo (sfxbuffer_t *sc, byte *data, int length, void *prev);

/** Copy stereo data into sample buffer. No resampling is done. Useful for
	generated effects/music.
	\param sc		buffer to write resampled sound
	\param data		raw sample data
	\param length	number of raw samples to resample
	\param prev		pointer to end of last resample for smoothing
*/
void SND_NoResampleStereo (sfxbuffer_t *sc, byte *data, int length, void *prev);
//@}


/** \defgroup sound_render_load Sound loading functions
	\ingroup sound_render
*/
//@{
/** Load the referenced sound.
	\param sfx		sound reference
*/
void SND_Load (sfx_t *sfx);

/** Load the referenced sound from the specified Ogg file.
	\param file		pre-opened Ogg file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
*/
void SND_LoadOgg (QFile *file, sfx_t *sfx, char *realname);

/** Load the referenced sound from the specified FLAC file.
	\param file		pre-opened FLAC file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
*/
void SND_LoadFLAC (QFile *file, sfx_t *sfx, char *realname);

/** Load the referenced sound from the specified WAV file.
	\param file		pre-opened WAV file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
*/
void SND_LoadWav (QFile *file, sfx_t *sfx, char *realname);

/** Load the referenced sound from the specified MIDI file.
	\param file		pre-opened MIDI file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
*/
void SND_LoadMidi (QFile *file, sfx_t *sfx, char *realname);
//@}

/** \defgroup sound_render_cache_stream Cache/Stream Functions.
	\ingroup sound_render
*/
//@{
/** Retrieve wavinfo from a cached sound.
	\param sfx		sound reference
	\return			pointer to sound's wavinfo
*/
wavinfo_t *SND_CacheWavinfo (sfx_t *sfx);

/** Retrieve wavinfo from a streamed sound.
	\param sfx		sound reference
	\return			pointer to sound's wavinfo
*/
wavinfo_t *SND_StreamWavinfo (sfx_t *sfx);

/** Ensure a cached sound is in memory.
	\param sfx		sound reference
	\return			poitner to sound buffer
*/
sfxbuffer_t *SND_CacheTouch (sfx_t *sfx);

/** Get the pointer to the sound buffer.
	\param sfx		sound reference
	\return			sound buffer or null
	\note	The sound must be retained with SND_CacheRetain() for the returned
			buffer to be valid.
*/
sfxbuffer_t *SND_CacheGetBuffer (sfx_t *sfx);

/** Lock a cached sound into memory. After calling this, SND_CacheGetBffer()
	will return a valid buffer.
	\param sfx		sound reference
	\return			poitner to sound buffer
*/
sfxbuffer_t *SND_CacheRetain (sfx_t *sfx);

/** Unlock a cached sound from memory. After calling this, SND_CacheGetBffer()
	will return a null buffer.
	\param sfx		sound reference
*/
void SND_CacheRelease (sfx_t *sfx);

/** Get the pointer to the sound buffer.
	\param sfx		sound reference
	\return			poitner to sound buffer
*/
sfxbuffer_t *SND_StreamGetBuffer (sfx_t *sfx);

/** Lock a streamed sound into memory. Doesn't actually do anything other than
	return a pointer to the buffer.
	\param sfx		sound reference
	\return			poitner to sound buffer
*/
sfxbuffer_t *SND_StreamRetain (sfx_t *sfx);

/** Unlock a streamed sound from memory. Doesn't actually do anything.
	\param sfx		sound reference
*/
void SND_StreamRelease (sfx_t *sfx);

/** Advance the position with the stream, updating the ring buffer as
	necessary. Null for chached sounds.
	\param buffer	"this"
	\param count	number of samples to advance
*/
void SND_StreamAdvance (sfxbuffer_t *buffer, unsigned int count);

/** Seek to an absolute position within the stream, resetting the ring
	buffer.
	\param buffer	"this"
	\param pos		sample position with the stream
*/
void SND_StreamSetPos (sfxbuffer_t *buffer, unsigned int pos);

/** Allocate a sound buffer from cache for cached sounds.
	\param samples	size in samples
	\param rate		sample rate
	\param inwidth	bits per sample of input data
	\param channels	number of channels in input data
	\param block	cached sound descriptor to initialize
	\param allocator cache allocator function
	\return			pointer to sound sample buffer (setup for block mode)
*/
sfxbuffer_t *SND_GetCache (long samples, int rate, int inwidth, int channels,
						   sfxblock_t *block, cache_allocator_t allocator);
//@}

#endif//__snd_render_h
