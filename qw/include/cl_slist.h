/*
	cl_slist.h

	Server listing address book interface

	Copyright (C) 1999,2000  Brian Koropoff

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

	$Id$
*/
#ifndef _CL_SLIST_H
#define _CL_SLIST_H

#include "quakeio.h"

typedef struct server_entry_s {
  	char *server;
	char *desc;
	char *status;
	int waitstatus;
	double pingsent;
	double pongback;
	struct server_entry_s *next;
	struct server_entry_s *prev;
  } server_entry_t;
  
extern server_entry_t	*slist;
  
server_entry_t *SL_Add(server_entry_t *start, char *ip, char *desc);
server_entry_t *SL_Del(server_entry_t *start, server_entry_t *del);
server_entry_t *SL_InsB(server_entry_t *start, server_entry_t *place, char *ip, char *desc);
void SL_Swap(server_entry_t *swap1, server_entry_t *swap2);
server_entry_t *SL_Get_By_Num(server_entry_t *start, int n);
int SL_Len(server_entry_t *start);

server_entry_t *SL_LoadF(QFile *f, server_entry_t *start);
void SL_SaveF(QFile *f, server_entry_t *start);

void SL_Del_All(server_entry_t *start);
void SL_Shutdown(server_entry_t *start);

char *gettokstart(char *str, int req, char delim);
int gettoklen(char *str, int req, char delim);

void timepassed (double time1, double *time2);
#endif	// _CL_SLIST_H
