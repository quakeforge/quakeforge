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

	$Id$
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

#include "QF/console.h"

/*
	Con_DisplayList

	New function for tab-completion system
	Added by EvilTypeGuy
	MEGA Thanks to Taniwha

*/
void
Con_DisplayList(const char **list, int con_linewidth)
{
	int	i = 0;
	int	pos = 0;
	int	len = 0;
	int	maxlen = 0;
	int	width = (con_linewidth - 4);
	const char **walk = list;

	while (*walk) {
		len = strlen(*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list) {
		len = strlen(*list);
		if (pos + maxlen >= width) {
			Con_Printf("\n");
			pos = 0;
		}

		Con_Printf("%s", *list);
		for (i = 0; i < (maxlen - len); i++)
			Con_Printf(" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		Con_Printf("\n\n");
}
