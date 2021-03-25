/*
	map_cfg.c

	map config file handling

	Copyright (C) 2005 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2005/06/19

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/idparse.h"
#include "QF/quakefs.h"

#include "qw/include/map_cfg.h"

void
map_cfg (const char *mapname, int all)
{
	char       *name = malloc (strlen (mapname) + 4 + 1);
	QFile      *f;
	cbuf_t     *cbuf = Cbuf_New (&id_interp);

	QFS_StripExtension (mapname, name);
	strcat (name, ".cfg");
	f = QFS_FOpenFile (name);
	if (f) {
		Qclose (f);
		Cmd_Exec_File (cbuf, name, 1);
	} else {
		Cmd_Exec_File (cbuf, "maps_default.cfg", 1);
	}
	if (all)
		Cbuf_Execute_Stack (cbuf);
	else
		Cbuf_Execute_Sets (cbuf);
	free (name);
	Cbuf_Delete(cbuf);
}

