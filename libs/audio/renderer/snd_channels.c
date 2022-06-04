/*
	snd_dma.c

	main control for any streaming sound output device

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2003-2022  Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/model.h"
#include "QF/quakefs.h"
#include "QF/set.h"
#include "QF/sys.h"

#include "QF/scene/transform.h"

#include "snd_internal.h"

#define SND_STATIC_ID -1

typedef struct entchan_s {
	int         id;			// entity id
	int         channel;	// per-entity sound channel
} entchan_t;

int             snd_total_channels;
channel_t       snd_channels[MAX_CHANNELS];
static entchan_t snd_entity_channels[MAX_CHANNELS];
static int      snd_free_channels[MAX_CHANNELS];
static int      snd_num_free_channels;
/* Dynamic channels are (usually) short sound bytes, never looped. They do not
 * override other dynamic channels even for the same entity channel. However,
 * they DO override (stop) looped channels on the same entity channel.
 */
static set_bits_t dynamic_channel_bits[SET_WORDS_STATIC (MAX_CHANNELS)];
static set_t    dynamic_channels = SET_STATIC_ARRAY (dynamic_channel_bits);
/* Looped channels are sounds that automatically repeat until they are stopped.
 * They can be stopped via SND_ChannelStop or by starting another sound
 * (dynamic or looped) on the same entity channel.
 */
static set_bits_t looped_channel_bits[SET_WORDS_STATIC (MAX_CHANNELS)];
static set_t    looped_channels = SET_STATIC_ARRAY (looped_channel_bits);
static set_bits_t static_channel_bits[SET_WORDS_STATIC (MAX_CHANNELS)];
static set_t    static_channels = SET_STATIC_ARRAY (static_channel_bits);

static channel_t *ambient_channels[NUM_AMBIENTS];

static qboolean snd_ambient = 1;
static sfx_t   *ambient_sfx[NUM_AMBIENTS];

static vec_t    sound_nominal_clip_dist = 1000.0;

static vec4f_t  listener_origin;
static vec4f_t  listener_forward;
static vec4f_t  listener_right;
static vec4f_t  listener_up;

static float snd_phasesep;
static cvar_t snd_phasesep_cvar = {
	.name = "snd_phasesep",
	.description =
		"max stereo phase separation in ms. 0.6 is for 20cm head",
	.default_value = "0.0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &snd_phasesep },
};
static float snd_volumesep;
static cvar_t snd_volumesep_cvar = {
	.name = "snd_volumesep",
	.description =
		"max stereo volume separation. 1.0 is max",
	.default_value = "1.0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &snd_volumesep },
};
static int snd_swapchannelside;
static cvar_t snd_swapchannelside_cvar = {
	.name = "snd_swapchannelside",
	.description =
		"Toggle swapping of left and right channels",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &snd_swapchannelside },
};
static float ambient_fade;
static cvar_t ambient_fade_cvar = {
	.name = "ambient_fade",
	.description =
		"How quickly ambient sounds fade in or out",
	.default_value = "100",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &ambient_fade },
};
static float ambient_level;
static cvar_t ambient_level_cvar = {
	.name = "ambient_level",
	.description =
		"Ambient sounds' volume",
	.default_value = "0.3",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &ambient_level },
};

static void
snd_free_channel (channel_t *ch)
{
	ch->sfx = 0;
	ch->stop = 0;
	ch->done = 0;
	int         chan_ind = ch - snd_channels;
	if (snd_num_free_channels >= MAX_CHANNELS) {
		Sys_Error ("snd_num_free_channels: free channel list overflow");
	}
	snd_free_channels[snd_num_free_channels++] = chan_ind;
	snd_entity_channels[chan_ind] = (entchan_t) {};
}

channel_t *
SND_AllocChannel (snd_t *snd)
{
	channel_t  *chan;

	// chech for any channels that have become available as the mixer thread
	// has finished with them
	for (int i = 0; i < MAX_CHANNELS; i++) {
		channel_t  *ch = &snd_channels[i];
		if (ch->done) {
			snd_free_channel (ch);
		}
	}
	//Sys_MaskPrintf (SYS_snd, "SND_AllocChannel: free channels: %d\n",
	//				snd_num_free_channels);
	if (!snd_num_free_channels) {
		Sys_MaskPrintf (SYS_warn, "SND_AllocChannel: out of channels.\n");
		return 0;
	}
	int         chan_ind = snd_free_channels[--snd_num_free_channels];
	chan = &snd_channels[chan_ind];

	memset (chan, 0, sizeof (*chan));

	return chan;
}

void
SND_ChannelStop (snd_t *snd, channel_t *chan)
{
	if (!chan->sfx) {
		Sys_MaskPrintf (SYS_warn, "Sound: stop called on invalid channel\n");
	}
	chan->stop = 1;
	int         chan_ind = chan - snd_channels;
	set_remove (&dynamic_channels, chan_ind);
	set_remove (&looped_channels, chan_ind);
	set_remove (&static_channels, chan_ind);
}

void
SND_ScanChannels (snd_t *snd, int wait)
{
	int         i;
	channel_t  *ch;
	int         count = 0;

	if (!snd || !snd->speed)
		return;

	if (wait) {
		Sys_MaskPrintf (SYS_dev, "scanning channels...\n");
		do {
			count = 0;
			for (i = 0; i < MAX_CHANNELS; i++) {
				ch = &snd_channels[i];
				if (!ch->sfx || ch->done)
					continue;
				ch->stop = 1;
				count++;
			}
			Sys_MaskPrintf (SYS_dev, "count = %d\n", count);
#ifdef HAVE_USLEEP
			usleep (1000);
#endif
		} while (count);
		Sys_MaskPrintf (SYS_dev, "scanning done.\n");
	} else {
		for (i = 0; i < MAX_CHANNELS; i++) {
			ch = &snd_channels[i];
			if (ch->sfx && ch->stop && !ch->done) {
				ch->done = 1;
				count++;
			}
		}
		//printf ("count: %d\n", count);
	}
	for (i = 0; i < MAX_CHANNELS; i++) {
		ch = &snd_channels[i];
		if (!ch->sfx || !ch->done)
			continue;
		ch->sfx->release (ch->sfx);
		ch->sfx->close (ch->sfx);
		ch->sfx = 0;
	}
}

void
SND_FinishChannels (void)
{
	int         i;
	channel_t  *ch;

	for (i = 0; i < MAX_CHANNELS; i++) {
		ch = &snd_channels[i];
		ch->done = ch->stop = 1;
	}
}

void
SND_StopAllSounds (snd_t *snd)
{
	for (int i = 0; i < MAX_CHANNELS; i++) {
		if (set_is_member (&dynamic_channels, i)
			|| set_is_member (&looped_channels, i)
			|| set_is_member (&static_channels, i)) {
			SND_ChannelStop (snd, &snd_channels[i]);
		}
	}
	set_empty (&dynamic_channels);
	set_empty (&looped_channels);
	set_empty (&static_channels);
	for (int i = 0; i < NUM_AMBIENTS; i++) {
		if (ambient_channels[i])
			SND_ChannelStop (snd, ambient_channels[i]);
		ambient_channels[i] = 0;
	}
}

static void
s_play_f (void *_snd)
{
	snd_t      *snd = _snd;
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
		sfx = SND_PrecacheSound (snd, name->str);
		printf ("%s %p\n", name->str, sfx);
		SND_StartSound (snd, hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
	dstring_delete (name);
}

static void
s_playcenter_f (void *_snd)
{
	snd_t      *snd = _snd;
	dstring_t  *name = dstring_new ();
	int			i;
	sfx_t	   *sfx;
	int         viewent = 0;

	if (snd_render_data.viewentity)
		viewent = *snd_render_data.viewentity;

	for (i = 1; i < Cmd_Argc (); i++) {
		if (!strrchr (Cmd_Argv (i), '.')) {
			dsprintf (name, "%s.wav", Cmd_Argv (i));
		} else {
			dsprintf (name, "%s", Cmd_Argv (i));
		}
		sfx = SND_PrecacheSound (snd, name->str);
		SND_StartSound (snd, viewent, 0, sfx, listener_origin, 1.0, 1.0);
	}
	dstring_delete (name);
}

static void
s_playvol_f (void *_snd)
{
	snd_t      *snd = _snd;
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
		sfx = SND_PrecacheSound (snd, name->str);
		vol = atof (Cmd_Argv (i + 1));
		SND_StartSound (snd, hash++, 0, sfx, listener_origin, vol, 1.0);
		i += 2;
	}
	dstring_delete (name);
}

static void
s_channels_gamedir (int phase, void *_snd)
{
	//FIXME for some reason, a gamedir change causes semi-random
	//"already released" cache errors. fortunatly, servers don't change
	//gamedir often, so I'll put this in the too-hard basket for now.
	//XXX FIXME set ambient sounds
	//if (phase) {
	//	ambient_sfx[AMBIENT_WATER] = SND_PrecacheSound (snd, "ambience/water1.wav");
	//	ambient_sfx[AMBIENT_SKY] = SND_PrecacheSound (snd, "ambience/wind2.wav");
	//}
}

void
SND_Channels_Init (snd_t *snd)
{
	Cvar_Register (&snd_phasesep_cvar, 0, 0);
	Cvar_Register (&snd_volumesep_cvar, 0, 0);
	Cvar_Register (&snd_swapchannelside_cvar, 0, 0);
	Cvar_Register (&ambient_fade_cvar, 0, 0);
	Cvar_Register (&ambient_level_cvar, 0, 0);

	Cmd_AddDataCommand ("play", s_play_f, snd,
						"Play selected sound effect (play pathto/sound.wav)");
	Cmd_AddDataCommand ("playcenter", s_playcenter_f, snd,
						"Play selected sound effect without 3D "
						"spatialization.");
	Cmd_AddDataCommand ("playvol", s_playvol_f, snd,
						"Play selected sound effect at selected volume "
						"(playvol pathto/sound.wav num");

	for (int i = 0; i < MAX_CHANNELS; i++) {
		snd_free_channels[i] = MAX_CHANNELS - 1 - i;
	}
	snd_num_free_channels = MAX_CHANNELS;
	snd_total_channels = MAX_CHANNELS;

	QFS_GamedirCallback (s_channels_gamedir, snd);
}

static channel_t *
s_pick_channel (snd_t *snd, int entnum, int entchannel, int looped)
{
	// check for finished non-looped sounds
	for (int i = 0; i < MAX_CHANNELS; i++) {
		channel_t  *ch = &snd_channels[i];
		if (set_is_member (&dynamic_channels, i)) {
			if (ch->done) {
				// mixer is done with the channel, it can be freed
				snd_free_channel (ch);
				set_remove (&dynamic_channels, i);
			}
		} else if (set_is_member (&looped_channels, i)) {
			// non-looped sounds are used to stop looped sounds on an entity
			// channel also clean out any caught by SND_ScanChannels
			entchan_t  *entchan = &snd_entity_channels[i];
			if (entchan->id == entnum
				&& (entchan->channel == entchannel || entchannel == -1)) {
				// the mixer is still using the channel, so send a request
				// for it to stopp
				SND_ChannelStop (snd, ch);
			}
		}
	}

	return SND_AllocChannel (snd);
}

void
SND_AmbientOff (snd_t *snd)
{
	snd_ambient = false;
}

void
SND_AmbientOn (snd_t *snd)
{
	snd_ambient = true;
}

static void
s_updateAmbientSounds (snd_t *snd, const byte *ambient_sound_level)
{
	float		vol;
	int			ambient_channel;
	channel_t  *chan;
	sfx_t      *sfx;

	if (!snd_ambient)
		return;
	// calc ambient sound levels
	if (!ambient_sound_level || !ambient_level) {
		// if we're not in a leaf (huh?) or ambients have been turned off,
		// stop all ambient channels.
		for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS;
			 ambient_channel++) {
			chan = ambient_channels[ambient_channel];
			if (chan) {
				chan->volume = 0;
				chan->leftvol = chan->rightvol = chan->volume;
			}
		}
		return;
	}

	for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS;
		 ambient_channel++) {
		sfx = ambient_sfx[ambient_channel];
		if (!sfx)
			continue;

		chan = ambient_channels[ambient_channel];
		if (!chan) {
			chan = ambient_channels[ambient_channel] = SND_AllocChannel (snd);
			if (!chan)
				continue;
		}

		if (!chan->sfx) {
			sfx = sfx->open (sfx);
			if (!sfx)
				continue;
			sfx->retain (sfx);
		} else {
			sfx = chan->sfx;
			//sfx->retain (sfx); //FIXME why is this necessary?
		}
		// sfx will be written to chan->sfx later to ensure mixer doesn't use
		// channel prematurely.

		vol = ambient_level * ambient_sound_level[ambient_channel] * (1/255.0);
		if (vol < 8/255.0)
			vol = 0;

		// don't adjust volume too fast
		float       fade = ambient_fade * (1/255.0);
		if (chan->volume < vol) {
			chan->volume += *snd_render_data.host_frametime * fade;
			if (chan->volume > vol)
				chan->volume = vol;
		} else if (chan->volume > vol) {
			chan->volume -= *snd_render_data.host_frametime * fade;
			if (chan->volume < vol)
				chan->volume = vol;
		}

		chan->leftvol = chan->rightvol = chan->volume;
		chan->sfx = sfx;
	}
}

static void
s_spatialize (snd_t *snd, channel_t *ch)
{
	int			phase;					// in samples
	vec_t		dist, dot, lscale, rscale, scale;
	vec3_t		source_vec;
	int         chan_ind = ch - snd_channels;

	// prepare to lerp from prev to next phase
	ch->oldphase = ch->phase;

	// anything coming from the view entity will always be full volume
	if (!snd_render_data.viewentity
		|| snd_entity_channels[chan_ind].id == *snd_render_data.viewentity) {
		ch->leftvol = ch->volume;
		ch->rightvol = ch->volume;
		ch->phase = 0;
		return;
	}
	// calculate stereo seperation and distance attenuation

	VectorSubtract (ch->origin, listener_origin, source_vec);

	dist = VectorNormalize (source_vec) * ch->dist_mult;

	dot = DotProduct (listener_right, source_vec);
	if (snd_swapchannelside)
		dot = -dot;

	if (snd->channels == 1) {
		rscale = 1.0;
		lscale = 1.0;
		phase = 0;
	} else {
		rscale = 1.0 + dot * snd_volumesep;
		lscale = 1.0 - dot * snd_volumesep;
		phase = snd_phasesep * 0.001 * snd->speed * dot;
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = ch->volume * scale;
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = ch->volume * scale;
	if (ch->leftvol < 0)
		ch->leftvol = 0;

	ch->phase = phase;
}

static inline int
s_update_channel (snd_t *snd, channel_t *ch)
{
	if (!ch->sfx)
		return 0;
	s_spatialize (snd, ch);
	if (!ch->leftvol && !ch->rightvol)
		return 0;
	return 1;
}

static void
s_combine_channel (channel_t *combine, channel_t *ch)
{
	combine->leftvol += ch->leftvol;
	combine->rightvol += ch->rightvol;
	ch->leftvol = ch->rightvol = 0;
}

void
SND_SetListener (snd_t *snd, transform_t *ear, const byte *ambient_sound_level)
{
	if (ear) {
		listener_origin  = Transform_GetWorldPosition (ear);
		listener_forward = Transform_Forward (ear);
		listener_right   = Transform_Right (ear);
		listener_up      = Transform_Up (ear);
	} else {
		listener_origin  = (vec4f_t) {0, 0, 0, 1};
		listener_forward = (vec4f_t) {1, 0, 0, 0};
		listener_right   = (vec4f_t) {0, -1, 0, 0};
		listener_up      = (vec4f_t) {0, 0, 1, 0};
	}

	// update general area ambient sound sources
	s_updateAmbientSounds (snd, ambient_sound_level);

	channel_t  *combine = 0;
	for (int i = 0; i < MAX_CHANNELS; i++) {
		channel_t  *ch = &snd_channels[i];
		if (!ch->sfx || ch->done) {
			continue;
		}
		if (set_is_member (&dynamic_channels, i)
			|| set_is_member (&looped_channels, i)) {
			// update spatialization for dynamic and looped sounds
			s_update_channel (snd, ch);
		} else if (set_is_member (&static_channels, i)) {
			if (!s_update_channel (snd, ch)) {
				// too quiet
				continue;
			}
			// try to combine static sounds with a previous channel of
			// the same sound effect so we don't mix five torches every
			// frame see if it can just use the last one
			if (combine && combine->sfx == ch->sfx) {
				s_combine_channel (combine, ch);
				continue;
			}
			// search for one
			channel_t  *c = 0;
			for (int j = 0; j < i; j++) {
				if (set_is_member (&static_channels, j)) {
					if (snd_channels[j].sfx == ch->sfx) {
						c = &snd_channels[j];
						break;
					}
				}
			}
			if ((combine = c)) {
				s_combine_channel (combine, ch);
			}
		}
	}
}

static int
snd_check_channels (snd_t *snd, channel_t *target_chan, const channel_t *check,
					const sfx_t *osfx)
{
	if (!check || !check->sfx || check == target_chan)
		return 0;
	if (check->sfx->owner == osfx->owner && !check->pos) {
		int skip = rand () % (int) (0.01 * snd->speed);
		target_chan->pos = -skip;
		return 1;
	}
	return 0;
}

void
SND_StartSound (snd_t *snd, int entnum, int entchannel, sfx_t *sfx,
				vec4f_t origin, float vol, float attenuation)
{
	int          looped;
	channel_t   *target_chan;
	sfx_t       *osfx;

	if (!sfx || !snd->speed)
		return;
	// pick a channel to play on
	looped = sfx->loopstart != (unsigned) -1;
	target_chan = s_pick_channel (snd, entnum, entchannel, looped);
	if (!target_chan)
		return;

	int         chan_ind = target_chan - snd_channels;
	// spatialize
	VectorCopy (origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->volume = vol;
	snd_entity_channels[chan_ind] = (entchan_t) {
		.id = entnum,
		.channel = entchannel,
	};
	s_spatialize (snd, target_chan);

	if (!(osfx = sfx->open (sfx))) {
		// because the channel was never started, it's safe to directly free it
		snd_free_channel (target_chan);
		return;
	}
	target_chan->pos = 0;
	target_chan->end = 0;

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	for (int i = 0; i < MAX_CHANNELS; i++) {
		if (set_is_member (&dynamic_channels, i)
			|| set_is_member (&looped_channels, i)) {
			channel_t  *check = &snd_channels[i];
			if (snd_check_channels (snd, target_chan, check, osfx))
				break;
		}
	}
	if (!osfx->retain (osfx)) {
		// because the channel was never started, it's safe to directly free it
		snd_free_channel (target_chan);
		return;						// couldn't load the sound's data
	}
	target_chan->sfx = osfx;
	set_add (looped ? &looped_channels : &dynamic_channels, chan_ind);
}

static int
s_check_stop (snd_t *snd, int chan_ind, int entnum, int entchannel)
{
	entchan_t  *entchan = &snd_entity_channels[chan_ind];
	if (entchan->id == entnum && entchan->channel == entchannel) {
		SND_ChannelStop (snd, &snd_channels[chan_ind]);
		return 1;
	}
	return 0;
}

void
SND_StopSound (snd_t *snd, int entnum, int entchannel)
{
	for (int i = 0; i < MAX_CHANNELS; i++) {
		if (set_is_member (&dynamic_channels, i)
			|| set_is_member (&looped_channels, i)) {
			s_check_stop (snd, i, entnum, entchannel);
		}
	}
}

void
SND_StaticSound (snd_t *snd, sfx_t *sfx, vec4f_t origin, float vol,
				 float attenuation)
{
	channel_t  *ss;
	sfx_t      *osfx;

	if (!sfx)
		return;
	if (sfx->loopstart == (unsigned int) -1) {
		Sys_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}

	if (!(ss = SND_AllocChannel (snd))) {
		Sys_Printf ("ran out of channels\n");
		return;
	}
	int         ss_ind = ss - snd_channels;

	if (!(osfx = sfx->open (sfx)))
		return;

	VectorCopy (origin, ss->origin);
	ss->volume = vol;
	ss->dist_mult = attenuation / sound_nominal_clip_dist;
	ss->end = 0;

	s_spatialize (snd, ss);
	ss->oldphase = ss->phase;

	if (!osfx->retain (osfx))
		return;

	set_add (&static_channels, ss_ind);
	snd_entity_channels[ss_ind] = (entchan_t) {
		.id = SND_STATIC_ID,
		.channel = 0,
	};
	ss->sfx = osfx;
}

void
SND_LocalSound (snd_t *snd, const char *sound)
{
	sfx_t	   *sfx;
	int         viewent = 0;

	sfx = SND_PrecacheSound (snd, sound);
	if (!sfx) {
		Sys_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	if (snd_render_data.viewentity)
		viewent = *snd_render_data.viewentity;
	SND_StartSound (snd, viewent, -1, sfx, (vec4f_t) {0, 0, 0, 1}, 1, 1);
}
