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
#include "QF/info.h"

#include "client.h"
#include "connection.h"

void
Client_Init (void)
{
}

static void
client_handler (void *object)
{
	client_t   *cl = (client_t *) object;
	byte        d[1];

	Con_Printf ("hi\n");
	Netchan_Transmit (&cl->netchan, 0, d);
}

void
Client_Connect (connection_t *con, int qport, info_t *info)
{
	client_t   *cl;

	cl = calloc (1, sizeof (client_t));
	Netchan_Setup (&cl->netchan, con->address, qport, NC_READ_QPORT);
	cl->userinfo = info;
	con->object = cl;
	con->handler = client_handler;
	Con_Printf ("client %s (%s) connected\n", Info_ValueForKey (info, "name"),
				NET_AdrToString (con->address));
}
