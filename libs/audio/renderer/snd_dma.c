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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "winquake.h"

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/model.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/sound.h"
#include "QF/plugin.h"
#include "QF/va.h"
#include "QF/quakefs.h"

#include "snd_render.h"

// Internal sound data & structures ===========================================

volatile dma_t *shm = 0;
unsigned int    paintedtime;				// sample PAIRS

cvar_t         *snd_loadas8bit;
cvar_t         *volume;
cvar_t         *snd_interp;

channel_t       channels[MAX_CHANNELS];
int             total_channels;

static channel_t *ambient_channels[NUM_AMBIENTS];
static channel_t *dynamic_channels[MAX_DYNAMIC_CHANNELS];
static channel_t *static_channels[MAX_CHANNELS];
static int        num_statics;

static qboolean snd_initialized = false;
static int      snd_blocked = 0;
static qboolean snd_ambient = 1;

static vec3_t   listener_origin;
static vec3_t   listener_forward;
static vec3_t   listener_right;
static vec3_t   listener_up;
static vec_t    sound_nominal_clip_dist = 1000.0;

static unsigned soundtime;					// sample PAIRS

#define	MAX_SFX		512
static sfx_t   *known_sfx;					// hunk allocated [MAX_SFX]
static int      num_sfx;

static sfx_t   *ambient_sfx[NUM_AMBIENTS];

static int      sound_started = 0;


static cvar_t  *ambient_fade;
static cvar_t  *ambient_level;
static cvar_t  *nosound;
static cvar_t  *precache;
static cvar_t  *snd_mixahead;
static cvar_t  *snd_noextraupdate;
static cvar_t  *snd_phasesep;
static cvar_t  *snd_show;
static cvar_t  *snd_volumesep;

static general_data_t plugin_info_general_data;
static snd_render_data_t render_data = {
	0,
	0,
	0,
	&soundtime,
	&paintedtime,
	0,
};

static snd_output_funcs_t *snd_output_funcs;


// User-setable variables =====================================================

// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times s_update() is called per second.

static qboolean	fakedma = false;
//static int      fakedma_updates = 15;


static void
s_ambient_off (void)
{
	snd_ambient = false;
}

static void
s_ambient_on (void)
{
	snd_ambient = true;
}

static void
s_soundinfo_f (void)
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

static void
s_startup (void)
{
	if (!snd_initialized)
		return;

	if (!fakedma) {
		shm = snd_output_funcs->pS_O_Init ();

		if (!shm) {
			Sys_Printf ("S_Startup: S_O_Init failed.\n");
			sound_started = 0;
			return;
		}
	}

	sound_started = 1;
}

// Load a sound ===============================================================
static sfx_t *
s_load_sound (const char *name)
{
	int			i;
	sfx_t	   *sfx;

	if (!known_sfx)
		return 0;

	// see if already loaded
	for (i = 0; i < num_sfx; i++)
		if (known_sfx[i].name && !strcmp (known_sfx[i].name, name)) {
			return &known_sfx[i];
		}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	if (sfx->name)
		free ((char *) sfx->name);
	sfx->name = strdup (name);
	SND_Load (sfx);

	num_sfx++;

	return sfx;
}

static void
s_touch_sound (const char *name)
{
	sfx_t	   *sfx;

	if (!sound_started)
		return;

	if (!name)
		Sys_Error ("s_touch_sound: NULL");

	name = va ("sound/%s", name);
	sfx = s_load_sound (name);
	sfx->touch (sfx);
}

static sfx_t *
s_precache_sound (const char *name)
{
	sfx_t	   *sfx;

	if (!sound_started || nosound->int_val)
		return NULL;

	if (!name)
		Sys_Error ("s_precache_sound: NULL");

	name = va ("sound/%s", name);
	sfx = s_load_sound (name);

	// cache it in
	if (precache->int_val) {
		if (sfx->retain (sfx))
			sfx->release (sfx);
	}
	return sfx;
}

//=============================================================================

static channel_t *
s_alloc_channel (void)
{
	if (total_channels < MAX_CHANNELS)
		return &channels[total_channels++];
	return 0;
}

static channel_t *
s_pick_channel (int entnum, int entchannel)
{
	int			ch_idx;
	unsigned    life_left;
	channel_t  *ch, *first_to_die;

	// Check for replacement sound, or find the best one to replace
	first_to_die = 0;
	life_left = 0x7fffffff;
	for (ch_idx = 0; ch_idx < MAX_DYNAMIC_CHANNELS; ch_idx++) {
		ch = dynamic_channels[ch_idx];
		if (entchannel != 0				// channel 0 never overrides
			&& ch->entnum == entnum
			&& (ch->entchannel == entchannel || entchannel == -1)) {
			// always override sound from same entity
			first_to_die = ch;
			break;
		}
		// don't let monster sounds override player sounds
		if (ch->entnum == *render_data.viewentity
			&& entnum != *render_data.viewentity
			&& ch->sfx)
			continue;

		if (paintedtime + life_left > ch->end) {
			life_left = ch->end - paintedtime;
			first_to_die = ch;
		}
	}

	if (!first_to_die)
		return NULL;

	if (first_to_die->sfx) {
		first_to_die->sfx->close (first_to_die->sfx);
		first_to_die->sfx = NULL;
	}

	return first_to_die;
}

static void
s_spatialize (channel_t *ch)
{
	int			phase;					// in samples
	vec_t		dist, dot, lscale, rscale, scale;
	vec3_t		source_vec;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == *render_data.viewentity) {
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
static void
s_start_sound (int entnum, int entchannel, sfx_t *sfx, const vec3_t origin,
				float fvol, float attenuation)
{
	int			 ch_idx, vol;
	unsigned int skip;
	channel_t   *target_chan, *check;

	if (!sound_started)
		return;
	if (!sfx)
		return;
	if (nosound->int_val)
		return;
	// pick a channel to play on
	target_chan = s_pick_channel (entnum, entchannel);
	if (!target_chan)
		return;

	vol = fvol * 255;

	// spatialize
	memset (target_chan, 0, sizeof (*target_chan));
	VectorCopy (origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	s_spatialize (target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;							// not audible at all

	// new channel
	if (!sfx->retain (sfx)) {
		if (target_chan->sfx)
			target_chan->sfx->close (target_chan->sfx);
		target_chan->sfx = NULL;
		return;						// couldn't load the sound's data
	}

	if (!(target_chan->sfx = sfx->open (sfx))) {
		sfx->release (sfx);
		return;
	}
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + target_chan->sfx->length;
	sfx->release (sfx);

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	for (ch_idx = 0; ch_idx < MAX_DYNAMIC_CHANNELS; ch_idx++) {
		check = dynamic_channels[ch_idx];
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

static void
s_stop_sound (int entnum, int entchannel)
{
	int			i;
	channel_t  *ch;

	if (!sound_started)
		return;

	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++) {
		ch = dynamic_channels[i];
		if (ch->entnum == entnum && ch->entchannel == entchannel) {
			ch->end = 0;
			if (ch->sfx)
				ch->sfx->close (ch->sfx);
			ch->sfx = NULL;
			return;
		}
	}
}

static void
s_clear_buffer (void)
{
	int			clear, i;

	if (!sound_started || !shm || !shm->buffer)
		return;

	if (shm->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	for (i = 0; i < shm->samples * shm->samplebits / 8; i++)
		shm->buffer[i] = 0;
}

static void
s_stop_all_sounds (qboolean clear)
{
	int			i;

	if (!sound_started)
		return;

	num_statics = 0;

	for (i = 0; i < MAX_CHANNELS; i++)
		if (channels[i].sfx) {
			channels[i].sfx->close (channels[i].sfx);
			channels[i].sfx = NULL;
		}

	memset (channels, 0, MAX_CHANNELS * sizeof (channel_t));

	if (clear)
		s_clear_buffer ();
}

static void
s_stop_all_sounds_f (void)
{
	s_stop_all_sounds (true);
}

static void
s_static_sound (sfx_t *sfx, const vec3_t origin, float vol,
				 float attenuation)
{
	channel_t  *ss;

	if (!sound_started || !sfx)
		return;

	if (!static_channels[num_statics]) {
		if (!(static_channels[num_statics] = s_alloc_channel ())) {
			Sys_Printf ("ran out of channels\n");
			return;
		}
	}

	ss = static_channels[num_statics];

	if (!sfx->retain (sfx))
		return;

	if (sfx->loopstart == (unsigned int) -1) {
		Sys_Printf ("Sound %s not looped\n", sfx->name);
		sfx->release (sfx);
		return;
	}

	if (!(ss->sfx = sfx->open (sfx))) {
		sfx->release (sfx);
		return;
	}
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation / 64) / sound_nominal_clip_dist;
	ss->end = paintedtime + sfx->length;
	sfx->release (sfx);

	s_spatialize (ss);
	ss->oldphase = ss->phase;
	num_statics++;
}

//=============================================================================

static void
s_updateAmbientSounds (void)
{
	float		vol;
	int			ambient_channel;
	channel_t  *chan;
	mleaf_t    *l;

	if (!snd_ambient)
		return;
	// calc ambient sound levels
	if (!*render_data.worldmodel)
		return;

	l = Mod_PointInLeaf (listener_origin, *render_data.worldmodel);
	if (!l || !ambient_level->value) {
		for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS;
			 ambient_channel++) {
			chan = ambient_channels[ambient_channel];
			if (chan->sfx)
				chan->sfx->close (chan->sfx);
			chan->sfx = NULL;
		}
		return;
	}

	for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS;
		 ambient_channel++) {
		chan = ambient_channels[ambient_channel];
		if (!ambient_sfx[ambient_channel]) {
			chan->sfx = 0;
			continue;
		} else if (!chan->sfx)
			chan->sfx = ambient_sfx[ambient_channel]->open (ambient_sfx[ambient_channel]);

		vol = ambient_level->value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol) {
			chan->master_vol += *render_data.host_frametime
				* ambient_fade->value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		} else if (chan->master_vol > vol) {
			chan->master_vol -= *render_data.host_frametime
				* ambient_fade->value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

static void
s_get_soundtime (void)
{
	int			fullsamples, samplepos;
	static int	buffers, oldsamplepos;

	fullsamples = shm->samples / shm->channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to s_update.  Oh well.
	if ((samplepos = snd_output_funcs->pS_O_GetDMAPos ()) == -1)
		return;
	
	if (samplepos < oldsamplepos) {
		buffers++;						// buffer wrapped

		if (paintedtime > 0x40000000) {	// time to chop things off to avoid
			// 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			s_stop_all_sounds (true);
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / shm->channels;
}

static void
s_update_ (void)
{
	unsigned int	endtime, samps;

	if (!sound_started || (snd_blocked > 0))
		return;

	// Updates DMA time
	s_get_soundtime ();

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

	SND_PaintChannels (endtime);
	snd_output_funcs->pS_O_Submit ();
}

static inline int
s_update_channel (channel_t *ch)
{
	if (!ch->sfx)
		return 0;
	ch->oldphase = ch->phase;		// prepare to lerp from prev to next phase
	s_spatialize (ch);				// respatialize channel
	if (!ch->leftvol && !ch->rightvol)
		return 0;
	return 1;
}

/*
	s_update

	Called once each time through the main loop
*/
static void
s_update (const vec3_t origin, const vec3_t forward, const vec3_t right,
			const vec3_t up)
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
	s_updateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds	
	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
		s_update_channel (dynamic_channels[i]);

	for (i = 0; i < num_statics; i++) {
		ch = static_channels[i];
		if (!s_update_channel (ch))
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame
		// see if it can just use the last one
		if (combine && combine->sfx == ch->sfx) {
			combine->leftvol += ch->leftvol;
			combine->rightvol += ch->rightvol;
			ch->leftvol = ch->rightvol = 0;
			continue;
		}
		// search for one
		for (j = 0; j < i; j++) {
			combine = static_channels[j];
			if (combine->sfx == ch->sfx)
				break;
		}

		if (j == i) {
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
	s_update_ ();
}

static void
s_extra_update (void)
{
	if (!sound_started || snd_noextraupdate->int_val)
		return;							// don't pollute timings
	s_update_ ();
}

/* console functions */

static void
s_play (void)
{
	dstring_t  *name = dstring_new ();
	int			i;
	static int	hash = 345;
	sfx_t	   *sfx;

	i = 1;
	while (i < Cmd_Argc ()) {
		if (!strrchr (Cmd_Argv (i), '.')) {
			dsprintf (name, "%s.wav", Cmd_Argv (i));
		} else {
			dsprintf (name, "%s", Cmd_Argv (i));
		}
		sfx = s_precache_sound (name->str);
		s_start_sound (hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
	dstring_delete (name);
}

static void
s_playcenter_f (void)
{
	dstring_t  *name = dstring_new ();
	int			i;
	sfx_t	   *sfx;

	i = 1;
	while (i < Cmd_Argc ()) {
		if (!strrchr (Cmd_Argv (i), '.')) {
			dsprintf (name, "%s.wav", Cmd_Argv (i));
		} else {
			dsprintf (name, "%s", Cmd_Argv (i));
		}
		sfx = s_precache_sound (name->str);
		s_start_sound (*render_data.viewentity, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
	dstring_delete (name);
}

static void
s_playvol_f (void)
{
	dstring_t  *name = dstring_new ();
	float		vol;
	int			i;
	static int	hash = 543;
	sfx_t	   *sfx;

	i = 1;
	while (i < Cmd_Argc ()) {
		if (!strrchr (Cmd_Argv (i), '.')) {
			dsprintf (name, "%s.wav", Cmd_Argv (i));
		} else {
			dsprintf (name, "%s", Cmd_Argv (i));
		}
		sfx = s_precache_sound (name->str);
		vol = atof (Cmd_Argv (i + 1));
		s_start_sound (hash++, 0, sfx, listener_origin, vol, 1.0);
		i += 2;
	}
	dstring_delete (name);
}

static void
s_soundlist_f (void)
{
	int			load, total, i;
	sfx_t	   *sfx;

	if (Cmd_Argc() >= 2 && Cmd_Argv (1)[0])
		load = 1;
	else
		load = 0;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++) {
		if (load) {
			if (!sfx->retain (sfx))
				continue;
		} else {
			if (!sfx->touch (sfx))
				continue;
		}
		total += sfx->length;
		Sys_Printf ("%6d %6d %s\n", sfx->loopstart, sfx->length, sfx->name);

		if (load)
			sfx->release (sfx);
	}
	Sys_Printf ("Total resident: %i\n", total);
}

static void
s_local_sound (const char *sound)
{
	sfx_t	   *sfx;

	if (!sound_started)
		return;
	if (nosound->int_val)
		return;

	sfx = s_precache_sound (sound);
	if (!sfx) {
		Sys_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	s_start_sound (*render_data.viewentity, -1, sfx, vec3_origin, 1, 1);
}

static void
s_clear_precache (void)
{
}

static void
s_begin_precaching (void)
{
}

static void
s_end_precaching (void)
{
}

static void
s_block_sound (void)
{
	if (++snd_blocked == 1) {
		snd_output_funcs->pS_O_BlockSound ();
		s_clear_buffer ();
	}
}

static void
s_unblock_sound (void)
{
	if (!snd_blocked)
		return;

	if (!--snd_blocked) {
		s_clear_buffer ();
		snd_output_funcs->pS_O_UnblockSound ();
	}
}

static void
s_gamedir (void)
{
	num_sfx = 0;
}

static void
s_init (void)
{
	int         i;

	for (i = 0; i < NUM_AMBIENTS; i++)
		ambient_channels[i] = s_alloc_channel ();
	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
		dynamic_channels[i] = s_alloc_channel ();

	snd_output_funcs = render_data.output->functions->snd_output;
	Sys_Printf ("\nSound Initialization\n");

	Cmd_AddCommand ("play", s_play,
					"Play selected sound effect (play pathto/sound.wav)");
	Cmd_AddCommand ("playcenter", s_playcenter_f,
					"Play selected sound effect without 3D spatialization.");
	Cmd_AddCommand ("playvol", s_playvol_f, "Play selected sound effect at "
					"selected volume (playvol pathto/sound.wav num");
	Cmd_AddCommand ("stopsound", s_stop_all_sounds_f,
					"Stops all sounds currently being played");
	Cmd_AddCommand ("soundlist", s_soundlist_f,
					"Reports a list of sounds in the cache");
	Cmd_AddCommand ("soundinfo", s_soundinfo_f,
					"Report information on the sound system");

	snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
	                              "control sample interpolation");
	ambient_fade = Cvar_Get ("ambient_fade", "100", CVAR_NONE, NULL,
							 "How quickly ambient sounds fade in or out");
	ambient_level = Cvar_Get ("ambient_level", "0.3", CVAR_NONE, NULL,
							  "Ambient sounds' volume");
	nosound = Cvar_Get ("nosound", "0", CVAR_NONE, NULL,
						"Set to turn sound off");
	precache = Cvar_Get ("precache", "1", CVAR_NONE, NULL,
						 "Toggle the use of a precache");
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

	s_startup ();

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

	ambient_sfx[AMBIENT_WATER] = s_precache_sound ("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = s_precache_sound ("ambience/wind2.wav");

	s_stop_all_sounds (true);

	QFS_GamedirCallback (s_gamedir);
}

// Shutdown sound engine ======================================================
static void
s_shutdown (void)
{
	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = 0;

	sound_started = 0;

	if (!fakedma) {
		snd_output_funcs->pS_O_Shutdown ();
	}

	shm = 0;
}

static general_funcs_t plugin_info_general_funcs = {
	s_init,
	s_shutdown,
};

static snd_render_funcs_t plugin_info_render_funcs = {
	s_ambient_off,
	s_ambient_on,
	s_touch_sound,
	s_static_sound,
	s_start_sound,
	s_stop_sound,
	s_precache_sound,
	s_clear_precache,
	s_update,
	s_stop_all_sounds,
	s_begin_precaching,
	s_end_precaching,
	s_extra_update,
	s_local_sound,
	s_block_sound,
	s_unblock_sound,
	s_load_sound,
	s_alloc_channel,
};

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	0,
	0,
	&plugin_info_render_funcs,
};

static plugin_data_t plugin_info_data = {
	&plugin_info_general_data,
	0,
	0,
	0,
	0,
	&render_data,
};

static plugin_t plugin_info = {
	qfp_snd_render,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"Sound Renderer",
	"Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
		"project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(snd_render, default)
{
	return &plugin_info;
}
