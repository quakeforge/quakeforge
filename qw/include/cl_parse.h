/*
	client.h

	Client definitions

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

#ifndef _CL_PARSE_H
#define _CL_PARSE_H

#include "QF/qtypes.h"

#define NET_TIMINGS 256
#define NET_TIMINGSMASK 255

struct skin_s;

extern int	packet_latency[NET_TIMINGS];
extern int	parsecountmod;
extern double	parsecounttime;
extern int	cl_playerindex;
extern int	cl_flagindex;
extern int	viewentity;
extern int	cl_h_playerindex;
extern int	cl_gib1index;
extern int	cl_gib2index;
extern int	cl_gib3index;

int CL_CalcNet (void);
void CL_ParseServerMessage (void);
void CL_ParseClientdata (void);
void CL_NewTranslation (int slot, struct skin_s *skin);
qboolean	CL_CheckOrDownloadFile (const char *filename);
qboolean CL_IsUploading(void) __attribute__((pure));
void CL_NextUpload(void);
void CL_FinishDownload (void);
void CL_FailDownload (void);
void CL_StartUpload (byte *data, int size);
void CL_StopUpload(void);

#endif
