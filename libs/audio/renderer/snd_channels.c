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
#include "QF/sys.h"

#include "snd_internal.h"

static channel_t *free_channels;
channel_t       snd_channels[MAX_CHANNELS];
int             snd_total_channels;

static channel_t *ambient_channels[NUM_AMBIENTS];
static channel_t *dynamic_channels;
static channel_t *looped_dynamic_channels;
static channel_t *static_channels[MAX_STATIC_CHANNELS];
static int      snd_num_statics;

static qboolean snd_ambient = 1;
static sfx_t   *ambient_sfx[NUM_AMBIENTS];

static vec_t    sound_nominal_clip_dist = 1000.0;

static vec3_t   listener_origin;
static vec3_t   listener_forward;
static vec3_t   listener_right;
static vec3_t   listener_up;

static cvar_t  *snd_phasesep;
static cvar_t  *snd_volumesep;
static cvar_t  *snd_swapchannelside;
static cvar_t  *ambient_fade;
static cvar_t  *ambient_level;

static inline channel_t *
unlink_channel (channel_t **_ch)
{
	channel_t  *ch = *_ch;
	*_ch = ch->next;
	ch->next = 0;
	return ch;
}

channel_t *
SND_AllocChannel (snd_t *snd)
{
	channel_t **free = &free_channels;
	channel_t  *chan;

	while (*free) {
		if (!(*free)->sfx)			// free channel
			break;
		if ((*free)->done)			// mixer is finished with this channel
			break;
		if (!(*free)->stop)
			Sys_Error ("SND_AllocChannel: bogus channel free list");
		free = &(*free)->next;
	}
	if (!*free) {
		int         num_free = 0;
		for (free = &free_channels; *free; free = &(*free)->next) {
			num_free++;
		}
		Sys_MaskPrintf (SYS_warn, "SND_AllocChannel: out of channels. %d\n",
						num_free);
		return 0;
	}
	chan = *free;
	*free = chan->next;
	if (chan->sfx) {
		chan->sfx->release (chan->sfx);
		chan->sfx->close (chan->sfx);
		chan->sfx = 0;	// make sure mixer doesn't use channel during memset
	}

	memset (chan, 0, sizeof (*chan));
	chan->next = 0;
	chan->sfx = 0;

	return chan;
}

void
SND_ChannelStop (snd_t *snd, channel_t *chan)
{
	/* if chan->next is set, then this channel may have already been freed.
	   a rather serious bug as it will create a loop in the free list
	 */
	if (chan->next)
		Sys_Error ("Stopping a freed channel");
	chan->stop = 1;
	chan->next = free_channels;
	free_channels = chan;
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
	int         i;

	snd_num_statics = 0;
	while (dynamic_channels)
		SND_ChannelStop (snd, unlink_channel (&dynamic_channels));
	while (looped_dynamic_channels)
		SND_ChannelStop (snd, unlink_channel (&looped_dynamic_channels));
	for (i = 0; i < NUM_AMBIENTS; i++) {
		if (ambient_channels[i])
			SND_ChannelStop (snd, ambient_channels[i]);
		ambient_channels[i] = 0;
	}
	for (i = 0; i < MAX_STATIC_CHANNELS; i++) {
		if (static_channels[i])
			SND_ChannelStop (snd, static_channels[i]);
		static_channels[i] = 0;
	}
	if (0) {
		channel_t  *ch;
		Sys_Printf ("SND_StopAllSounds\n");
		for (i = 0, ch = free_channels; ch; ch = ch->next)
			i++;
		Sys_Printf ("	free channels:%d\n", i);
		for (i = 0, ch = free_channels; ch; ch = ch->next)
			if (!ch->sfx || ch->done)
				i++;
		Sys_Printf ("	truely free channels:%d\n", i);
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
s_channels_gamedir (int phase)
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
	int         i;

	snd_phasesep = Cvar_Get ("snd_phasesep", "0.0", CVAR_ARCHIVE, NULL,
							 "max stereo phase separation in ms. 0.6 is for "
							 "20cm head");
	snd_volumesep = Cvar_Get ("snd_volumesep", "1.0", CVAR_ARCHIVE, NULL,
							  "max stereo volume separation. 1.0 is max");
	snd_swapchannelside = Cvar_Get ("snd_swapchannelside", "0", CVAR_ARCHIVE,
									NULL, "Toggle swapping of left and right "
									"channels");
	ambient_fade = Cvar_Get ("ambient_fade", "100", CVAR_NONE, NULL,
							 "How quickly ambient sounds fade in or out");
	ambient_level = Cvar_Get ("ambient_level", "0.3", CVAR_NONE, NULL,
							  "Ambient sounds' volume");

	Cmd_AddDataCommand ("play", s_play_f, snd,
						"Play selected sound effect (play pathto/sound.wav)");
	Cmd_AddDataCommand ("playcenter", s_playcenter_f, snd,
						"Play selected sound effect without 3D "
						"spatialization.");
	Cmd_AddDataCommand ("playvol", s_playvol_f, snd,
						"Play selected sound effect at selected volume "
						"(playvol pathto/sound.wav num");

	for (i = 0; i < MAX_CHANNELS - 1; i++)
		snd_channels[i].next = &snd_channels[i + 1];
	free_channels = &snd_channels[0];
	snd_total_channels = MAX_CHANNELS;

	snd_num_statics = 0;

	QFS_GamedirCallback (s_channels_gamedir);
}

static channel_t *
s_pick_channel (snd_t *snd, int entnum, int entchannel, int looped)
{
	channel_t  *ch, **_ch;

	// check for finished non-looped sounds
	for (_ch = &dynamic_channels; *_ch; ) {
		if (!(*_ch)->sfx || (*_ch)->done) {
			SND_ChannelStop (snd, unlink_channel (_ch));
			continue;
		}
		_ch = &(*_ch)->next;
	}

	// non-looped sounds are used to stop looped sounds on an ent channel
	// also clean out any caught by SND_ScanChannels
	for (_ch = &looped_dynamic_channels; *_ch; ) {
		if (!(*_ch)->sfx || (*_ch)->done
			|| ((*_ch)->entnum == entnum
				&& ((*_ch)->entchannel == entchannel || entchannel == -1))) {
			SND_ChannelStop (snd, unlink_channel (_ch));
			continue;
		}
		_ch = &(*_ch)->next;
	}
	_ch = looped ? &looped_dynamic_channels : &dynamic_channels;
	if ((ch = SND_AllocChannel (snd))) {
		ch->next = *_ch;
		*_ch = ch;
	}
	return ch;
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
	if (!ambient_sound_level || !ambient_level->value) {
		// if we're not in a leaf (huh?) or ambients have been turned off,
		// stop all ambient channels.
		for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS;
			 ambient_channel++) {
			chan = ambient_channels[ambient_channel];
			if (chan) {
				chan->master_vol = 0;
				chan->leftvol = chan->rightvol = chan->master_vol;
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

		vol = ambient_level->value * ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol) {
			chan->master_vol += *snd_render_data.host_frametime
				* ambient_fade->value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		} else if (chan->master_vol > vol) {
			chan->master_vol -= *snd_render_data.host_frametime
				* ambient_fade->value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
		chan->sfx = sfx;
	}
}

static void
s_spatialize (snd_t *snd, channel_t *ch)
{
	int			phase;					// in samples
	vec_t		dist, dot, lscale, rscale, scale;
	vec3_t		source_vec;

	// prepare to lerp from prev to next phase
	ch->oldphase = ch->phase;

	// anything coming from the view entity will always be full volume
	if (!snd_render_data.viewentity
		|| ch->entnum == *snd_render_data.viewentity) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		ch->phase = 0;
		return;
	}
	// calculate stereo seperation and distance attenuation

	VectorSubtract (ch->origin, listener_origin, source_vec);

	dist = VectorNormalize (source_vec) * ch->dist_mult;

	dot = DotProduct (listener_right, source_vec);
	if (snd_swapchannelside->int_val)
		dot = -dot;

	if (snd->channels == 1) {
		rscale = 1.0;
		lscale = 1.0;
		phase = 0;
	} else {
		rscale = 1.0 + dot * snd_volumesep->value;
		lscale = 1.0 - dot * snd_volumesep->value;
		phase = snd_phasesep->value * 0.001 * snd->speed * dot;
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
SND_SetListener (snd_t *snd, const vec3_t origin, const vec3_t forward,
				 const vec3_t right, const vec3_t up,
				 const byte *ambient_sound_level)
{
	int         i, j;
	channel_t  *combine, *ch;

	VectorCopy (origin, listener_origin);
	VectorCopy (forward, listener_forward);
	VectorCopy (right, listener_right);
	VectorCopy (up, listener_up);

	// update general area ambient sound sources
	s_updateAmbientSounds (snd, ambient_sound_level);

	// update spatialization for dynamic sounds
	for (ch = dynamic_channels; ch; ch = ch->next)
		s_update_channel (snd, ch);
	for (ch = looped_dynamic_channels; ch; ch = ch->next)
		s_update_channel (snd, ch);

	// update spatialization for static sounds
	combine = 0;
	for (i = 0; i < snd_num_statics; i++) {
		ch = static_channels[i];
		if (!s_update_channel (snd, ch))
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame
		// see if it can just use the last one
		if (combine && combine->sfx == ch->sfx) {
			s_combine_channel (combine, ch);
			continue;
		}
		// search for one
		for (j = 0; j < i; j++) {
			combine = static_channels[j];
			if (combine->sfx == ch->sfx)
				break;
		}

		if (j == i) {
			combine = 0;
		} else {
			if (combine != ch)
				s_combine_channel (combine, ch);
			continue;
		}
	}
}

static int
snd_check_channels (snd_t *snd, channel_t *target_chan, const channel_t *check,
					const sfx_t *osfx)
{
	if (!check || check == target_chan)
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
				const vec3_t origin, float fvol, float attenuation)
{
	int			 vol;
	int          looped;
	channel_t   *target_chan, *check;
	sfx_t       *osfx;

	if (!sfx || !snd->speed)
		return;
	// pick a channel to play on
	looped = sfx->loopstart != (unsigned) -1;
	target_chan = s_pick_channel (snd, entnum, entchannel, looped);
	if (!target_chan)
		return;

	vol = fvol * 255;

	// spatialize
	VectorCopy (origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	s_spatialize (snd, target_chan);

	// new channel
	if (!(osfx = sfx->open (sfx))) {
		SND_ChannelStop (snd, unlink_channel (looped ? &looped_dynamic_channels
												: &dynamic_channels));
		return;
	}
	target_chan->pos = 0;
	target_chan->end = 0;

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	for (check = dynamic_channels; check; check = check->next)
		if (snd_check_channels (snd, target_chan, check, osfx))
			break;
	for (check = looped_dynamic_channels; check; check = check->next)
		if (snd_check_channels (snd, target_chan, check, osfx))
			break;
	if (!osfx->retain (osfx)) {
		SND_ChannelStop (snd, unlink_channel (looped ? &looped_dynamic_channels
												: &dynamic_channels));
		return;						// couldn't load the sound's data
	}
	target_chan->sfx = osfx;
}

static int
s_check_stop (snd_t *snd, channel_t **_ch, int entnum, int entchannel)
{
	if ((*_ch)->entnum == entnum && (*_ch)->entchannel == entchannel) {
		SND_ChannelStop (snd, unlink_channel (_ch));
		return 1;
	}
	return 0;
}

void
SND_StopSound (snd_t *snd, int entnum, int entchannel)
{
	channel_t **_ch;

	for (_ch = &dynamic_channels; *_ch; )
		if (!s_check_stop (snd, _ch, entnum, entchannel))
			_ch = &(*_ch)->next;
	for (_ch = &looped_dynamic_channels; *_ch; )
		if (!s_check_stop (snd, _ch, entnum, entchannel))
			_ch = &(*_ch)->next;
}

void
SND_StaticSound (snd_t *snd, sfx_t *sfx, const vec3_t origin, float vol,
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

	if (!static_channels[snd_num_statics]) {
		if (!(static_channels[snd_num_statics] = SND_AllocChannel (snd))) {
			Sys_Printf ("ran out of channels\n");
			return;
		}
	}

	ss = static_channels[snd_num_statics];

	if (!(osfx = sfx->open (sfx)))
		return;

	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation / 64) / sound_nominal_clip_dist;
	ss->end = 0;

	s_spatialize (snd, ss);
	ss->oldphase = ss->phase;

	if (!osfx->retain (osfx))
		return;
	snd_num_statics++;
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
	SND_StartSound (snd, viewent, -1, sfx, vec3_origin, 1, 1);
}
