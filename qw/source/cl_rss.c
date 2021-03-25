/*
	cl_screen.c

	master for refresh, status bar, console, chat, notify, etc

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <time.h>

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/pcx.h"
#include "QF/sys.h"

#include "qw/include/cl_parse.h"
#include "qw/include/client.h"

void
CL_RSShot_f (void)
{
	dstring_t  *st;
	int         pcx_len;
	pcx_t      *pcx = 0;
	tex_t      *tex = 0;
	time_t      now;

	if (CL_IsUploading ())
		return;							// already one pending
	if (cls.state < ca_onserver)
		return;							// must be connected

	Sys_Printf ("Remote screen shot requested.\n");

	tex = r_funcs->SCR_ScreenShot (RSSHOT_WIDTH, RSSHOT_HEIGHT);

	if (tex) {
		time (&now);
		st = dstring_strdup (ctime (&now));
		dstring_snip (st, strlen (st->str) - 1, 1);
		r_funcs->SCR_DrawStringToSnap (st->str, tex,
									   tex->width - strlen (st->str) * 8,
									   tex->height - 1);

		dstring_copystr (st, cls.servername->str);
		r_funcs->SCR_DrawStringToSnap (st->str, tex,
									   tex->width - strlen (st->str) * 8,
									   tex->height - 11);

		dstring_copystr (st, cl_name->string);
		r_funcs->SCR_DrawStringToSnap (st->str, tex,
									   tex->width - strlen (st->str) * 8,
									   tex->height - 21);

		pcx = EncodePCX (tex->data, tex->width, tex->height, tex->width,
						 r_data->vid->basepal, true, &pcx_len);
		free (tex);
	}
	if (pcx) {
		CL_StartUpload ((void *)pcx, pcx_len);
		Sys_Printf ("Wrote %s\n", "rss.pcx");
		Sys_Printf ("Sending shot to server...\n");
	}
}
