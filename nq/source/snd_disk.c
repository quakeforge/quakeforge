/*
	snd_disk.c

	write sound to a disk file

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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>

#include "QF/console.h"
#include "sound.h"
#include "QF/qargs.h"

static int  snd_inited;
QFile      *snd_file;

qboolean
SNDDMA_Init (void)
{
	shm = &sn;
	memset ((dma_t *) shm, 0, sizeof (*shm));
	shm->splitbuffer = 0;
	shm->channels = 2;
	shm->submission_chunk = 1;			// don't mix less than this #
	shm->samplepos = 0;					// in mono samples
	shm->samplebits = 16;
	shm->samples = 16384;				// mono samples in buffer
	shm->speed = 44100;
	shm->buffer = malloc (shm->samples * shm->channels * shm->samplebits / 8);

	Con_Printf ("%5d stereo\n", shm->channels - 1);
	Con_Printf ("%5d samples\n", shm->samples);
	Con_Printf ("%5d samplepos\n", shm->samplepos);
	Con_Printf ("%5d samplebits\n", shm->samplebits);
	Con_Printf ("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf ("%5d speed\n", shm->speed);
	Con_Printf ("0x%x dma buffer\n", (int) shm->buffer);
	Con_Printf ("%5d total_channels\n", total_channels);

	if (!(snd_file = Qopen ("qf.raw", "wb")))
		return 0;

	snd_inited = 1;
	return 1;
}

int
SNDDMA_GetDMAPos (void)
{
	shm->samplepos = 0;
	return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		Qclose (snd_file);
		snd_file = 0;
		free (shm->buffer);
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
	int         count = (paintedtime - soundtime) * shm->samplebits / 8;

	Qwrite (snd_file, shm->buffer, count);
}
