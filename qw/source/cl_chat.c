/*
	cl_ignore.c

	Client-side ignore list management

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

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/dstring.h"
#include "QF/info.h"
#include "QF/keys.h"
#include "QF/llist.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "client/input.h"

#include "qw/include/client.h"
#include "qw/include/cl_chat.h"

llist_t *ignore_list, *dead_ignore_list;

static void
CL_Ignore_Free (void *ele, void *unused)
{
	ignore_t *ig = (ignore_t *) ele;
	if (ig->lastname)
		free ((void *)ig->lastname);
	free (ig);
}

static qboolean
CL_Ignore_Compare (const void *ele, const void *cmp, void *unused)
{
	return *(int *)cmp == ((ignore_t *) ele)->uid;
}

static qboolean
isc_iterator (ignore_t *ig, llist_node_t *node)
{
	if (cl.players[ig->slot].userid != ig->uid) // We got out of sync somehow
		llist_remove (node);
	return true;
}

static void
CL_Ignore_Sanity_Check (void)
{
	llist_iterate (ignore_list, LLIST_ICAST (isc_iterator));
}

static qboolean live_iterator (ignore_t *ig, llist_node_t *node)
{
	Sys_Printf ("%5i - %s\n", ig->uid,
				Info_ValueForKey (cl.players[ig->slot].userinfo, "name"));
	return true;
}

static qboolean dead_iterator (ignore_t *ig, llist_node_t *node)
{
	Sys_Printf ("%s\n", ig->lastname);
	return true;
}

static void
CL_Ignore_f (void)
{
	CL_Ignore_Sanity_Check ();
	if (Cmd_Argc () == 1) {
		Sys_Printf (
			"Users ignored by user id\n"
			"------------------------\n"
		);
		llist_iterate (ignore_list, LLIST_ICAST (live_iterator));
		Sys_Printf (
			"\n"
			"Users ignored by name (not currently connected)\n"
			"-----------------------------------------------\n"
		);
		llist_iterate (dead_ignore_list, LLIST_ICAST (dead_iterator));
	} else if (Cmd_Argc () == 2) {
		int i, uid = atoi (Cmd_Argv (1));

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cl.players[i].userid == uid) {
				ignore_t *new = calloc (1, sizeof (ignore_t));
				new->slot = i;
				new->uid = uid;
				llist_append (ignore_list, new);
				Sys_Printf ("User %i (%s) is now ignored.\n", uid,
							Info_ValueForKey (cl.players[i].userinfo, "name"));
				return;
			}
		}
		Sys_Printf ("No user %i present on the server.\n", uid);
	} else
		Sys_Printf ("Usage: ignore [userid]\n");
}

static void
CL_Unignore_f (void)
{
	int uid;
	llist_node_t *node;

	CL_Ignore_Sanity_Check ();
	if (Cmd_Argc() != 2)
		Sys_Printf ("usage: unignore userid\n");
	else {
		uid = atoi (Cmd_Argv (1));
		if ((node = llist_findnode (ignore_list, &uid))) {
			int         slot = LLIST_DATA (node, ignore_t)->slot;
			Sys_Printf ("User %i (%s) is no longer ignored.\n", uid,
						Info_ValueForKey (cl.players[slot].userinfo, "name"));
			CL_Ignore_Free (llist_remove (node), 0);
			return;
		}
		Sys_Printf ("User %i does not exist or is not presently ignored.\n",
					uid);
	}
}

/*
	HACK HACK HACK (phrosty)
	replaced GCC-specific nested functions with this.
	come back and make this cleaner later.
*/
static const char *g_cam_str;
static dstring_t *g_cam_test;
static qboolean g_cam_allowed;

static qboolean cam_iterator (ignore_t *ig, llist_node_t *node)
{
	if (cl.players[ig->slot].userid != ig->uid) { // We got out of sync somehow
		llist_remove (node);
		return true;
	}
	dsprintf (g_cam_test, "%s: ",
			  Info_ValueForKey (cl.players[ig->slot].userinfo, "name"));
	if (!strncmp (g_cam_test->str, g_cam_str, g_cam_test->size - 1)) {
		return g_cam_allowed = false;
	} else
		return true;
}

qboolean
CL_Chat_Allow_Message (const char *str)
{
	g_cam_str = str;
	g_cam_test = dstring_newstr ();
	g_cam_allowed = true;

	llist_iterate (ignore_list, LLIST_ICAST (cam_iterator));
	return g_cam_allowed;
}

void
CL_Chat_User_Disconnected (int uid)
{
	ignore_t *ig;

	CL_Ignore_Sanity_Check ();

	if ((ig = llist_remove (llist_findnode (ignore_list, &uid)))) {
		if (ig->lastname)
			free ((void *)ig->lastname);
		ig->lastname = strdup (Info_ValueForKey (cl.players[ig->slot].userinfo,
												 "name"));
		llist_append (dead_ignore_list, ig);
		Sys_Printf ("Ignored user %i (%s) left the server.  "
					"Now ignoring by name...\n", ig->uid, ig->lastname);
	}
}

static const char *g_ccn_name;
static ignore_t *g_ccn_found;

static qboolean ccn_iterator (ignore_t *ig, llist_node_t *node)
{
	if (!strcmp (ig->lastname, g_ccn_name)) {
		g_ccn_found = ig;
		return false;
	} else
		return true;
}

void
CL_Chat_Check_Name (const char *name, int slot)
{
	g_ccn_name = name;
	g_ccn_found = 0;

	llist_iterate (dead_ignore_list, LLIST_ICAST (ccn_iterator));
	if (g_ccn_found) {
		g_ccn_found->slot = slot;
		g_ccn_found->uid = cl.players[slot].userid;
		llist_append (ignore_list,
					  llist_remove (llist_getnode (dead_ignore_list,
												   g_ccn_found)));
		Sys_Printf ("User %i (%s) is using an ignored name.  "
					"Now ignoring by user id...\n", g_ccn_found->uid,
					g_ccn_found->lastname);
	}
}

void
CL_Chat_Flush_Ignores (void)
{
	llist_flush (ignore_list);
}

static void
CL_ChatInfo (int val)
{
	if (val < 1 || val > 3)
		val = 0;
	if (cls.chat != val) {
		cls.chat = val;
		Cbuf_AddText(cl_cbuf, va (0, "setinfo chat \"%d\"\n", val));
	}
}

static void
cl_chat_on_focus_change (int game)
{
	//FIXME afk mode
	CL_ChatInfo (!game);
}


void
CL_Chat_Init (void)
{
	ignore_list = llist_new (CL_Ignore_Free, CL_Ignore_Compare, 0);
	dead_ignore_list = llist_new (CL_Ignore_Free, CL_Ignore_Compare, 0);

	Cmd_AddCommand ("ignore", CL_Ignore_f, "Ignores chat and name-change messages from a user.");
	Cmd_AddCommand ("unignore", CL_Unignore_f, "Removes a previously ignored user from the ignore list.");
	CL_OnFocusChange (cl_chat_on_focus_change);
}
