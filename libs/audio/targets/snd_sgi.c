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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <dmedia/audio.h>
#include <stdlib.h>

#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sound.h"
#include "QF/sys.h"

static int  snd_inited = 0;
static ALconfig alc;
static ALport alp;

static int  tryrates[] = { 11025, 22050, 44100, 8000 };

static unsigned char *dma_buffer, *write_buffer;
static int  bufsize;
static int  wbufp;
static int  framecount;
static volatile dma_t sn;

static plugin_t           plugin_info;
static plugin_data_t      plugin_info_data;
static plugin_funcs_t     plugin_info_funcs;
static general_data_t     plugin_info_general_data;
static general_funcs_t    plugin_info_general_funcs;
static snd_output_data_t       plugin_info_snd_output_data;
static snd_output_funcs_t      plugin_info_snd_output_funcs;


static qboolean
SNDDMA_Init (void)
{
	char	   *s;
	int			i;
	ALpv		alpv;

	alc = alNewConfig ();

	if (!alc) {
		Sys_Printf ("Could not make an new sound config: %s\n",
					alGetErrorString (oserror ()));
		return 0;
	}

	shm = &sn;
	shm->splitbuffer = 0;

	/* get & probe settings */
	/* sample format */
	if (alSetSampFmt (alc, AL_SAMPFMT_TWOSCOMP) < 0) {
		Sys_Printf ("Could not sample format of default output to two's "
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
			Sys_Printf ("Could not get supported wordsize of default "
						"output: %s\n", alGetErrorString (oserror ()));
			return 0;
		}

		if (alpv.value.i >= 16) {
			shm->samplebits = 16;
		} else {
			if (alpv.value.i >= 8)
				shm->samplebits = 8;
			else {
				Sys_Printf ("Sound disabled since interface "
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
			Sys_Printf ("Sound disabled since interface doesn't even "
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
			Sys_Printf ("Unable to set number of channels to %d, "
						"trying half\n", shm->channels);
			shm->channels /= 2;
		} else
			break;
	}

	if (shm->channels <= 0) {
		Sys_Printf ("Sound disabled since interface doesn't even support 1 "
					"channel\n");
		alFreeConfig (alc);
		return 0;
	}

	/* sample rate */
	alpv.param = AL_RATE;
	alpv.value.ll = alDoubleToFixed (shm->speed);

	if (alSetParams (AL_DEFAULT_OUTPUT, &alpv, 1) < 0) {
		Sys_Printf ("Could not set samplerate of default output to %d: %s\n",
					shm->speed, alGetErrorString (oserror ()));
		alFreeConfig (alc);
		return 0;
	}

	/* set sizes of buffers relative to sizes of those for the 'standard'
	   frequency of 11025 use *huge* buffers since at least my indigo2
	   has enough to do to get sound on the way anyway
	*/
	bufsize = 32768 * (int) ((double) shm->speed / 11025.0);

	dma_buffer = malloc (bufsize);

	if (dma_buffer == NULL) {
		Sys_Printf ("Could not get %d bytes of memory for audio dma buffer\n",
					bufsize);
		alFreeConfig (alc);
		return 0;
	}

	write_buffer = malloc (bufsize);

	if (write_buffer == NULL) {
		Sys_Printf ("Could not get %d bytes of memory for audio write "
					"buffer\n", bufsize);
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
		Sys_Printf ("Could not set wordsize of default output to %d: %s\n",
					shm->samplebits, alGetErrorString (oserror ()));
		free (write_buffer);
		free (dma_buffer);
		alFreeConfig (alc);
		return 0;
	}

	alp = alOpenPort ("quakeforge", "w", alc);

	if (!alp) {
		Sys_Printf ("Could not open sound port: %s\n",
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

static void
SNDDMA_Init_Cvars (void)
{
}

static int
SNDDMA_GetDMAPos (void)
{
	/* Sys_Printf("framecount: %d %d\n", (framecount * shm->channels) %
	   shm->samples, alGetFilled(alp)); */
	shm->samplepos = ((framecount - alGetFilled (alp))
					  * shm->channels) % shm->samples;
	return shm->samplepos;
}

static void
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
static void
SNDDMA_Submit (void)
{
	unsigned char  *p;
	int				bsize, bytes, idx, b;
	int				stop = *plugin_info_snd_output_data.paintedtime;

	if (snd_blocked)
		return;

	if (*plugin_info_snd_output_data.paintedtime < wbufp)
		wbufp = 0;						// reset

	bsize = shm->channels * (shm->samplebits / 8);
	bytes = (*plugin_info_snd_output_data.paintedtime - wbufp) * bsize;

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

static void
SNDDMA_BlockSound (void)
{
}

static void
SNDDMA_UnblockSound (void)
{
}

QFPLUGIN plugin_t *PLUGIN_INFO(snd_output, sgi) (void);
QFPLUGIN plugin_t *
PLUGIN_INFO(snd_output, sgi) (void)
{
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "SGI IRIX digital output";
	plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
			"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
			"project\n"
			"Please see the file \"AUTHORS\" for a list of contributors";
	plugin_info.functions = &plugin_info_funcs;
	plugin_info.data = &plugin_info_data;

	plugin_info_data.general = &plugin_info_general_data;
	plugin_info_data.input = NULL;
	plugin_info_data.snd_output = &plugin_info_snd_output_data;

	plugin_info_funcs.general = &plugin_info_general_funcs;
	plugin_info_funcs.input = NULL;
	plugin_info_funcs.snd_output = &plugin_info_snd_output_funcs;

	plugin_info_general_funcs.p_Init = SNDDMA_Init_Cvars;
	plugin_info_general_funcs.p_Shutdown = NULL;
	plugin_info_snd_output_funcs.pS_O_Init = SNDDMA_Init;
	plugin_info_snd_output_funcs.pS_O_Shutdown = SNDDMA_Shutdown;
	plugin_info_snd_output_funcs.pS_O_GetDMAPos = SNDDMA_GetDMAPos;
	plugin_info_snd_output_funcs.pS_O_Submit = SNDDMA_Submit;
	plugin_info_snd_output_funcs.pS_O_BlockSound = SNDDMA_BlockSound;
	plugin_info_snd_output_funcs.pS_O_UnblockSound = SNDDMA_UnblockSound;

	return &plugin_info;
}

