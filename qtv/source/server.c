/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/hash.h"

#include "server.h"

static hashtab_t *servers;

static const char *
server_get_key (void *sv, void *unused)
{
	return ((server_t *) sv)->name;
}

static void
server_free (void *_sv, void *unused)
{
}

static int
server_compare (const void *a, const void *b)
{
	return strcmp ((*(server_t **) a)->name, (*(server_t **) b)->name);
}

static void
sv_new_f (void)
{
	const char *name;
	const char *address;
	server_t   *sv;

	if (Cmd_Argc () != 3) {
		Con_Printf ("Usage: sv_new <name> <address>\n");
		return;
	}
	name = Cmd_Argv (1);
	if (Hash_Find (servers, name)) {
		Con_Printf ("sv_new: %s already exists\n", name);
		return;
	}
	address = Cmd_Argv (2);
	sv = calloc (1, sizeof (server_t));
	sv->name = strdup (name);
	sv->address = strdup (address);
	Hash_Add (servers, sv);
}

static void
sv_del_f (void)
{
	const char *name;
	server_t   *sv;

	if (Cmd_Argc () != 2) {
		Con_Printf ("Usage: sv_del <name>\n");
		return;
	}
	name = Cmd_Argv (1);
	if (!(sv = Hash_Del (servers, name))) {
		Con_Printf ("sv_new: %s unkown\n", name);
		return;
	}
	Hash_Free (servers, sv);
}

static void
sv_list_f (void)
{
	server_t  **list, **l, *sv;
	int         count;

	list = (server_t **) Hash_GetList (servers);
	for (l = list, count = 0; *l; l++)
		count++;
	if (!count) {
		Con_Printf ("no servers\n");
		return;
	}
	qsort (list, count, sizeof (*list), server_compare);
	for (l = list; *l; l++) {
		sv = *l;
		Con_Printf ("%-20s %s\n", sv->name, sv->address);
	}
}

void
Server_Init (void)
{
	servers = Hash_NewTable (61, server_get_key, server_free, 0);
	Cmd_AddCommand ("sv_new", sv_new_f, "Add a new server");
	Cmd_AddCommand ("sv_del", sv_del_f, "Remove an existing server");
	Cmd_AddCommand ("sv_list", sv_list_f, "List available servers");
}
