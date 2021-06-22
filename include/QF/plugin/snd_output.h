/*
	QF/plugin/sound.h

	Sound Output plugin data types

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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
#ifndef __QF_plugin_snd_output_h
#define __QF_plugin_snd_output_h

#include <QF/plugin.h>
#include <QF/qtypes.h>

/*
	All sound plugins must export these functions
*/
typedef volatile struct dma_s *(*P_S_O_Init) (void);
typedef void (*P_S_O_Shutdown) (void);
typedef int (*P_S_O_GetDMAPos) (void);
typedef void (*P_S_O_Submit) (void);
typedef void (*P_S_O_BlockSound) (void);
typedef void (*P_S_O_UnblockSound) (void);

typedef struct snd_output_funcs_s {
	P_S_O_Init			pS_O_Init;
	P_S_O_Shutdown		pS_O_Shutdown;
	P_S_O_GetDMAPos		pS_O_GetDMAPos;
	P_S_O_Submit		pS_O_Submit;
	P_S_O_BlockSound	pS_O_BlockSound;
	P_S_O_UnblockSound	pS_O_UnblockSound;
} snd_output_funcs_t;

typedef struct snd_output_data_s {
	unsigned   *soundtime;
	unsigned   *paintedtime;
} snd_output_data_t;

#endif // __QF_plugin_snd_output_h
