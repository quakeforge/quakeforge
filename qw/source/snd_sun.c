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

#include "qtypes.h"
#include "sound.h"
#include "qargs.h"
#include "console.h"

int         audio_fd;
int         snd_inited;

static int  wbufp;
static audio_info_t info;

#define BUFFER_SIZE		8192

unsigned char dma_buffer[BUFFER_SIZE];
unsigned char pend_buffer[BUFFER_SIZE];
int         pending;

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
