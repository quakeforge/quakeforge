/*
	snd_sun.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/audioio.h>
#include <errno.h>

#include "QF/console.h"
#include "QF/qtypes.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/plugin.h"

int         audio_fd;
int         snd_inited;

static int  wbufp;
static audio_info_t info;

#define BUFFER_SIZE		8192

unsigned char dma_buffer[BUFFER_SIZE];
unsigned char pend_buffer[BUFFER_SIZE];
int         pending;

plugin_t           plugin_info;
plugin_data_t      plugin_info_data;
plugin_funcs_t     plugin_info_funcs;
general_data_t     plugin_info_general_data;
general_funcs_t    plugin_info_general_funcs;
sound_data_t       plugin_info_sound_data;
sound_funcs_t      plugin_info_sound_funcs;

void SND_Init (void);
void SND_Shutdown (void);
void SND_AmbientOff (void);
void SND_AmbientOn (void);
void SND_TouchSound (char *sample);
void SND_ClearBuffer (void);
void SND_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void SND_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void SND_StopSound (int entnum, int entchannel);
sfx_t *SND_PrecacheSound (char *sample);
void SND_ClearPrecache (void);
void SND_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);
void SND_StopAllSounds (qboolean clear);
void SND_BeginPrecaching (void);
void SND_EndPrecaching (void);
void SND_ExtraUpdate (void);
void SND_LocalSound (char *s);

qboolean
SNDDMA_Init (void)
{
	if (snd_inited) {
		printf ("Sound already init'd\n");
		return 0;
	}

	shm = &sn;
	shm->splitbuffer = 0;

	audio_fd = open ("/dev/audio", O_WRONLY | O_NDELAY);

	if (audio_fd < 0) {
		if (errno == EBUSY) {
			Con_Printf ("Audio device is being used by another process\n");
		}
		perror ("/dev/audio");
		Con_Printf ("Could not open /dev/audio\n");
		return (0);
	}

	if (ioctl (audio_fd, AUDIO_GETINFO, &info) < 0) {
		perror ("/dev/audio");
		Con_Printf ("Could not communicate with audio device.\n");
		close (audio_fd);
		return 0;
	}
	// 
	// set to nonblock
	// 
	if (fcntl (audio_fd, F_SETFL, O_NONBLOCK) < 0) {
		perror ("/dev/audio");
		close (audio_fd);
		return 0;
	}

	AUDIO_INITINFO (&info);

	shm->speed = 11025;

	// try 16 bit stereo
	info.play.encoding = AUDIO_ENCODING_LINEAR;
	info.play.sample_rate = 11025;
	info.play.channels = 2;
	info.play.precision = 16;

	if (ioctl (audio_fd, AUDIO_SETINFO, &info) < 0) {
		info.play.encoding = AUDIO_ENCODING_LINEAR;
		info.play.sample_rate = 11025;
		info.play.channels = 1;
		info.play.precision = 16;
		if (ioctl (audio_fd, AUDIO_SETINFO, &info) < 0) {
			Con_Printf ("Incapable sound hardware.\n");
			close (audio_fd);
			return 0;
		}
		Con_Printf ("16 bit mono sound initialized\n");
		shm->samplebits = 16;
		shm->channels = 1;
	} else {							// 16 bit stereo
		Con_Printf ("16 bit stereo sound initialized\n");
		shm->samplebits = 16;
		shm->channels = 2;
	}

	shm->soundalive = true;
	shm->samples = sizeof (dma_buffer) / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) dma_buffer;

	snd_inited = 1;

	return 1;
}

int
SNDDMA_GetDMAPos (void)
{
	if (!snd_inited)
		return (0);

	if (ioctl (audio_fd, AUDIO_GETINFO, &info) < 0) {
		perror ("/dev/audio");
		Con_Printf ("Could not communicate with audio device.\n");
		close (audio_fd);
		snd_inited = 0;
		return (0);
	}

	return ((info.play.samples * shm->channels) % shm->samples);
}

int
SNDDMA_GetSamples (void)
{
	if (!snd_inited)
		return (0);

	if (ioctl (audio_fd, AUDIO_GETINFO, &info) < 0) {
		perror ("/dev/audio");
		Con_Printf ("Could not communicate with audio device.\n");
		close (audio_fd);
		snd_inited = 0;
		return (0);
	}

	return info.play.samples;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		close (audio_fd);
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
	static unsigned char writebuf[1024];
	unsigned char *p;
	int         idx;
	int         stop = paintedtime;

	if (paintedtime < wbufp)
		wbufp = 0;						// reset

	bsize = shm->channels * (shm->samplebits / 8);
	bytes = (paintedtime - wbufp) * bsize;

	if (!bytes)
		return;

	if (bytes > sizeof (writebuf)) {
		bytes = sizeof (writebuf);
		stop = wbufp + bytes / bsize;
	}

	p = writebuf;
	idx = (wbufp * bsize) & (BUFFER_SIZE - 1);

	for (b = bytes; b; b--) {
		*p++ = dma_buffer[idx];
		idx = (idx + 1) & (BUFFER_SIZE - 1);
	}

	wbufp = stop;

	if (write (audio_fd, writebuf, bytes) < bytes)
		printf ("audio can't keep up!\n");

}

plugin_t *
PluginInfo (void) {
    plugin_info.type = qfp_sound;
    plugin_info.api_version = QFPLUGIN_VERSION;
    plugin_info.plugin_version = "0.1";
    plugin_info.description = "SUN digital output";
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

    plugin_info_general_funcs.p_Init = SND_Init;
    plugin_info_general_funcs.p_Shutdown = SND_Shutdown;

    plugin_info_sound_funcs.pS_AmbientOff = SND_AmbientOff;
    plugin_info_sound_funcs.pS_AmbientOn = SND_AmbientOn;
    plugin_info_sound_funcs.pS_TouchSound = SND_TouchSound;
    plugin_info_sound_funcs.pS_ClearBuffer = SND_ClearBuffer;
    plugin_info_sound_funcs.pS_StaticSound = SND_StaticSound;
    plugin_info_sound_funcs.pS_StartSound = SND_StartSound;
    plugin_info_sound_funcs.pS_StopSound = SND_StopSound;
    plugin_info_sound_funcs.pS_PrecacheSound = SND_PrecacheSound;
    plugin_info_sound_funcs.pS_ClearPrecache = SND_ClearPrecache;
    plugin_info_sound_funcs.pS_Update = SND_Update;
    plugin_info_sound_funcs.pS_StopAllSounds = SND_StopAllSounds;
    plugin_info_sound_funcs.pS_BeginPrecaching = SND_BeginPrecaching;
    plugin_info_sound_funcs.pS_EndPrecaching = SND_EndPrecaching;
    plugin_info_sound_funcs.pS_ExtraUpdate = SND_ExtraUpdate;
    plugin_info_sound_funcs.pS_LocalSound = SND_LocalSound;

    return &plugin_info;
}
