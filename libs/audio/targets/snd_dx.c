/*
	snd_dx.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define CINTERFACE

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "snd_internal.h"
#include "context_win.h"

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

static qboolean dsound_init;
static qboolean snd_firsttime = true;
static qboolean primary_format_set;

static int  sample16;

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

static sndinitstat SNDDMA_InitDirect (snd_t *snd);

static cvar_t	   *snd_stereo;
static cvar_t	   *snd_rate;
static cvar_t	   *snd_bits;

static DWORD *DSOUND_LockBuffer (snd_t *snd, qboolean lockit);

static void
SNDDMA_Init_Cvars (void)
{
	snd_stereo = Cvar_Get ("snd_stereo", "1", CVAR_ROM, NULL,
						   "sound stereo output");
	snd_rate = Cvar_Get ("snd_rate", "11025", CVAR_ROM, NULL,
						 "sound playback rate. 0 is system default");
	snd_bits = Cvar_Get ("snd_bits", "16", CVAR_ROM, NULL,
						 "sound sample depth. 0 is system default");
}

static void
SNDDMA_BlockSound (snd_t *snd)
{
}

static void
SNDDMA_UnblockSound (snd_t *snd)
{
}

static void
FreeSound (void)
{
	if (pDSBuf) {
		IDirectSoundBuffer_Stop (pDSBuf);
		IDirectSound_Release (pDSBuf);
	}
// release primary buffer only if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf)) {
		IDirectSound_Release (pDSPBuf);
	}

	if (pDS) {
		IDirectSound_SetCooperativeLevel (pDS, win_mainwindow, DSSCL_NORMAL);
		IDirectSound_Release (pDS);
	}
	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
}

/*
	SNDDMA_InitDirect

	Direct-Sound support
*/
static sndinitstat
SNDDMA_InitDirect (snd_t *snd)
{
	int				reps;
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DSCAPS			dscaps;
	DWORD			dwSize, dwWrite;
	HRESULT			hresult;
	WAVEFORMATEX	format, pformat;

	if (!snd_stereo->int_val) {
		snd->channels = 1;
	} else {
		snd->channels = 2;
	}

	snd->samplebits = snd_bits->int_val;
	snd->speed = snd_rate->int_val;

	memset (&format, 0, sizeof (format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = snd->channels;
	format.wBitsPerSample = snd->samplebits;
	format.nSamplesPerSec = snd->speed;
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
		IDirectSound_SetCooperativeLevel (pDS, win_mainwindow,
										  DSSCL_EXCLUSIVE)) {
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

		snd->channels = format.nChannels;
		snd->samplebits = format.wBitsPerSample;
		snd->speed = format.nSamplesPerSec;

		if (DS_OK != IDirectSound_GetCaps (pDSBuf, &dsbcaps)) {
			Sys_Printf ("DS:GetCaps failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}
	} else {
		if (DS_OK !=
			IDirectSound_SetCooperativeLevel (pDS, win_mainwindow,
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
		(LPVOID *)(char *)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK) {
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
//	lpData = NULL;

	IDirectSoundBuffer_Stop (pDSBuf);
	IDirectSoundBuffer_GetCurrentPosition (pDSBuf, &mmstarttime.u.sample,
										   &dwWrite);
	IDirectSoundBuffer_Play (pDSBuf, 0, 0, DSBPLAY_LOOPING);

	snd->frames = gSndBufSize / (snd->samplebits / 8) / snd->channels;
	snd->framepos = 0;
	snd->submission_chunk = 1;
	snd->buffer = (byte *) lpData;
	sample16 = (snd->samplebits / 8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}


/*
	SNDDMA_Init

	Try to find a sound device to mix for.
	Returns false if nothing is found.
*/
static int
SNDDMA_Init (snd_t *snd)
{
	sndinitstat stat;
	int         ret = 0;

	stat = SIS_FAILURE;					// assume DirectSound won't
	// initialize

	/* Init DirectSound */
	if (snd_firsttime) {
		snd_firsttime = false;
		stat = SNDDMA_InitDirect (snd);

		if ((ret = (stat == SIS_SUCCESS))) {
			Sys_Printf ("DirectSound initialized\n");
		} else {
			Sys_Printf ("DirectSound failed to init\n");
		}
	}

	return ret;
}

/*
	SNDDMA_GetDMAPos

	return the current sample position (in mono samples read)
	inside the recirculating dma buffer, so the mixing code will know
	how many sample are required to fill it up.
*/
static int
SNDDMA_GetDMAPos (snd_t *snd)
{
	int		s = 0;
	DWORD		dwWrite;
	MMTIME	mmtime;
	unsigned long *pbuf;

	pbuf = DSOUND_LockBuffer (snd, true);
	if (!pbuf) {
		Sys_Printf ("DSOUND_LockBuffer fails!\n");
		return -1;
	}
	snd->buffer = (unsigned char *) pbuf;
	mmtime.wType = TIME_SAMPLES;
	IDirectSoundBuffer_GetCurrentPosition (pDSBuf, &mmtime.u.sample,
											   &dwWrite);
	s = mmtime.u.sample - mmstarttime.u.sample;

	s >>= sample16;
	s /= snd->channels;

	s %= snd->frames;
	snd->framepos = s;

	return snd->framepos;
}

/*
	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer
*/
static void
SNDDMA_Submit (snd_t *snd)
{
	DSOUND_LockBuffer (snd, false);
}

static void
SNDDMA_shutdown (snd_t *snd)
{
	FreeSound ();
}

static DWORD *
DSOUND_LockBuffer (snd_t *snd, qboolean lockit)
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
				(pDSBuf, 0, gSndBufSize, (LPVOID *)(char *) &pbuf1, &dwSize,
				 (LPVOID *)(char *) &pbuf2, &dwSize2, 0)) != DS_OK) {
			if (hresult != DSERR_BUFFERLOST) {
				Sys_Printf
					("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				SNDDMA_shutdown (snd);
				SNDDMA_Init (snd);
				return NULL;
			}

			if (++reps > 10000) {
				Sys_Printf
					("S_TransferStereo16: DS: couldn't restore buffer\n");
				SNDDMA_shutdown (snd);
				SNDDMA_Init (snd);
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

static void __attribute__((used)) //FIXME make it true
DSOUND_ClearBuffer (snd_t *snd, int clear)
{
	DWORD      *pData;

// FIXME: this should be called with 2nd pbuf2 = NULL, dwsize =0
	pData = DSOUND_LockBuffer (snd, true);
	memset (pData, clear, snd->frames * snd->channels * snd->samplebits / 8);
	DSOUND_LockBuffer (snd, false);
}

static void __attribute__((used)) //FIXME make it true
DSOUND_Restore (void)
{
// if the buffer was lost or stopped, restore it and/or restart it
	DWORD       dwStatus;

	if (!pDSBuf)
		return;

	if (IDirectSoundBuffer_GetStatus (pDSBuf, &dwStatus) != DS_OK)
		Sys_Printf ("Couldn't get sound buffer status\n");

	if (dwStatus & DSBSTATUS_BUFFERLOST)
		IDirectSoundBuffer_Restore (pDSBuf);

	if (!(dwStatus & DSBSTATUS_PLAYING))
		IDirectSoundBuffer_Play (pDSBuf, 0, 0, DSBPLAY_LOOPING);

	return;
}

static snd_output_data_t plugin_info_snd_output_data = {
};

static snd_output_funcs_t plugin_info_snd_output_funcs = {
	.init          = SNDDMA_Init,
	.shutdown      = SNDDMA_shutdown,
	.get_dma_pos   = SNDDMA_GetDMAPos,
	.submit        = SNDDMA_Submit,
	.block_sound   = SNDDMA_BlockSound,
	.unblock_sound = SNDDMA_UnblockSound,
};

static general_data_t plugin_info_general_data = {
};

static general_funcs_t plugin_info_general_funcs = {
	.init = SNDDMA_Init_Cvars,
};

static plugin_data_t plugin_info_data = {
	.general    = &plugin_info_general_data,
	.snd_output = &plugin_info_snd_output_data,
};

static plugin_funcs_t plugin_info_funcs = {
	.general    = &plugin_info_general_funcs,
	.snd_output = &plugin_info_snd_output_funcs,
};

static plugin_t plugin_info = {
	.type           = qfp_snd_output,
	.api_version    = QFPLUGIN_VERSION,
	.plugin_version = "0.1",
	.description    = "Windows DirectX output",
	.copyright      = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001,2002,2003  contributors of the "
		"QuakeForge project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	.functions      = &plugin_info_funcs,
	.data           = &plugin_info_data,
};

PLUGIN_INFO(snd_output, dx)
{
	return &plugin_info;
}
