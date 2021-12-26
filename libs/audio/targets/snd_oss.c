/*
	snd_oss.c

	OSS sound output plugin.

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

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#elif defined HAVE_LINUX_SOUNDCARD_H
# include <linux/soundcard.h>
#elif HAVE_MACHINE_SOUNDCARD_H
# include <machine/soundcard.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "snd_internal.h"

#ifndef MAP_FAILED
# define MAP_FAILED ((void *) -1)
#endif

static int			audio_fd;
static int			snd_inited;
static int			mmaped_io = 0;
static const char  *snd_dev = "/dev/dsp";

static int			tryrates[] = { 44100, 48000, 11025, 22050, 22051, 44100, 8000 };

static cvar_t	   *snd_stereo;
static cvar_t	   *snd_rate;
static cvar_t	   *snd_device;
static cvar_t	   *snd_bits;
static cvar_t	   *snd_oss_mmaped;

static plugin_t           plugin_info;
static plugin_data_t      plugin_info_data;
static plugin_funcs_t     plugin_info_funcs;
static general_data_t     plugin_info_general_data;
static general_funcs_t    plugin_info_general_funcs;
static snd_output_data_t       plugin_info_snd_output_data;
static snd_output_funcs_t      plugin_info_snd_output_funcs;


static void
SNDDMA_Init_Cvars (void)
{
	snd_stereo = Cvar_Get ("snd_stereo", "1", CVAR_ROM, NULL,
						   "sound stereo output");
	snd_rate = Cvar_Get ("snd_rate", "0", CVAR_ROM, NULL,
						 "sound playback rate. 0 is system default");
	snd_device = Cvar_Get ("snd_device", "", CVAR_ROM, NULL,
						   "sound device. \"\" is system default");
	snd_bits = Cvar_Get ("snd_bits", "0", CVAR_ROM, NULL,
						 "sound sample depth. 0 is system default");
	snd_oss_mmaped = Cvar_Get ("snd_oss_mmaped", "1", CVAR_ROM, NULL,
							   "mmaped io");
}

static int
try_open (snd_t *snd, int rw)
{
	int         caps, fmt, rc, tmp, i;
	int		    retries = 3;
	int         omode = O_WRONLY;
	int         mmmode = PROT_WRITE;
	int         mmflags = MAP_FILE;
	struct audio_buf_info info;

	snd_inited = 0;
	mmaped_io = snd_oss_mmaped->int_val;

	// open snd_dev, confirm capability to mmap, and get size of dma buffer
	if (snd_device->string[0])
		snd_dev = snd_device->string;

	if (rw) {
		omode = O_RDWR;
		mmmode |= PROT_READ;
		mmflags |= MAP_SHARED;
	}
	omode |= O_NONBLOCK;

	audio_fd = open (snd_dev, omode);
	if (audio_fd < 0) {					// Failed open, retry up to 3 times
		// if it's busy
		while ((audio_fd < 0) && retries-- &&
			   ((errno == EAGAIN) || (errno == EBUSY))) {
			sleep (1);
			audio_fd = open (snd_dev, omode);
		}
		if (audio_fd < 0) {
			Sys_Printf ("Could not open %s: %s\n", snd_dev, strerror (errno));
			return 0;
		}
	}

	if ((rc = ioctl (audio_fd, SNDCTL_DSP_RESET, 0)) < 0) {
		Sys_Printf ("Could not reset %s: %s\n", snd_dev, strerror (errno));
		close (audio_fd);
		return 0;
	}

	if (ioctl (audio_fd, SNDCTL_DSP_GETCAPS, &caps) == -1) {
		Sys_Printf ("Sound driver too old: %s\n", strerror (errno));
		close (audio_fd);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP)) {
		Sys_Printf ("Sound device can't do memory-mapped I/O.\n");
		close (audio_fd);
		return 0;
	}

	// set sample bits & speed
	snd->samplebits = snd_bits->int_val;

	if (snd->samplebits != 16 && snd->samplebits != 8) {
		ioctl (audio_fd, SNDCTL_DSP_GETFMTS, &fmt);

		if (fmt & AFMT_S16_LE) {		// little-endian 16-bit signed
			snd->samplebits = 16;
		} else {
			if (fmt & AFMT_U8) {		// unsigned 8-bit ulaw
				snd->samplebits = 8;
			}
		}
	}

	if (snd->samplebits == 16) {
		rc = AFMT_S16_LE;
		rc = ioctl (audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0) {
			Sys_Printf ("Could not support 16-bit data.  Try 8-bit. %s\n",
						strerror (errno));
			close (audio_fd);
			return 0;
		}
	} else if (snd->samplebits == 8) {
		rc = AFMT_U8;
		rc = ioctl (audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0) {
			Sys_Printf ("Could not support 8-bit data. %s\n",
						strerror (errno));
			close (audio_fd);
			return 0;
		}
	} else {
		Sys_Printf ("%d-bit sound not supported. %s", snd->samplebits,
					strerror (errno));
		close (audio_fd);
		return 0;
	}

	tmp = snd->channels;
	rc = ioctl (audio_fd, SNDCTL_DSP_CHANNELS, &tmp);
	if (rc < 0) {
		Sys_Printf ("Could not set %s to stereo=%d: %s\n", snd_dev,
					snd->channels, strerror (errno));
		close (audio_fd);
		return 0;
	}

	if (snd_rate->int_val) {
		snd->speed = snd_rate->int_val;
	} else {
		for (i = 0; i < ((int) sizeof (tryrates) / 4); i++)
			if (!ioctl (audio_fd, SNDCTL_DSP_SPEED, &tryrates[i]))
				break;
		snd->speed = tryrates[i];
	}

	if (!snd_stereo->int_val) {
		snd->channels = 1;
	} else {
		snd->channels = 2;
	}

	rc = ioctl (audio_fd, SNDCTL_DSP_SPEED, &snd->speed);
	if (rc < 0) {
		Sys_Printf ("Could not set %s speed to %d: %s\n", snd_dev, snd->speed,
					strerror (errno));
		close (audio_fd);
		return 0;
	}

	if (ioctl (audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
		Sys_Printf ("Um, can't do GETOSPACE?: %s\n", strerror (errno));
		close (audio_fd);
		return 0;
	}

	snd->frames = info.fragstotal * info.fragsize;
	snd->frames /= snd->channels * snd->samplebits / 8;
	snd->submission_chunk = 1;

	if (mmaped_io) {				// memory map the dma buffer
		unsigned long sz = sysconf (_SC_PAGESIZE);
		unsigned long len = info.fragstotal * info.fragsize;

		len = (len + sz - 1) & ~(sz - 1);
		snd->buffer = (byte *) mmap (NULL, len, mmmode, mmflags, audio_fd, 0);
		if (snd->buffer == MAP_FAILED) {
			Sys_MaskPrintf (SYS_snd, "Could not mmap %s: %s\n", snd_dev,
							strerror (errno));
			close (audio_fd);
			return 0;
		}
	} else {
		snd->buffer = malloc (snd->frames * snd->channels * (snd->samplebits / 8));
		if (!snd->buffer) {
			Sys_Printf ("SNDDMA_Init: memory allocation failure\n");
			close (audio_fd);
			return 0;
		}
	}

	// toggle the trigger & start her up
	tmp = 0;
	rc = ioctl (audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		Sys_Printf ("Could not toggle.: %s\n", strerror (errno));
		if (mmaped_io)
			munmap (snd->buffer, snd->frames * snd->channels * snd->samplebits / 8);
		close (audio_fd);
		return 0;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl (audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0) {
		Sys_Printf ("Could not toggle.: %s\n", strerror (errno));
		if (mmaped_io)
			munmap (snd->buffer, snd->frames * snd->channels * snd->samplebits / 8);
		close (audio_fd);
		return 0;
	}

	snd->framepos = 0;

	snd_inited = 1;
	return 1;
}

static int
SNDDMA_Init (snd_t *snd)
{
	return try_open (snd, 0) || try_open (snd, 1);
}

static int
SNDDMA_GetDMAPos (snd_t *snd)
{
	struct count_info count;

	if (!snd_inited)
		return 0;

	if (ioctl (audio_fd, SNDCTL_DSP_GETOPTR, &count) == -1) {
		Sys_Printf ("Uh, %s dead: %s\n", snd_dev, strerror (errno));
		if (mmaped_io)
			munmap (snd->buffer, snd->frames * snd->channels * snd->samplebits / 8);
		close (audio_fd);
		snd_inited = 0;
		return 0;
	}
	snd->framepos = count.ptr / (snd->channels * snd->samplebits / 8);

	return snd->framepos;

}

static void
SNDDMA_shutdown (snd_t *snd)
{
	if (snd_inited) {
		if (mmaped_io)
			munmap (snd->buffer, snd->frames * snd->channels * snd->samplebits / 8);
		close (audio_fd);
		snd_inited = 0;
	}
}

static int
sample_bytes (snd_t *snd, int frames)
{
	return frames * snd->channels * snd->samplebits / 8;
}

/*
	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer
*/
static void
SNDDMA_Submit (snd_t *snd)
{
	int		frames;
	int     len;
	int     offset;

	if (snd_inited && !mmaped_io) {
		frames = *plugin_info_snd_output_data.paintedtime
			- *plugin_info_snd_output_data.soundtime;
		offset = frames * snd->channels * snd->samplebits / 8;

		if (snd->framepos + frames <= snd->frames) {
			len = sample_bytes (snd, frames);
			if (write (audio_fd, snd->buffer + offset, len) != len)
				Sys_Printf ("SNDDMA_Submit(): %s\n", strerror (errno));
		} else {
			int     len = sample_bytes (snd, snd->frames - snd->framepos);
			if (write (audio_fd, snd->buffer + offset, len) != len)
				Sys_Printf ("SNDDMA_Submit(): %s\n", strerror (errno));
			if (write (audio_fd, snd->buffer, offset - len) != offset - len)
				Sys_Printf ("SNDDMA_Submit(): %s\n", strerror (errno));
		}
		*plugin_info_snd_output_data.soundtime += frames;
	}
}

static void
SNDDMA_BlockSound (snd_t *snd)
{
}

static void
SNDDMA_UnblockSound (snd_t *snd)
{
}

PLUGIN_INFO(snd_output, oss)
{
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "OSS digital output";
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

	plugin_info_general_funcs.init = SNDDMA_Init_Cvars;
	plugin_info_general_funcs.shutdown = NULL;
	plugin_info_snd_output_funcs.init = SNDDMA_Init;
	plugin_info_snd_output_funcs.shutdown = SNDDMA_shutdown;
	plugin_info_snd_output_funcs.get_dma_pos = SNDDMA_GetDMAPos;
	plugin_info_snd_output_funcs.submit = SNDDMA_Submit;
	plugin_info_snd_output_funcs.block_sound = SNDDMA_BlockSound;
	plugin_info_snd_output_funcs.unblock_sound = SNDDMA_UnblockSound;

	return &plugin_info;
}
