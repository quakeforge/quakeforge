/*
	sound.h

	Sound headers.

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

	$Id$
*/
// sound.h -- client sound i/o functions

#ifndef _SOUND_H
#define _SOUND_H

#include "QF/mathlib.h"

#define AMBIENT_WATER	0
#define AMBIENT_SKY		1
#define AMBIENT_SLIME	2
#define AMBIENT_LAVA	3
#define NUM_AMBIENTS	4	// automatic ambient sounds

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

typedef struct sfx_s sfx_t;
struct sfx_s
{
	const char *name;

	unsigned int length;
	unsigned int loopstart;

	void       *data;

	struct sfxbuffer_s *(*touch) (sfx_t *sfx);
	struct sfxbuffer_s *(*retain) (sfx_t *sfx);
	struct wavinfo_s *(*wavinfo) (sfx_t *sfx);
	sfx_t      *(*open) (sfx_t *sfx);
	void        (*close) (sfx_t *sfx);
	void        (*release) (sfx_t *sfx);
};

typedef struct dma_s {
	qboolean		gamealive;
	qboolean		soundalive;
	qboolean		splitbuffer;
	int				channels;
	int				samples;				// mono samples in buffer
	int				submission_chunk;		// don't mix less than this #
	int				samplepos;				// in mono samples
	int				samplebits;
	int				speed;
	unsigned char	*buffer;
} dma_t;

struct model_s;
void S_Init (struct model_s **worldmodel, int *viewentity,
			 double *host_frametime);
void S_Init_Cvars (void);
void S_Startup (void);
void S_Shutdown (void);
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, const vec3_t origin,
				   float fvol,  float attenuation);
void S_StaticSound (sfx_t *sfx, const vec3_t origin, float vol,
					float attenuation);
void S_StopSound (int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_Update (const vec3_t origin, const vec3_t v_forward,
			   const vec3_t v_right, const vec3_t v_up);
void S_ExtraUpdate (void);
void S_BlockSound (void);
void S_UnblockSound (void);

sfx_t *S_PrecacheSound (const char *sample);
void S_TouchSound (const char *sample);
void S_ClearPrecache (void);
void S_BeginPrecaching (void);
void S_EndPrecaching (void);

sfx_t *S_LoadSound (const char *name);
struct channel_s *S_AllocChannel (void);

// ====================================================================
// User-setable variables
// ====================================================================

#define	MAX_CHANNELS			256
#define	MAX_DYNAMIC_CHANNELS	8

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern unsigned paintedtime;

extern	struct cvar_s *snd_loadas8bit;
extern	struct cvar_s *volume;

extern	struct cvar_s	*snd_interp;
extern	struct cvar_s *snd_stereo_phase_separation;

void S_LocalSound (const char *s);

void S_AmbientOff (void);
void S_AmbientOn (void);

extern struct model_s **snd_worldmodel;

#endif // _SOUND_H
