/*
	gib_modules.c

 	#DESCRIPTION#

 	Copyright (C) 2000       #AUTHOR#

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
 
        along with this program; if not, write to:
 
        Free Software Foundation, Inc.
        59 Temple Place - Suite 330
        Boston, MA  02111-1307, USA
 
        $Id$

*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <ctype.h>
#include <stdlib.h>

#include "QF/console.h"
#include "QF/gib.h"
#include "QF/quakeio.h"

#include "gib_modules.h"

static gib_module_t *gibmodules;

void
GIB_Module_Load (char *name, QFile *f)
{
	char        line[1024];
	gib_module_t *newmod;
	gib_sub_t  *newsub;
	int         nameofs;
	int         namelen;

	newmod = GIB_Create_Module (name);

	while (Qgets (f, line, 1024)) {
		if (strncmp ("sub", line, 3) == 0) {
			nameofs = 3;
			while (isspace (line[nameofs]))
				nameofs++;
			namelen = 0;
			while (!(isspace (line[nameofs + namelen]))) {
				namelen++;
			}
			line[nameofs + namelen] = 0;
			newsub = GIB_Create_Sub (newmod, line + nameofs);
			GIB_Read_Sub (newsub, f);
		}
	}
}

gib_module_t *
GIB_Create_Module (char *name)
{
	gib_module_t *new;

	new = malloc (sizeof (gib_module_t));
	new->name = malloc (strlen (name) + 1);
	strcpy (new->name, name);
	new->subs = 0;
	new->vars = 0;
	new->next = gibmodules;
	gibmodules = new;
	return new;
}

gib_sub_t  *
GIB_Create_Sub (gib_module_t * mod, char *name)
{
	gib_sub_t  *new;

	new = malloc (sizeof (gib_sub_t));
	new->name = malloc (strlen (name) + 1);
	strcpy (new->name, name);
	new->code = 0;
	new->vars = 0;
	new->next = mod->subs;
	mod->subs = new;
	return new;
}

void
GIB_Read_Sub (gib_sub_t * sub, QFile *f)
{
	char        line[1024];
	fpos_t      begin;
	int         sublen = 0;
	int         insub = 0;

	while (Qgets (f, line, 1024)) {
		if (strncmp ("}}", line, 2) == 0 && insub == 1) {
			insub = 0;
			break;
		}
		if (insub == 1) {
			sublen += strlen (line);
		}
		if (strncmp ("{{", line, 2) == 0) {
			Qgetpos (f, &begin);
			insub = 1;
		}
	}

	sub->code = malloc (sublen + 1);
	Qsetpos (f, &begin);
	Qread (f, sub->code, sublen);
	sub->code[sublen] = 0;
	Con_Printf ("Loaded sub %s\n", sub->name);
}

gib_module_t *
GIB_Find_Module (char *name)
{
	gib_module_t *mod;

	if (!(gibmodules))
		return 0;
	for (mod = gibmodules; strcmp (mod->name, name); mod = mod->next)
		if (mod->next == 0)
			return 0;
	return mod;
}

gib_sub_t  *
GIB_Find_Sub (gib_module_t * mod, char *name)
{
	gib_sub_t  *sub;

	if (!(mod->subs))
		return 0;
	for (sub = mod->subs; strcmp (sub->name, name); sub = sub->next)
		if (sub->next == 0)
			return 0;
	return sub;
}

void
GIB_Stats_f (void)
{
	int         modc, subc;

	gib_module_t *mod;
	gib_sub_t  *sub;

	modc = 0;
	Con_Printf ("---=== GIB statistics ===---\n");
	for (mod = gibmodules; mod; mod = mod->next) {
		Con_Printf ("\nSubroutines for module %s:\n", mod->name);
		Con_Printf ("-------------------------------------\n");
		subc = 0;
		for (sub = mod->subs; sub; sub = sub->next) {
			Con_Printf ("%s::%s\n", mod->name, sub->name);
			subc++;
		}
		Con_Printf ("-------------------------------------\nSubroutines: %i\n",
					subc);
		modc++;
	}
	Con_Printf ("Modules installed: %i\n", modc);
	Con_Printf ("---=== GIB statistics ===---\n");
}
