/*
	teamplay.c

	Teamplay enhancements ("proxy features")

	Copyright (C) 2000       Anton Gavrilov (tonik@quake.ru)

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

#include <errno.h>

#include <ctype.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/gib.h"
#include "QF/model.h"
#include "QF/va.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/teamplay.h"

#include "QF/scene/scene.h"

#include "compat.h"

#include "client/locs.h"
#include "client/world.h"

#include "qw/bothdefs.h"
#include "qw/include/cl_input.h"
#include "qw/include/client.h"

static bool died = false, recorded_location = false;
static vec4f_t  death_location, last_recorded_location;

int cl_deadbodyfilter;
static cvar_t cl_deadbodyfilter_cvar = {
	.name = "cl_deadbodyfilter",
	.description =
		"Hide dead player models",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_deadbodyfilter },
};
int cl_gibfilter;
static cvar_t cl_gibfilter_cvar = {
	.name = "cl_gibfilter",
	.description =
		"Hide gibs",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_gibfilter },
};
int cl_parsesay;
static cvar_t cl_parsesay_cvar = {
	.name = "cl_parsesay",
	.description =
		"Use .loc files to find your present location when you put %l in "
		"messages",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_parsesay },
};
int cl_nofake;
static cvar_t cl_nofake_cvar = {
	.name = "cl_nofake",
	.description =
		"Unhide fake messages",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_nofake },
};
float cl_freply;
static cvar_t cl_freply_cvar = {
	.name = "cl_freply",
	.description =
		"Delay between replies to f_*. 0 disables. Minimum suggested setting "
		"is 20",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &cl_freply },
};


void
Team_BestWeaponImpulse (void)
{
	int		best, i, imp, items;

	items = cl.stats[STAT_ITEMS];
	best = 0;

	for (i = Cmd_Argc () - 1; i > 0; i--) {
		imp = atoi (Cmd_Argv (i));
		if (imp < 1 || imp > 8)
			continue;

		switch (imp) {
		case 1:
			if (items & IT_AXE)
				best = 1;
			break;
		case 2:
			if (items & IT_SHOTGUN && cl.stats[STAT_SHELLS] >= 1)
				best = 2;
			break;
		case 3:
			if (items & IT_SUPER_SHOTGUN && cl.stats[STAT_SHELLS] >= 2)
				best = 3;
			break;
		case 4:
			if (items & IT_NAILGUN && cl.stats[STAT_NAILS] >= 1)
				best = 4;
			break;
		case 5:
			if (items & IT_SUPER_NAILGUN && cl.stats[STAT_NAILS] >= 2)
				best = 5;
			break;
		case 6:
			if (items & IT_GRENADE_LAUNCHER && cl.stats[STAT_ROCKETS] >= 1)
				best = 6;
			break;
		case 7:
			if (items & IT_ROCKET_LAUNCHER && cl.stats[STAT_ROCKETS] >= 1)
				best = 7;
			break;
		case 8:
			if (items & IT_LIGHTNING && cl.stats[STAT_CELLS] >= 1)
				best = 8;
		}
	}

	if (best)
		in_impulse = best;
}

//FIXME slow use of dstring
const char *
Team_ParseSay (dstring_t *buf, const char *s)
{
	char        chr, t2[128], t3[2];
	const char *t1;
	size_t      i, bracket;
	static location_t *location = NULL;

	if (!cl_parsesay || (cl.fpd & FPD_NO_MACROS))
		return s;

	i = 0;

	while (*s && (i <= sizeof (buf))) {
		if ((*s == '%') && (s[1] != '\0')) {
			t1 = NULL;
			memset (t2, '\0', sizeof (t2));
			memset (t3, '\0', sizeof (t3));

			if ((s[1] == '[') && (s[3] == ']')) {
				bracket = 1;
				chr = s[2];
				s += 4;
			} else {
				bracket = 0;
				chr = s[1];
				s += 2;
			}

			switch (chr) {
			case '%':
				t2[0] = '%';
				t2[1] = 0;
				t1 = t2;
				break;
			case 'S':
				bracket = 0;
				t1 = skin;
				break;
			case 'd':
				bracket = 0;
				if (died) {
					location = locs_find (death_location);
					if (location) {
						recorded_location = true;
						last_recorded_location = death_location;
						t1 = location->name;
						break;
					}
				}
				goto location;
			case 'r':
				bracket = 0;
				if (recorded_location) {
					location = locs_find (last_recorded_location);
					if (location) {
						t1 = location->name;
						break;
					}
				}
				goto location;
			case 'l':
			location:
				bracket = 0;
				location = locs_find (cl.viewstate.player_origin);
				if (location) {
					recorded_location = true;
					last_recorded_location = cl.viewstate.player_origin;
					t1 = location->name;
				} else
					snprintf (t2, sizeof (t2), "Unknown!");
				break;
			case 'a':
				if (bracket) {
					if (cl.stats[STAT_ARMOR] > 50)
						bracket = 0;

					if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
						t3[0] = (char) ('R' | 0x80);
					else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
						t3[0] = (char) ('Y' | 0x80);
					else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
						t3[0] = (char) ('G' | 0x80);
					else {
						t2[0] = (char) ('N' | 0x80);
						t2[1] = (char) ('O' | 0x80);
						t2[2] = (char) ('N' | 0x80);
						t2[3] = (char) ('E' | 0x80);
						t2[4] = (char) ('!' | 0x80);
					}

					snprintf (t2, sizeof (t2), "%sa:%i", t3,
							  cl.stats[STAT_ARMOR]);
				} else {
					snprintf (t2, sizeof (t2), "%i", cl.stats[STAT_ARMOR]);
				}
				break;
			case 'A':
				bracket = 0;
				if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
					t2[0] = (char) ('R' | 0x80);
				else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
					t2[0] = (char) ('Y' | 0x80);
				else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
					t2[0] = (char) ('G' | 0x80);
				else {
					t2[0] = (char) ('N' | 0x80);
					t2[1] = (char) ('O' | 0x80);
					t2[2] = (char) ('N' | 0x80);
					t2[3] = (char) ('E' | 0x80);
					t2[4] = (char) ('!' | 0x80);
				}
				break;
			case 'h':
				if (bracket) {
					if (cl.stats[STAT_HEALTH] > 50)
						bracket = 0;
					snprintf (t2, sizeof (t2), "h:%i",
							  cl.stats[STAT_HEALTH]);
				} else
					snprintf (t2, sizeof (t2), "%i", cl.stats[STAT_HEALTH]);
				break;
			default:
				bracket = 0;
			}

			if (!t1) {
				if (!t2[0]) {
					t2[0] = '%';
					t2[1] = chr;
				}

				t1 = t2;
			}

			if (bracket) {
				dstring_appendstr (buf, "\x90");	// '['
			}

			if (t1) {
				dstring_appendstr (buf, t1);
			}

			if (bracket) {
				dstring_appendstr (buf, "\x91");	// ']'
			}

			continue;
		}
		dstring_appendsubstr (buf, s++, 1);
	}

	return buf->str;
}

void
Team_Dead (void)
{
	died = true;
	death_location = cl.viewstate.player_origin;
}

void
Team_NewMap (void)
{
	char       *mapname, *t1, *t2;

	died = false;
	recorded_location = false;
	mapname = strdup (cl_world.scene->worldmodel->path);
	t2 = malloc (sizeof (cl_world.scene->worldmodel->path));
	if (!mapname || !t2)
		Sys_Error ("Can't duplicate mapname!");
	map_to_loc (mapname,t2);
	t1 = strrchr (t2, '/');
	if (!t1)
		Sys_Error ("Can't find /!");
	t1++;								// skip over /
	locs_reset ();
	locs_load (t1);
	free (mapname);
	free (t2);
}

void
Team_Init_Cvars (void)
{
	Cvar_Register (&cl_deadbodyfilter_cvar, 0, 0);
	Cvar_Register (&cl_gibfilter_cvar, 0, 0);
	Cvar_Register (&cl_parsesay_cvar, 0, 0);
	Cvar_Register (&cl_nofake_cvar, 0, 0);
	Cvar_Register (&cl_freply_cvar, 0, 0);
}

/*
  locs_loc

  Location marker manipulation
*/
static void
locs_loc (void)
{
	char            locfile[MAX_OSPATH];
	char           *mapname;
	const char     *desc = NULL;

	// FIXME: need to check to ensure you are actually in the game and alive.
	if (Cmd_Argc () == 1 || strcmp (Cmd_Argv (1), "help") == 0) {
		Sys_Printf ("loc <add|delete|rename|move|save|zsave> [<description>] "
					":Modifies location data, add|rename take <description> "
					"parameter\n");
		return;
	}
	if (!cl_world.scene->worldmodel) {
		Sys_Printf ("No map loaded. Unable to work with location markers.\n");
		return;
	}
	if (Cmd_Argc () >= 3)
		desc = Cmd_Args (2);
	mapname = malloc (sizeof (cl_world.scene->worldmodel->path));
	if (!mapname)
		Sys_Error ("Can't duplicate mapname!");
	map_to_loc (cl_world.scene->worldmodel->path, mapname);
	snprintf (locfile, sizeof (locfile), "%s/%s",
			  qfs_gamedir->dir.def, mapname);
	free (mapname);

	if (strcasecmp (Cmd_Argv (1), "save") == 0) {
		if (Cmd_Argc () == 2) {
			locs_save (locfile, false);
		} else
			Sys_Printf ("loc save :saves locs from memory into a .loc file\n");
	}

	if (strcasecmp (Cmd_Argv (1), "zsave") == 0) {
		if (Cmd_Argc () == 2) {
			locs_save (locfile, true);
		} else
			Sys_Printf ("loc save :saves locs from memory into a .loc file\n");
	}

	if (strcasecmp (Cmd_Argv (1), "add") == 0) {
		if (Cmd_Argc () >= 3)
			locs_mark (cl.viewstate.player_origin, desc);
		else
			Sys_Printf ("loc add <description> :marks the current location "
						"with the description and records the information "
						"into a loc file.\n");
	}

	if (strcasecmp (Cmd_Argv (1), "rename") == 0) {
		if (Cmd_Argc () >= 3)
			locs_edit (cl.viewstate.player_origin, desc);
		else
			Sys_Printf ("loc rename <description> :changes the description of "
					    "the nearest location marker\n");
	}

	if (strcasecmp (Cmd_Argv (1),"delete") == 0) {
		if (Cmd_Argc () == 2)
			locs_del (cl.viewstate.player_origin);
		else
			Sys_Printf ("loc delete :removes nearest location marker\n");
	}

	if (strcasecmp (Cmd_Argv (1),"move") == 0) {
		if (Cmd_Argc () == 2)
			locs_edit (cl.viewstate.player_origin, NULL);
		else
			Sys_Printf ("loc move :moves the nearest location marker to your "
						"current location\n");
	}
}

static void
Locs_Loc_Get (void)
{
	location_t *location;

	if (GIB_Argc () != 1)
		GIB_USAGE ("");
	else {
		location = locs_find (cl.viewstate.player_origin);
		GIB_Return (location ? location->name : "unknown");
	}
}

void
Locs_Init (void)
{
	Cmd_AddCommand ("loc", locs_loc, "Location marker editing commands: 'loc "
					"help' for more");
	GIB_Builtin_Add ("loc::get", Locs_Loc_Get);
}

static const char *
Team_F_Version (char *args)
{
	return va ("say %s", PACKAGE_STRING);
}

static const char *
Team_F_Skins (char *args)
{
	int		totalfb, l;
	float		allfb = 0.0;

	allfb = min (cl.fbskins, cl_fb_players);

	if (allfb >= 1.0) {
		return "say Player models fullbright";
	}

	while (isspace ((byte) *args))
		args++;
	for (l = 0; args[l] && !isspace ((byte) args[l]); l++);

	if (l == 0) {
		//XXXtotalfb = Skin_FbPercent (0);
		totalfb = 0;
		return va ("say Player models have %f%% brightness\n"
			   "say Average percent fullbright for all loaded skins is "
			   "%d.%d%%", allfb * 100, totalfb / 10, totalfb % 10);
	}

	//XXX totalfb = Skin_FbPercent (args);
	totalfb = 0;

	if (totalfb >= 0)
		return va ("say \"Skin %s is %d.%d%% fullbright\"",
				   args, totalfb / 10, totalfb % 10);
	else
		return ("say \"Skin not currently loaded.\"");
}

static freply_t f_replies[] = {
	{"f_version", Team_F_Version, 0},
	{"f_skins", Team_F_Skins, 0},
};

void
Team_ParseChat (const char *string)
{
	char	*s;
	unsigned int i;

	if (!cl_freply)
		return;

	if (!(s = strchr (string, ':')))
		return;
	s++;
	while (isspace ((byte) *s))
		s++;

	for (i = 0; i < sizeof (f_replies) / sizeof (f_replies[0]); i++) {
		if (!strncmp (f_replies[i].name, s, strlen (f_replies[i].name))
			&& realtime - f_replies[i].lasttime >= cl_freply) {
			while (*s && !isspace ((byte) *s))
				s++;
			Cbuf_AddText (cl_cbuf, f_replies[i].func (s));
			Cbuf_AddText (cl_cbuf, "\n");
			f_replies[i].lasttime = realtime;
		}
	}
}

void
Team_ResetTimers (void)
{
	unsigned int i;

	for (i = 0; i < sizeof (f_replies) / sizeof (f_replies[0]); i++)
		f_replies[i].lasttime = realtime - cl_freply;
	return;
}
