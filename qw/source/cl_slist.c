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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
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
# include <windows.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <ctype.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vfile.h"

#include "bothdefs.h"
#include "cl_main.h"
#include "cl_slist.h"
#include "client.h"

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

cvar_t *sl_sortby;
cvar_t *sl_filter;
cvar_t *sl_game;
cvar_t *sl_ping;


void
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
	NET_SendPacket (6, data_ping, addy);
	NET_SendPacket (11, data_status, addy);
	slrefresh->waitstatus = 1;
}

server_entry_t *
SL_Add (server_entry_t *start, char *ip, char *desc)
{
	server_entry_t *p;

	p = start;
	if (!start) {						// Nothing at beginning of list,
										// create it
		start = calloc (1, sizeof (server_entry_t));

		start->prev = 0;
		start->next = 0;
		start->server = malloc (strlen (ip) + 1);
		start->desc = malloc (strlen (desc ? desc : ip) + 1);
		if (!start->server || !start->desc)
			Sys_Error ("SL_Add: Memory Allocation Failure\n");
		strcpy (start->server, ip);
		strcpy (start->desc, desc ? desc : ip);
		start->status = NULL;
		return (start);
	}

	for (p = start; p->next; p = p->next)  //Get to end of list
		if (strcmp(ip,p->server) == 0) //don't add duplicate
			return (start);

	p->next = calloc (1, sizeof (server_entry_t));

	p->next->prev = p;
	p->next->server = malloc (strlen (ip) + 1);
	p->next->desc = malloc (strlen (desc ? desc : ip) + 1);
	if (!p->next->server || !p->next->desc)
		Sys_Error ("SL_Add: Memory Allocation Failure\n");

	strcpy (p->next->server, ip);
	strcpy (p->next->desc, desc ? desc : ip);
	p->status = NULL;
	return (start);
}

server_entry_t *
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

server_entry_t *
SL_InsB (server_entry_t *start, server_entry_t *place, char *ip, char *desc)
{
	server_entry_t *new, *other;

	new = calloc (1, sizeof (server_entry_t));

	new->server = malloc (strlen (ip) + 1);
	new->desc = malloc (strlen (desc) + 1);
	if (!new->server || !new->desc)
		Sys_Error ("SL_InsB: Memory Allocation Failure\n");
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

void
SL_Swap (server_entry_t *swap1, server_entry_t *swap2)
{
	int i;
	server_entry_t *next1, *next2, *prev1, *prev2;
	
	next1 = swap1->next;
	next2 = swap2->next;
	prev1 = swap1->prev;
	prev2 = swap2->prev;

	for (i = 0; i < sizeof(server_entry_t); i++)
	{
		((char *)swap1)[i] ^= ((char *)swap2)[i];
		((char *)swap2)[i] ^= ((char *)swap1)[i];
		((char *)swap1)[i] ^= ((char *)swap2)[i];
	}

	swap1->next = next1;
	swap2->next = next2;
	swap1->prev = prev1;
	swap2->prev = prev2;
}

int
SL_CheckFilter (server_entry_t *sl_filteritem)
{
	if (!sl_filter->int_val)
		return(1);
	if (!sl_filteritem->status)
		return(0);
	if (strlen(sl_game->string))
	{
		if (strcasecmp(Info_ValueForKey (sl_filteritem->status,	"*gamedir"), 
					sl_game->string) != 0)
			return(0);
	}
	if (sl_ping->int_val)
	{
		if (!sl_filteritem->pongback)
			return(0);
		if (((int)(sl_filteritem->pongback * 1000)) >= sl_ping->int_val)
			return(0);
	}
	return(1);
}

server_entry_t *
SL_Get_By_Num (server_entry_t *start, int n)
{
	int         i;

	for (i = 0; i <= n; i++)
	{
		if(!start)
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

int
SL_Len (server_entry_t *start)
{
	int         i;

	for (i = 0; start; i++)
		start = start->next;
	return i;
}

void
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

void
SL_SaveF (VFile *f, server_entry_t *start)
{
	do {
		Qprintf (f, "%s   %s\n", start->server, start->desc);
		start = start->next;

	} while (start);
}

void
SL_Shutdown (void)
{
	char        e_path[MAX_OSPATH];
	VFile      *f;
	
	if (fav_slist) {
		Qexpand_squiggle (fs_userpath->string, e_path);
		if ((f = Qopen (va ("%s/servers.txt", e_path), "w"))) {
			SL_SaveF (f, fav_slist);
			Qclose (f);
		}
		SL_Del_All (fav_slist);
	}
	if (all_slist)
		SL_Del_All (all_slist);
}

char       *
gettokstart (char *str, int req, char delim)
{
	char       *start = str;
	int         tok = 1;

	while (*start == delim) {
		start++;
	}
	if (*start == '\0')
		return '\0';
	while (tok < req) {					// Stop when we get to the requested
										// token
		if (*++start == delim) {		// Increment pointer and test
			while (*start == delim) {	// Get to next token
				start++;
			}
			tok++;
		}
		if (*start == '\0') {
			return '\0';
		}
	}
	return start;
}

int
gettoklen (char *str, int req, char delim)
{
	char       *start = 0;
	int         len = 0;

	start = gettokstart (str, req, delim);
	if (start == '\0') {
		return 0;
	}
	while (*start != delim && *start != '\0') {
		start++;
		len++;
	}
	return len;
}

void timepassed (double time1, double *time2)
{
	*time2 -= time1;
}

void
SL_SortEntry (server_entry_t *start)
{
	int i = 0;
	server_entry_t *q;
	
	if (!start || !sl_sortby)
		return;

	for (q = start->next; q; q = q->next)
	{
		if (sl_sortby->int_val)
		{
			if ((q->pongback) && (start->pongback) && (start->pongback >
													   q->pongback))
			{
				SL_Swap(start,q);
				q = start;
			}
		} else {
			i = 0;
			
			while ((start->desc[i] != '\0') && (q->desc[i] != '\0') &&
				   (toupper(start->desc[i]) == toupper(q->desc[i])))
				i++;
			if (toupper(start->desc[i]) > toupper(q->desc[i]))
			{
				SL_Swap(start,q);
				q = start;
			}
		}
	}
}

void
SL_Sort (cvar_t *var)
{
	server_entry_t *p;
	
	if (!slist)
		return;

	for (p = slist; p->next; p = p->next)
		SL_SortEntry (p);
}

void
SL_Con_List (server_entry_t *sldata)
{
	int serv;
	server_entry_t *cp;
	
	SL_Sort(sl_sortby);
	
	for(serv = 0; serv < SL_Len (sldata); serv++)
	{
		cp = SL_Get_By_Num (sldata, serv);
		if (!cp)
			break;
		Con_Printf("%i) %s\n",(serv + 1),cp->desc);
	}
}

void
SL_Connect (server_entry_t *sldata, int slitemno)
{
	CL_Disconnect ();
	strncpy (cls.servername, SL_Get_By_Num (sldata, (slitemno - 1))->server,
		sizeof (cls.servername) - 0);
	CL_BeginServerConnect ();
}

void
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

void
SL_Con_Details (server_entry_t *sldata, int slitemno)
{
	int i, playercount;
	server_entry_t *cp;

	playercount = 0;
	slist_last_details = slitemno;
	cp = SL_Get_By_Num (sldata, (slitemno - 1));
	if (!cp)
		return;
	Con_Printf("Server: %s\n", cp->server);
	Con_Printf("Ping: ");
	if (cp->pongback)
		Con_Printf("%i\n", (int)(cp->pongback * 1000));
	else
		Con_Printf("N/A\n");
	if (cp->status)
	{
		char *s;

		Con_Printf("Name: %s\n", cp->desc);
		Con_Printf("Game: %s\n", Info_ValueForKey (cp->status, "*gamedir"));
		Con_Printf("Map: %s\n", Info_ValueForKey (cp->status, "map"));

		s = Info_MakeString (cp->status, 0);
		for (i = 0; i < strlen (s); i++)
			if (s[i] == '\n')
				playercount++;
		Con_Printf("Players: %i/%s\n", playercount,
				   Info_ValueForKey(cp->status, "maxclients"));

		// For Debug of Server Info 
		// Con_Printf("%s\n",cp->status);
	} else
		Con_Printf("No Details Available\n");
}

void
SL_MasterUpdate(void)
{
	char data[] = "c\n\0";
	netadr_t addy;
	
	SL_Del_All(slist);
	slist = NULL;
	NET_StringToAdr ("194.251.249.32:27000", &addy);
	NET_SendPacket (3, data, addy);
	NET_StringToAdr ("qwmaster.barrysworld.com:27000", &addy);
	NET_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27000", &addy);
	NET_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27002", &addy);
	NET_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27003", &addy);
	NET_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27004", &addy);
	NET_SendPacket (3, data, addy);
	NET_StringToAdr ("192.246.40.37:27006", &addy);
	NET_SendPacket (3, data, addy);
	NET_StringToAdr ("203.9.148.7:27000", &addy);
	NET_SendPacket (3, data, addy);
}

int
SL_Switch (void)
{
	if (!which_slist)
	{
		fav_slist = slist;
		slist = all_slist;
		which_slist = 1;
	} else {
		all_slist = slist;
		slist = fav_slist;
		which_slist = 0;
	}
	SL_Sort (sl_sortby);
	return (which_slist);
}
	
void
SL_Command (void)
{
	int sltemp = 0;
	
	if (Cmd_Argc () == 1)
		SL_Con_List(slist); 
	else if (strcasecmp(Cmd_Argv(1),"switch") == 0)
	{
		if (SL_Switch ())
			Con_Printf("Switched to Server List from Masters\n");
		else
			Con_Printf("Switched to Favorate Server List\n");
	}	
	else if (strcasecmp(Cmd_Argv(1),"refresh") == 0)
	{
		if (Cmd_Argc () == 2)
			SL_Update(slist);
		else
			Con_Printf("Syntax: slist refresh\n");
	}
	else if (strcasecmp(Cmd_Argv(1),"update") == 0)
	{
		if (Cmd_Argc () == 2)
		{
			if(!which_slist)
				Con_Printf("ERROR: This of for updating the servers from a "
						   "list of masters\n");
			else
				SL_MasterUpdate();
		}
		else
			Con_Printf("Syntax: slist update\n");
	}
	else if (strcasecmp(Cmd_Argv(1),"connect") == 0)
	{
		if (Cmd_Argc () == 3)
		{
			sltemp = atoi(Cmd_Argv(2)); 
			if(sltemp && (sltemp <= SL_Len (slist)))
				SL_Connect(slist,sltemp);
			else
				Con_Printf("Error: Invalid Server Number -> %s\n",Cmd_Argv(2));
		} 
		else if ((Cmd_Argc () == 2) && slist_last_details)
			SL_Connect(slist,slist_last_details);
		else
			Con_Printf("Syntax: slist connect #\n");
	}
	else
	{
		sltemp = atoi(Cmd_Argv(1));
		if((Cmd_Argc () == 2) && sltemp && (sltemp <= SL_Len (slist)))
			SL_Con_Details(slist,sltemp);
	}
}

void
MSL_ParseServerList(const char *msl_data)
{
	int msl_ptr;

	for (msl_ptr = 0; msl_ptr < strlen(msl_data); msl_ptr = msl_ptr + 6)
	{
		slist = SL_Add(slist, va("%i.%i.%i.%i:%i",
			(byte)msl_data[msl_ptr],
			(byte)msl_data[msl_ptr+1],
			(byte)msl_data[msl_ptr+2],
			(byte)msl_data[msl_ptr+3],
			((byte)msl_data[msl_ptr+4]<<8)|(byte)msl_data[msl_ptr+5]), NULL);
	}
}

server_entry_t *
SL_LoadF (VFile *f, server_entry_t *start)
{
	//This could get messy
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
			if (!addr)
				Sys_Error ("SL_LoadF: Memory Allocation Failure\n");
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

void SL_Init (void)
{
	char        e_path[MAX_OSPATH];
	VFile      *servlist;

	Qexpand_squiggle (fs_userpath->string, e_path);
	if ((servlist = Qopen (va ("%s/servers.txt", e_path), "r"))) {
		slist = SL_LoadF (servlist, slist);
		Qclose (servlist);
	} else {
		Qexpand_squiggle (fs_sharepath->string, e_path);
		if ((servlist = Qopen (va ("%s/servers.txt", e_path), "r"))) {
			slist = SL_LoadF (servlist, slist);
			Qclose (servlist);
		}
	}
	fav_slist = slist;
	all_slist = NULL;
	which_slist = 0;
	Cmd_AddCommand("slist",SL_Command,"console commands to access server "
				   "list\n");
	sl_sortby = Cvar_Get ("sl_sortby", "0", CVAR_ARCHIVE, SL_Sort, "0 = sort "
						  "by name, 1 = sort by ping");
	sl_filter = Cvar_Get ("sl_filter", "0", CVAR_NONE, NULL, "enable server "
						  "filter");
	sl_game = Cvar_Get ("sl_game", "", CVAR_ARCHIVE, NULL, "sets the "
						"serverlist game filter");
	sl_ping = Cvar_Get ("sl_ping", "", CVAR_ARCHIVE, NULL, "sets the "
						"serverlist ping filter");
}

int
SL_CheckStatus (const char *cs_from, const char *cs_data)
{
	const char *tmp_desc;
	server_entry_t *temp;
	
	for (temp = slist; temp; temp = temp->next)
		if (temp->waitstatus)
		{
			if (strcmp (cs_from, temp->server) == 0)
			{
				//int i;
				if (temp->status)
					Info_Destroy (temp->status);
				temp->status = Info_ParseString (cs_data, strlen (cs_data));
				temp->waitstatus = 0;
				tmp_desc = Info_ValueForKey (temp->status, "hostname");
				/*FIXME update for new info api
				if (tmp_desc[0] != '\0') {
					temp->desc = realloc(temp->desc, strlen(tmp_desc) + 1);
					strcpy(temp->desc, tmp_desc);
				} else {
					temp->desc = realloc(temp->desc, strlen(temp->status) + 1);
					strcpy(temp->desc, temp->server);
				}
				for (i = 0; i < strlen(temp->status); i++)
					if (temp->status[i] == '\n') {
						temp->status[i] = '\\';
						break;
					}
				*/
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
				timepassed(temp->pingsent, &temp->pongback);
			}
		}
}
