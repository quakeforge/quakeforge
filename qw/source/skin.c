/*
	skin.c

	(description)

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

#include "QF/cmd.h"
#include "QF/compat.h"
#include "QF/console.h"
#include "QF/hash.h"
#include "QF/info.h"
#include "QF/msg.h"
#include "QF/pcx.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/texture.h"
#include "QF/va.h"

#include "cl_parse.h"
#include "client.h"
#include "host.h"

cvar_t     *baseskin;
cvar_t     *noskins;
cvar_t     *skin;
cvar_t     *topcolor;
cvar_t     *bottomcolor;

char        allskins[128];

skin_t      skin_cache[MAX_CACHED_SKINS];
hashtab_t  *skin_hash;
int         numskins;

static char *
skin_get_key (void *_skin, void *unused)
{
	skin_t *skin = (skin_t*)_skin;
	return skin->name;
}

/*
	Skin_Find

	Determines the best skin for the given scoreboard
	slot, and sets scoreboard->skin
*/
void
Skin_Find (player_info_t *sc)
{
	skin_t     *skin;
	char        name[128], *s;

	if (allskins[0])
		strncpy (name, allskins, sizeof (name));
	else {
		s = Info_ValueForKey (sc->userinfo, "skin");
		if (s && s[0])
			strncpy (name, s, sizeof (name));
		else
			strncpy (name, baseskin->string, sizeof (name));
	}

	if (strstr (name, "..") || *name == '.')
		strcpy (name, "base");

	COM_StripExtension (name, name);

	skin = Hash_Find (skin_hash, name);
	if (skin) {
		sc->skin = skin;
		Skin_Cache (sc->skin);
		return;
	}

	if (numskins == MAX_CACHED_SKINS) {	// ran out of spots, so flush
										// everything
		Skin_Skins_f ();
		return;
	}

	skin = &skin_cache[numskins];
	sc->skin = skin;
	numskins++;

	memset (skin, 0, sizeof (*skin));
	strncpy (skin->name, name, sizeof (skin->name) - 1);

	Hash_Add (skin_hash, skin);
}


/*
	Skin_Cache

	Returns a pointer to the skin bitmap, or NULL to use the default
*/
tex_t *
Skin_Cache (skin_t *skin)
{
	char        name[1024];
	tex_t      *out;
	QFile      *file;
	tex_t      *tex;
	int         pixels;
	byte       *ipix, *opix;
	int         i;

	if (cls.downloadtype == dl_skin)		// use base until downloaded
		return NULL;
	// NOSKINS > 1 will show skins, but not download new ones.
	if (noskins->int_val == 1)
		return NULL;

	if (skin->failedload)
		return NULL;

	out = Cache_Check (&skin->cache);
	if (out)
		return out;

	// load the pic from disk
	snprintf (name, sizeof (name), "skins/%s.pcx", skin->name);
	COM_FOpenFile (name, &file);
	if (!file) {
		Con_Printf ("Couldn't load skin %s\n", name);
		snprintf (name, sizeof (name), "skins/%s.pcx", baseskin->string);
		COM_FOpenFile (name, &file);
		if (!file) {
			skin->failedload = true;
			return NULL;
		}
	}
	tex = LoadPCX (file, 0);
	Qclose (file);

	if (!tex || tex->width > 320 || tex->height > 200) {
		skin->failedload = true;
		Con_Printf ("Bad skin %s\n", name);
		return NULL;
	}
	pixels = 320 * 200;

	out = Cache_Alloc (&skin->cache, sizeof (tex_t) + pixels, skin->name);
	if (!out)
		Sys_Error ("Skin_Cache: couldn't allocate");
	opix = out->data;
	out->width = 320;
	out->height = 200;
	out->palette = tex->palette; //FIXME assumes 0 or vid_basepal
	memset (opix, 0, pixels);
	for (i = 0, ipix = tex->data; i < tex->height;
	     i++, opix += 320, ipix += tex->width)
		memcpy (opix, ipix, tex->width);

	Skin_Process (skin, out);

	skin->failedload = false;

	return out;
}


/*
	Skin_NextDownload
*/
void
Skin_NextDownload (void)
{
	player_info_t *sc;
	int         i;

	if (cls.downloadnumber == 0) {
		Con_Printf ("Checking skins...\n");
		SCR_UpdateScreen ();
	}
	cls.downloadtype = dl_skin;

	for (; cls.downloadnumber != MAX_CLIENTS; cls.downloadnumber++) {
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name[0])
			continue;
		Skin_Find (sc);
		if (noskins->int_val)
			continue;
		if (!CL_CheckOrDownloadFile (va ("skins/%s.pcx", sc->skin->name)))
			return;						// started a download
	}

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i = 0; i < MAX_CLIENTS; i++) {
		sc = &cl.players[i];
		if (!sc->name[0])
			continue;
		Skin_Find (sc);
		Skin_Cache (sc->skin);
		sc->skin = NULL;
	}

	if (cls.state != ca_active) {		// get next signon phase
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va ("begin %i", cl.servercount));
		Cache_Report ();				// print remaining memory
	}
}


/*
	Skin_Skins_f

	Refind all skins, downloading if needed.
*/
void
Skin_Skins_f (void)
{
	int         i;

	for (i = 0; i < numskins; i++) {
		if (skin_cache[i].cache.data)
			Cache_Free (&skin_cache[i].cache);
	}
	numskins = 0;
	Hash_FlushTable (skin_hash);

	cls.downloadnumber = 0;
	cls.downloadtype = dl_skin;
	Skin_NextDownload ();
}


/*
	Skin_AllSkins_f

	Sets all skins to one specific one
*/
void
Skin_AllSkins_f (void)
{
	strcpy (allskins, Cmd_Argv (1));
	Skin_Skins_f ();
}

void
CL_Color_f (void)
{
	// just for quake compatability...
	int         top, bottom;
	char        num[16];

	if (Cmd_Argc () == 1) {
		Con_Printf ("\"color\" is \"%s %s\"\n",
					Info_ValueForKey (cls.userinfo, "topcolor"),
					Info_ValueForKey (cls.userinfo, "bottomcolor"));
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc () == 2)
		top = bottom = atoi (Cmd_Argv (1));
	else {
		top = atoi (Cmd_Argv (1));
		bottom = atoi (Cmd_Argv (2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	snprintf (num, sizeof (num), "%i", top);
	Cvar_Set (topcolor, num);
	snprintf (num, sizeof (num), "%i", bottom);
	Cvar_Set (bottomcolor, num);
}

void
Skin_Init (void)
{
	skin_hash = Hash_NewTable (1021, skin_get_key, 0, 0);
	Cmd_AddCommand ("skins", Skin_Skins_f, "Download all skins that are currently in use");
	Cmd_AddCommand ("allskins", Skin_AllSkins_f, "Force all player skins to one skin");
	Cmd_AddCommand ("color", CL_Color_f, "The pant and shirt color (color shirt pants) Note that if only shirt color is given, pants will match");
	Skin_Init_Translation ();
}

void
Skin_Init_Cvars (void)
{
	baseskin = Cvar_Get ("baseskin", "base", CVAR_NONE, NULL,
						 "default base skin name");
	noskins = Cvar_Get ("noskins", "0", CVAR_NONE, NULL,
						"set to 1 to not download new skins");
	skin = Cvar_Get ("skin", "", CVAR_ARCHIVE | CVAR_USERINFO, Cvar_Info,
			"Players skin");
	topcolor = Cvar_Get ("topcolor", "0", CVAR_ARCHIVE | CVAR_USERINFO,
			Cvar_Info, "Players color on top");
	bottomcolor = Cvar_Get ("bottomcolor", "0", CVAR_ARCHIVE | CVAR_USERINFO,
			Cvar_Info, "Players color on bottom");
}

/*
	CL_NewTranslation
*/
void
CL_NewTranslation (int slot)
{
	player_info_t *player;
	char        s[512];

	if (slot > MAX_CLIENTS)
		Host_EndGame ("CL_NewTranslation: slot > MAX_CLIENTS");

	player = &cl.players[slot];
	if (!player->name[0])
		return;

	strcpy (s, Info_ValueForKey (player->userinfo, "skin"));
	COM_StripExtension (s, s);
	if (player->skin && !strequal (s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor != player->topcolor ||
		player->_bottomcolor != player->bottomcolor || !player->skin) {
		player->_topcolor = player->topcolor;
		player->_bottomcolor = player->bottomcolor;

		Skin_Set_Translate (player->topcolor, player->bottomcolor,
							player->translations);
		if (!player->skin)
			Skin_Find (player);
		Skin_Do_Translation (player->skin, slot);
	}
}
