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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define CINTERFACE

#include "winquake.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "snd_internal.h"

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

typedef enum { SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL } sndinitstat;

static bool snd_firsttime = true;

static int  sample16;
static int  snd_sent, snd_completed;
static int snd_blocked = 0;

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

static bool        SNDDMA_InitWav (snd_t *snd);

static int snd_stereo;
static cvar_t snd_stereo_cvar = {
	.name = "snd_stereo",
	.description =
		"sound stereo output",
	.default_value = "1",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &snd_stereo },
};
static int snd_rate;
static cvar_t snd_rate_cvar = {
	.name = "snd_rate",
	.description =
		"sound playback rate. 0 is system default",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &snd_rate },
};
static int snd_bits;
static cvar_t snd_bits_cvar = {
	.name = "snd_bits",
	.description =
		"sound sample depth. 0 is system default",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &snd_bits },
};


static void
SNDDMA_Init_Cvars (void)
{
	Cvar_Register (&snd_stereo_cvar, 0, 0);
	Cvar_Register (&snd_rate_cvar, 0, 0);
	Cvar_Register (&snd_bits_cvar, 0, 0);
}

static void
SNDDMA_BlockSound (snd_t *snd)
{
	if (++snd_blocked == 1)
		waveOutReset (hWaveOut);
}

static void
SNDDMA_UnblockSound (snd_t *snd)
{
	if (snd_blocked)
		--snd_blocked;
}

static void
FreeSound (void)
{
	int         i;

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

	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
}

/*
	SNDDM_InitWav

	Crappy windows multimedia base
*/
static bool
SNDDMA_InitWav (snd_t *snd)
{
	int			i;
	HRESULT		hr;
	WAVEFORMATEX format;

	snd_sent = 0;
	snd_completed = 0;

	if (!snd_stereo) {
		snd->channels = 1;
	} else {
		snd->channels = 2;
	}

	snd->samplebits = snd_bits;
	snd->speed = snd_rate;

	memset (&format, 0, sizeof (format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = snd->channels;
	format.wBitsPerSample = snd->samplebits;
	format.nSamplesPerSec = snd->speed;
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

	snd->frames = gSndBufSize / (snd->samplebits / 8) / snd->channels;
	snd->framepos = 0;
	snd->submission_chunk = 1;
	snd->buffer = (unsigned char *) lpData;
	sample16 = (snd->samplebits / 8) - 1;

	return true;
}

/*
	SNDDMA_Init

	Try to find a sound device to mix for.
	Returns false if nothing is found.
*/
static int
SNDDMA_Init (snd_t *snd)
{
	int         ret = 0;
	if (snd_firsttime) {
		if ((ret = SNDDMA_InitWav (snd))) {
			Sys_Printf ("Wave sound initialized\n");
		} else {
			Sys_Printf ("Wave sound failed to init\n");
		}
	}

	snd_firsttime = false;
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

	s = snd_sent * WAV_BUFFER_SIZE;

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
	int			wResult;
	LPWAVEHDR	h;

	// find which sound blocks have completed
	while (1) {
		if (snd_completed == snd_sent) {
			Sys_MaskPrintf (SYS_snd, "Sound overrun\n");
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

static void
SNDDMA_shutdown (snd_t *snd)
{
	FreeSound ();
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
	.description    = "Windows digital output",
	.copyright      = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
		"project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	.functions      = &plugin_info_funcs,
	.data           = &plugin_info_data,
};

PLUGIN_INFO(snd_output, win)
{
	return &plugin_info;
}
