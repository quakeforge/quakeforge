/*
	snd_internal.h

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

*/

#ifndef __snd_internal_h
#define __snd_internal_h


/** \defgroup sound_render Sound rendering sub-system.
	\ingroup sound
*/
///@{

#include "QF/plugin/general.h"
#include "QF/plugin/snd_render.h"
#include "QF/plugin/snd_output.h"
#include "QF/quakeio.h"
#include "QF/sound.h"
#include "QF/zone.h"

struct transform_s;
typedef struct portable_samplepair_s portable_samplepair_t;
typedef struct snd_s snd_t;
typedef struct wavinfo_s wavinfo_t;
typedef struct channel_s channel_t;
typedef struct sfxbuffer_s sfxbuffer_t;
typedef struct sfxblock_s sfxblock_t;
typedef struct sfxstream_s sfxstream_t;
/** paint samples into the mix buffer
	\param offset   offset into the mix buffer at which to start mixing
					the channel
	\param ch		sound channel
	\param buffer	sound data
	\param count	number of frames to paint
*/
typedef void sfxpaint_t (int offset, channel_t *ch, float *buffer,
						 unsigned count);

/** Represent a sound sample in the mixer.
*/
struct portable_samplepair_s {
	float        left;						//!< left sample
	float        right;						//!< right sample
};

/** Sound system state
*/
struct snd_s {
	int				speed;					//!< sample rate
	int				samplebits;				//!< bits per sample
	int				channels;				//!< number of output channels
	int				frames;					//!< frames in buffer
											//!< 1 frame = channels samples
	int				submission_chunk;		//!< don't mix less than this #
	unsigned        paintedtime;			//!< sound clock in samples
	int				framepos;				//!< position of dma cursor
	unsigned char	*buffer;				//!< destination for mixed sound
	/** Transfer mixed samples to the output.
		\param paintbuffer The buffer of mixed samples to be transferred.
		\param count	The number of sample to transfer.
		\param volume	The gain for the samples.
	*/
	void            (*xfer) (struct snd_s *snd,
							 portable_samplepair_t *paintbuffer, int count,
							 float volume);
	/** Optional data for the xfer function.
	*/
	void            *xfer_data;

	void      (*finish_channels) (void);
	void      (*paint_channels) (struct snd_s *snd, unsigned endtime);
};

/** Describes the sound data.
	For looped data (loopstart >= 0), play starts at sample 0, goes to the end,
	then loops back to loopstart. This allows an optional lead in section
	followed by a continuously looped (until the sound is stopped) section.
*/
struct wavinfo_s {
	unsigned    rate;			//!< sample rate
	unsigned    width;			//!< bytes per sample
	unsigned    channels;		//!< number of channels
	unsigned    loopstart;		//!< start frame of loop. -1 = no loop
	unsigned    frames;			//!< size of sound in frames
	unsigned    dataofs;		//!< chunk starts this many bytes from BOF
	unsigned    datalen;		//!< chunk bytes
};

/** Buffer for storing sound samples in memory. For cached sounds, acts as
	an ordinary buffer. For streamed sounds, acts as a ring buffer.
*/
struct sfxbuffer_s {
	unsigned    head;			//!< ring buffer head position in sampels
	unsigned    tail;			//!< ring buffer tail position in sampels
	unsigned    length;			//!< length of buffer in frames
	unsigned    pos;			//!< position of tail within full stream
	unsigned    channels;		//!< number of channels per frame
	sfxpaint_t *paint;			//!< channel count specific paint function
	/** Advance the position with the stream, updating the ring buffer as
		necessary. Null for chached sounds.
		\param buffer	"this"
		\param count	number of frames to advance
		\return			true for success, false if an error occured
	*/
	int         (*advance) (sfxbuffer_t *buffer, unsigned int count);
	/** Seek to an absolute position within the stream, resetting the ring
		buffer.
		\param buffer	"this"
		\param pos		frame position with the stream
	*/
	void        (*setpos) (sfxbuffer_t *buffer, unsigned int pos);
	sfx_t      *sfx;			//!< owning sfx_t instance
	/** Sample data. The block at the beginning of the buffer (size depends on
		sample size)
	*/
	float       data[1];
};

/** Representation of sound loaded that is streamed in as needed.
*/
struct sfxstream_s {
	sfx_t      *sfx;			//!< owning sfx_t instance
	void       *file;			//!< handle for "file" representing the stream
	wavinfo_t   wavinfo;		//!< description of sound data
	unsigned    pos;			//!< position of next frame full stream
	int         error;			//!< an error occured while reading

	void       *state;			//!< resampler state information
	/** Read data from the stream.
		This is a low-level function used by sfxstream_t::read. The read
		samples will be at the sample rate of the underlying stream.
		\param cb_data	stream relevant infomation (actually ::sfxstream_t
						("this"), but this signature is used for compatibility
						with libsamplerate
		\param data		address of pointer to be set to point to the buffer
						holding the samples
	*/
	long        (*ll_read)(void *cb_data, float **data);
	/** Seek to an absolute position within the stream (low level).
		\param stream	"this"
		\param pos		frame position with the stream
	*/
	int         (*ll_seek)(sfxstream_t *stream, int pos);
	/** Read data from the stream.
		This is a high-level function for use in the mixer. The read samples
		will always at be the correct sample rate for the mixer, reguardless
		of the sample rate in the underlying stream.
		\param stream	"this"
		\param data		destination of read data
		\param frames	maximum number of frames to read from stream
		\return			number of frames read from stream
	*/
	int         (*read)(sfxstream_t *stream, float *data, int frames);
	/** Seek to an absolute position within the stream.
		\param stream	"this"
		\param pos		frame position with the stream
	*/
	int         (*seek)(sfxstream_t *stream, int pos);
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

extern struct cvar_s *snd_volume;

extern snd_render_data_t snd_render_data;
#define PAINTBUFFER_SIZE    512
extern portable_samplepair_t snd_paintbuffer[PAINTBUFFER_SIZE * 2];

///@}

/** \defgroup sound_render_sfx Sound sfx
	\ingroup sound_render_mix
*/
///@{
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
						   long (*read)(void *, float **),
						   int (*seek)(sfxstream_t *, int),
						   void (*close) (sfx_t *));

/** Close a stream.
	\param sfx
*/
void SND_SFX_StreamClose (sfx_t *sfx);

/** Pre-load a sound into the cache.
	\param snd		sound system state
	\param sample	name of sound to precache
*/
sfx_t *SND_PrecacheSound (snd_t *snd, const char *sample);

/** Pre-load a sound.
	\param snd		sound system state
	\param name		name of sound to load
*/
sfx_t *SND_LoadSound (snd_t *snd, const char *name);

/** Initialize the sfx sub-subsystem
	\param snd		sound system state
*/
void SND_SFX_Init (snd_t *snd);

///@}

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
///@{
#define	MAX_CHANNELS			512	//!< number of available mixing channels
#define	MAX_DYNAMIC_CHANNELS	128	//!< number of dynamic channels
#define MAX_STATIC_CHANNELS		256	//!< number of static channels
extern	channel_t   snd_channels[MAX_CHANNELS];	//!< pool of available channels
extern	int			snd_total_channels;	//!< number of active channels

/** Allocate a sound channel that can be used for playing sounds.
	\param snd		sound system state
*/
struct channel_s *SND_AllocChannel (snd_t *snd);

/** Stop a channel from playing.
	\param snd		sound system state
	\param chan the channel to stop
*/
void SND_ChannelStop (snd_t *snd, channel_t *chan);

/** Mark all channels as no longer in use.

	For use by asynchronous output drivers.
*/
void SND_FinishChannels (void);

/** Scan channels looking for stopped channels.
	\param snd		sound system state
	\param wait	if true, wait for the channels to be done. if false, force the
				channels to be done. true is for threaded, false for
				non-threaded.
*/
void SND_ScanChannels (snd_t *snd, int wait);

/** Disable ambient sounds.
	\param snd		sound system state
	\todo not used, remove?
*/
void SND_AmbientOff (snd_t *snd);

/** Enable ambient sounds.
	\param snd		sound system state
	\todo not used, remove?
*/
void SND_AmbientOn (snd_t *snd);

/** Update the sound engine with the client's position and orientation and
	render some sound.
	\param snd		sound system state
	\param origin	3d coords of the client
	\param v_forward 3d vector of the client's facing direction
	\param v_right	3d vector of the client's rightward direction
	\param v_up		3d vector of the client's upward direction
	\param ambient_sound_level Pointer to 4 bytes indicating the levels at
					which to play the ambient sounds.
*/
void SND_SetListener (snd_t *snd, struct transform_s *ear,
					  const byte *ambient_sound_level);

/** Stop all sounds from playing.
	\param snd		sound system state
*/
void SND_StopAllSounds (snd_t *snd);

/** Initialize the channels sub-subsystem
	\param snd		sound system state
*/
void SND_Channels_Init (snd_t *snd);

/** Start a sound playing.
	\param snd		sound system state
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
void SND_StartSound (snd_t *snd, int entnum, int entchannel, sfx_t *sfx,
					 vec4f_t origin, float fvol,  float attenuation);

/** Create a sound generated by the world.
	\param snd		sound system state
	\param sfx		sound to play
	\param origin	3d coords of where the sound originates
	\param vol		absolute volume of the sound
	\param attenuation rate of volume dropoff vs distance
*/
void SND_StaticSound (snd_t *snd, sfx_t *sfx, vec4f_t origin, float vol,
					  float attenuation);
/** Stop an entity's sound.
	\param snd		sound system state
	\param entnum	index of entity the sound is associated with.
	\param entchannel channel to silence
*/
void SND_StopSound (snd_t *snd, int entnum, int entchannel);

/** Start a sound local to the client view.
	\param snd		sound system state
	\param s name of sound to play
*/
void SND_LocalSound (snd_t *snd, const char *s);
///@}


/** \defgroup sound_render_mix Mixer engine.
	\ingroup sound_render
*/
///@{

/** Mix all active channels into the output buffer.
	\param snd		sound system state
	\param endtime	sample time until when to mix
*/
void SND_PaintChannels(snd_t *snd, unsigned int endtime);

/** Initialize the scale table for painting of 8 bit samples.
	\param snd		sound system state
*/
void SND_InitScaletable (snd_t *snd);

/** Set the paint function of the sfxbuffer
	\param sc		sfxbuffer to set.
*/
void SND_SetPaint (sfxbuffer_t *sc);
///@}


/** \defgroup sound_render_resample Resampling functions
	\ingroup sound_render
*/
///@{
/** Set up the various parameters that depend on the actual sample rate.
	\param sc		buffer to setup
	\param streamed	non-zero if this is for a stream.
*/
void SND_SetupResampler (sfxbuffer_t *sc, int streamed);

/** Free memory allocated for the resampler.
	\param stream	stream to pulldown
*/
void SND_PulldownResampler (sfxstream_t *stream);

/** Copy/resample data into buffer, resampling as necessary.
	\param sc		buffer to write resampled sound
	\param data		raw sample data
	\param length	number of frames to resample
*/
void SND_Resample (sfxbuffer_t *sc, float *data, int length);

/** Convert integer sample data to float sample data.
	\param idata	integer data buffer
	\param fdata	float data buffer
	\param frames	number of frames
	\param channels	number of channels per frame
	\param width	bytes per channel
*/
void SND_Convert (byte *idata, float *fdata, int frames,
				  int channels, int width);
///@}


/** \defgroup sound_render_load Sound loading functions
	\ingroup sound_render
*/
///@{
/** Load the referenced sound.
	\param sfx		sound reference
	\return			0 if ok, -1 on error
*/
int SND_Load (sfx_t *sfx);

/** Load the referenced sound from the specified Ogg file.
	\param file		pre-opened Ogg file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
	\return			0 if ok, -1 on error
*/
int SND_LoadOgg (QFile *file, sfx_t *sfx, char *realname);

/** Load the referenced sound from the specified FLAC file.
	\param file		pre-opened FLAC file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
	\return			0 if ok, -1 on error
*/
int SND_LoadFLAC (QFile *file, sfx_t *sfx, char *realname);

/** Load the referenced sound from the specified WAV file.
	\param file		pre-opened WAV file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
	\return			0 if ok, -1 on error
*/
int SND_LoadWav (QFile *file, sfx_t *sfx, char *realname);

/** Load the referenced sound from the specified MIDI file.
	\param file		pre-opened MIDI file
	\param sfx		sound reference
	\param realname	path of sound file should it need to be re-opened
	\return			0 if ok, -1 on error
*/
int SND_LoadMidi (QFile *file, sfx_t *sfx, char *realname);
///@}

/** \defgroup sound_render_cache_stream Cache/Stream Functions.
	\ingroup sound_render
*/
///@{
/** Retrieve wavinfo from a cached sound.
	\param sfx		sound reference
	\return			pointer to sound's wavinfo
*/
wavinfo_t *SND_CacheWavinfo (sfx_t *sfx) __attribute__((pure));

/** Retrieve wavinfo from a streamed sound.
	\param sfx		sound reference
	\return			pointer to sound's wavinfo
*/
wavinfo_t *SND_StreamWavinfo (sfx_t *sfx) __attribute__((pure));

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
sfxbuffer_t *SND_CacheGetBuffer (sfx_t *sfx) __attribute__((pure));

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
sfxbuffer_t *SND_StreamGetBuffer (sfx_t *sfx) __attribute__((pure));

/** Lock a streamed sound into memory. Doesn't actually do anything other than
	return a pointer to the buffer.
	\param sfx		sound reference
	\return			poitner to sound buffer
*/
sfxbuffer_t *SND_StreamRetain (sfx_t *sfx) __attribute__((pure));

/** Unlock a streamed sound from memory. Doesn't actually do anything.
	\param sfx		sound reference
*/
void SND_StreamRelease (sfx_t *sfx);

/** Advance the position with the stream, updating the ring buffer as
	necessary. Null for chached sounds.
	\param buffer	"this"
	\param count	number of samples to advance
*/
int SND_StreamAdvance (sfxbuffer_t *buffer, unsigned int count);

/** Seek to an absolute position within the stream, resetting the ring
	buffer.
	\param buffer	"this"
	\param pos		sample position with the stream
*/
void SND_StreamSetPos (sfxbuffer_t *buffer, unsigned int pos);

/** Allocate a sound buffer from cache for cached sounds.
	\param samples	size in samples
	\param rate		sample rate
	\param channels	number of channels in input data
	\param block	cached sound descriptor to initialize
	\param allocator cache allocator function
	\return			pointer to sound sample buffer (setup for block mode)
*/
sfxbuffer_t *SND_GetCache (long samples, int rate, int channels,
						   sfxblock_t *block, cache_allocator_t allocator);
///@}

#endif//__snd_internal_h
