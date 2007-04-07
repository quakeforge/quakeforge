/*
	list.c

	pretty print a list of strings to the console

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"

VISIBLE void (*con_list_print)(const char *fmt, ...) = Con_Printf;

/*
	Con_DisplayList

	New function for tab-completion system
	Added by EvilTypeGuy
	MEGA Thanks to Taniwha

*/
void
Con_DisplayList (const char **list, int con_linewidth)
{
	const char **walk = list;
	int         len = 0, maxlen = 0, pos =0, i = 0;
	int         width = (con_linewidth - 4);

	while (*walk) {
		len = strlen (*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list) {
		len = strlen (*list);
		if (pos + maxlen >= width) {
			con_list_print ("\n");
			pos = 0;
		}

		con_list_print ("%s", *list);
		for (i = 0; i < (maxlen - len); i++)
			con_list_print (" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		con_list_print ("\n\n");
}
