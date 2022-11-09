/*
	sbar.c

	Status bar

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

#include <time.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/gib.h"
#include "QF/info.h"
#include "QF/quakefs.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/wad.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "QF/ui/passage.h"
#include "QF/ui/view.h"

#include "compat.h"
#include "sbar.h"

#include "client/hud.h"
#include "client/screen.h"
#include "client/state.h"
#include "client/world.h"

#include "gamedefs.h"

int         sb_updates;				// if >= vid.numpages, no update needed
static int sb_update_flags;
static int sb_view_size;
static int fps_count;

static const char *sbar_levelname;
static const char *sbar_servername;
static player_info_t *sbar_players;
static int *sbar_stats;
static float *sbar_item_gettime;
static double sbar_time;
static double sbar_completed_time;
static double sbar_faceanimtime;
static int sbar_maxplayers;
static int sbar_playernum;
static int sbar_viewplayer;
static int sbar_spectator;
static int sbar_teamplay;
static int sbar_active;
static int sbar_intermission;

static view_t intermission_view;
static view_t   intermission_time;
static view_t   intermission_secr;
static view_t   intermission_kill;
//     view_t hud_view;
static view_t   hud_miniteam;
static view_t   sbar_main;
static view_t     hud_minifrags;		// child of sbar_main for positioning
static view_t     sbar_inventory;
static view_t       sbar_frags;
static view_t       sbar_sigils;
static view_t       sbar_items;
static view_t       sbar_armament;
static view_t         sbar_weapons;
static view_t         sbar_miniammo;
static view_t     sbar_statusbar;
static view_t       sbar_armor;
static view_t       sbar_face;
static view_t       sbar_health;
static view_t       sbar_ammo;
static view_t     sbar_solo;
static view_t       sbar_solo_monsters;
static view_t       sbar_solo_secrets;
static view_t       sbar_solo_time;
static view_t       sbar_solo_anchor;
static view_t         sbar_solo_name;
static view_t     sbar_tile[2];
static view_t     sbar_tile[2];
static view_t dmo_view;

typedef struct view_def_s {
	view_t     *view;
	struct {
		int         x;
		int         y;
		int         w;
		int         h;
	}           rect;
	grav_t      gravity;
	view_t     *parent;
	int         count;
	int         xstep;
	int         ystep;
	struct view_def_s *subviews;
} view_def_t;

// used for "current view"
static view_t pseudo_parent = nullview;
static view_def_t frags_defs[] = {
	{0, {4, 1, 28, 4}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, {4, 5, 28, 3}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, {6, 0, 24, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, {0, 0, 34, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{}
};
static view_def_t minifrags_defs[] = {
	{0, { 2, 1, 37, 3}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, { 2, 4, 37, 4}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, { 8, 0, 24, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, { 0, 0, 40, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	// teamplay team, name
	{0, {48, 0, 32, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, {88, 0,104, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	// name
	{0, {48, 0,128, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{}
};
static view_def_t miniteam_defs[] = {
	{0, { 0, 0, 32, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, {40, 0, 40, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{0, {-8, 0, 48, 8}, grav_northwest, &pseudo_parent, 1, 0, 0},
	{}
};

static view_def_t sbar_defs[] = {
	{&hud_overlay_view,  {  0, 0,320,200}, grav_center, &cl_screen_view},
	{&intermission_view, {  0, 0,320,200}, grav_northwest, &hud_overlay_view},
	{0, {64, 24, 24, 24}, grav_northwest, &intermission_view, 1, 0, 0},
	{0, {0, 56, 24, 24}, grav_northwest, &intermission_view, 1, 0, 0},
	{&intermission_time, {160,64,134,24}, grav_northwest, &intermission_view},
	{0, {0, 0, 24, 24}, grav_northwest, &intermission_time, 3, 24, 0},
	{0, {74, 0, 16, 24}, grav_northwest, &intermission_time, 1, 0, 0},
	{0, {86, 0, 24, 24}, grav_northwest, &intermission_time, 2, 24, 0},
	{&intermission_secr, {160,104,152,24}, grav_northwest, &intermission_view},
	{0, {0, 0, 24, 24}, grav_northwest, &intermission_secr, 3, 24, 0},
	{0, {72, 0, 16, 24}, grav_northwest, &intermission_secr, 1, 0, 0},
	{0, {80, 0, 24, 24}, grav_northwest, &intermission_secr, 3, 24, 0},
	{&intermission_kill, {160,144,152,24}, grav_northwest, &intermission_view},
	{0, {0, 0, 24, 24}, grav_northwest, &intermission_kill, 3, 24, 0},
	{0, {72, 0, 16, 24}, grav_northwest, &intermission_kill, 1, 0, 0},
	{0, {80, 0, 24, 24}, grav_northwest, &intermission_kill, 3, 24, 0},

	{&hud_view,           {  0, 0,320, 48}, grav_south,     &cl_screen_view},
	{&hud_miniteam,       {  0, 0, 96, 48}, grav_southeast, &hud_view},
	{0, {0,0,96,8}, grav_northwest, &hud_miniteam, 6, 0, 8, miniteam_defs},
	{&sbar_main,          {  0, 0,320, 48}, grav_south,     &hud_view},
	{&hud_minifrags,     {-192, 0,192, 48}, grav_southeast, &sbar_main},
	{0, {0,0,192,8}, grav_northwest, &hud_minifrags, 6, 0, 8, minifrags_defs},

	{&sbar_inventory,     {  0, 0,320, 24}, grav_northwest, &sbar_main},
	{&sbar_frags,         {  0, 0,130,  8}, grav_northeast, &sbar_inventory},
	{&sbar_sigils,        {  0, 0, 32, 16}, grav_southeast, &sbar_inventory},
	{&sbar_items,         { 32, 0, 96, 16}, grav_southeast, &sbar_inventory},
	//NOTE sbar_armament moves and gets layed out again on hud_sbar change
	{&sbar_armament,      {  0, 0,202, 24}, grav_northwest, &sbar_inventory},
	{&sbar_weapons,       {  0, 0,192, 16}, grav_southwest, &sbar_armament},
	{&sbar_miniammo,      {  0, 0, 32,  8}, grav_northwest, &sbar_armament},

	{&sbar_statusbar,     {  0, 0,320, 24}, grav_southwest, &sbar_main},
	{&sbar_armor,         {  0, 0, 96, 24}, grav_northwest, &sbar_statusbar},
	{&sbar_face,          {112, 0, 24, 24}, grav_northwest, &sbar_statusbar},
	{&sbar_health,        {136, 0, 72, 24}, grav_northwest, &sbar_statusbar},
	{&sbar_ammo,          {224, 0, 96, 24}, grav_northwest, &sbar_statusbar},
	{&sbar_tile[0],       {  0, 0,  0, 48}, grav_southwest, &sbar_main},
	{&sbar_tile[1],       {  0, 0,  0, 48}, grav_southeast, &sbar_main},
	{&sbar_solo,          {  0, 0,320, 24}, grav_southwest, &sbar_main},
	{&sbar_solo_monsters, {  8, 4,136,  8}, grav_northwest, &sbar_solo},
	{&sbar_solo_secrets,  {  8,12,136,  8}, grav_northwest, &sbar_solo},
	{&sbar_solo_time,     {184, 4, 96,  8}, grav_northwest, &sbar_solo},
	{&sbar_solo_anchor,   {232,12,  0,  8}, grav_northwest, &sbar_solo},
	{&sbar_solo_name,     {  0, 0,  0,  8}, grav_center,    &sbar_solo_anchor},
	{0, { 0, 0, 32,  8}, grav_northwest, &sbar_frags,    4, 32, 0, frags_defs},
	{0, { 0, 0,  8, 16}, grav_northwest, &sbar_sigils,   4,  8, 0},
	{0, { 0, 0, 24, 24}, grav_northwest, &sbar_armor,    4, 24, 0},
	{0, { 0, 0, 24, 24}, grav_northwest, &sbar_ammo,     4, 24, 0},
	{0, { 0, 0, 16, 16}, grav_northwest, &sbar_items,    6, 16, 0},
	{0, { 0, 0, 24, 16}, grav_northwest, &sbar_weapons,  7, 24, 0},
	{0, { 0, 0, 24, 24}, grav_northwest, &sbar_health,   3, 24, 0},
	{0, {10, 0, 24,  8}, grav_northwest, &sbar_miniammo, 4, 48, 0},

	{&dmo_view,           {  0, 0,320, 200}, grav_center,   &cl_screen_view},

	{}
};

static draw_charbuffer_t *time_buff;
static draw_charbuffer_t *fps_buff;

static draw_charbuffer_t *solo_monsters;
static draw_charbuffer_t *solo_secrets;
static draw_charbuffer_t *solo_time;
static draw_charbuffer_t *solo_name;

static view_t
sbar_view (int x, int y, int w, int h, grav_t gravity, view_t parent)
{
	view_t      view = View_New (hud_registry, parent);
	View_SetPos (view, x, y);
	View_SetLen (view, w, h);
	View_SetGravity (view, gravity);
	View_SetVisible (view, 1);
	return view;
}

static inline void
sbar_setcomponent (view_t view, uint32_t comp, const void *data)
{
	Ent_SetComponent (view.id, comp, view.reg, data);
}

static inline int
sbar_hascomponent (view_t view, uint32_t comp)
{
	return Ent_HasComponent (view.id, comp, view.reg);
}

static inline void *
sbar_getcomponent (view_t view, uint32_t comp)
{
	return Ent_GetComponent (view.id, comp, view.reg);
}

static inline void
sbar_remcomponent (view_t view, uint32_t comp)
{
	Ent_RemoveComponent (view.id, comp, view.reg);
}

#define STAT_MINUS		10			// num frame for '-' stats digit

static qpic_t *sb_nums[2][11];
static qpic_t *sb_colon, *sb_slash;
static qpic_t *sb_ibar;
static qpic_t *sb_sbar;
static qpic_t *sb_scorebar;

// 0 is active, 1 is owned, 2-6 are flashes
static hud_subpic_t sb_weapons[7][8];
static qpic_t *sb_ammo[4];
static qpic_t *sb_sigil[4];
static qpic_t *sb_armor[3];
static qpic_t *sb_items[32];

static qpic_t *sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are alive
									// 0 is static, 1 is temporary animation
static qpic_t *sb_face_invis;
static qpic_t *sb_face_quad;
static qpic_t *sb_face_invuln;
static qpic_t *sb_face_invis_invuln;

static qboolean sb_showscores;
static qboolean sb_showteamscores;

static int sb_lines;				// scan lines to draw

static qpic_t *rsb_invbar[2];
static qpic_t *rsb_weapons[5];
static qpic_t *rsb_items[2];
static qpic_t *rsb_ammo[3];
static qpic_t *rsb_teambord;		// PGM 01/19/97 - team color border

								// MED 01/04/97 added two more weapons + 3
								// alternates for grenade launcher
static qpic_t *hsb_weapons[7][5];	// 0 is active, 1 is owned, 2-5 are flashes

								// MED 01/04/97 added array to simplify
								// weapon parsing
//static int hipweapons[4] =
//	{ HIT_LASER_CANNON_BIT, HIT_MJOLNIR_BIT, 4, HIT_PROXIMITY_GUN_BIT };
qpic_t     *hsb_items[2];			// MED 01/04/97 added hipnotic items array

//static qboolean largegame = false;

char *fs_fraglog;
static cvar_t fs_fraglog_cvar = {
	.name = "fs_fraglog",
	.description =
		"Filename of the automatic frag-log.",
	.default_value = "qw-scores.log",
	.flags = CVAR_ARCHIVE,
	.value = { .type = 0, .value = &fs_fraglog },
};
int cl_fraglog;
static cvar_t cl_fraglog_cvar = {
	.name = "cl_fraglog",
	.description =
		"Automatic fraglogging, non-zero value will switch it on.",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_fraglog },
};
int hud_scoreboard_uid;
static cvar_t hud_scoreboard_uid_cvar = {
	.name = "hud_scoreboard_uid",
	.description =
		"Set to 1 to show uid instead of ping. Set to 2 to show both.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &hud_scoreboard_uid },
};
float scr_centertime;
static cvar_t scr_centertime_cvar = {
	.name = "scr_centertime",
	.description =
		"How long in seconds screen hints are displayed",
	.default_value = "2",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scr_centertime },
};
float scr_printspeed;
static cvar_t scr_printspeed_cvar = {
	.name = "scr_printspeed",
	.description =
		"How fast the text is displayed at the end of the single player "
		"episodes",
	.default_value = "8",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scr_printspeed },
};

static void
viewsize_f (int view_size)
{
	sb_view_size = view_size;
	HUD_Calc_sb_lines (view_size);
	if (hud_sbar) {
		SCR_SetBottomMargin (hud_sbar ? sb_lines : 0);
	}
}

static int __attribute__((used))
Sbar_ColorForMap (int m)
{
	return (bound (0, m, 13) * 16) + 8;
}

static void
draw_num (view_t *view, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame, x = 0;

	if (num > 999999999)
		num = 999999999;

	l = snprintf (str, sizeof (str), "%d", num);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	while (digits > l) {
		sbar_remcomponent (view[x++], hud_pic);
		digits--;
	}

	while (*ptr) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		sbar_setcomponent (view[x++], hud_pic, &sb_nums[color][frame]);
		ptr++;
	}
}

static inline void
draw_smallnum (view_t view, int n, int packed, int colored)
{
	void       *comp = sbar_getcomponent (view, hud_charbuff);
	__auto_type charbuff = *(draw_charbuffer_t **) comp;
	char        num[4];

	packed = packed != 0;				// ensure 0 or 1

	n = bound (-99, n, 999);
	snprintf (num, sizeof (num), "%3d", n);
	for (int i = 0; i < 3; i++) {
		charbuff->chars[i] = num[i] - (colored && num[i] != ' ' ? '0' - 18 : 0);
	}
}

static void
draw_miniammo (view_t view)
{
	int         i, count;

	// ammo counts
	for (i = 0; i < 4; i++) {
		view_t      v = View_GetChild (view, i);
		count = sbar_stats[STAT_SHELLS + i];
		draw_smallnum (v, count, 0, 1);
	}
}

static void
draw_ammo (view_t view)
{
	qpic_t     *pic = 0;
	if (sbar_stats[STAT_ITEMS] & IT_SHELLS)
		pic = sb_ammo[0];
	else if (sbar_stats[STAT_ITEMS] & IT_NAILS)
		pic = sb_ammo[1];
	else if (sbar_stats[STAT_ITEMS] & IT_ROCKETS)
		pic = sb_ammo[2];
	else if (sbar_stats[STAT_ITEMS] & IT_CELLS)
		pic = sb_ammo[3];

	view_t      ammo = View_GetChild (view, 0);
	if (pic) {
		sbar_setcomponent (ammo, hud_pic, &pic);
	} else {
		sbar_remcomponent (ammo, hud_pic);
	}

	view_t      num[3] = {
		View_GetChild (view, 1),
		View_GetChild (view, 2),
		View_GetChild (view, 3),
	};
	draw_num (num, sbar_stats[STAT_AMMO], 3, sbar_stats[STAT_AMMO] <= 10);
}

static int
calc_flashon (float time, int mask)
{
	int         flashon;

	flashon = (int) ((sbar_time - time) * 10);
	if (flashon < 0)
		flashon = 0;
	if (flashon >= 10) {
		if (sbar_stats[STAT_ACTIVEWEAPON] == mask)
			flashon = 1;
		else
			flashon = 0;
	} else
		flashon = (flashon % 5) + 2;
	return flashon;
}

static void
draw_weapons (view_t view)
{
	int         flashon, i;

	for (i = 0; i < 7; i++) {
		view_t      weap = View_GetChild (view, i);
		if (sbar_stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = calc_flashon (sbar_item_gettime[i], IT_SHOTGUN << i);
			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
			sbar_setcomponent (weap, hud_subpic, &sb_weapons[flashon][i]);
		} else {
			sbar_remcomponent (weap, hud_subpic);
		}
	}
}

static void
draw_items (view_t view)
{
	//float       time;
	//int         flashon = 0, i;

	for (int i = 0; i < 6; i++) {
		view_t      item = View_GetChild (view, i);
		if (sbar_stats[STAT_ITEMS] & (1 << (17 + i))) {
#if 0
			time = sbar_item_gettime[17 + i];
			if (time && time > sbar_time - 2 && flashon) {	// Flash frame
				sb_updates = 0;
			} else {
				draw_pic (view, i * 16, 0, sb_items[i]);
			}
			if (time && time > sbar_time - 2)
				sb_updates = 0;
#endif
			sbar_setcomponent (item, hud_pic, &sb_items[i]);
		} else {
			sbar_remcomponent (item, hud_pic);
		}
	}
}

static void
draw_sigils (view_t view)
{
	//float       time;
	//int         flashon = 0, i;

	for (int i = 0; i < 4; i++) {
		view_t      sigil = View_GetChild (view, i);
		if (sbar_stats[STAT_ITEMS] & (1 << (28 + i))) {
#if 0
			time = sbar_item_gettime[28 + i];
			if (time && time > sbar_time - 2 && flashon) {	// flash frame
				sb_updates = 0;
			} else {
				draw_pic (view, i * 8, 0, sb_sigil[i]);
			}
			if (time && time > sbar_time - 2)
				sb_updates = 0;
#endif
			sbar_setcomponent (sigil, hud_pic, &sb_sigil[i]);
		} else {
			sbar_remcomponent (sigil, hud_pic);
		}
	}
}

typedef struct {
	char        team[16 + 1];
	int         frags;
	int         players;
	int         plow, phigh, ptotal;
} team_t;

team_t      teams[MAX_PLAYERS];
int         teamsort[MAX_PLAYERS];
int         fragsort[MAX_PLAYERS];
static view_t sb_views[MAX_PLAYERS];
static draw_charbuffer_t *sb_fph[MAX_PLAYERS];
static draw_charbuffer_t *sb_time[MAX_PLAYERS];
static draw_charbuffer_t *sb_frags[MAX_PLAYERS];
static draw_charbuffer_t *sb_team[MAX_PLAYERS];
static draw_charbuffer_t *sb_ping[MAX_PLAYERS];
static draw_charbuffer_t *sb_pl[MAX_PLAYERS];
static draw_charbuffer_t *sb_uid[MAX_PLAYERS];
static draw_charbuffer_t *sb_name[MAX_PLAYERS];
static draw_charbuffer_t *sb_team_frags[MAX_PLAYERS];
static draw_charbuffer_t *sb_spectator;
int         scoreboardlines, scoreboardteams;

static void
Sbar_SortFrags (qboolean includespec)
{
	int         i, j, k;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < sbar_maxplayers; i++) {
		if (sbar_players[i].name && sbar_players[i].name->value[0]
			&& (!sbar_players[i].spectator || includespec)) {
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
			if (sbar_players[i].spectator)
				sbar_players[i].frags = -999;
		}
	}
	for (i = scoreboardlines; i < sbar_maxplayers; i++) {
		fragsort[i] = -1;
	}

	player_info_t *p = sbar_players;
	for (i = 0; i < scoreboardlines; i++) {
		for (j = 0; j < scoreboardlines - 1 - i; j++) {
			if (p[fragsort[j]].frags < p[fragsort[j + 1]].frags) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}

static void __attribute__((used))
Sbar_SortTeams (void)
{
	char        t[16 + 1];
	int         i, j, k;
	player_info_t *s;

	// request new ping times every two second
	scoreboardteams = 0;

	if (!sbar_teamplay)
		return;

	// sort the teams
	memset (teams, 0, sizeof (teams));
	for (i = 0; i < MAX_PLAYERS; i++)
		teams[i].plow = 999;

	for (i = 0; i < MAX_PLAYERS; i++) {
		s = &sbar_players[i];
		if (!s->name || !s->name->value[0])
			continue;
		if (s->spectator)
			continue;

		// find his team in the list
		t[16] = 0;
		strncpy (t, s->team->value, 16);
		if (!t[0])
			continue;					// not on team
		for (j = 0; j < scoreboardteams; j++)
			if (!strcmp (teams[j].team, t)) {
				teams[j].frags += s->frags;
				teams[j].players++;
				goto addpinginfo;
			}
		if (j == scoreboardteams) {		// must add him
			j = scoreboardteams++;
			strcpy (teams[j].team, t);
			teams[j].frags = s->frags;
			teams[j].players = 1;
		  addpinginfo:
			if (teams[j].plow > s->ping)
				teams[j].plow = s->ping;
			if (teams[j].phigh < s->ping)
				teams[j].phigh = s->ping;
			teams[j].ptotal += s->ping;
		}
	}

	// sort
	for (i = 0; i < scoreboardteams; i++)
		teamsort[i] = i;

	// good 'ol bubble sort
	for (i = 0; i < scoreboardteams - 1; i++) {
		for (j = i + 1; j < scoreboardteams; j++) {
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags) {
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}
		}
	}
}

static int
write_charbuff (draw_charbuffer_t *buffer, int x, int y, const char *str)
{
	char       *dst = buffer->chars;
	int         count = buffer->width - x;
	int         chars = 0;
	dst += y * buffer->width + x;
	while (*str && count-- > 0) {
		*dst++ = *str++;
		chars++;
	}
	while (count-- > 0) {
		*dst++ = ' ';
	}
	return chars;
}

static void
draw_solo_time (void)
{
	int         minutes = sbar_time / 60;
	int         seconds = sbar_time - 60 * minutes;
	write_charbuff (solo_time, 0, 0,
					va (0, "Time :%3i:%02i", minutes, seconds));
}

static void
draw_solo (void)
{
	write_charbuff (solo_monsters, 0, 0,
					va (0, "Monsters:%3i /%3i",
						sbar_stats[STAT_MONSTERS], sbar_stats[STAT_TOTALMONSTERS]));
	write_charbuff (solo_secrets, 0, 0,
					va (0, "Secrets :%3i /%3i",
						sbar_stats[STAT_SECRETS], sbar_stats[STAT_TOTALSECRETS]));
	draw_solo_time ();
	view_pos_t len = View_GetLen (sbar_solo_name);
	len.x = 8 * write_charbuff (solo_name, 0, 0, sbar_levelname);
	View_SetLen (sbar_solo_name, len.x, len.y);
}

static void
clear_frags_bar (view_t view)
{
	sbar_remcomponent (View_GetChild (view, 0), hud_fill);
	sbar_remcomponent (View_GetChild (view, 1), hud_fill);
	sbar_remcomponent (View_GetChild (view, 2), hud_charbuff);
	sbar_remcomponent (View_GetChild (view, 3), hud_func);
}

static void
clear_minifrags_bar (view_t view)
{
	clear_frags_bar (view);
	sbar_remcomponent (View_GetChild (view, 4), hud_charbuff);
	sbar_remcomponent (View_GetChild (view, 5), hud_charbuff);
	sbar_remcomponent (View_GetChild (view, 6), hud_charbuff);
}

static void
set_frags_bar (view_t view, byte top, byte bottom, draw_charbuffer_t *buff,
			   hud_func_f func)
{
	sbar_setcomponent (View_GetChild (view, 0), hud_fill, &top);
	sbar_setcomponent (View_GetChild (view, 1), hud_fill, &bottom);
	sbar_setcomponent (View_GetChild (view, 2), hud_charbuff, &buff);
	if (func) {
		sbar_setcomponent (View_GetChild (view, 3), hud_func, &func);
	} else {
		sbar_remcomponent (View_GetChild (view, 3), hud_func);
	}
}

static void
set_minifrags_bar (view_t view, byte top, byte bottom, draw_charbuffer_t *buff,
				   hud_func_f func, draw_charbuffer_t *team,
				   draw_charbuffer_t *name)
{
	set_frags_bar (view, top, bottom, buff, func);
	if (team) {
		sbar_setcomponent (View_GetChild (view, 4), hud_charbuff, &team);
		sbar_setcomponent (View_GetChild (view, 5), hud_charbuff, &name);
		sbar_remcomponent (View_GetChild (view, 6), hud_charbuff);
	} else {
		sbar_remcomponent (View_GetChild (view, 4), hud_charbuff);
		sbar_remcomponent (View_GetChild (view, 5), hud_charbuff);
		sbar_setcomponent (View_GetChild (view, 6), hud_charbuff, &name);
	}
}

static void
frags_marker (view_pos_t pos, view_pos_t len)
{
	r_funcs->Draw_Character (pos.x, pos.y, 16);
	r_funcs->Draw_Character (pos.x + len.x - 8, pos.y, 17);
}

static void
draw_frags (view_t view)
{
	if (sbar_maxplayers == 1) {
		return;
	}
	Sbar_SortFrags (0);

	int         numbars = 4;
	int         count = min (scoreboardlines, numbars);
	int         i;

	for (i = 0; i < count; i++) {
		int         k = fragsort[i];
		__auto_type s = &sbar_players[k];
		view_t      bar = View_GetChild (view, i);
		set_frags_bar (bar,
					   Sbar_ColorForMap (s->topcolor),
					   Sbar_ColorForMap (s->bottomcolor),
					   sb_frags[k],
					   (k == sbar_viewplayer) ? frags_marker : 0);
		draw_smallnum (View_GetChild (bar, 2), s->frags, 0, 0);
	}
	for (; i < numbars; i++) {
		clear_frags_bar (View_GetChild (view, i));
	}
}

static void
draw_minifrags (view_t view)
{
	if (sbar_maxplayers == 1) {
		return;
	}
	Sbar_SortFrags (0);

	// find us
	view_pos_t  len = View_GetLen (view);
	int         numbars = len.y / 8;
	int         start = 0;
	int         i;

	for (i = 0; i < scoreboardlines; i++) {
		if (fragsort[i] == sbar_playernum) {
			start = min (i - numbars / 2, scoreboardlines - numbars);
			start = max (0, start);
		}
	}

	int         count = min (scoreboardlines - start, numbars);

	for (i = 0; i < count; i++) {
		int         k = fragsort[i + start];
		__auto_type s = &sbar_players[k];
		view_t      bar = View_GetChild (view, i);
		set_minifrags_bar (bar,
						   Sbar_ColorForMap (s->topcolor),
						   Sbar_ColorForMap (s->bottomcolor),
						   sb_frags[k],
						   (k == sbar_viewplayer) ? frags_marker : 0,
						   sbar_teamplay ? sb_team[k] : 0,
						   sb_name[k]);
		if (sbar_teamplay) {
			write_charbuff (sb_team[k], 0, 0, s->team->value);
		}
		write_charbuff (sb_name[k], 0, 0, s->name->value);
		draw_smallnum (View_GetChild (bar, 2), s->frags, 0, 0);
	}
	for (; i < numbars; i++) {
		clear_minifrags_bar (View_GetChild (view, i));
	}
}

static void
clear_miniteam_bar (view_t view)
{
	sbar_remcomponent (View_GetChild (view, 0), hud_charbuff);
	sbar_remcomponent (View_GetChild (view, 1), hud_charbuff);
	sbar_remcomponent (View_GetChild (view, 2), hud_func);
}

static void
set_miniteam_bar (view_t view, draw_charbuffer_t *team,
				  draw_charbuffer_t *frags, hud_func_f func)
{
	sbar_setcomponent (View_GetChild (view, 0), hud_charbuff, &team);
	sbar_setcomponent (View_GetChild (view, 1), hud_charbuff, &frags);
	if (func) {
		sbar_setcomponent (View_GetChild (view, 2), hud_func, &func);
	} else {
		sbar_remcomponent (View_GetChild (view, 2), hud_func);
	}
}

static void
draw_miniteam (view_t view)
{
	if (!sbar_teamplay) {
		return;
	}
	Sbar_SortTeams ();

	view_pos_t  len = View_GetLen (view);
	int         numbars = len.y / 8;
	int         count = min (scoreboardteams, numbars);
	int         i;
	info_key_t *player_team = sbar_players[sbar_playernum].team;

	for (i = 0; i < count; i++) {
		int         k = teamsort[i];
		team_t     *tm = teams + k;
		__auto_type s = &sbar_players[k];
		view_t      bar = View_GetChild (view, i);
		hud_func_f  func = 0;
		if (player_team && strnequal (player_team->value, tm->team, 16)) {
			func = frags_marker;
		}
		set_miniteam_bar (bar, sb_team[k], sb_team_frags[k], func);
		write_charbuff (sb_team[k], 0, 0, s->team->value);
		write_charbuff (sb_team_frags[k], 0, 0, va (0, "%5d", tm->frags));
	}
	for (; i < numbars; i++) {
		clear_miniteam_bar (View_GetChild (view, i));
	}
}

static void
draw_face (view_t view)
{
	qpic_t     *face;

	if (sbar_stats[STAT_HEALTH] <= 0) {//FIXME hide_Face or hide_sbar
		sbar_remcomponent (sbar_face, hud_pic);
		return;
	}
	if ((sbar_stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY))
		== (IT_INVISIBILITY | IT_INVULNERABILITY)) {
		face = sb_face_invis_invuln;
	} else if (sbar_stats[STAT_ITEMS] & IT_QUAD) {
		face = sb_face_quad;
	} else if (sbar_stats[STAT_ITEMS] & IT_INVISIBILITY) {
		face = sb_face_invis;
	} else if (sbar_stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		face = sb_face_invuln;
	} else {
		int         f, anim;
		if (sbar_stats[STAT_HEALTH] >= 100) {
			f = 4;
		} else {
			f = sbar_stats[STAT_HEALTH] / 20;
		}

		if (sbar_time <= sbar_faceanimtime) {
			anim = 1;
			sb_updates = 0;				// make sure the anim gets drawn over
		} else {
			anim = 0;
		}
		face = sb_faces[f][anim];
	}
	sbar_setcomponent (view, hud_pic, &face);
}

static void __attribute__((used))
draw_spectator (view_t *view)
{
#if 0
	char        st[512];

	if (autocam != CAM_TRACK) {
		draw_string (view, 160 - 7 * 8, 4, "SPECTATOR MODE");
		draw_string (view, 160 - 14 * 8 + 4, 12,
					 "Press [ATTACK] for AutoCamera");
	} else {
//		Sbar_DrawString (160-14*8+4,4, "SPECTATOR MODE - TRACK CAMERA");
		if (sbar_players[spec_track].name) {
			snprintf (st, sizeof (st), "Tracking %-.13s, [JUMP] for next",
					  sbar_players[spec_track].name->value);
		} else {
			snprintf (st, sizeof (st), "Lost player, [JUMP] for next");
		}
		draw_string (view, 0, -8, st);
	}
#endif
}

static inline void
draw_armor (view_t view)
{
	view_t      armor = View_GetChild (view, 0);
	view_t      num[3] = {
		View_GetChild (view, 1),
		View_GetChild (view, 2),
		View_GetChild (view, 3),
	};
	if (sbar_stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		draw_num (num, 666, 3, 1);
	} else {
		draw_num (num, sbar_stats[STAT_ARMOR], 3, sbar_stats[STAT_ARMOR] <= 25);
		if (sbar_stats[STAT_ITEMS] & IT_ARMOR3)
			sbar_setcomponent (armor, hud_pic, &sb_armor[2]);
		else if (sbar_stats[STAT_ITEMS] & IT_ARMOR2)
			sbar_setcomponent (armor, hud_pic, &sb_armor[1]);
		else if (sbar_stats[STAT_ITEMS] & IT_ARMOR1)
			sbar_setcomponent (armor, hud_pic, &sb_armor[0]);
		else
			sbar_remcomponent (armor, hud_pic);
	}
}

static inline void
draw_health (view_t view)
{
	view_t      num[3] = {
		View_GetChild (view, 0),
		View_GetChild (view, 1),
		View_GetChild (view, 2),
	};
	draw_num (num, sbar_stats[STAT_HEALTH], 3, sbar_stats[STAT_HEALTH] <= 25);
}
#if 0
static void
draw_status (view_t *view)
{
	if (sbar_spectator) {
		draw_spectator (view);
		if (autocam != CAM_TRACK)
			return;
	}
	if (sb_showscores || sbar_stats[STAT_HEALTH] <= 0) {
		draw_solo (view);
		return;
	}

	draw_armor (sbar_status_armor);
	draw_face (sbar_status_face);
	draw_health (sbar_status_health);
	draw_ammo (sbar_status_ammo);
}
#endif

static void __attribute__((used))
draw_rogue_weapons_sbar (view_t *view)
{
#if 0
	int         i;

	draw_weapons_sbar (view);

	// check for powered up weapon.
	if (sbar_stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN) {
		for (i = 0; i < 5; i++) {
			if (sbar_stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i)) {
				draw_pic (view, (i + 2) * 24, 0, rsb_weapons[i]);
			}
		}
	}
#endif
}

static void __attribute__((used))
draw_rogue_weapons_hud (view_t *view)
{
#if 0
	int         flashon, i, j;
	qpic_t     *pic;

	for (i = cl_screen_view->ylen < 204; i < 7; i++) {
		if (sbar_stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = calc_flashon (sbar_item_gettime[i], IT_SHOTGUN << i);
			if (i >= 2) {
				j = i - 2;
				if (sbar_stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << j))
					pic = rsb_weapons[j];
				else
					pic = sb_weapons[flashon][i];
			} else {
				pic = sb_weapons[flashon][i];
			}
			draw_subpic (view, 0, i * 16, pic, 0, 0, 24, 16);
			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
		}
	}
#endif
}

static void __attribute__((used))
draw_rogue_ammo_hud (view_t *view)
{
#if 0
	int         i, count;
	qpic_t     *pic;

	if (sbar_stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
		pic = rsb_invbar[0];
	else
		pic = rsb_invbar[1];

	for (i = 0; i < 4; i++) {
		count = sbar_stats[STAT_SHELLS + i];
		draw_subpic (view, 0, i * 11, pic, 3 + (i * 48), 0, 42, 11);
		draw_smallnum (view, 7, i * 11, count, 0, 1);
	}
#endif
}

static void __attribute__((used))
draw_rogue_items (view_t *view)
{
#if 0
	int         i;
	float       time;

	draw_items (view);

	for (i = 0; i < 2; i++) {
		if (sbar_stats[STAT_ITEMS] & (1 << (29 + i))) {
			time = sbar_item_gettime[29 + i];
			draw_pic (view, 96 + i * 16, 0, rsb_items[i]);
			if (time && time > (sbar_time - 2))
				sb_updates = 0;
		}
	}
#endif
}

static void __attribute__((used))
draw_rogue_inventory_sbar (view_t *view)
{
#if 0
	if (sbar_stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
		draw_pic (view, 0, 0, rsb_invbar[0]);
	else
		draw_pic (view, 0, 0, rsb_invbar[1]);

	view_draw (view);
#endif
}

static void __attribute__((used))
draw_rogue_face (view_t *view)
{
#if 0
	int         top, bottom;
	player_info_t *s;

	// PGM 01/19/97 - team color drawing

	s = &sbar_players[sbar_viewplayer];

	top = Sbar_ColorForMap (s->topcolor);
	bottom = Sbar_ColorForMap (s->bottomcolor);

	draw_pic (view, 112, 0, rsb_teambord);
	draw_fill (view, 113, 3, 22, 9, top);
	draw_fill (view, 113, 12, 22, 9, bottom);

	draw_smallnum (view, 108, 3, s->frags, 1, top == 8);
#endif
}

static void __attribute__((used))
draw_rogue_status (view_t *view)
{
#if 0
	if (sb_showscores || sbar_stats[STAT_HEALTH] <= 0) {
		draw_solo (view);
		return;
	}

	draw_num (view, 24, 0, sbar_stats[STAT_ARMOR], 3,
			  sbar_stats[STAT_ARMOR] <= 25);
	if (sbar_stats[STAT_ITEMS] & RIT_ARMOR3)
		draw_pic (view, 0, 0, sb_armor[2]);
	else if (sbar_stats[STAT_ITEMS] & RIT_ARMOR2)
		draw_pic (view, 0, 0, sb_armor[1]);
	else if (sbar_stats[STAT_ITEMS] & RIT_ARMOR1)
		draw_pic (view, 0, 0, sb_armor[0]);

	// PGM 03/02/97 - fixed so color swatch appears in only CTF modes
	if (sbar_maxplayers != 1 && teamplay > 3 && teamplay < 7)
		draw_rogue_face (view);
	else
		draw_face (view);

	draw_health (view);

	if (sbar_stats[STAT_ITEMS] & RIT_SHELLS)
		draw_pic (view, 224, 0, sb_ammo[0]);
	else if (sbar_stats[STAT_ITEMS] & RIT_NAILS)
		draw_pic (view, 224, 0, sb_ammo[1]);
	else if (sbar_stats[STAT_ITEMS] & RIT_ROCKETS)
		draw_pic (view, 224, 0, sb_ammo[2]);
	else if (sbar_stats[STAT_ITEMS] & RIT_CELLS)
		draw_pic (view, 224, 0, sb_ammo[3]);
	else if (sbar_stats[STAT_ITEMS] & RIT_LAVA_NAILS)
		draw_pic (view, 224, 0, rsb_ammo[0]);
	else if (sbar_stats[STAT_ITEMS] & RIT_PLASMA_AMMO)
		draw_pic (view, 224, 0, rsb_ammo[1]);
	else if (sbar_stats[STAT_ITEMS] & RIT_MULTI_ROCKETS)
		draw_pic (view, 224, 0, rsb_ammo[2]);
	draw_num (view, 248, 0, sbar_stats[STAT_AMMO], 3, sbar_stats[STAT_AMMO] <= 10);
#endif
}

static void __attribute__((used))
draw_hipnotic_weapons_sbar (view_t *view)
{
#if 0
	static int  x[] = {0, 24, 48, 72, 96, 120, 144, 176, 200, 96};
	static int  h[] = {0, 1, 3};
	int         flashon, i;
	qpic_t     *pic;
	int         mask;
	float       time;

	for (i = 0; i < 10; i++) {
		if (i < 7) {
			mask = IT_SHOTGUN << i;
			time = sbar_item_gettime[i];
		} else {
			mask = 1 << hipweapons[h[i - 7]];
			time = sbar_item_gettime[hipweapons[h[i - 7]]];
		}
		if (sbar_stats[STAT_ITEMS] & mask) {
			flashon = calc_flashon (time, mask);

			if (i == 4 && sbar_stats[STAT_ACTIVEWEAPON] == (1 << hipweapons[3]))
				continue;
			if (i == 9 && sbar_stats[STAT_ACTIVEWEAPON] != (1 << hipweapons[3]))
				continue;
			if (i < 7) {
				pic = sb_weapons[flashon][i];
			} else {
				pic = hsb_weapons[flashon][h[i - 7]];
			}

			draw_subpic (view, x[i], 0, pic, 0, 0, 24, 16);

			if (flashon > 1)
				sb_updates = 0;		// force update to remove flash
		}
	}
#endif
}

static void __attribute__((used))
draw_hipnotic_weapons_hud (view_t *view)
{
#if 0
	int         flashon, i;
	static int  y[] = {0, 16, 32, 48, 64, 96, 112, 128, 144, 80};
	static int  h[] = {0, 1, 3};
	qpic_t     *pic;
	int         mask;
	float       time;

	for (i = 0; i < 10; i++) {
		if (i < 7) {
			mask = IT_SHOTGUN << i;
			time = sbar_item_gettime[i];
		} else {
			mask = 1 << hipweapons[h[i - 7]];
			time = sbar_item_gettime[hipweapons[h[i - 7]]];
		}
		if (sbar_stats[STAT_ITEMS] & mask) {
			flashon = calc_flashon (time, mask);

			if (i < 7) {
				pic = sb_weapons[flashon][i];
			} else {
				pic = hsb_weapons[flashon][h[i - 7]];
			}

			draw_subpic (view, 0, y[i], pic, 0, 0, 24, 16);

			if (flashon > 1)
				sb_updates = 0;		// force update to remove flash
		}
	}
#endif
}

static void __attribute__((used))
draw_hipnotic_items (view_t *view)
{
#if 0
	int         i;
	float       time;

	// items
	for (i = 2; i < 6; i++) {
		if (sbar_stats[STAT_ITEMS] & (1 << (17 + i))) {
			time = sbar_item_gettime[17 + i];
			draw_pic (view, 192 + i * 16, 0, sb_items[i]);
			if (time && time > sbar_time - 2)
				sb_updates = 0;
		}
	}

	// hipnotic items
	for (i = 0; i < 2; i++) {
		if (sbar_stats[STAT_ITEMS] & (1 << (24 + i))) {
			time = sbar_item_gettime[24 + i];
			draw_pic (view, 288 + i * 16, 0, hsb_items[i]);
			if (time && time > (sbar_time - 2))
				sb_updates = 0;
		}
	}
#endif
}

static void __attribute__((used))
draw_hipnotic_inventory_sbar (view_t *view)
{
#if 0
	draw_pic (view, 0, 0, sb_ibar);
	view_draw (view);
#endif
}

static void __attribute__((used))
draw_hipnotic_status (view_t *view)
{
#if 0
	if (sb_showscores || sbar_stats[STAT_HEALTH] <= 0) {
		draw_solo (view);
		return;
	}

	draw_armor (view);
	draw_face (view);
	draw_health (view);
	draw_ammo (view);

	if (sbar_stats[STAT_ITEMS] & IT_KEY1)
		draw_pic (view, 209, 3, sb_items[0]);
	if (sbar_stats[STAT_ITEMS] & IT_KEY2)
		draw_pic (view, 209, 12, sb_items[1]);
#endif
}

static void
setup_frags (view_t frags, int player)
{
	sbar_view (0, 0, 40, 4, grav_northwest, frags);
	sbar_view (0, 4, 40, 4, grav_northwest, frags);
	sbar_view (8, 0, 24, 8, grav_northwest, frags);
	sbar_view (0, 0, 40, 8, grav_northwest, frags);

	player_info_t *p = &sbar_players[player];
	set_frags_bar (frags,
				   Sbar_ColorForMap (p->topcolor),
				   Sbar_ColorForMap (p->bottomcolor),
				   sb_frags[player],
				   (player == sbar_viewplayer) ? frags_marker : 0);
}

static void
setup_spect (view_t spect, int player)
{
	view_t      v = sbar_view (0, 0, 88, 4, grav_north, spect);
	sbar_setcomponent (v, hud_charbuff, &sb_spectator);
}

typedef struct dmo_def_s {
	int         width;			// in pixels
	draw_charbuffer_t **buffer;
	void      (*setup) (view_t, int);
} dmo_def_t;

static dmo_def_t ping_def      = { .width =  24, .buffer = sb_ping, };
static dmo_def_t pl_def        = { .width =  24, .buffer = sb_pl, };
static dmo_def_t fph_def       = { .width =  24, .buffer = sb_fph, };
static dmo_def_t time_def      = { .width =  32, .buffer = sb_time, };
static dmo_def_t frags_def     = { .width =  40,     .setup = setup_frags, };
static dmo_def_t team_def      = { .width =  32, .buffer = sb_uid, };
static dmo_def_t uid_def       = { .width =  32, .buffer = sb_uid, };
static dmo_def_t name_def      = { .width = 128, .buffer = sb_name, };
static dmo_def_t spectator_def = { .width = 112,     .setup = setup_spect, };
static dmo_def_t spec_team_def = { .width =  32, };

static dmo_def_t *nq_dmo_defs[] = {
	&frags_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_team_uid_ping_defs[] = {
	&ping_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&team_def,
	&uid_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_team_uid_defs[] = {
	&uid_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&team_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_team_ping_defs[] = {
	&ping_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&team_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_uid_ping_defs[] = {
	&ping_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&uid_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_uid_defs[] = {
	&ping_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_ping_defs[] = {
	&uid_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_team_uid_ping_defs[] = {
	&ping_def,
	&pl_def,
	&spectator_def,
	&spec_team_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_team_uid_defs[] = {
	&ping_def,
	&pl_def,
	&spectator_def,
	&spec_team_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_team_ping_defs[] = {
	&ping_def,
	&pl_def,
	&spectator_def,
	&spec_team_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_uid_ping_defs[] = {
	&ping_def,
	&pl_def,
	&spectator_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_uid_defs[] = {
	&ping_def,
	&pl_def,
	&spectator_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_ping_defs[] = {
	&ping_def,
	&pl_def,
	&spectator_def,
	&name_def,
	0
};
static dmo_def_t **dmo_defs[] = {
	nq_dmo_defs,
	qw_dmo_ping_defs,
	qw_dmo_uid_defs,
	qw_dmo_uid_ping_defs,
	qw_dmo_team_ping_defs,
	qw_dmo_team_uid_defs,
	qw_dmo_team_uid_ping_defs,
	qw_dmo_spect_ping_defs,
	qw_dmo_spect_uid_defs,
	qw_dmo_spect_uid_ping_defs,
	qw_dmo_spect_team_ping_defs,
	qw_dmo_spect_team_uid_defs,
	qw_dmo_spect_team_uid_ping_defs,
};

static view_t
make_dmo_line (view_t parent, int player)
{
	dmo_def_t **defs = dmo_defs[3];	//FIXME nq/qw/team/spec/cvar
	int         x = -8;
	view_t      line = sbar_view (0, 0, 0, 0, grav_north, parent);

	while (*defs) {
		dmo_def_t  *d = *defs++;
		x += 8 + d->width;
		if (d->buffer || d->setup) {
			view_t      v = sbar_view (x - d->width, 0, d->width, 8,
									   grav_northwest, line);
			if (d->buffer) {
				draw_charbuffer_t *buff = d->buffer[player];
				sbar_setcomponent (v, hud_charbuff, &buff);
			} else if (d->setup) {
				d->setup (v, player);
			}
		}
	}
	View_SetLen (line, x, 8);
	return line;
}

static inline int
calc_fph (int frags, int total)
{
	int     fph;

	if (total != 0) {
		fph = (3600 * frags) / total;

		if (fph >= 999) {
			fph = 999;
		} else if (fph <= -999) {
			fph = -999;
		}
	} else {
		fph = 0;
	}

	return fph;
}

static void
Sbar_DeathmatchOverlay (view_t view)
{
#if 0
	// this should be on a timer and needs nq/qw specific variants
	// request new ping times every two seconds
	if (!cls.demoplayback && realtime - cl.last_ping_request > 2) {
		cl.last_ping_request = realtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}
	if (hud_ping) {
		int ping = sbar_players[sbar_playernum].ping;

		ping = bound (0, ping, 999);
		draw_string (view, 0, 0, va (0, "%3d ms ", ping));
	}

	if (hud_ping && hud_pl)
		draw_character (view, 48, 0, '/');

	if (hud_pl) {
		int lost = CL_CalcNet ();

		lost = bound (0, lost, 999);
		draw_string (view, 56, 0, va (0, "%3d pl", lost));
	}
#endif
	// 0, 0 "gfx/ranking.lmp"
	// 80, 40
	//
	// for all qw, spectator replaces main, team at 72,0 88,8

	Sbar_SortFrags (0);

	int         y = 40;
	view_pos_t  len = View_GetLen (view);
	int         numbars = (len.y - y) / 10;
	int         count = min (scoreboardlines, numbars);
	int         i;

	if (sbar_stats[STAT_HEALTH] > 0) {
		count = 0;
	}

	double      cur_time = sbar_intermission ? sbar_completed_time : sbar_time;
	for (i = 0; i < count; i++, y += 10) {
		int         k = fragsort[i];
		player_info_t *p = &sbar_players[k];
		if (!View_Valid (sb_views[k])) {
			sb_views[k] = make_dmo_line (view, k);
		}
		int         total = cur_time - p->entertime;
		write_charbuff (sb_fph[k], 0, 0, va (0, "%3d",
											 calc_fph (p->frags, total)));
		write_charbuff (sb_time[k], 0, 0, va (0, "%3d", total / 60));
		View_SetPos (sb_views[k], 0, y);
	}
	for (; i < MAX_PLAYERS; i++) {
		int         k = fragsort[i];
		if (k >= 0 && View_Valid (sb_views[k])) {
			View_Delete (sb_views[k]);
		}
	}
	View_UpdateHierarchy (view);
}

static void __attribute__((used))
Sbar_TeamOverlay (view_t *view)
{
#if 0
	char        num[20];
	int         pavg, plow, phigh, i, k, x, y;
	team_t     *tm;
	info_key_t *player_team = sbar_players[sbar_playernum].team;

	if (!sbar_teamplay) { // FIXME: if not teamplay, why teamoverlay?
		Sbar_DeathmatchOverlay (view, 0);
		return;
	}

	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	draw_cachepic (view, 0, 0, "gfx/ranking.lmp", 1);

	y = 24;
	x = 36;
	draw_string (view, x, y, "low/avg/high team total players");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	// sort the teams
	Sbar_SortTeams ();

	// draw the text
	for (i = 0; i < scoreboardteams && y <= view->ylen - 10; i++) {
		k = teamsort[i];
		tm = teams + k;

		// draw pings
		plow = tm->plow;
		if (plow < 0 || plow > 999)
			plow = 999;
		phigh = tm->phigh;
		if (phigh < 0 || phigh > 999)
			phigh = 999;
		if (!tm->players)
			pavg = 999;
		else
			pavg = tm->ptotal / tm->players;
		if (pavg < 0 || pavg > 999)
			pavg = 999;

		snprintf (num, sizeof (num), "%3i/%3i/%3i", plow, pavg, phigh);
		draw_string (view, x, y, num);

		// draw team
		draw_nstring (view, x + 104, y, tm->team, 4);

		// draw total
		snprintf (num, sizeof (num), "%5i", tm->frags);
		draw_string (view, x + 104 + 40, y, num);

		// draw players
		snprintf (num, sizeof (num), "%5i", tm->players);
		draw_string (view, x + 104 + 88, y, num);

		if (player_team && strnequal (player_team->value, tm->team, 16)) {
			draw_character (view, x + 104 - 8, y, 16);
			draw_character (view, x + 104 + 32, y, 17);
		}

		y += 8;
	}
	y += 8;
	Sbar_DeathmatchOverlay (view, y);
#endif
}

static void
Sbar_DrawScoreboard (void)
{
#if 0
	//Sbar_SoloScoreboard ();
	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay (hud_overlay_view);
#endif
}

static void
draw_overlay (view_t *view)
{
	if (sb_showscores || sbar_stats[STAT_HEALTH] <= 0) {
		Sbar_DrawScoreboard ();
	}
#if 0
	if (!sbar_active
		|| !((sbar_stats[STAT_HEALTH] <= 0 && !sbar_spectator)
			 || sb_showscores || sb_showteamscores))
		return;
	// main screen deathmatch rankings
	// if we're dead show team scores in team games
	if (sbar_stats[STAT_HEALTH] <= 0 && !sbar_spectator)
		if (sbar_teamplay > 0 && !sb_showscores)
			Sbar_TeamOverlay (view);
		else
			Sbar_DeathmatchOverlay (view, 0);
	else if (sb_showscores)
		Sbar_DeathmatchOverlay (view, 0);
	else if (sb_showteamscores)
		Sbar_TeamOverlay (view);
#endif
}

/*
	Sbar_LogFrags

	autologging of frags after a match ended
	(called by recived network packet with command scv_intermission)
	TODO: Find a new and better place for this function
		  (i am nearly shure this is wrong place)
	added by Elmex
*/
void
Sbar_LogFrags (double completed_time)
{
	char       *name;
	char       *team;
	byte       *cp = NULL;
	QFile      *file = NULL;
	int         minutes, fph, total, d, f, i, k, l, p;
	player_info_t *s = NULL;
	const char *t = NULL;
	time_t      tt = time (NULL);

	if (!cl_fraglog)
		return;

	if ((file = QFS_Open (fs_fraglog, "a")) == NULL)
		return;

	t = ctime (&tt);
	if (t)
		Qwrite (file, t, strlen (t));

	Qprintf (file, "%s\n%s %s\n", sbar_servername,
			 cl_world.scene->worldmodel->path, sbar_levelname);

	// scores
	Sbar_SortFrags (true);

	// draw the text
	l = scoreboardlines;

	if (sbar_teamplay) {
		// TODO: test if the teamplay does correct output
		Qwrite (file, "pl   fph time frags team name\n",
				strlen ("pl   fph time frags team name\n"));
	} else {
		Qwrite (file, "pl   fph time frags name\n",
				strlen ("pl   fph time frags name\n"));
	}

	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &sbar_players[k];
		if (!s->name || !s->name->value[0])
			continue;

		// draw pl
		p = s->pl;
		(void) p; //FIXME

		// get time
		if (sbar_intermission)
			total = completed_time - s->entertime;
		else
			total = sbar_time - s->entertime;
		minutes = total / 60;

		// get frags
		f = s->frags;

		fph = calc_fph (f, total);

		name = malloc (strlen (s->name->value) + 1);
		for (cp = (byte *) s->name->value, d = 0; *cp; cp++, d++)
			name[d] = sys_char_map[*cp];
		name[d] = 0;

		if (s->spectator) {
			Qprintf (file, "%-3i%% %s (spectator)", s->pl, name);
		} else {
			if (sbar_teamplay) {
				team = malloc (strlen (s->team->value) + 1);
				for (cp = (byte *) s->team, d = 0; *cp; cp++, d++)
					team[d] = sys_char_map[*cp];
				team[d] = 0;

				Qprintf (file, "%-3i%% %-3i %-4i %-3i    %-4s %s",
						 s->pl, fph, minutes, f, team, name);
				free (team);
			} else {
				Qprintf (file, "%-3i%% %-3i %-4i %-3i   %s",
						  s->pl, fph, minutes, f, name);
			}
		}
		free (name);
		Qwrite (file, "\n\n", 1);
	}

	Qclose (file);
}

static void
draw_time (view_t *view)
{
	struct tm  *local = 0;
	time_t      utc = 0;
	char        st[80];		//FIXME: overflow

	// Get local time
	utc = time (0);
	local = localtime (&utc);

#if defined(_WIN32) || defined(_WIN64)
#  define HOUR12 "%I"
#  define HOUR24 "%H"
#  define PM "%p"
#else
#  define HOUR12 "%l"
#  define HOUR24 "%k"
#  define PM "%P"
#endif
	if (hud_time == 1) {  // Use international format
		strftime (st, sizeof (st), HOUR24":%M", local);
	} else if (hud_time >= 2) {   // US AM/PM display
		strftime (st, sizeof (st), HOUR12":%M "PM, local);
	}
	write_charbuff (time_buff, 0, 0, st);
}

static void
draw_fps (view_t view)
{
	static char   st[80];
	double        t;
	static double lastframetime;
	static double lastfps;

	t = Sys_DoubleTime ();
	if ((t - lastframetime) >= 0.2) {
		lastfps = fps_count / (t - lastframetime);
		fps_count = 0;
		lastframetime = t;
		int         prec = lastfps < 1000 ? 1 : 0;
		snprintf (st, sizeof (st), "%5.*f FPS", prec, lastfps);
	}
	write_charbuff (fps_buff, 0, 0, st);
}

#if 0
static void
draw_net (view_t *view)
{
	// request new ping times every two seconds
	if (!cls.demoplayback && realtime - cl.last_ping_request > 2) {
		cl.last_ping_request = realtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}
	if (hud_ping) {
		int ping = sbar_players[sbar_playernum].ping;

		ping = bound (0, ping, 999);
		draw_string (view, 0, 0, va (0, "%3d ms ", ping));
	}

	if (hud_ping && hud_pl)
		draw_character (view, 48, 0, '/');

	if (hud_pl) {
		int lost = CL_CalcNet ();

		lost = bound (0, lost, 999);
		draw_string (view, 56, 0, va (0, "%3d pl", lost));
	}
}
if (sbar_active && (hud_ping || hud_pl))
#endif

static void
draw_intermission (view_t view)
{
	const char *n;

	n = "gfx/complete.lmp";
	sbar_setcomponent (View_GetChild (view, 0), hud_cachepic, &n);
	n = "gfx/inter.lmp";
	sbar_setcomponent (View_GetChild (view, 1), hud_cachepic, &n);

	view_t      time_views[] = {
		View_GetChild (intermission_time, 0),
		View_GetChild (intermission_time, 1),
		View_GetChild (intermission_time, 2),
		View_GetChild (intermission_time, 3),
		View_GetChild (intermission_time, 4),
		View_GetChild (intermission_time, 5),
	};
	int         dig = sbar_completed_time / 60;
	int         num = sbar_completed_time - dig * 60;
	draw_num (time_views + 0, dig, 3, 0);
	sbar_setcomponent (time_views[3], hud_pic, &sb_colon);
	draw_num (time_views + 4, num, 2, 0);

	view_t      secr_views[] = {
		View_GetChild (intermission_secr, 0),
		View_GetChild (intermission_secr, 1),
		View_GetChild (intermission_secr, 2),
		View_GetChild (intermission_secr, 3),
		View_GetChild (intermission_secr, 4),
		View_GetChild (intermission_secr, 5),
		View_GetChild (intermission_secr, 6),
	};
	draw_num (secr_views + 0, sbar_stats[STAT_SECRETS], 3, 0);
	sbar_setcomponent (secr_views[3], hud_pic, &sb_slash);
	draw_num (secr_views + 4, sbar_stats[STAT_TOTALSECRETS], 3, 0);

	view_t      kill_views[] = {
		View_GetChild (intermission_kill, 0),
		View_GetChild (intermission_kill, 1),
		View_GetChild (intermission_kill, 2),
		View_GetChild (intermission_kill, 3),
		View_GetChild (intermission_kill, 4),
		View_GetChild (intermission_kill, 5),
		View_GetChild (intermission_kill, 6),
	};
	draw_num (kill_views + 0, sbar_stats[STAT_MONSTERS], 3, 0);
	sbar_setcomponent (kill_views[3], hud_pic, &sb_slash);
	draw_num (kill_views + 4, sbar_stats[STAT_TOTALMONSTERS], 3, 0);
}

static void
clear_views (view_t view)
{
	sbar_remcomponent (view, hud_cachepic);
	sbar_remcomponent (view, hud_pic);

	for (uint32_t i = 0; i < View_ChildCount (view); i++) {
		clear_views (View_GetChild (view, i));
	}
}

#if 0
void
Sbar_IntermissionOverlay (void)
{
	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;
	if (cl.gametype == GAME_DEATHMATCH) {
		Sbar_DeathmatchOverlay (hud_overlay_view);
		return;
	}
	draw_intermission (intermission_view);
}
#endif

void
Sbar_Intermission (int mode, double completed_time)
{
	sbar_completed_time = completed_time;
	void       *f = clear_views;
	if (mode == 1) {
		f = draw_intermission;
	}
	sbar_setcomponent (intermission_view, hud_updateonce, &f);
}

/* CENTER PRINTING */
static dstring_t center_string = {&dstring_default_mem};
static passage_t center_passage;
static float centertime_start;				// for slow victory printing
static float centertime_off;
static int   center_lines;

/*
	Called for important messages that should stay in the center of the screen
	for a few moments
*/
void
Sbar_CenterPrint (const char *str)
{
	if (!str || !*str) {
		centertime_off = 0;
		return;
	}

	centertime_off = scr_centertime;
	centertime_start = sbar_time;

	if (center_string.str && !strcmp (str, center_string.str)) {
		// same string as last time, no need to lay out the text again
		return;
	}

	dstring_copystr (&center_string, str);
	Passage_ParseText (&center_passage, center_string.str);
	// Standard centerprint strings are pre-flowed so each line in the message
	// is a paragraph in the passage.
	center_lines = center_passage.hierarchy->childCount[0];
}

static void
Sbar_DrawCenterString (view_t view, unsigned remaining)
{
	view_pos_t  abs = View_GetAbs (view);
	view_pos_t  len = View_GetLen (view);

	int         x, y;

	if (center_lines <= 4)
		y = abs.y + len.y * 0.35;
	else
		y = abs.y + 48;

	__auto_type h = center_passage.hierarchy;
	psg_text_t *line = h->components[passage_type_text_obj];
	int         line_count = center_lines;
	while (line_count-- > 0 && remaining > 0) {
		line++;
		const char *text = center_passage.text + line->text;
		unsigned    count = min (40, line->size);
		x = abs.x + (len.x - count * 8) / 2;
		count = min (count, remaining);
		remaining -= count;
		r_funcs->Draw_nString (x, y, text, count);
		y += 8;
	}
}

void
Sbar_FinaleOverlay (void)
{
	int         remaining;

	r_data->scr_copyeverything = 1;

#if 0
	draw_cachepic (hud_overlay_view, 0, 16, "gfx/finale.lmp", 1);
#endif
	// the finale prints the characters one at a time
	remaining = scr_printspeed * (sbar_time - centertime_start);
	Sbar_DrawCenterString (hud_overlay_view, remaining);
}

void
Sbar_DrawCenterPrint (void)
{
	r_data->scr_copytop = 1;

	centertime_off -= r_data->frametime;
	if (centertime_off <= 0)
		return;

	Sbar_DrawCenterString (hud_overlay_view, -1);
}

typedef struct {
	hud_update_f update;
	view_t     *view;
} sb_updater_t;

static const sb_updater_t ammo_update[] = {
	{draw_miniammo, &sbar_miniammo},
	{draw_ammo, &sbar_ammo},
	{}
};
static const sb_updater_t armor_update[] = {
	{draw_armor, &sbar_armor},
	{}
};
static const sb_updater_t frags_update[] = {
	{draw_frags, &sbar_frags},
	{draw_minifrags, &hud_minifrags},
	{draw_miniteam, &hud_miniteam},
	{Sbar_DeathmatchOverlay, &dmo_view},
	{}
};
static const sb_updater_t health_update[] = {
	{draw_health, &sbar_health},
	{draw_face, &sbar_face},
	{}
};
static const sb_updater_t info_update[] = {
	{draw_frags, &sbar_frags},
	{draw_minifrags, &hud_minifrags},
	{draw_miniteam, &hud_miniteam},
	{Sbar_DeathmatchOverlay, &dmo_view},
	{}
};
static const sb_updater_t items_update[] = {
	{draw_items, &sbar_items},
	{draw_sigils, &sbar_sigils},
	{}
};
static const sb_updater_t weapon_update[] = {
	{draw_weapons, &sbar_weapons},
	{draw_ammo, &sbar_ammo},
	{}
};

static const sb_updater_t * const sb_updaters[sbc_num_changed] = {
	[sbc_ammo]   = ammo_update,
	[sbc_armor]  = armor_update,
	[sbc_frags]  = frags_update,
	[sbc_health] = health_update,
	[sbc_info]   = info_update,
	[sbc_items]  = items_update,
	[sbc_weapon] = weapon_update,
};

void
Sbar_Changed (sbar_changed change)
{
	sb_update_flags |= 1 << change;
	sb_updates = 0;						// update next frame
	if ((unsigned) change >= (unsigned) sbc_num_changed) {
		Sys_Error ("invalid sbar changed enum");
	}
	const sb_updater_t *ud = sb_updaters[change];
	if (ud) {
		while (ud->update) {
			sbar_setcomponent (*ud->view, hud_updateonce, &ud->update);
			ud++;
		}
	}
}

void
Sbar_Update (double time)
{
	fps_count++;
	sbar_time = time;
	if (!sbar_active) {
		return;
	}
	if (sb_update_flags & (1 << sbc_info)) {
		draw_overlay (0);
	}
	if (sb_showscores) {
		draw_solo_time ();
	}
	sb_update_flags = 0;
}

void
Sbar_UpdatePings ()
{
	for (int i = 0; i < sbar_maxplayers; i++) {
		player_info_t *p = &sbar_players[i];
		if (!p->name || !p->name->value) {
			continue;
		}
		write_charbuff (sb_ping[i], 0, 0, va (0, "%3d", p->ping));
		write_charbuff (sb_pl[i], 0, 0, va (0, "%3d", p->pl));
	}
}

void
Sbar_UpdateFrags (int playernum)
{
	player_info_t *p = &sbar_players[playernum];
	write_charbuff (sb_frags[playernum], 0, 0, va (0, "%3d", p->frags));
}

void
Sbar_UpdateInfo (int playernum)
{
	player_info_t *p = &sbar_players[playernum];
	//FIXME update top/bottom color
	write_charbuff (sb_uid[playernum], 0, 0, va (0, "%3d", p->userid));
	write_charbuff (sb_name[playernum], 0, 0, p->name->value);
	if (sbar_teamplay) {
		write_charbuff (sb_team[playernum], 0, 0, p->team->value);
	}
}

void
Sbar_UpdateStats (int stat)
{
	if (stat == -1) {
	} else {
	}
}

void
Sbar_Damage (double time)
{
	sbar_faceanimtime = time + 0.2;
}

void
Sbar_SetPlayerNum (int playernum, int spectator)
{
	sbar_playernum = playernum;
	sbar_spectator = spectator;
}

void
Sbar_SetViewEntity (int viewentity)
{
	sbar_viewplayer = viewentity - 1;
}

void
Sbar_SetLevelName (const char *levelname, const char *servername)
{
	sbar_levelname = levelname;
	sbar_servername = servername;
}

void
Sbar_SetTeamplay (int teamplay)
{
	sbar_teamplay = teamplay;
}

void
Sbar_SetActive (int active)
{
	sbar_active = active;
}

static void
sbar_hud_swap_f (void *data, const cvar_t *cvar)
{
	if (hud_sbar) {
		return;
	}
	grav_t      armament_grav = hud_swap ? grav_southwest : grav_southeast;
	grav_t      weapons_grav = hud_swap ? grav_northwest : grav_northeast;
	View_SetGravity (sbar_armament, armament_grav);
	View_SetGravity (sbar_weapons, weapons_grav);
	View_UpdateHierarchy (hud_view);
}

static int
href_cmp (const void *_a, const void *_b, void *arg)
{
	uint32_t    enta = *(const uint32_t *)_a;
	uint32_t    entb = *(const uint32_t *)_b;
	hierref_t  *ref_a = Ent_GetComponent (enta, hud_href, hud_registry);
	hierref_t  *ref_b = Ent_GetComponent (entb, hud_href, hud_registry);
	if (ref_a->hierarchy == ref_b->hierarchy) {
		return ref_a->index - ref_b->index;
	}
	ptrdiff_t  diff = ref_a->hierarchy - ref_b->hierarchy;
	return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}

static void
set_hud_sbar (void)
{
	view_t      v;

	if (hud_sbar) {
		View_SetParent (sbar_armament, sbar_inventory);
		View_SetPos (sbar_armament, 0, 0);
		View_SetLen (sbar_armament, 202, 24);
		View_SetGravity (sbar_armament, grav_northwest);

		View_SetLen (sbar_weapons, 192, 16);
		View_SetGravity (sbar_weapons, grav_southwest);

		View_SetLen (sbar_miniammo, 202, 8);
		View_SetGravity (sbar_miniammo, grav_northwest);

		for (int i = 0; i < 7; i++) {
			v = View_GetChild (sbar_weapons, i);
			View_SetPos (v, i * 24, 0);
			View_SetLen (v, 24 + 24 * (i == 6), 16);

			if (sbar_hascomponent (v, hud_subpic)) {
				hud_subpic_t *subpic = sbar_getcomponent(v, hud_subpic);
				subpic->w = subpic->pic->width;
			}
		}
		for (int i = 0; i < 4; i++) {
			v = View_GetChild (sbar_miniammo, i);
			View_SetPos (v, i * 48 + 10, 0);
			View_SetLen (v, 42, 8);
			sbar_remcomponent (v, hud_subpic);
		}
		for (int i = 0; i < 7; i++) {
			for (int j = 0; j < 8; j++) {
				if (sb_weapons[i][j].pic) {
					sb_weapons[i][j].w = sb_weapons[i][j].pic->width;
					sb_weapons[i][j].h = sb_weapons[i][j].pic->height;
				}
			}
		}

		sbar_setcomponent (sbar_inventory, hud_pic, &sb_ibar);
		sbar_setcomponent (sbar_statusbar, hud_pic, &sb_sbar);
	} else {
		View_SetParent (sbar_armament, hud_view);
		View_SetPos (sbar_armament, 0, 48);
		View_SetLen (sbar_armament, 42, 156);
		View_SetGravity (sbar_armament, grav_southeast);

		View_SetLen (sbar_weapons, 24, 112);
		View_SetGravity (sbar_weapons, grav_northeast);

		View_SetLen (sbar_miniammo, 42, 44);
		View_SetGravity (sbar_miniammo, grav_southeast);

		for (int i = 0; i < 7; i++) {
			v = View_GetChild (sbar_weapons, i);
			View_SetPos (v, 0, i * 16);
			View_SetLen (v, 24, 16);

			if (sbar_hascomponent (v, hud_subpic)) {
				hud_subpic_t *subpic = sbar_getcomponent(v, hud_subpic);
				subpic->w = 24;
			}
		}
		for (int i = 0; i < 4; i++) {
			v = View_GetChild (sbar_miniammo, i);
			View_SetPos (v, 0, i * 11);
			View_SetLen (v, 42, 11);
			hud_subpic_t subpic = { sb_ibar, 3 + (i * 48), 0, 42, 11 };
			sbar_setcomponent (v, hud_subpic, &subpic);
		}
		for (int i = 0; i < 7; i++) {
			for (int j = 0; j < 8; j++) {
				if (sb_weapons[i][j].pic) {
					sb_weapons[i][j].w = 24;
					sb_weapons[i][j].h = sb_weapons[i][j].pic->height;
				}
			}
		}

		sbar_remcomponent (sbar_inventory, hud_pic);
		sbar_remcomponent (sbar_statusbar, hud_pic);
	}
	ECS_SortComponentPool (hud_registry, hud_pic, href_cmp, 0);
}

static void
sbar_hud_sbar_f (void *data, const cvar_t *cvar)
{
	set_hud_sbar ();
	View_UpdateHierarchy (hud_view);
}

static void
sbar_hud_fps_f (void *data, const cvar_t *cvar)
{
	if (hud_fps) {
		void       *f = draw_fps;
		sbar_setcomponent (hud_fps_view, hud_update, &f);
		sbar_setcomponent (hud_fps_view, hud_charbuff, &fps_buff);
	} else {
		sbar_remcomponent (hud_fps_view, hud_update);
		sbar_remcomponent (hud_fps_view, hud_charbuff);
	}
}

static void
sbar_hud_time_f (void *data, const cvar_t *cvar)
{
	if (hud_time) {
		void       *f = draw_time;
		sbar_setcomponent (hud_time_view, hud_update, &f);
		sbar_setcomponent (hud_time_view, hud_charbuff, &time_buff);
	} else {
		sbar_remcomponent (hud_time_view, hud_update);
		sbar_remcomponent (hud_time_view, hud_charbuff);
	}
}

static void
create_views (view_def_t *view_defs, view_t parent)
{
	for (int i = 0; view_defs[i].view || view_defs[i].parent; i++) {
		view_def_t *def = &view_defs[i];
		view_t      p = parent;
		int         x = def->rect.x;
		int         y = def->rect.y;
		int         w = def->rect.w;
		int         h = def->rect.h;
		if (def->parent != &pseudo_parent) {
			p = def->parent ? *def->parent : nullview;
		}
		if (def->view) {
			*def->view = sbar_view (x, y, w, h, def->gravity, p);
			if (def->subviews) {
				create_views (def->subviews, *def->view);
			}
		} else {
			for (int j = 0; j < def->count; j++) {
				view_t      v = sbar_view (x, y, w, h, def->gravity, p);
				if (def->subviews) {
					create_views (def->subviews, v);
				}
				x += def->xstep;
				y += def->ystep;
			}
		}
	}
}

static void
init_sbar_views (void)
{
	create_views (sbar_defs, nullview);
	view_pos_t  slen = View_GetLen (cl_screen_view);
	view_pos_t  hlen = View_GetLen (hud_view);
	View_SetLen (hud_view, slen.x, hlen.y);
	View_SetResize (hud_view, 1, 0);

	for (int i = 0; i < 4; i++) {
		view_t      v = View_GetChild (sbar_miniammo, i);
		draw_charbuffer_t *buffer = Draw_CreateBuffer (3, 1);
		Draw_ClearBuffer (buffer);
		sbar_setcomponent (v, hud_charbuff, &buffer);
	}

	if (r_data->vid->width > 320) {
		int         l = (r_data->vid->width - 320) / 2;
		View_SetPos (sbar_tile[0], -l, 0);
		View_SetLen (sbar_tile[0], l, 48);
		View_SetPos (sbar_tile[1], -l, 0);
		View_SetLen (sbar_tile[1], l, 48);
		sbar_setcomponent (sbar_tile[0], hud_tile, 0);
		sbar_setcomponent (sbar_tile[1], hud_tile, 0);
	}

	solo_monsters = Draw_CreateBuffer (17, 1);
	solo_secrets = Draw_CreateBuffer (17, 1);
	solo_time = Draw_CreateBuffer (12, 1);
	solo_name = Draw_CreateBuffer (20, 1);
}

static void
init_hipnotic_sbar_views (void)
{
#if 0
	view_t     *view;

	sbar_view = view_new (0, 0, 320, 48, grav_south);

	sbar_frags_view = view_new (0, 0, 130, 8, grav_northeast);
	sbar_frags_view->draw = draw_frags;

	sbar_inventory_view = view_new (0, 0, 320, 24, grav_northwest);
	sbar_inventory_view->draw = draw_hipnotic_inventory_sbar;

	view = view_new (0, 0, 224, 16, grav_southwest);
	view->draw = draw_hipnotic_weapons_sbar;
	view_add (sbar_inventory_view, view);

	view = view_new (0, 0, 32, 8, grav_northwest);
	view->draw = draw_ammo_sbar;
	view_add (sbar_inventory_view, view);

	view = view_new (0, 0, 96, 16, grav_southeast);
	view->draw = draw_hipnotic_items;
	view_add (sbar_inventory_view, view);

	view = view_new (0, 0, 32, 16, grav_southeast);
	view->draw = draw_sigils;
	view_add (sbar_inventory_view, view);

	if (sbar_frags_view)
		view_add (sbar_inventory_view, sbar_frags_view);

	view_add (sbar_view, sbar_inventory_view);

	view = view_new (0, 0, 320, 24, grav_southwest);
	view->draw = draw_status_bar;
	view_add (sbar_view, view);

	view = view_new (0, 0, 320, 24, grav_southwest);
	view->draw = draw_hipnotic_status;
	view_add (sbar_view, view);

	if (cl_screen_view->xlen > 320) {
		int         l = (cl_screen_view->xlen - 320) / 2;

		view = view_new (-l, 0, l, 48, grav_southwest);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);

		view = view_new (-l, 0, l, 48, grav_southeast);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);
	}
#endif
}

static void
init_hipnotic_hud_views (void)
{
#if 0
	view_t     *view;

	hud_view = view_new (0, 0, 320, 48, grav_south);
	hud_frags_view = view_new (0, 0, 130, 8, grav_northeast);
	hud_frags_view->draw = draw_frags;

	hud_view->resize_y = 1;

	if (cl_screen_view->ylen < 252) {
		hud_armament_view = view_new (0, min (cl_screen_view->ylen - 160, 48),
									  66, 160, grav_southeast);
	} else {
		hud_armament_view = view_new (0, 48, 42, 204, grav_southeast);
	}

	view = view_new (0, 0, 24, 160, grav_northeast);
	view->draw = draw_hipnotic_weapons_hud;
	view_add (hud_armament_view, view);

	view = view_new (0, 0, 42, 44, grav_southeast);
	view->draw = draw_ammo_hud;
	view_add (hud_armament_view, view);

	hud_inventory_view = view_new (0, 0, 320, 24, grav_northwest);
	view_add (hud_view, hud_inventory_view);

	view = view_new (0, 0, 320, 24, grav_southwest);
	view->draw = draw_hipnotic_status;
	view_add (hud_view, view);

	view = view_new (0, 0, 96, 16, grav_southeast);
	view->draw = draw_hipnotic_items;
	view_add (hud_inventory_view, view);

	view = view_new (0, 0, 32, 16, grav_southeast);
	view->draw = draw_sigils;
	view_add (hud_inventory_view, view);

	if (hud_frags_view)
		view_add (hud_inventory_view, hud_frags_view);

	view = view_new (0, 0, cl_screen_view->xlen, 48, grav_south);
	view->resize_x = 1;
	view_add (view, hud_view);
	hud_view = view;

	view_add (hud_view, hud_armament_view);

	view_insert (hud_main_view, hud_view, 0);
#endif
}

static void
init_rogue_sbar_views (void)
{
#if 0
	view_t     *view;

	sbar_view = view_new (0, 0, 320, 48, grav_south);

	sbar_frags_view = view_new (0, 0, 130, 8, grav_northeast);
	sbar_frags_view->draw = draw_frags;

	sbar_inventory_view = view_new (0, 0, 320, 24, grav_northwest);
	sbar_inventory_view->draw = draw_rogue_inventory_sbar;

	view = view_new (0, 0, 224, 16, grav_southwest);
	view->draw = draw_rogue_weapons_sbar;
	view_add (sbar_inventory_view, view);

	view = view_new (0, 0, 32, 8, grav_northwest);
	view->draw = draw_ammo_sbar;
	view_add (sbar_inventory_view, view);

	view = view_new (0, 0, 128, 16, grav_southeast);
	view->draw = draw_rogue_items;
	view_add (sbar_inventory_view, view);

	if (sbar_frags_view)
		view_add (sbar_inventory_view, sbar_frags_view);

	view_add (sbar_view, sbar_inventory_view);

	view = view_new (0, 0, 320, 24, grav_southwest);
	view->draw = draw_status_bar;
	view_add (sbar_view, view);

	view = view_new (0, 0, 320, 24, grav_southwest);
	view->draw = draw_rogue_status;
	view_add (sbar_view, view);

	if (cl_screen_view->xlen > 320) {
		int         l = (cl_screen_view->xlen - 320) / 2;

		view = view_new (-l, 0, l, 48, grav_southwest);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);

		view = view_new (-l, 0, l, 48, grav_southeast);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);
	}
#endif
}

static void
init_rogue_hud_views (void)
{
#if 0
	view_t     *view;

	hud_view = view_new (0, 0, 320, 48, grav_south);
	hud_frags_view = view_new (0, 0, 130, 8, grav_northeast);
	hud_frags_view->draw = draw_frags;

	hud_view->resize_y = 1;

	hud_armament_view = view_new (0, 48, 42, 156, grav_southeast);

	view = view_new (0, 0, 24, 112, grav_northeast);
	view->draw = draw_rogue_weapons_hud;
	view_add (hud_armament_view, view);

	view = view_new (0, 0, 42, 44, grav_southeast);
	view->draw = draw_rogue_ammo_hud;
	view_add (hud_armament_view, view);

	hud_inventory_view = view_new (0, 0, 320, 24, grav_northwest);
	view_add (hud_view, hud_inventory_view);

	view = view_new (0, 0, 320, 24, grav_southwest);
	view->draw = draw_rogue_status;
	view_add (hud_view, view);

	view = view_new (0, 0, 128, 16, grav_southeast);
	view->draw = draw_rogue_items;
	view_add (hud_inventory_view, view);

	if (hud_frags_view)
		view_add (hud_inventory_view, hud_frags_view);

	view = view_new (0, 0, cl_screen_view->xlen, 48, grav_south);
	view->resize_x = 1;
	view_add (view, hud_view);
	hud_view = view;

	view_add (hud_view, hud_armament_view);

	view_insert (hud_main_view, hud_view, 0);
#endif
}

static void
init_views (void)
{
	hud_stuff_view = sbar_view (0, 48, 152, 16, grav_southwest, cl_screen_view);
	hud_time_view = sbar_view (8, 0, 64, 8, grav_northwest, hud_stuff_view);
	hud_fps_view = sbar_view (80, 0, 72, 8, grav_northwest, hud_stuff_view);

	time_buff = Draw_CreateBuffer (8, 1);
	fps_buff = Draw_CreateBuffer (11, 1);
	for (int i = 0; i < MAX_PLAYERS; i++) {
		sb_fph[i] = Draw_CreateBuffer (3, 1);
		sb_time[i] = Draw_CreateBuffer (4, 1);
		sb_frags[i] = Draw_CreateBuffer (3, 1);
		sb_team[i] = Draw_CreateBuffer (4, 1);
		sb_ping[i] = Draw_CreateBuffer (3, 1);
		sb_pl[i] = Draw_CreateBuffer (3, 1);
		sb_uid[i] = Draw_CreateBuffer (4, 1);
		sb_name[i] = Draw_CreateBuffer (16, 1);
		sb_team_frags[i] = Draw_CreateBuffer (5, 1);
		fragsort[i] = -1;
	}
	sb_spectator = Draw_CreateBuffer (11, 1);
	write_charbuff (sb_spectator, 0, 0, "(spectator)");

	if (!strcmp (qfs_gamedir->hudtype, "hipnotic")) {
		init_hipnotic_sbar_views ();
		init_hipnotic_hud_views ();
	} else if (!strcmp (qfs_gamedir->hudtype, "rogue")) {
		init_rogue_sbar_views ();
		init_rogue_hud_views ();
	} else {
		init_sbar_views ();
	}
}

static void
Sbar_GIB_Print_Center_f (void)
{
	if (GIB_Argc () != 2) {
		GIB_USAGE ("text");
	} else
		Sbar_CenterPrint (GIB_Argv(1));
}

static void
load_pics (void)
{
	for (int i = 0; i < 10; i++) {
		sb_nums[0][i] = r_funcs->Draw_PicFromWad (va (0, "num_%i", i));
		sb_nums[1][i] = r_funcs->Draw_PicFromWad (va (0, "anum_%i", i));
	}

	sb_nums[0][10] = r_funcs->Draw_PicFromWad ("num_minus");
	sb_nums[1][10] = r_funcs->Draw_PicFromWad ("anum_minus");

	sb_colon = r_funcs->Draw_PicFromWad ("num_colon");
	sb_slash = r_funcs->Draw_PicFromWad ("num_slash");

	sb_weapons[0][0].pic = r_funcs->Draw_PicFromWad ("inv_shotgun");
	sb_weapons[0][1].pic = r_funcs->Draw_PicFromWad ("inv_sshotgun");
	sb_weapons[0][2].pic = r_funcs->Draw_PicFromWad ("inv_nailgun");
	sb_weapons[0][3].pic = r_funcs->Draw_PicFromWad ("inv_snailgun");
	sb_weapons[0][4].pic = r_funcs->Draw_PicFromWad ("inv_rlaunch");
	sb_weapons[0][5].pic = r_funcs->Draw_PicFromWad ("inv_srlaunch");
	sb_weapons[0][6].pic = r_funcs->Draw_PicFromWad ("inv_lightng");

	sb_weapons[1][0].pic = r_funcs->Draw_PicFromWad ("inv2_shotgun");
	sb_weapons[1][1].pic = r_funcs->Draw_PicFromWad ("inv2_sshotgun");
	sb_weapons[1][2].pic = r_funcs->Draw_PicFromWad ("inv2_nailgun");
	sb_weapons[1][3].pic = r_funcs->Draw_PicFromWad ("inv2_snailgun");
	sb_weapons[1][4].pic = r_funcs->Draw_PicFromWad ("inv2_rlaunch");
	sb_weapons[1][5].pic = r_funcs->Draw_PicFromWad ("inv2_srlaunch");
	sb_weapons[1][6].pic = r_funcs->Draw_PicFromWad ("inv2_lightng");

	for (int i = 0; i < 5; i++) {
		sb_weapons[2 + i][0].pic =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1].pic =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2].pic =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3].pic =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4].pic =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5].pic =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6].pic =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_lightng", i + 1));
	}
	for (int i = 0; i < 7; i++) {
		for (int j = 0; j < 8; j++) {
			if (sb_weapons[i][j].pic) {
				sb_weapons[i][j].w = sb_weapons[i][j].pic->width;
				sb_weapons[i][j].h = sb_weapons[i][j].pic->height;
			}
		}
	}

	sb_ammo[0] = r_funcs->Draw_PicFromWad ("sb_shells");
	sb_ammo[1] = r_funcs->Draw_PicFromWad ("sb_nails");
	sb_ammo[2] = r_funcs->Draw_PicFromWad ("sb_rocket");
	sb_ammo[3] = r_funcs->Draw_PicFromWad ("sb_cells");

	sb_armor[0] = r_funcs->Draw_PicFromWad ("sb_armor1");
	sb_armor[1] = r_funcs->Draw_PicFromWad ("sb_armor2");
	sb_armor[2] = r_funcs->Draw_PicFromWad ("sb_armor3");

	sb_items[0] = r_funcs->Draw_PicFromWad ("sb_key1");
	sb_items[1] = r_funcs->Draw_PicFromWad ("sb_key2");
	sb_items[2] = r_funcs->Draw_PicFromWad ("sb_invis");
	sb_items[3] = r_funcs->Draw_PicFromWad ("sb_invuln");
	sb_items[4] = r_funcs->Draw_PicFromWad ("sb_suit");
	sb_items[5] = r_funcs->Draw_PicFromWad ("sb_quad");

	sb_sigil[0] = r_funcs->Draw_PicFromWad ("sb_sigil1");
	sb_sigil[1] = r_funcs->Draw_PicFromWad ("sb_sigil2");
	sb_sigil[2] = r_funcs->Draw_PicFromWad ("sb_sigil3");
	sb_sigil[3] = r_funcs->Draw_PicFromWad ("sb_sigil4");

	sb_faces[4][0] = r_funcs->Draw_PicFromWad ("face1");
	sb_faces[4][1] = r_funcs->Draw_PicFromWad ("face_p1");
	sb_faces[3][0] = r_funcs->Draw_PicFromWad ("face2");
	sb_faces[3][1] = r_funcs->Draw_PicFromWad ("face_p2");
	sb_faces[2][0] = r_funcs->Draw_PicFromWad ("face3");
	sb_faces[2][1] = r_funcs->Draw_PicFromWad ("face_p3");
	sb_faces[1][0] = r_funcs->Draw_PicFromWad ("face4");
	sb_faces[1][1] = r_funcs->Draw_PicFromWad ("face_p4");
	sb_faces[0][0] = r_funcs->Draw_PicFromWad ("face5");
	sb_faces[0][1] = r_funcs->Draw_PicFromWad ("face_p5");

	sb_face_invis = r_funcs->Draw_PicFromWad ("face_invis");
	sb_face_invuln = r_funcs->Draw_PicFromWad ("face_invul2");
	sb_face_invis_invuln = r_funcs->Draw_PicFromWad ("face_inv2");
	sb_face_quad = r_funcs->Draw_PicFromWad ("face_quad");

	sb_sbar = r_funcs->Draw_PicFromWad ("sbar");
	sb_ibar = r_funcs->Draw_PicFromWad ("ibar");
	sb_scorebar = r_funcs->Draw_PicFromWad ("scorebar");

	// MED 01/04/97 added new hipnotic weapons
	if (!strcmp (qfs_gamedir->hudtype, "hipnotic")) {
		hsb_weapons[0][0] = r_funcs->Draw_PicFromWad ("inv_laser");
		hsb_weapons[0][1] = r_funcs->Draw_PicFromWad ("inv_mjolnir");
		hsb_weapons[0][2] = r_funcs->Draw_PicFromWad ("inv_gren_prox");
		hsb_weapons[0][3] = r_funcs->Draw_PicFromWad ("inv_prox_gren");
		hsb_weapons[0][4] = r_funcs->Draw_PicFromWad ("inv_prox");

		hsb_weapons[1][0] = r_funcs->Draw_PicFromWad ("inv2_laser");
		hsb_weapons[1][1] = r_funcs->Draw_PicFromWad ("inv2_mjolnir");
		hsb_weapons[1][2] = r_funcs->Draw_PicFromWad ("inv2_gren_prox");
		hsb_weapons[1][3] = r_funcs->Draw_PicFromWad ("inv2_prox_gren");
		hsb_weapons[1][4] = r_funcs->Draw_PicFromWad ("inv2_prox");

		for (int i = 0; i < 5; i++) {
			hsb_weapons[2 + i][0] =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_laser", i + 1));
			hsb_weapons[2 + i][1] =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_mjolnir", i + 1));
			hsb_weapons[2 + i][2] =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_gren_prox", i + 1));
			hsb_weapons[2 + i][3] =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_prox_gren", i + 1));
			hsb_weapons[2 + i][4] =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_prox", i + 1));
		}

		hsb_items[0] = r_funcs->Draw_PicFromWad ("sb_wsuit");
		hsb_items[1] = r_funcs->Draw_PicFromWad ("sb_eshld");
	}

	// FIXME: MISSIONHUD
	if (!strcmp (qfs_gamedir->hudtype, "rogue")) {
		rsb_invbar[0] = r_funcs->Draw_PicFromWad ("r_invbar1");
		rsb_invbar[1] = r_funcs->Draw_PicFromWad ("r_invbar2");

		rsb_weapons[0] = r_funcs->Draw_PicFromWad ("r_lava");
		rsb_weapons[1] = r_funcs->Draw_PicFromWad ("r_superlava");
		rsb_weapons[2] = r_funcs->Draw_PicFromWad ("r_gren");
		rsb_weapons[3] = r_funcs->Draw_PicFromWad ("r_multirock");
		rsb_weapons[4] = r_funcs->Draw_PicFromWad ("r_plasma");

		rsb_items[0] = r_funcs->Draw_PicFromWad ("r_shield1");
		rsb_items[1] = r_funcs->Draw_PicFromWad ("r_agrav1");

		// PGM 01/19/97 - team color border
		rsb_teambord = r_funcs->Draw_PicFromWad ("r_teambord");
		// PGM 01/19/97 - team color border

		rsb_ammo[0] = r_funcs->Draw_PicFromWad ("r_ammolava");
		rsb_ammo[1] = r_funcs->Draw_PicFromWad ("r_ammomulti");
		rsb_ammo[2] = r_funcs->Draw_PicFromWad ("r_ammoplasma");
	}
}

static void
Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;

	sb_showscores = true;
	sb_updates = 0;

	sbar_setcomponent (sbar_solo, hud_pic, &sb_scorebar);
	sbar_setcomponent (sbar_solo_monsters, hud_charbuff, &solo_monsters);
	sbar_setcomponent (sbar_solo_secrets, hud_charbuff, &solo_secrets);
	sbar_setcomponent (sbar_solo_time, hud_charbuff, &solo_time);
	sbar_setcomponent (sbar_solo_name, hud_charbuff, &solo_name);

	draw_solo ();
	View_UpdateHierarchy (sbar_solo);
}

static void
Sbar_DontShowScores (void)
{
	if (!sb_showscores)
		return;

	sb_showscores = false;
	sb_updates = 0;

	sbar_remcomponent (sbar_solo, hud_pic);
	sbar_remcomponent (sbar_solo_monsters, hud_charbuff);
	sbar_remcomponent (sbar_solo_secrets, hud_charbuff);
	sbar_remcomponent (sbar_solo_time, hud_charbuff);
	sbar_remcomponent (sbar_solo_name, hud_charbuff);
}

static void
Sbar_ShowTeamScores (void)
{
	if (sb_showteamscores)
		return;

	sb_showteamscores = true;
	sb_updates = 0;
}

static void
Sbar_DontShowTeamScores (void)
{
	sb_showteamscores = false;
	sb_updates = 0;
}

void
Sbar_SetPlayers (player_info_t *players, int maxplayers)
{
	sbar_players = players;
	sbar_maxplayers = maxplayers;
}

void
Sbar_Init (int *stats, float *item_gettime)
{
	sbar_stats = stats;
	sbar_item_gettime = item_gettime;

	center_passage.reg = hud_registry;
	HUD_Init_Cvars ();
	Cvar_AddListener (Cvar_FindVar ("hud_sbar"), sbar_hud_sbar_f, 0);
	Cvar_AddListener (Cvar_FindVar ("hud_swap"), sbar_hud_swap_f, 0);
	Cvar_AddListener (Cvar_FindVar ("hud_fps"), sbar_hud_fps_f, 0);
	Cvar_AddListener (Cvar_FindVar ("hud_time"), sbar_hud_time_f, 0);

	load_pics ();
	init_views ();

	View_UpdateHierarchy (sbar_main);
	set_hud_sbar ();
	View_UpdateHierarchy (sbar_main);

	sbar_hud_fps_f (0, 0);
	sbar_hud_time_f (0, 0);

	Cmd_AddCommand ("+showscores", Sbar_ShowScores,
					"Display information on everyone playing");
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores,
					"Stop displaying information on everyone playing");
	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores,
					"Display information for your team");
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores,
					"Stop displaying information for your team");

	r_data->viewsize_callback = viewsize_f;
	Cvar_Register (&hud_scoreboard_uid_cvar, 0/*FIXME Sbar_DMO_Init_f*/, 0);
	Cvar_Register (&fs_fraglog_cvar, 0, 0);
	Cvar_Register (&cl_fraglog_cvar, 0, 0);
	Cvar_Register (&scr_centertime_cvar, 0, 0);
	Cvar_Register (&scr_printspeed_cvar, 0, 0);

	// register GIB builtins
	GIB_Builtin_Add ("print::center", Sbar_GIB_Print_Center_f);
}
