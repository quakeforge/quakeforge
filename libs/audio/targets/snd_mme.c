/*
	snd_mme.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000	 Marcus Sundberg [mackan@stacken.kth.se]
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

#include <mme/mmsystem.h>
#ifdef HAVE_MME_MME_PUBLIC_H
# include <mme/mme_public.h>
#endif

#include "console.h"
#include "sound.h"

// 64K is > 1 second at 16-bit, 11025 Hz
#define	WAV_BUFFERS		64
#define	WAV_MASK		0x3F
#define	WAV_BUFFER_SIZE		0x0400

static qboolean	wav_init;
static qboolean	snd_firsttime = true, snd_iswave;

static int	sample16;
static int	snd_sent, snd_completed;

static HPSTR		lpData;
static LPWAVEHDR	lpWaveHdr;
static HWAVEOUT		hWaveOut;
static DWORD		gSndBufSize;


/* MME Callback function
 */
static void
mme_callback ( HANDLE h, UINT wMsg, DWORD instance, LPARAM p1, LPARAM p2 )
{
	if (wMsg == WOM_DONE) {
		snd_completed++;
	}
}


/*
==================
S_BlockSound
==================
*/
void
S_BlockSound ( void )
{
	snd_blocked++;

	if (snd_blocked == 1) {
		waveOutReset(hWaveOut);
	}
}


/*
==================
S_UnblockSound
==================
*/
void
S_UnblockSound ( void )
{
	snd_blocked--;
}


/*
==================
FreeSound
==================
*/
void
FreeSound ( void )
{
// only release primary buffer if it's not also the mixing buffer we just released
	if (hWaveOut) {
		waveOutReset (hWaveOut);

		waveOutClose (hWaveOut);

		if (lpWaveHdr) {
			mmeFreeMem(lpWaveHdr);
		}

		if (lpData) {
			mmeFreeBuffer(lpData);
		}
	}

	hWaveOut = 0;
	lpWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	wav_init = false;
}


/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
qboolean
SNDDMA_InitWav ( void )
{
	LPPCMWAVEFORMAT format;
	int i, hr;

	if ((format = (LPPCMWAVEFORMAT)
		mmeAllocMem(sizeof(*format))) == NULL) {
		Con_Printf("Failed to allocate PCMWAVEFORMAT struct\n");
		return false;
	}

	snd_sent = 0;
	snd_completed = 0;

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;

	memset(format, 0, sizeof(*format));
	format->wf.wFormatTag = WAVE_FORMAT_PCM;
	format->wf.nChannels = shm->channels;
	format->wBitsPerSample = shm->samplebits;
	format->wf.nSamplesPerSec = shm->speed;
	format->wf.nBlockAlign = format->wf.nChannels
		*format->wBitsPerSample / 8;
	format->wf.nAvgBytesPerSec = format->wf.nSamplesPerSec
		*format->wf.nBlockAlign;

	/* Open a waveform device for output using our callback function. */
	while ((hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER,
				 (LPWAVEFORMAT)format,
				 (void (*)())mme_callback, 0,
				 CALLBACK_FUNCTION | WAVE_OPEN_SHAREABLE))
			!= MMSYSERR_NOERROR)
	{
		if (hr != MMSYSERR_ALLOCATED) {
			mmeFreeMem(format);
			Con_Printf ("waveOutOpen failed: %d\n", hr);
			return false;
		} else {
			Con_Printf ("waveOutOpen failed 2222\n");
		}
	}
	mmeFreeMem(format);

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.

	*/
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	lpData = mmeAllocBuffer(gSndBufSize);
	if (!lpData) {
		Con_Printf ("Sound: Out of memory.\n");
		FreeSound ();
		return false;
	}
	memset (lpData, 0, gSndBufSize);

	/*
	 * Allocate and lock memory for the header. This memory must
	 * also be globally allocated with GMEM_MOVEABLE and
	 * GMEM_SHARE flags.
	 */
	lpWaveHdr = mmeAllocMem(sizeof(WAVEHDR) * WAV_BUFFERS);

	if (lpWaveHdr == NULL)
	{
		Con_Printf ("Sound: Failed to Alloc header.\n");
		FreeSound ();
		return false;
	}

	memset (lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */
	for (i=0 ; i<WAV_BUFFERS ; i++)	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i*WAV_BUFFER_SIZE;
	}

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize/(shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits/8) - 1;

	wav_init = true;

	return true;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/

qboolean
SNDDMA_Init ( void )
{
	wav_init = 0;

	if (snd_firsttime || snd_iswave) {

		snd_iswave = SNDDMA_InitWav ();

		if (snd_iswave) {
			if (snd_firsttime)
				Con_Printf ("Wave sound initialized\n");
		} else {
			Con_Printf ("Wave sound failed to init\n");
		}
	}

	snd_firsttime = false;

	if (!wav_init) {
		if (snd_firsttime)
			Con_Printf ("No sound device initialized\n");

		return 0;
	}

	return 1;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int
SNDDMA_GetDMAPos ( void )
{
	int	s = 0;

	if (wav_init) {
		s = snd_sent * WAV_BUFFER_SIZE;
	}

	s >>= sample16;

	s &= (shm->samples-1);

	return s;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void
SNDDMA_Submit ( void )
{
	LPWAVEHDR	h;
	int			wResult;

	if (!wav_init) return;

	if (mmeCheckForCallbacks()) mmeProcessCallbacks();

	if (snd_completed == snd_sent) {
		Con_DPrintf ("Sound overrun\n");
	}

	//
	// submit two new sound blocks
	//
	while (((snd_sent - snd_completed) >> sample16) < 4)
	{
		int i = (snd_sent&WAV_MASK);

		h = lpWaveHdr + i;

		h->dwBufferLength = WAV_BUFFER_SIZE;
		h->lpData = lpData + i*WAV_BUFFER_SIZE;

		snd_sent++;
		/*
		 * Now the data block can be sent to the output device. The
		 * waveOutWrite function returns immediately and waveform
		 * data is sent to the output device in the background.
		 */
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR)
		{
			Con_Printf ("Failed to write block to device\n");
			FreeSound ();
			return;
		}
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void
SNDDMA_Shutdown ( void )
{
	FreeSound ();
}
