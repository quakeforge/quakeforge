/*
	snd_alsa_0_6.c

	(description)

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

#include <stdio.h>

#include <sys/asoundlib.h>

#include "console.h"
#include "qargs.h"
#include "sound.h"
#include "va.h"

static int  snd_inited;

static snd_pcm_t *pcm_handle;
static snd_pcm_hw_info_t hwinfo;
static snd_pcm_hw_params_t hwparams;
static snd_pcm_sw_params_t swparams;
static const snd_pcm_channel_area_t *mmap_running_areas;
static int  card = -1, dev = -1;

int
check_card (int card)
{
	snd_ctl_t  *handle;
	int         rc;

	if ((rc = snd_ctl_open (&handle, va ("hw:%d", card))) < 0) {
		Con_Printf ("Error: control open (%i): %s\n", card, snd_strerror (rc));
		return rc;
	}
	if (dev == -1) {
		while (1) {
			 if ((rc = snd_ctl_pcm_next_device (handle, &dev)) < 0) {
			 	Con_Printf ("Error: next device: %s\n", snd_strerror (rc));
				return rc;
			}
			if (dev < 0)
				break;
			if ((rc = snd_pcm_open (&pcm_handle, va ("hw:%d,%d", card, dev),
									SND_PCM_STREAM_PLAYBACK,
									SND_PCM_NONBLOCK)) == 0) {
				goto exit;
			}
		}
	} else {
		if ((rc = snd_pcm_open (&pcm_handle, va ("hw:%d,%d", card, dev),
								SND_PCM_STREAM_PLAYBACK,
								SND_PCM_NONBLOCK)) == 0) {
			goto exit;
		}
		Con_Printf ("Error: snd_pcm_open %d: %s\n", dev, snd_strerror (rc));
		goto exit;
	}
	rc = 1;
exit:
	snd_ctl_close(handle);
	return rc;
}

qboolean
SNDDMA_Init (void)
{
	int         rc = 0, i;
	char       *err_msg = "";
	int         rate = -1, format = -1, stereo = -1, frag_size;

	if ((i = COM_CheckParm ("-sndcard")) != 0) {
		card = atoi (com_argv[i + 1]);
	}
	if ((i = COM_CheckParm ("-snddev")) != 0) {
		dev = atoi (com_argv[i + 1]);
	}
	if ((i = COM_CheckParm ("-sndbits")) != 0) {
		i = atoi (com_argv[i + 1]);
		if (i == 16) {
			format = SND_PCM_FMTBIT_S16_LE;
		} else if (i == 8) {
			format = SND_PCM_FMTBIT_U8;
		} else {
			Con_Printf ("Error: invalid sample bits: %d\n", i);
			return 0;
		}
	}
	if ((i = COM_CheckParm ("-sndspeed")) != 0) {
		rate = atoi (com_argv[i + 1]);
		if (rate != 44100 && rate != 22050 && rate != 11025) {
			Con_Printf ("Error: invalid sample rate: %d\n", rate);
			return 0;
		}
	}
	if ((i = COM_CheckParm ("-sndmono")) != 0) {
		stereo = 0;
	}
	if (card == -1) {
		if (snd_card_next(&card) < 0 || card < 0) {
			Con_Printf ("No sound cards detected\n");
			return 0;
		}
		while (card >= 0) {
			rc = check_card (card);
			if (rc < 0)
				return 0;
			if (!rc)
				goto dev_openned;
		}
	} else {
		if (dev == -1) {
			rc = check_card (card);
			if (rc < 0)
				return 0;
			if (!rc)
				goto dev_openned;
		} else {
			if ((rc = snd_pcm_open (&pcm_handle, va ("hw:%d,%d", card, dev),
									SND_PCM_STREAM_PLAYBACK,
									SND_PCM_NONBLOCK)) == 0) {
				Con_Printf ("Error: audio open error: %s\n", snd_strerror (rc));
				return 0;
			}
			goto dev_openned;
		}
	}
	Con_Printf ("Error: audio open error: %s\n", snd_strerror (rc));
	return 0;

  dev_openned:
	Con_Printf ("Using card %d, device %d.\n", card, dev);
	memset (&hwinfo, 0, sizeof (hwinfo));
	snd_pcm_hw_info_any (&hwinfo);
	hwinfo.access_mask = SND_PCM_ACCBIT_MMAP_INTERLEAVED;
	err_msg = "snd_pcm_hw_info";
	if ((rc = snd_pcm_hw_info (pcm_handle, &hwinfo)) < 0)
		goto error;
	Con_Printf ("%08x %08x\n", hwinfo.access_mask, hwinfo.format_mask);
	rate = 44100;
	frag_size = 4;
#if 0									// XXX
	if ((rate == -1 || rate == 44100) && hwinfo.rates & SND_PCM_RATE_44100) {
		rate = 44100;
		frag_size = 256;				/* assuming stereo 8 bit */
	} else if ((rate == -1 || rate == 22050)
			   && hwinfo.rates & SND_PCM_RATE_22050) {
		rate = 22050;
		frag_size = 128;				/* assuming stereo 8 bit */
	} else if ((rate == -1 || rate == 11025)
			   && hwinfo.rates & SND_PCM_RATE_11025) {
		rate = 11025;
		frag_size = 64;					/* assuming stereo 8 bit */
	} else {
		Con_Printf ("ALSA: desired rates not supported\n");
		goto error_2;
	}
#endif
	if ((format == -1 || format == SND_PCM_FMTBIT_S16_LE)
		&& hwinfo.format_mask & SND_PCM_FORMAT_S16_LE) {
		format = SND_PCM_FORMAT_S16_LE;
	} else if ((format == -1 || format == SND_PCM_FORMAT_U8)
			   && hwinfo.format_mask & SND_PCM_FORMAT_U8) {
		format = SND_PCM_FORMAT_U8;
	} else {
		Con_Printf ("ALSA: desired formats not supported\n");
		goto error_2;
	}
	// XXX can't support non-interleaved stereo
	if (stereo && (hwinfo.access_mask & SND_PCM_ACCBIT_MMAP_INTERLEAVED)
		&& hwinfo.channels_max >= 2) {
		stereo = 1;
	} else {
		stereo = 0;
	}

	memset (&hwparams, 0, sizeof (hwparams));
	// XXX can't support non-interleaved stereo
	hwparams.access = stereo ? SND_PCM_ACCESS_MMAP_INTERLEAVED
		: SND_PCM_ACCESS_MMAP_NONINTERLEAVED;
	hwparams.format = format;
	hwparams.rate = rate;
	hwparams.channels = stereo + 1;

	hwparams.fragment_size = 32;
	hwparams.fragments = 512;

	err_msg = "snd_pcm_hw_params";
	if ((rc = snd_pcm_hw_params (pcm_handle, &hwparams)) < 0) {
		Con_Printf("failed: %08x\n", hwparams.fail_mask);
		goto error;
	}

	memset (&swparams, 0, sizeof (swparams));
	swparams.start_mode = SND_PCM_START_EXPLICIT;
	swparams.xrun_mode = SND_PCM_XRUN_FRAGMENT;
	swparams.xfer_min = 1;
	swparams.xfer_align = 1;
	err_msg = "snd_pcm_sw_params";
	if ((rc = snd_pcm_sw_params (pcm_handle, &swparams)) < 0) {
		Con_Printf("failed: %08x\n", swparams.fail_mask);
		goto error;
	}

	err_msg = "audio prepare";
	if ((rc = snd_pcm_prepare (pcm_handle)) < 0)
		goto error;

	mmap_running_areas = snd_pcm_mmap_areas (pcm_handle);

	shm = &sn;
	memset ((dma_t *) shm, 0, sizeof (*shm));
	shm->splitbuffer = 0;
	shm->channels = hwparams.channels;
	shm->submission_chunk = frag_size;	// don't mix less than this #
	shm->samplepos = 0;					// in mono samples
	shm->samplebits = hwparams.format == SND_PCM_FORMAT_S16_LE ? 16 : 8;
	shm->samples = hwparams.fragment_size * hwparams.fragments
	               * shm->channels;		// mono samples in buffer
	shm->speed = hwparams.rate;
	shm->buffer = (unsigned char *) mmap_running_areas->addr;
	Con_Printf ("%5d stereo\n", shm->channels - 1);
	Con_Printf ("%5d samples\n", shm->samples);
	Con_Printf ("%5d samplepos\n", shm->samplepos);
	Con_Printf ("%5d samplebits\n", shm->samplebits);
	Con_Printf ("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf ("%5d speed\n", shm->speed);
	Con_Printf ("0x%x dma buffer\n", (int) shm->buffer);
	Con_Printf ("%5d total_channels\n", total_channels);

	snd_inited = 1;
	return 1;
  error:
	Con_Printf ("Error: %s: %s\n", err_msg, snd_strerror (rc));
  error_2:
	snd_pcm_close (pcm_handle);
	return 0;
}

static inline int
get_hw_ptr ()
{
	size_t      app_ptr;
	ssize_t     delay;
	int         hw_ptr;

	if (snd_pcm_state (pcm_handle) != SND_PCM_STATE_RUNNING)
		return 0;
	app_ptr = snd_pcm_mmap_offset (pcm_handle);
	snd_pcm_delay (pcm_handle, &delay);
	hw_ptr = app_ptr - delay;
	return hw_ptr;
}

int
SNDDMA_GetDMAPos (void)
{
	int         hw_ptr;

	if (!snd_inited)
		return 0;

	hw_ptr = get_hw_ptr ();
	hw_ptr *= shm->channels;
	shm->samplepos = hw_ptr;
	return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		snd_pcm_close (pcm_handle);
		snd_inited = 0;
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void
SNDDMA_Submit (void)
{
	int         count = paintedtime - soundtime;
	int         avail;
	int         missed;
	int         state;
	int         hw_ptr;
	int         offset;

	state = snd_pcm_state (pcm_handle);

	switch (state) {
		case SND_PCM_STATE_XRUN:
			//snd_pcm_reset (pcm_handle);
			snd_pcm_prepare (pcm_handle);
			//break;
		case SND_PCM_STATE_PREPARED:
			snd_pcm_mmap_forward (pcm_handle, count);
			snd_pcm_start (pcm_handle);
			//break;
		case SND_PCM_STATE_RUNNING:
			hw_ptr = get_hw_ptr ();
			missed = hw_ptr - shm->samplepos / shm->channels;
			count -= missed;
			offset = snd_pcm_mmap_offset (pcm_handle);
			if (offset > hw_ptr)
				count -= (offset - hw_ptr);
			else
				count -= (hwparams.fragments - hw_ptr + offset);
			if (count < 0) {
				snd_pcm_rewind (pcm_handle, -count);
			} else if (count > 0) {
				avail = snd_pcm_avail_update (pcm_handle);
				if (avail > 0 && count > avail) {
					snd_pcm_mmap_forward (pcm_handle, avail);
					count -= avail;
				}
				snd_pcm_mmap_forward (pcm_handle, count);
			}
			break;
		default:
			printf("snd_alsa: nexpected state: %d\n", state);
			break;
	}
}
