/*
	snd_sgi.c

	sound support for sgi

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <dmedia/audio.h>

#include "QF/console.h"
#include "QF/qtypes.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/plugin.h"

static int  snd_inited = 0;
static ALconfig alc;
static ALport alp;

static int  tryrates[] = { 11025, 22050, 44100, 8000 };

static unsigned char *dma_buffer, *write_buffer;
static int  bufsize;
static int  wbufp;
static int  framecount;

plugin_t           plugin_info;
plugin_data_t      plugin_info_data;
plugin_funcs_t     plugin_info_funcs;
general_data_t     plugin_info_general_data;
general_funcs_t    plugin_info_general_funcs;
sound_data_t       plugin_info_sound_data;
sound_funcs_t      plugin_info_sound_funcs;

void I_S_Init (void);
void I_S_Shutdown (void);
void I_S_AmbientOff (void);
void I_S_AmbientOn (void);
void I_S_TouchSound (char *sample);
void I_S_ClearBuffer (void);
void I_S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void I_S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void I_S_StopSound (int entnum, int entchannel);
sfx_t *I_S_PrecacheSound (char *sample);
void I_S_ClearPrecache (void);
void I_S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);
void I_S_StopAllSounds (qboolean clear);
void I_S_BeginPrecaching (void);
void I_S_EndPrecaching (void);
void I_S_ExtraUpdate (void);
void I_S_LocalSound (char *s);


qboolean
SNDDMA_Init (void)
{
	ALpv        alpv;
	int         i;
	char       *s;

	alc = alNewConfig ();

	if (!alc) {
		Con_Printf ("Could not make an new sound config: %s\n",
					alGetErrorString (oserror ()));
		return 0;
	}

	shm = &sn;
	shm->splitbuffer = 0;

	/* get & probe settings */
	/* sample format */
	if (alSetSampFmt (alc, AL_SAMPFMT_TWOSCOMP) < 0) {
		Con_Printf ("Could not sample format of default output to two's "
					"complement\n");
		alFreeConfig (alc);
		return 0;
	}

	/* sample bits */
	s = getenv ("QUAKE_SOUND_SAMPLEBITS");
	if (s)
		shm->samplebits = atoi (s);
	else if ((i = COM_CheckParm ("-sndbits")) != 0)
		shm->samplebits = atoi (com_argv[i + 1]);

	if (shm->samplebits != 16 && shm->samplebits != 8) {
		alpv.param = AL_WORDSIZE;

		if (alGetParams (AL_DEFAULT_OUTPUT, &alpv, 1) < 0) {
			Con_Printf ("Could not get supported wordsize of default "
						"output: %s\n", alGetErrorString (oserror ()));
			return 0;
		}

		if (alpv.value.i >= 16) {
			shm->samplebits = 16;
		} else {
			if (alpv.value.i >= 8)
				shm->samplebits = 8;
			else {
				Con_Printf ("Sound disabled since interface "
							"doesn't even support 8 bit.");
				alFreeConfig (alc);
				return 0;
			}
		}
	}

	/* sample rate */
	s = getenv ("QUAKE_SOUND_SPEED");
	if (s)
		shm->speed = atoi (s);
	else if ((i = COM_CheckParm ("-sndspeed")) != 0)
		shm->speed = atoi (com_argv[i + 1]);
	else {
		alpv.param = AL_RATE;

		for (i = 0; i < sizeof (tryrates) / sizeof (int); i++) {
			alpv.value.ll = alDoubleToFixed (tryrates[i]);

			if (alSetParams (AL_DEFAULT_OUTPUT, &alpv, 1) >= 0)
				break;
		}

		if (i >= sizeof (tryrates) / sizeof (int)) {
			Con_Printf ("Sound disabled since interface doesn't even "
						"support a sample rate of %d\n", tryrates[i - 1]);
			alFreeConfig (alc);
			return 0;
		}

		shm->speed = tryrates[i];
	}

	/* channels */
	s = getenv ("QUAKE_SOUND_CHANNELS");
	if (s)
		shm->channels = atoi (s);
	else if ((i = COM_CheckParm ("-sndmono")) != 0)
		shm->channels = 1;
	else if ((i = COM_CheckParm ("-sndstereo")) != 0)
		shm->channels = 2;
	else
		shm->channels = 2;

	/* set 'em */

	/* channels */
	while (shm->channels > 0) {
		if (alSetChannels (alc, shm->channels) < 0) {
			Con_Printf ("Unable to set number of channels to %d, trying half\n",
						shm->channels);
			shm->channels /= 2;
		} else
			break;
	}

	if (shm->channels <= 0) {
		Con_Printf ("Sound disabled since interface doesn't even support 1 "
					"channel\n");
		alFreeConfig (alc);
		return 0;
	}

	/* sample rate */
	alpv.param = AL_RATE;
	alpv.value.ll = alDoubleToFixed (shm->speed);

	if (alSetParams (AL_DEFAULT_OUTPUT, &alpv, 1) < 0) {
		Con_Printf ("Could not set samplerate of default output to %d: %s\n",
					shm->speed, alGetErrorString (oserror ()));
		alFreeConfig (alc);
		return 0;
	}

	/* set sizes of buffers relative to sizes of those for ** the 'standard'
	   frequency of 11025 ** ** use *huge* buffers since at least my indigo2
	   has enough ** to do to get sound on the way anyway */
	bufsize = 32768 * (int) ((double) shm->speed / 11025.0);

	dma_buffer = malloc (bufsize);

	if (dma_buffer == NULL) {
		Con_Printf ("Could not get %d bytes of memory for audio dma buffer\n",
					bufsize);
		alFreeConfig (alc);
		return 0;
	}

	write_buffer = malloc (bufsize);

	if (write_buffer == NULL) {
		Con_Printf ("Could not get %d bytes of memory for audio write buffer\n",
					bufsize);
		free (dma_buffer);
		alFreeConfig (alc);
		return 0;
	}

	/* sample bits */
	switch (shm->samplebits) {
		case 24:
			i = AL_SAMPLE_24;
			break;

		case 16:
			i = AL_SAMPLE_16;
			break;

		default:
			i = AL_SAMPLE_8;
			break;
	}

	if (alSetWidth (alc, i) < 0) {
		Con_Printf ("Could not set wordsize of default output to %d: %s\n",
					shm->samplebits, alGetErrorString (oserror ()));
		free (write_buffer);
		free (dma_buffer);
		alFreeConfig (alc);
		return 0;
	}

	alp = alOpenPort ("quakeforge", "w", alc);

	if (!alp) {
		Con_Printf ("Could not open sound port: %s\n",
					alGetErrorString (oserror ()));
		free (write_buffer);
		free (dma_buffer);
		alFreeConfig (alc);
		return 0;
	}

	shm->soundalive = true;
	shm->samples = bufsize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = dma_buffer;

	framecount = 0;

	snd_inited = 1;
	return 1;
}


int
SNDDMA_GetDMAPos (void)
{
	/* Con_Printf("framecount: %d %d\n", (framecount * shm->channels) %
	   shm->samples, alGetFilled(alp)); */
	shm->samplepos = ((framecount - alGetFilled (alp))
					  * shm->channels) % shm->samples;
	return shm->samplepos;
}


void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		free (write_buffer);
		free (dma_buffer);
		alClosePort (alp);
		alFreeConfig (alc);
		snd_inited = 0;
	}
}


/*
	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer
*/
void
SNDDMA_Submit (void)
{
	int         bsize;
	int         bytes, b;
	unsigned char *p;
	int         idx;
	int         stop = paintedtime;

	if (paintedtime < wbufp)
		wbufp = 0;						// reset

	bsize = shm->channels * (shm->samplebits / 8);
	bytes = (paintedtime - wbufp) * bsize;

	if (!bytes)
		return;

	if (bytes > bufsize) {
		bytes = bufsize;
		stop = wbufp + bytes / bsize;
	}

	p = write_buffer;
	idx = (wbufp * bsize) & (bufsize - 1);

	for (b = bytes; b; b--) {
		*p++ = dma_buffer[idx];
		idx = (idx + 1) & (bufsize - 1);
	}

	wbufp = stop;

	alWriteFrames (alp, write_buffer, bytes / bsize);
	framecount += bytes / bsize;
}


plugin_t *
PluginInfo (void) {
    plugin_info.type = qfp_sound;
    plugin_info.api_version = QFPLUGIN_VERSION;
    plugin_info.plugin_version = "0.1";
    plugin_info.description = "SGI digital output";
    plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
        "Copyright (C) 1999,2000,2001  contributors of the QuakeForge project\n"        "Please see the file \"AUTHORS\" for a list of contributors";
    plugin_info.functions = &plugin_info_funcs;
    plugin_info.data = &plugin_info_data;

    plugin_info_data.general = &plugin_info_general_data;
    plugin_info_data.input = NULL;
    plugin_info_data.sound = &plugin_info_sound_data;

    plugin_info_funcs.general = &plugin_info_general_funcs;
    plugin_info_funcs.input = NULL;
    plugin_info_funcs.sound = &plugin_info_sound_funcs;

    plugin_info_general_funcs.p_Init = I_S_Init;
    plugin_info_general_funcs.p_Shutdown = I_S_Shutdown;

    plugin_info_sound_funcs.pS_AmbientOff = I_S_AmbientOff;
    plugin_info_sound_funcs.pS_AmbientOn = I_S_AmbientOn;
    plugin_info_sound_funcs.pS_TouchSound = I_S_TouchSound;
    plugin_info_sound_funcs.pS_ClearBuffer = I_S_ClearBuffer;
    plugin_info_sound_funcs.pS_StaticSound = I_S_StaticSound;
    plugin_info_sound_funcs.pS_StartSound = I_S_StartSound;
    plugin_info_sound_funcs.pS_StopSound = I_S_StopSound;
    plugin_info_sound_funcs.pS_PrecacheSound = I_S_PrecacheSound;
    plugin_info_sound_funcs.pS_ClearPrecache = I_S_ClearPrecache;
    plugin_info_sound_funcs.pS_Update = I_S_Update;
    plugin_info_sound_funcs.pS_StopAllSounds = I_S_StopAllSounds;
    plugin_info_sound_funcs.pS_BeginPrecaching = I_S_BeginPrecaching;
    plugin_info_sound_funcs.pS_EndPrecaching = I_S_EndPrecaching;
    plugin_info_sound_funcs.pS_ExtraUpdate = I_S_ExtraUpdate;
    plugin_info_sound_funcs.pS_LocalSound = I_S_LocalSound;

    return &plugin_info;
}
