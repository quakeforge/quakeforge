/*
	snd_dma.c

	main control for any streaming sound output device

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
		Boston, MA	02111-1307, USA

*/
static const char rcsid[] = 
	"$Id$";

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

#ifdef _WIN32
# include "winquake.h"
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/model.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/sound.h"
#include "QF/plugin.h"

void		SND_Play (void);
void		SND_PlayVol (void);
void		SND_SoundList (void);
void		SND_StopAllSounds (qboolean clear);
void		SND_StopAllSoundsC (void);
void		SND_Update_ (void);

sfx_t		*SND_PrecacheSound (const char *name);
void		SND_ClearBuffer (void);
void		SND_PaintChannels (int endtime);
void		SND_CallbackLoad (void *object, cache_allocator_t allocator);

void		SND_Init_Cvars ();

// Internal sound data & structures ===========================================

extern channel_t   channels[MAX_CHANNELS];
extern int		   total_channels;
//extern double host_frametime; // From host.h

int			snd_blocked = 0;
static qboolean snd_ambient = 1;
extern qboolean    snd_initialized;

// pointer should go away
extern volatile dma_t *shm;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist = 1000.0;

int			soundtime;					// sample PAIRS
extern int	paintedtime;				// sample PAIRS

#define	MAX_SFX		512
sfx_t	   *known_sfx;					// hunk allocated [MAX_SFX]
int			num_sfx;

sfx_t	   *ambient_sfx[NUM_AMBIENTS];

int			sound_started = 0;

extern cvar_t	  *snd_loadas8bit;
extern cvar_t	  *snd_interp;
extern cvar_t	  *bgmvolume;
extern cvar_t	  *volume;

cvar_t	   *ambient_fade;
cvar_t	   *ambient_level;
cvar_t	   *nosound;
cvar_t	   *precache;
cvar_t	   *snd_mixahead;
cvar_t	   *snd_noextraupdate;
cvar_t	   *snd_phasesep;
cvar_t	   *snd_show;
cvar_t	   *snd_volumesep;

static plugin_t					plugin_info;
static plugin_data_t			plugin_info_data;
static plugin_funcs_t			plugin_info_funcs;
static general_data_t			plugin_info_general_data;
static general_funcs_t			plugin_info_general_funcs;
static snd_render_data_t		plugin_info_snd_render_data;
static snd_render_funcs_t		plugin_info_snd_render_funcs;


// User-setable variables =====================================================

// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times SND_Update() is called per second.

qboolean	fakedma = false;
int			fakedma_updates = 15;


void
SND_AmbientOff (void)
{
	snd_ambient = false;
}

void
SND_AmbientOn (void)
{
	snd_ambient = true;
}

void
SND_SoundInfo_f (void)
{
	if (!sound_started || !shm) {
		Sys_Printf ("sound system not started\n");
		return;
	}

	Sys_Printf ("%5d stereo\n", shm->channels - 1);
	Sys_Printf ("%5d samples\n", shm->samples);
	Sys_Printf ("%5d samplepos\n", shm->samplepos);
	Sys_Printf ("%5d samplebits\n", shm->samplebits);
	Sys_Printf ("%5d submission_chunk\n", shm->submission_chunk);
	Sys_Printf ("%5d speed\n", shm->speed);
	Sys_Printf ("0x%lx dma buffer\n", (unsigned long) shm->buffer);
	Sys_Printf ("%5d total_channels\n", total_channels);
}

void
SND_Startup (void)
{
	int			rc;

	if (!snd_initialized)
		return;

	if (!fakedma) {
		rc = S_O_Init ();

		if (!rc) {
#ifndef	_WIN32
			Sys_Printf ("S_Startup: S_O_Init failed.\n");
#endif
			sound_started = 0;
			return;
		}
	}

	sound_started = 1;
}

void
SND_Init (void)
{
	Sys_Printf ("\nSound Initialization\n");

	Cmd_AddCommand ("play", SND_Play,
					"Play selected sound effect (play pathto/sound.wav)");
	Cmd_AddCommand ("playvol", SND_PlayVol, "Play selected sound effect at "
					"selected volume (playvol pathto/sound.wav num");
	Cmd_AddCommand ("stopsound", SND_StopAllSoundsC,
					"Stops all sounds currently being played");
	Cmd_AddCommand ("soundlist", SND_SoundList,
					"Reports a list of sounds in the cache");
	Cmd_AddCommand ("soundinfo", SND_SoundInfo_f,
					"Report information on the sound system");

	SND_Init_Cvars ();

	if (COM_CheckParm ("-nosound"))
		return;

	if (COM_CheckParm ("-simsound"))
		fakedma = true;

// FIXME
//	if (host_parms.memsize < 0x800000) {
//		Cvar_Set (snd_loadas8bit, "1");
//		Sys_Printf ("loading all sounds as 8bit\n");
//	}

	snd_initialized = true;

	SND_Startup ();

	if (sound_started == 0)				// sound startup failed? Bail out.
		return;

	SND_InitScaletable ();

	known_sfx = Hunk_AllocName (MAX_SFX * sizeof (sfx_t), "sfx_t");

	num_sfx = 0;

	// create a piece of DMA memory
	if (fakedma) {
		shm = (void *) Hunk_AllocName (sizeof (*shm), "shm");
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = 22050;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = Hunk_AllocName (1 << 16, "shmbuf");
	}
//	Sys_Printf ("Sound sampling rate: %i\n", shm->speed);

	// provides a tick sound until washed clean

//	if (shm->buffer)
//		shm->buffer[4] = shm->buffer[5] = 0x7f; // force a pop for debugging

	ambient_sfx[AMBIENT_WATER] = SND_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = SND_PrecacheSound ("ambience/wind2.wav");

	SND_StopAllSounds (true);
}

void
SND_Init_Cvars (void)
{
	ambient_fade = Cvar_Get ("ambient_fade", "100", CVAR_NONE, NULL,
							 "How quickly ambient sounds fade in or out");
	ambient_level = Cvar_Get ("ambient_level", "0.3", CVAR_NONE, NULL,
							  "Ambient sounds' volume");
	nosound = Cvar_Get ("nosound", "0", CVAR_NONE, NULL,
						"Set to turn sound off");
	precache = Cvar_Get ("precache", "1", CVAR_NONE, NULL,
						 "Toggle the use of a precache");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, NULL,
						  "Volume of CD music");
	volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
					   "Set the volume for sound playback");
	snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
						   "control sample interpolation");
	snd_loadas8bit = Cvar_Get ("snd_loadas8bit", "0", CVAR_NONE, NULL,
							   "Toggles loading sounds as 8-bit samples");
	snd_mixahead = Cvar_Get ("snd_mixahead", "0.1", CVAR_ARCHIVE, NULL,
							  "Delay time for sounds");
	snd_noextraupdate = Cvar_Get ("snd_noextraupdate", "0", CVAR_NONE, NULL,
								  "Toggles the correct value display in "
								  "host_speeds. Usually messes up sound "
								  "playback when in effect");
	snd_phasesep = Cvar_Get ("snd_phasesep", "0.0", CVAR_ARCHIVE, NULL,
							 "max stereo phase separation in ms. 0.6 is for "
							 "20cm head");
	snd_show = Cvar_Get ("snd_show", "0", CVAR_NONE, NULL,
						 "Toggles display of sounds currently being played");
	snd_volumesep = Cvar_Get ("snd_volumesep", "1.0", CVAR_ARCHIVE, NULL,
							  "max stereo volume separation. 1.0 is max");
}

// Shutdown sound engine ======================================================
void
SND_Shutdown (void)
{

	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = 0;

	sound_started = 0;

	if (!fakedma) {
		S_O_Shutdown ();
	}

	shm = 0;
}

// Load a sound ===============================================================
sfx_t *
SND_FindName (const char *name)
{
	int			i;
	sfx_t	   *sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL\n");

	if (strlen (name) >= MAX_QPATH)
		Sys_Error ("Sound name too long: %s", name);

	// see if already loaded
	for (i = 0; i < num_sfx; i++)
		if (!strcmp (known_sfx[i].name, name)) {
			return &known_sfx[i];
		}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy (sfx->name, name);
	Cache_Add (&sfx->cache, sfx, SND_CallbackLoad);

	num_sfx++;

	return sfx;
}

void
SND_TouchSound (const char *name)
{
	sfx_t	   *sfx;

	if (!sound_started)
		return;

	sfx = SND_FindName (name);
	Cache_Check (&sfx->cache);
}

sfx_t *
SND_PrecacheSound (const char *name)
{
	sfx_t	   *sfx;

	if (!sound_started || nosound->int_val)
		return NULL;

	sfx = SND_FindName (name);

	// cache it in
	if (precache->int_val)
		if (Cache_TryGet (&sfx->cache))
			Cache_Release (&sfx->cache);

	return sfx;
}

//=============================================================================

channel_t *
SND_PickChannel (int entnum, int entchannel)
{
	int			ch_idx, first_to_die, life_left;

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS;
		 ch_idx++) {
		if (entchannel != 0				// channel 0 never overrides
			&& channels[ch_idx].entnum == entnum
			&& (channels[ch_idx].entchannel == entchannel ||
				entchannel == -1)) {
			// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}
		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == *plugin_info_snd_render_data.viewentity
			&& entnum != *plugin_info_snd_render_data.viewentity
			&& channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left) {
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

	return &channels[first_to_die];
}

void
SND_Spatialize (channel_t *ch)
{
	int			phase;					// in samples
	vec_t		dist, dot, lscale, rscale, scale;
	vec3_t		source_vec;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == *plugin_info_snd_render_data.viewentity) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		ch->phase = 0;
		return;
	}
	// calculate stereo seperation and distance attenuation

	VectorSubtract (ch->origin, listener_origin, source_vec);

	dist = VectorNormalize (source_vec) * ch->dist_mult;

	dot = DotProduct (listener_right, source_vec);

	if (shm->channels == 1) {
		rscale = 1.0;
		lscale = 1.0;
		phase = 0;
	} else {
		rscale = 1.0 + dot * snd_volumesep->value;
		lscale = 1.0 - dot * snd_volumesep->value;
		phase = snd_phasesep->value * 0.001 * shm->speed * dot;
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;

	ch->phase = phase;
}

// Start a sound effect =======================================================
void
SND_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin,
				float fvol, float attenuation)
{
	int			ch_idx, skip, vol;
	channel_t  *target_chan, *check;
	sfxcache_t *sc;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound->int_val)
		return;

	vol = fvol * 255;

	// pick a channel to play on
	target_chan = SND_PickChannel (entnum, entchannel);
	if (!target_chan)
		return;

	// spatialize
	memset (target_chan, 0, sizeof (*target_chan));
	VectorCopy (origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize (target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;							// not audible at all

	// new channel
	sc = Cache_TryGet (&sfx->cache);
	if (!sc) {
		target_chan->sfx = NULL;
		return;							// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + sc->length;
	Cache_Release (&sfx->cache);

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS;
		 ch_idx++, check++) {
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos) {
			skip = rand () % (int) (0.1 * shm->speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}

void
SND_StopSound (int entnum, int entchannel)
{
	int			i;

	if (!sound_started)
		return;

	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++) {
		if (channels[i].entnum == entnum
			&& channels[i].entchannel == entchannel) {
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void
SND_StopAllSounds (qboolean clear)
{
	int			i;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i = 0; i < MAX_CHANNELS; i++)
		if (channels[i].sfx)
			channels[i].sfx = NULL;

	memset (channels, 0, MAX_CHANNELS * sizeof (channel_t));

	if (clear)
		SND_ClearBuffer ();
}

void
SND_StopAllSoundsC (void)
{
	SND_StopAllSounds (true);
}

void
SND_ClearBuffer (void)
{
	int			clear;


//#ifdef _WIN32
//	if (!sound_started || !shm || (!shm->buffer && !pDSBuf))
//#endif

	if (!sound_started || !shm || !shm->buffer)
//#endif
		return;

	if (shm->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

#if 0
#ifdef _WIN32
	if (pDSBuf)
		DSOUND_ClearBuffer (clear);
	else
#endif
#endif
	{
		memset (shm->buffer, clear, shm->samples * shm->samplebits / 8);
	}
}

void
SND_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t  *ss;
	sfxcache_t *sc;

	if (!sound_started || !sfx)
		return;

	if (total_channels == MAX_CHANNELS) {
		Sys_Printf ("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = Cache_TryGet (&sfx->cache);
	if (!sc)
		return;

	if (sc->loopstart == -1) {
		Sys_Printf ("Sound %s not looped\n", sfx->name);
		Cache_Release (&sfx->cache);
		return;
	}

	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation / 64) / sound_nominal_clip_dist;
	ss->end = paintedtime + sc->length;
	Cache_Release (&sfx->cache);

	SND_Spatialize (ss);
	ss->oldphase = ss->phase;
}

//=============================================================================

void
SND_UpdateAmbientSounds (void)
{
	float		vol;
	int			ambient_channel;
	channel_t  *chan;
	mleaf_t    *l;

	if (!snd_ambient)
		return;

	// calc ambient sound levels
	if (!**plugin_info_snd_render_data.worldmodel) // FIXME: eww
		return;

	l = Mod_PointInLeaf (listener_origin,
						 **plugin_info_snd_render_data.worldmodel);
	if (!l || !ambient_level->value) {
		for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS;
			 ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS;
		 ambient_channel++) {
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = ambient_level->value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol) {
			chan->master_vol += *plugin_info_snd_render_data.host_frametime
				* ambient_fade->value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		} else if (chan->master_vol > vol) {
			chan->master_vol -= *plugin_info_snd_render_data.host_frametime
				* ambient_fade->value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

/*
	SND_Update

	Called once each time through the main loop
*/
void
SND_Update (vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			total, i, j;
	channel_t  *ch, *combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy (origin, listener_origin);
	VectorCopy (forward, listener_forward);
	VectorCopy (right, listener_right);
	VectorCopy (up, listener_up);

	// update general area ambient sound sources
	SND_UpdateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds	
	ch = channels + NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i < total_channels; i++, ch++) {
		if (!ch->sfx)
			continue;
		ch->oldphase = ch->phase;		// prepare to lerp from prev to next
										// phase
		SND_Spatialize (ch);			// respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame
		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS) {
			// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx) {
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
			// search for one
			combine = channels + MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++,
					 combine++)
				if (combine->sfx == ch->sfx)
					break;

			if (j == total_channels) {
				combine = NULL;
			} else {
				if (combine != ch) {
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
	}

	// debugging output
	if (snd_show->int_val) {
		total = 0;
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol)) {
				// Sys_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol,
				// ch->sfx->name);
				total++;
			}

		Sys_Printf ("----(%i)----\n", total);
	}

	// mix some sound
	SND_Update_ ();
}

void
SND_GetSoundtime (void)
{
	int			fullsamples, samplepos;
	static int	buffers, oldsamplepos;

	fullsamples = shm->samples / shm->channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to SND_Update.  Oh well.
	samplepos = S_O_GetDMAPos ();

	if (samplepos < oldsamplepos) {
		buffers++;						// buffer wrapped

		if (paintedtime > 0x40000000) {	// time to chop things off to avoid
			// 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			SND_StopAllSounds (true);
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / shm->channels;
}

void
SND_ExtraUpdate (void)
{
	if (!sound_started || snd_noextraupdate->int_val)
		return;							// don't pollute timings
	SND_Update_ ();
}

void
SND_Update_ (void)
{
	int				samps;
	unsigned int	endtime;

	if (!sound_started || (snd_blocked > 0))
		return;

	// Updates DMA time
	SND_GetSoundtime ();

	// check to make sure that we haven't overshot
	if (paintedtime < soundtime) {
//		Sys_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}
	// mix ahead of current position
	endtime = soundtime + snd_mixahead->value * shm->speed;
	samps = shm->samples >> (shm->channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

#if 0
#ifdef _WIN32
	if (pDSBuf)
		DSOUND_Restore ();
#endif
#endif

	SND_PaintChannels (endtime);
	S_O_Submit ();
}

/* console functions */

void
SND_Play (void)
{
	char		name[256];
	int			i;
	static int	hash = 345;
	sfx_t	   *sfx;

	i = 1;
	while (i < Cmd_Argc ()) {
		if (!strrchr (Cmd_Argv (i), '.')) {
			strcpy (name, Cmd_Argv (i));
			strncat (name, ".wav", sizeof (name) - strlen (name));
		} else
			strcpy (name, Cmd_Argv (i));
		sfx = SND_PrecacheSound (name);
		SND_StartSound (hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

void
SND_PlayVol (void)
{
	char		name[256];
	float		vol;
	int			i;
	static int	hash = 543;
	sfx_t	   *sfx;

	i = 1;
	while (i < Cmd_Argc ()) {
		if (!strrchr (Cmd_Argv (i), '.')) {
			strcpy (name, Cmd_Argv (i));
			strncat (name, ".wav", sizeof (name) - strlen (name));
		} else
			strcpy (name, Cmd_Argv (i));
		sfx = SND_PrecacheSound (name);
		vol = atof (Cmd_Argv (i + 1));
		SND_StartSound (hash++, 0, sfx, listener_origin, vol, 1.0);
		i += 2;
	}
}

void
SND_SoundList (void)
{
	int			load, size, total, i;
	sfx_t	   *sfx;
	sfxcache_t *sc;

	if (Cmd_Argc() >= 2 && Cmd_Argv (1)[0])
		load = 1;
	else
		load = 0;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++) {
		if (load)
			sc = Cache_TryGet (&sfx->cache);
		else
			sc = Cache_Check (&sfx->cache);

		if (!sc)
			continue;
		size = sc->length * sc->width * (sc->stereo + 1);
		total += size;
		if (sc->loopstart >= 0)
			Sys_Printf ("L");
		else
			Sys_Printf (" ");
		Sys_Printf ("(%2db) %6i : %s\n", sc->width * 8, size, sfx->name);

		if (load)
			Cache_Release (&sfx->cache);
	}
	Sys_Printf ("Total resident: %i\n", total);
}

void
SND_LocalSound (const char *sound)
{
	sfx_t	   *sfx;

	if (!sound_started)
		return;
	if (nosound->int_val)
		return;

	sfx = SND_PrecacheSound (sound);
	if (!sfx) {
		Sys_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	SND_StartSound (*plugin_info_snd_render_data.viewentity, -1, sfx,
					vec3_origin, 1, 1);
}

void
SND_ClearPrecache (void)
{
}

void
SND_BeginPrecaching (void)
{
}

void
SND_EndPrecaching (void)
{
}

void
SND_BlockSound (void)
{
	if (++snd_blocked == 1)
		S_O_BlockSound ();
}

void
SND_UnblockSound (void)
{
	if (!snd_blocked)
		return;
	if (!--snd_blocked)
		S_O_UnblockSound ();
}

QFPLUGIN plugin_t *
snd_render_default_PluginInfo (void)
{
	plugin_info.type = qfp_snd_render;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "Sound Renderer";
	plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
		"project\n"
		"Please see the file \"AUTHORS\" for a list of contributors";
	plugin_info.functions = &plugin_info_funcs;
	plugin_info.data = &plugin_info_data;

	plugin_info_data.general = &plugin_info_general_data;
	plugin_info_data.input = NULL;
	plugin_info_data.snd_render = &plugin_info_snd_render_data;

	plugin_info_snd_render_data.soundtime = &soundtime;
	plugin_info_snd_render_data.paintedtime = &paintedtime;

	plugin_info_funcs.general = &plugin_info_general_funcs;
	plugin_info_funcs.input = NULL;
	plugin_info_funcs.snd_render = &plugin_info_snd_render_funcs;

	plugin_info_general_funcs.p_Init = SND_Init;
	plugin_info_general_funcs.p_Shutdown = SND_Shutdown;

	plugin_info_snd_render_funcs.pS_AmbientOff = SND_AmbientOff;
	plugin_info_snd_render_funcs.pS_AmbientOn = SND_AmbientOn;
	plugin_info_snd_render_funcs.pS_TouchSound = SND_TouchSound;
	plugin_info_snd_render_funcs.pS_ClearBuffer = SND_ClearBuffer;
	plugin_info_snd_render_funcs.pS_StaticSound = SND_StaticSound;
	plugin_info_snd_render_funcs.pS_StartSound = SND_StartSound;
	plugin_info_snd_render_funcs.pS_StopSound = SND_StopSound;
	plugin_info_snd_render_funcs.pS_PrecacheSound = SND_PrecacheSound;
	plugin_info_snd_render_funcs.pS_ClearPrecache = SND_ClearPrecache;
	plugin_info_snd_render_funcs.pS_Update = SND_Update;
	plugin_info_snd_render_funcs.pS_StopAllSounds = SND_StopAllSounds;
	plugin_info_snd_render_funcs.pS_BeginPrecaching = SND_BeginPrecaching;
	plugin_info_snd_render_funcs.pS_EndPrecaching = SND_EndPrecaching;
	plugin_info_snd_render_funcs.pS_ExtraUpdate = SND_ExtraUpdate;
	plugin_info_snd_render_funcs.pS_LocalSound = SND_LocalSound;
	plugin_info_snd_render_funcs.pS_BlockSound = SND_BlockSound;
	plugin_info_snd_render_funcs.pS_UnblockSound = SND_UnblockSound;

	return &plugin_info;
}

