/*
	cl_slist.c

	serverlist addressbook

	Copyright (C) 2000      Brian Koropoff <brian.hk@home.com>
	Copyright (C) 2001	Chris Ison <ceison@yahoo.com>

	Author: Brian Koropoff
	Date: 03 May 2000

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

#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
# define model_t sun_model_t
# include <netinet/in.h>
# undef model_t
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_WINDOWS_H
# include "winquake.h"
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <ctype.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/quakeio.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "qw/bothdefs.h"
#include "qw/include/cl_main.h"
#include "qw/include/cl_slist.h"
#include "qw/include/client.h"

typedef struct server_entry_s {
	char *server;
	char *desc;
	info_t *status;
	int waitstatus;
	double pingsent;
	double pongback;
	struct server_entry_s *next;
	struct server_entry_s *prev;
} server_entry_t;

server_entry_t *slist;
server_entry_t *all_slist;
server_entry_t *fav_slist;

int which_slist;
int slist_last_details;

int sl_sortby;
static cvar_t sl_sortby_cvar = {
	.name = "sl_sortby",
	.description =
		"0 = sort by name, 1 = sort by ping",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &sl_sortby },
};
int sl_filter;
static cvar_t sl_filter_cvar = {
	.name = "sl_filter",
	.description =
		"enable server filter",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sl_filter },
};
char *sl_game;
static cvar_t sl_game_cvar = {
	.name = "sl_game",
	.description =
		"sets the serverlist game filter",
	.default_value = "",
	.flags = CVAR_ARCHIVE,
	.value = { .type = 0, .value = &sl_game },
};
int sl_ping;
static cvar_t sl_ping_cvar = {
	.name = "sl_ping",
	.description =
		"sets the serverlist ping filter",
	.default_value = "",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &sl_ping },
};


static void
S_Refresh (server_entry_t *slrefresh)
{
	netadr_t addy;
	char data_ping[] = "\377\377\377\377k";
	char data_status[] = "\377\377\377\377status";

	NET_StringToAdr (slrefresh->server, &addy);
	if (!addy.port)
		addy.port = ntohs (27500);

	slrefresh->pingsent = Sys_DoubleTime ();
	slrefresh->pongback = 0;
	Netchan_SendPacket (6, data_ping, addy);
	Netchan_SendPacket (11, data_status, addy);
	slrefresh->waitstatus = 1;
}

static server_entry_t *
SL_Add (server_entry_t *start, const char *ip, const char *desc)
{
	server_entry_t *p;

	if (!start) {						// Nothing at beginning of list,
										// create it
		start = calloc (1, sizeof (server_entry_t));

		start->prev = 0;
		start->next = 0;
		start->server = malloc (strlen (ip) + 1);
		start->desc = malloc (strlen (desc ? desc : ip) + 1);
		SYS_CHECKMEM (start->server && start->desc);
		strcpy (start->server, ip);
		strcpy (start->desc, desc ? desc : ip);
		start->status = NULL;
		return (start);
	}

	for (p = start; p->next; p = p->next)  //Get to end of list
		if (strcmp (ip, p->server) == 0) //don't add duplicate
			return (start);

	p->next = calloc (1, sizeof (server_entry_t));

	p->next->prev = p;
	p->next->server = malloc (strlen (ip) + 1);
	p->next->desc = malloc (strlen (desc ? desc : ip) + 1);
	SYS_CHECKMEM (p->next->server && p->next->desc);

	strcpy (p->next->server, ip);
	strcpy (p->next->desc, desc ? desc : ip);
	p->status = NULL;
	return (start);
}
/*
static server_entry_t *
SL_Del (server_entry_t *start, server_entry_t *del)
{
	server_entry_t *n;

	if (del == start) {
		free (start->server);
		free (start->desc);
		n = start->next;
		if (n)
			n->prev = 0;
		free (start);
		return (n);
	}

	free (del->server);
	free (del->desc);
	if (del->status)
		free (del->status);
	if (del->prev)
		del->prev->next = del->next;
	if (del->next)
		del->next->prev = del->prev;
	free (del);
	return (start);
}

static server_entry_t *
SL_InsB (server_entry_t *start, server_entry_t *place, char *ip, char *desc)
{
	server_entry_t *new, *other;

	new = calloc (1, sizeof (server_entry_t));

	new->server = malloc (strlen (ip) + 1);
	new->desc = malloc (strlen (desc) + 1);
	SYS_CHECKMEM (new->server && new->desc);
	strcpy (new->server, ip);
	strcpy (new->desc, desc);
	other = place->prev;
	if (other)
		other->next = new;
	place->prev = new;
	new->next = place;
	new->prev = other;
	if (!other)
		return new;
	return start;
}
*/
static void
SL_Swap (server_entry_t *swap1, server_entry_t *swap2)
{
	server_entry_t *next1, *next2, *prev1, *prev2;
	server_entry_t temp;

	next1 = swap1->next;
	next2 = swap2->next;
	prev1 = swap1->prev;
	prev2 = swap2->prev;

	temp = *swap1;
	*swap1 = *swap2;
	*swap2 = temp;

	swap1->next = next1;
	swap2->next = next2;
	swap1->prev = prev1;
	swap2->prev = prev2;
}

static int
SL_CheckFilter (server_entry_t *sl_filteritem)
{
	if (!sl_filter)
		return (1);
	if (!sl_filteritem->status)
		return (0);
	if (strlen (sl_game)) {
		if (strcasecmp (Info_ValueForKey (sl_filteritem->status, "*gamedir"),
						sl_game) != 0)
			return (0);
	}
	if (sl_ping) {
		if (!sl_filteritem->pongback)
			return (0);
		if (((int) (sl_filteritem->pongback * 1000)) >= sl_ping)
			return (0);
	}
	return (1);
}

static server_entry_t *
SL_Get_By_Num (server_entry_t *start, int n)
{
	int         i;

	for (i = 0; i <= n; i++)
	{
		if (!start)
			break;
		if (!SL_CheckFilter (start))
			i--;
		if (i != n)
			start = start->next;
	}
	if (!start)
		return (0);
	return (start);
}

static int
SL_Len (server_entry_t *start)
{
	int         i;

	for (i = 0; start; i++)
		start = start->next;
	return i;
}

static void
SL_Del_All (server_entry_t *start)
{
	server_entry_t *n;

	while (start) {
		n = start->next;
		free (start->server);
		free (start->desc);
		if (start->status)
			free (start->status);
		free (start);
		start = n;
	}
}

static void
SL_SaveF (QFile *f, server_entry_t *start)
{
	do {
		Qprintf (f, "%s   %s\n", start->server, start->desc);
		start = start->next;

	} while (start);
}

void
SL_Shutdown (void)
{
	QFile      *f;

	if (fav_slist) {
		if ((f = QFS_Open ("servers.txt", "w"))) {
			SL_SaveF (f, fav_slist);
			Qclose (f);
		}
		SL_Del_All (fav_slist);
	}
	if (all_slist)
		SL_Del_All (all_slist);
}

static __attribute__((pure)) char *
gettokstart (char *str, int req, char delim)
{
	char       *start = str;
	int         tok = 1;

	while (*start == delim) {
		start++;
	}
	if (*start == '\0')
		return 0;
	while (tok < req) {					// Stop when we get to the requested
										// token
		if (*++start == delim) {		// Increment pointer and test
			while (*start == delim) {	// Get to next token
				start++;
			}
			tok++;
		}
		if (*start == '\0') {
			return 0;
		}
	}
	return start;
}

static __attribute__((pure)) int
gettoklen (char *str, int req, char delim)
{
	char       *start = 0;
	int         len = 0;

	start = gettokstart (str, req, delim);
	if (*start == '\0') {
		return 0;
	}
	while (*start != delim && *start != '\0') {
		start++;
		len++;
	}
	return len;
}

static void
timepassed (double time1, double *time2)
{
	*time2 -= time1;
}

static void
SL_SortEntry (server_entry_t *start)
{
	int i = 0;
	server_entry_t *q;

	if (!start || !sl_sortby)
		return;

	for (q = start->next; q; q = q->next) {
		if (sl_sortby) {
			if ((q->pongback) && (start->pongback) && (start->pongback >
													   q->pongback)) {
				SL_Swap (start, q);
				q = start;
			}
		} else {
			i = 0;

			while ((start->desc[i] != '\0') && (q->desc[i] != '\0') &&
				   (toupper ((byte) start->desc[i])
					== toupper ((byte) q->desc[i])))
				i++;
			if (toupper ((byte) start->desc[i])
				> toupper ((byte) q->desc[i])) {
				SL_Swap (start, q);
				q = start;
			}
		}
	}
}

static void
SL_Sort (void *data, const cvar_t *cvar)
{
	server_entry_t *p;

	if (!slist)
		return;

	for (p = slist; p->next; p = p->next)
		SL_SortEntry (p);
}

static void
SL_Con_List (server_entry_t *sldata)
{
	int serv;
	server_entry_t *cp;

	SL_Sort (0, 0);

	for (serv = 0; serv < SL_Len (sldata); serv++) {
		cp = SL_Get_By_Num (sldata, serv);
		if (!cp)
			break;
		Sys_Printf ("%c%i) %s\n", 3, (serv + 1), cp->desc);
	}
}

static void
SL_Connect (server_entry_t *sldata, int slitemno)
{
	CL_Disconnect ();
	dstring_copystr (cls.servername,
					 SL_Get_By_Num (sldata, (slitemno - 1))->server);
	CL_BeginServerConnect ();
}

static void
SL_Update (server_entry_t *sldata)
{
	// FIXME - Need to change this so the info is not sent in 1 burst
	//         as it appears to be causing the occasional problem
	//         with some servers
	server_entry_t *cp;

	cp = sldata;
	while (cp)
	{
		S_Refresh (cp);
		cp = cp->next;
	}
}

static void
SL_Con_Details (server_entry_t *sldata, int slitemno)
{
	unsigned int i, playercount;
	server_entry_t *cp;

	playercount = 0;
	slist_last_details = slitemno;
	cp = SL_Get_By_Num (sldata, (slitemno - 1));
	if (!cp)
		return;
	Sys_Printf ("%cServer: %s\n", 3, cp->server);
	Sys_Printf ("Ping: ");
	if (cp->pongback)
		Sys_Printf ("%i\n", (int) (cp->pongback * 1000));
	else
		Sys_Printf ("N/A\n");
	if (cp->status) {
		char *s;

		Sys_Printf ("%cName: %s\n", 3, cp->desc);
		Sys_Printf ("%cGame: %s\n", 3,
					Info_ValueForKey (cp->status, "*gamedir"));
		Sys_Printf ("%cMap: %s\n", 3, Info_ValueForKey (cp->status, "map"));

		s = Info_MakeString (cp->status, 0);
		for (i = 0; i < strlen (s); i++)
			if (s[i] == '\n')
				playercount++;
		Sys_Printf ("%cPlayers: %i/%s\n", 3, playercount,
					Info_ValueForKey (cp->status, "maxclients"));
	} else
		Sys_Printf ("No Details Available\n");
}

static void
SL_MasterUpdate (void)
{
	char data[] = "c\n\0";
	netadr_t addy;

	SL_Del_All (slist);
	slist = NULL;
	NET_StringToAdr ("194.251.249.32:27000", &addy);
	Netchan_SendPacket (3, data, addy);
	NET_StringToAdr ("qwmaster.barrysworld.com:27000", &addy);
	Netchan_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27000", &addy);
	Netchan_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27002", &addy);
	Netchan_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27003", &addy);
	Netchan_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27004", &addy);
	Netchan_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27006", &addy);
	Netchan_SendPacket (3, data, addy);
	NET_StringToAdr ("203.9.148.7:27000", &addy);
	Netchan_SendPacket (3, data, addy);
}

static int
SL_Switch (void)
{
	if (!which_slist) {
		fav_slist = slist;
		slist = all_slist;
		which_slist = 1;
	} else {
		all_slist = slist;
		slist = fav_slist;
		which_slist = 0;
	}
	SL_Sort (0, 0);
	return (which_slist);
}

static void
SL_Command (void)
{
	int sltemp = 0;

	if (Cmd_Argc () == 1)
		SL_Con_List (slist);
	else if (strcasecmp (Cmd_Argv (1), "switch") == 0) {
		if (SL_Switch ())
			Sys_Printf ("Switched to Server List from Masters\n");
		else
			Sys_Printf ("Switched to Favorite Server List\n");
	} else if (strcasecmp (Cmd_Argv (1), "refresh") == 0) {
		if (Cmd_Argc () == 2)
			SL_Update (slist);
		else
			Sys_Printf ("Syntax: slist refresh\n");
	} else if (strcasecmp (Cmd_Argv (1),"update") == 0) {
		if (Cmd_Argc () == 2) {
			if (!which_slist)
				Sys_Printf ("ERROR: This of for updating the servers from a "
							"list of masters\n");
			else
				SL_MasterUpdate ();
		} else
			Sys_Printf ("Syntax: slist update\n");
	} else if (strcasecmp (Cmd_Argv (1), "connect") == 0) {
		if (Cmd_Argc () == 3) {
			sltemp = atoi (Cmd_Argv (2));
			if (sltemp && (sltemp <= SL_Len (slist)))
				SL_Connect (slist, sltemp);
			else
				Sys_Printf ("Error: Invalid Server Number -> %s\n",
							Cmd_Argv (2));
		} else if ((Cmd_Argc () == 2) && slist_last_details)
			SL_Connect (slist, slist_last_details);
		else
			Sys_Printf ("Syntax: slist connect #\n");
	} else {
		sltemp = atoi (Cmd_Argv (1));
		if ((Cmd_Argc () == 2) && sltemp && (sltemp <= SL_Len (slist)))
			SL_Con_Details (slist, sltemp);
	}
}

void
MSL_ParseServerList (const char *msl_data)
{
	unsigned int msl_ptr;

	for (msl_ptr = 0; msl_ptr < strlen (msl_data); msl_ptr = msl_ptr + 6) {
		slist = SL_Add (slist, va (0, "%i.%i.%i.%i:%i",
								   (byte) msl_data[msl_ptr],
								   (byte) msl_data[msl_ptr+1],
								   (byte) msl_data[msl_ptr+2],
								   (byte) msl_data[msl_ptr+3],
								   ((byte) msl_data[msl_ptr + 4] << 8)
								   | (byte) msl_data[msl_ptr + 5]), NULL);
	}
}

static server_entry_t *
SL_LoadF (QFile *f, server_entry_t *start)
{
	// This could get messy
	char        line[256];      /* Long lines get truncated. */
	char       *addr, *st;
	int         len, i;
	int         c = ' ';        /* int so it can be compared to EOF properly */

	while (1) {
	// First, get a line
		i = 0;
		c = ' ';
		while (c != '\n' && c != EOF) {
			c = Qgetc (f);
			if (i < 255) {
				line[i] = c;
				i++;
			}
		}
		line[i - 1] = '\0';                             // Now we can parse it
		if ((st = gettokstart (line, 1, ' ')) != NULL) {
			len = gettoklen (line, 1, ' ');
			addr = malloc (len + 1);
			SYS_CHECKMEM (addr);
			strncpy (addr, &line[0], len);
			addr[len] = '\0';
			if ((st = gettokstart (line, 2, ' '))) {
				start = SL_Add (start, addr, st);
			} else {
				start = SL_Add (start, addr, "Unknown");
			}
			free (addr);
		}
	if (c == EOF)                                   // We're done
		return start;
	}
}

void
SL_Init (void)
{
	QFile      *servlist;

	if ((servlist = QFS_Open ("servers.txt", "r"))) {
		slist = SL_LoadF (servlist, slist);
		Qclose (servlist);
	}
	fav_slist = slist;
	all_slist = NULL;
	which_slist = 0;
	Cmd_AddCommand ("slist", SL_Command, "console commands to access server "
					"list\n");
	Cvar_Register (&sl_sortby_cvar, SL_Sort, 0);
	Cvar_Register (&sl_filter_cvar, 0, 0);
	Cvar_Register (&sl_game_cvar, 0, 0);
	Cvar_Register (&sl_ping_cvar, 0, 0);
}

int
SL_CheckStatus (const char *cs_from, const char *cs_data)
{
	const char *tmp_desc;
	server_entry_t *temp;

	for (temp = slist; temp; temp = temp->next)
		if (temp->waitstatus) {
			if (strcmp (cs_from, temp->server) == 0) {
				unsigned int i;
				char *data = strdup (cs_data);

				strcpy (data, cs_data);
				for (i = 0; i < strlen (data); i++)
					if (data[i] == '\n') {
						data[i] = '\\';
						break;
					}
				if (temp->status)
					Info_Destroy (temp->status);
				temp->status = Info_ParseString (cs_data, strlen (data), 0);
				temp->waitstatus = 0;
				tmp_desc = Info_ValueForKey (temp->status, "hostname");
				if (tmp_desc[0] != '\0') {
					temp->desc = realloc (temp->desc, strlen (tmp_desc) + 1);
					strcpy (temp->desc, tmp_desc);
				} else {
					temp->desc = realloc (temp->desc, strlen (data) + 1);
					strcpy (temp->desc, data);
				}
				free (data);
				return (1);
			}
		}
	return (0);
}

void
SL_CheckPing (const char *cp_from)
{
	server_entry_t *temp;

	for (temp = slist; temp; temp = temp->next)
		if (temp->pingsent && !temp->pongback) {
			if (strcmp (cp_from, temp->server)) {
				temp->pongback = Sys_DoubleTime ();
				timepassed (temp->pingsent, &temp->pongback);
			}
		}
}
