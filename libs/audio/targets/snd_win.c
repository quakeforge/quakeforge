/*
	snd_win.c

	(description)

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define CINTERFACE

#include "winquake.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"

#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

HRESULT (WINAPI * pDirectSoundCreate) (GUID FAR * lpGUID,
									   LPDIRECTSOUND FAR * lplpDS,
									   IUnknown FAR * pUnkOuter);

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

typedef enum { SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL } sndinitstat;

static qboolean wavonly;
static qboolean dsound_init;
static qboolean wav_init;
static qboolean snd_firsttime = true, snd_isdirect, snd_iswave;
static qboolean primary_format_set;

static int  sample16;
static int  snd_sent, snd_completed;
static int snd_blocked = 0;
static volatile dma_t sn;

/*
  Global variables. Must be visible to window-procedure function
  so it can unlock and free the data block after it has been played.
*/

static HANDLE      hData;
static HPSTR       lpData;//, lpData2;

static HGLOBAL     hWaveHdr;
static LPWAVEHDR   lpWaveHdr;

static HWAVEOUT    hWaveOut;

//static WAVEOUTCAPS wavecaps;

static DWORD       gSndBufSize;

static MMTIME      mmstarttime;

static LPDIRECTSOUND pDS;
static LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

static HINSTANCE   hInstDS;

static sndinitstat SNDDMA_InitDirect (void);
static qboolean    SNDDMA_InitWav (void);

static plugin_t				plugin_info;
static plugin_data_t		plugin_info_data;
static plugin_funcs_t		plugin_info_funcs;
static general_data_t		plugin_info_general_data;
static general_funcs_t		plugin_info_general_funcs;
static snd_output_data_t	plugin_info_snd_output_data;
static snd_output_funcs_t	plugin_info_snd_output_funcs;



static void
SNDDMA_Init_Cvars (void)
{
}

void
SNDDMA_BlockSound (void)
{
	// DirectSound takes care of blocking itself
	if (snd_iswave)
		if (++snd_blocked == 1)
			waveOutReset (hWaveOut);
}

void
SNDDMA_UnblockSound (void)
{
	// DirectSound takes care of blocking itself
	if (snd_iswave)
		if (!snd_blocked)
			--snd_blocked;
}

static void
FreeSound (void)
{
	int         i;

	if (pDSBuf) {
		IDirectSoundBuffer_Stop (pDSBuf);
		IDirectSound_Release (pDSBuf);
	}
// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf)) {
		IDirectSound_Release (pDSPBuf);
	}

	if (pDS) {
		IDirectSound_SetCooperativeLevel (pDS, mainwindow, DSSCL_NORMAL);
		IDirectSound_Release (pDS);
	}

	if (hWaveOut) {
		waveOutReset (hWaveOut);

		if (lpWaveHdr) {
			for (i = 0; i < WAV_BUFFERS; i++)
				waveOutUnprepareHeader (hWaveOut, lpWaveHdr + i,
										sizeof (WAVEHDR));
		}

		waveOutClose (hWaveOut);

		if (hWaveHdr) {
			GlobalUnlock (hWaveHdr);
			GlobalFree (hWaveHdr);
		}

		if (hData) {
			GlobalUnlock (hData);
			GlobalFree (hData);
		}

	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}

/*
	SNDDMA_InitDirect

	Direct-Sound support
*/
static sndinitstat
SNDDMA_InitDirect (void)
{
	int				reps;
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DSCAPS			dscaps;
	DWORD			dwSize, dwWrite;
	HRESULT			hresult;
	WAVEFORMATEX	format, pformat;

	memset ((void *) &sn, 0, sizeof (sn));

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;

	memset (&format, 0, sizeof (format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	if (!hInstDS) {
		hInstDS = LoadLibrary ("dsound.dll");

		if (hInstDS == NULL) {
			Sys_Printf ("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate =
			(void *) GetProcAddress (hInstDS, "DirectSoundCreate");

		if (!pDirectSoundCreate) {
			Sys_Printf ("Couldn't get DS proc addr\n");
			return SIS_FAILURE;
		}
	}

	while ((hresult = iDirectSoundCreate (NULL, &pDS, NULL)) != DS_OK) {
		if (hresult != DSERR_ALLOCATED) {
			Sys_Printf ("DirectSound create failed\n");
			return SIS_FAILURE;
		}
		Sys_Printf ("DirectSoundCreate failure\n"
					"  hardware already in use\n");
		return SIS_NOTAVAIL;
	}

	dscaps.dwSize = sizeof (dscaps);
	if (DS_OK != IDirectSound_GetCaps (pDS, &dscaps)) {
		Sys_Printf ("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER) {
		Sys_Printf ("No DirectSound driver installed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

	if (DS_OK !=
		IDirectSound_SetCooperativeLevel (pDS, mainwindow, DSSCL_EXCLUSIVE)) {
		Sys_Printf ("Set coop level failed\n");
		FreeSound ();
		return SIS_FAILURE;
	}
	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	memset (&dsbuf, 0, sizeof (dsbuf));
	dsbuf.dwSize = sizeof (DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset (&dsbcaps, 0, sizeof (dsbcaps));
	dsbcaps.dwSize = sizeof (dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm ("-snoforceformat")) {
		if (DS_OK ==
			IDirectSound_CreateSoundBuffer (pDS, &dsbuf, &pDSPBuf, NULL)) {
			pformat = format;

			if (DS_OK != IDirectSoundBuffer_SetFormat (pDSPBuf, &pformat)) {
			} else
				primary_format_set = true;
		}
	}

	if (!primary_format_set || !COM_CheckParm ("-primarysound")) {
		// create the secondary buffer we'll actually work with
		memset (&dsbuf, 0, sizeof (dsbuf));
		dsbuf.dwSize = sizeof (DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset (&dsbcaps, 0, sizeof (dsbcaps));
		dsbcaps.dwSize = sizeof (dsbcaps);

		if (DS_OK !=
			IDirectSound_CreateSoundBuffer (pDS, &dsbuf, &pDSBuf, NULL)) {
			Sys_Printf ("DS:CreateSoundBuffer Failed");
			FreeSound ();
			return SIS_FAILURE;
		}

		shm->channels = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->speed = format.nSamplesPerSec;

		if (DS_OK != IDirectSound_GetCaps (pDSBuf, &dsbcaps)) {
			Sys_Printf ("DS:GetCaps failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}
	} else {
		if (DS_OK !=
			IDirectSound_SetCooperativeLevel (pDS, mainwindow,
											  DSSCL_WRITEPRIMARY)) {
			Sys_Printf ("Set coop level failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (DS_OK != IDirectSound_GetCaps (pDSPBuf, &dsbcaps)) {
			Sys_Printf ("DS:GetCaps failed\n");
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
	}

	// Make sure mixer is active
	IDirectSoundBuffer_Play (pDSBuf, 0, 0, DSBPLAY_LOOPING);

	gSndBufSize = dsbcaps.dwBufferBytes;

	// initialize the buffer
	reps = 0;

	while ((hresult = IDirectSoundBuffer_Lock (pDSBuf, 0, gSndBufSize,
											   (LPVOID *) & lpData, &dwSize,
											   NULL, NULL, 0)) != DS_OK) {
		if (hresult != DSERR_BUFFERLOST) {
			Sys_Printf ("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (++reps > 10000) {
			Sys_Printf ("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound ();
			return SIS_FAILURE;
		}

	}

	memset (lpData, 0, dwSize);
//	lpData[4] = lpData[5] = 0x7f;   // force a pop for debugging

	IDirectSoundBuffer_Unlock (pDSBuf, lpData, dwSize, NULL, 0);

	/* we don't want anyone to access the buffer directly w/o locking it
	   first. */
	lpData = NULL;

	IDirectSoundBuffer_Stop (pDSBuf);
	IDirectSoundBuffer_GetCurrentPosition (pDSBuf, &mmstarttime.u.sample,
										   &dwWrite);
	IDirectSoundBuffer_Play (pDSBuf, 0, 0, DSBPLAY_LOOPING);

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits / 8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}

/*
	SNDDM_InitWav

	Crappy windows multimedia base
*/
static qboolean
SNDDMA_InitWav (void)
{
	int			i;
	HRESULT		hr;
	WAVEFORMATEX format;

	snd_sent = 0;
	snd_completed = 0;

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;

	memset (&format, 0, sizeof (format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	while ((hr = waveOutOpen ((LPHWAVEOUT) & hWaveOut, WAVE_MAPPER,
							  &format, 0, 0L,
							  CALLBACK_NULL)) != MMSYSERR_NOERROR) {
		if (hr != MMSYSERR_ALLOCATED) {
			Sys_Printf ("waveOutOpen failed\n");
			return false;
		}
		Sys_Printf ("waveOutOpen failure;\n" "  hardware already in use\n");
		return false;
	}

	/* 
	   Allocate and lock memory for the waveform data. The memory 
	   for waveform data must be globally allocated with 
	   GMEM_MOVEABLE and GMEM_SHARE flags. 
	*/
	gSndBufSize = WAV_BUFFERS * WAV_BUFFER_SIZE;
	hData = GlobalAlloc (GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData) {
		Sys_Printf ("Sound: Out of memory.\n");
		FreeSound ();
		return false;
	}
	lpData = GlobalLock (hData);
	if (!lpData) {
		Sys_Printf ("Sound: Failed to lock.\n");
		FreeSound ();
		return false;
	}
	memset (lpData, 0, gSndBufSize);

	/* 
	 * Allocate and lock memory for the header. This memory must 
	 * also be globally allocated with GMEM_MOVEABLE and 
	 * GMEM_SHARE flags. 
	 */
	hWaveHdr = GlobalAlloc (GMEM_MOVEABLE | GMEM_SHARE,
							(DWORD) sizeof (WAVEHDR) * WAV_BUFFERS);

	if (hWaveHdr == NULL) {
		Sys_Printf ("Sound: Failed to Alloc header.\n");
		FreeSound ();
		return false;
	}

	lpWaveHdr = (LPWAVEHDR) GlobalLock (hWaveHdr);

	if (lpWaveHdr == NULL) {
		Sys_Printf ("Sound: Failed to lock header.\n");
		FreeSound ();
		return false;
	}

	memset (lpWaveHdr, 0, sizeof (WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */
	for (i = 0; i < WAV_BUFFERS; i++) {
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i * WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader (hWaveOut, lpWaveHdr + i, sizeof (WAVEHDR)) !=
			MMSYSERR_NOERROR) {
			Sys_Printf ("Sound: failed to prepare wave headers\n");
			FreeSound ();
			return false;
		}
	}

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits / 8) - 1;

	wav_init = true;

	return true;
}

/*
	SNDDMA_Init

	Try to find a sound device to mix for.
	Returns false if nothing is found.
*/
qboolean
SNDDMA_Init (void)
{
	sndinitstat stat;

	if (COM_CheckParm ("-wavonly"))
		wavonly = true;

	dsound_init = wav_init = 0;

	stat = SIS_FAILURE;					// assume DirectSound won't
	// initialize

	/* Init DirectSound */
	if (!wavonly) {
		if (snd_firsttime || snd_isdirect) {
			stat = SNDDMA_InitDirect ();;

			if (stat == SIS_SUCCESS) {
				snd_isdirect = true;

				if (snd_firsttime)
					Sys_Printf ("DirectSound initialized\n");
			} else {
				snd_isdirect = false;
				Sys_Printf ("DirectSound failed to init\n");
			}
		}
	}
// if DirectSound didn't succeed in initializing, try to initialize
// waveOut sound, unless DirectSound failed because the hardware is
// already allocated (in which case the user has already chosen not
// to have sound)
	if (!dsound_init && (stat != SIS_NOTAVAIL)) {
		if (snd_firsttime || snd_iswave) {

			snd_iswave = SNDDMA_InitWav ();

			if (snd_iswave) {
				if (snd_firsttime)
					Sys_Printf ("Wave sound initialized\n");
			} else {
				Sys_Printf ("Wave sound failed to init\n");
			}
		}
	}

	snd_firsttime = false;

	if (!dsound_init && !wav_init) {
		if (snd_firsttime)
			Sys_Printf ("No sound device initialized\n");

		return 0;
	}

	return 1;
}

/*
	SNDDMA_GetDMAPos

	return the current sample position (in mono samples read)
	inside the recirculating dma buffer, so the mixing code will know
	how many sample are required to fill it up.
*/
static int
SNDDMA_GetDMAPos (void)
{
	int			s = 0;
	DWORD		dwWrite;
	MMTIME		mmtime;

	if (dsound_init) {
		mmtime.wType = TIME_SAMPLES;
		IDirectSoundBuffer_GetCurrentPosition (pDSBuf, &mmtime.u.sample,
											   &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	} else if (wav_init) {
		s = snd_sent * WAV_BUFFER_SIZE;
	}


	s >>= sample16;

	s &= (shm->samples - 1);

	return s;
}

/*
	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer
*/
static void
SNDDMA_Submit (void)
{
	int			wResult;
	LPWAVEHDR	h;

	if (!wav_init)
		return;

	// find which sound blocks have completed
	while (1) {
		if (snd_completed == snd_sent) {
			Sys_DPrintf ("Sound overrun\n");
			break;
		}

		if (!(lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE)) {
			break;
		}

		snd_completed++;				// this buffer has been played
	}

	// submit two new sound blocks
	while (((snd_sent - snd_completed) >> sample16) < 4) {
		h = lpWaveHdr + (snd_sent & WAV_MASK);

		snd_sent++;
		/* 
		   Now the data block can be sent to the output device. The 
		   waveOutWrite function returns immediately and waveform 
		   data is sent to the output device in the background. 
		*/
		wResult = waveOutWrite (hWaveOut, h, sizeof (WAVEHDR));

		if (wResult != MMSYSERR_NOERROR) {
			Sys_Printf ("Failed to write block to device\n");
			FreeSound ();
			return;
		}
	}
}

/*
	SNDDMA_Shutdown

	Reset the sound device for exiting
*/
static void
SNDDMA_Shutdown (void)
{
	FreeSound ();
}

DWORD      *
DSOUND_LockBuffer (qboolean lockit)
{
	int		reps;

	static DWORD dwSize;
	static DWORD dwSize2;
	static DWORD *pbuf1;
	static DWORD *pbuf2;
	HRESULT     hresult;

	if (!pDSBuf)
		return NULL;

	if (lockit) {
		reps = 0;
		while ((hresult = IDirectSoundBuffer_Lock
				(pDSBuf, 0, gSndBufSize, (LPVOID *) & pbuf1, &dwSize,
				 (LPVOID *) & pbuf2, &dwSize2, 0)) != DS_OK) {
			if (hresult != DSERR_BUFFERLOST) {
				Sys_Printf
					("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				SNDDMA_Shutdown ();
				SNDDMA_Init ();
				return NULL;
			}

			if (++reps > 10000) {
				Sys_Printf
					("S_TransferStereo16: DS: couldn't restore buffer\n");
				SNDDMA_Shutdown ();
				SNDDMA_Init ();
				return NULL;
			}
		}
	} else {
		IDirectSoundBuffer_Unlock (pDSBuf, pbuf1, dwSize, NULL, 0);
		pbuf1 = NULL;
		pbuf2 = NULL;
		dwSize = 0;
		dwSize2 = 0;
	}
	return (pbuf1);
}

void
DSOUND_ClearBuffer (int clear)
{
	DWORD      *pData;

// FIXME: this should be called with 2nd pbuf2 = NULL, dwsize =0
	pData = DSOUND_LockBuffer (true);
	memset (pData, clear, shm->samples * shm->samplebits / 8);
	DSOUND_LockBuffer (false);
}

void
DSOUND_Restore (void)
{
// if the buffer was lost or stopped, restore it and/or restart it
	DWORD       dwStatus;

	if (!pDSBuf)
		return;

	if (IDirectSoundBuffer_GetStatus (pDSBuf, &dwStatus) != DD_OK)
		Sys_Printf ("Couldn't get sound buffer status\n");

	if (dwStatus & DSBSTATUS_BUFFERLOST)
		IDirectSoundBuffer_Restore (pDSBuf);

	if (!(dwStatus & DSBSTATUS_PLAYING))
		IDirectSoundBuffer_Play (pDSBuf, 0, 0, DSBPLAY_LOOPING);

	return;
}

QFPLUGIN plugin_t *
snd_output_win_PluginInfo (void) {
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "Windows digital output";
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

