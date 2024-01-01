/*
	hud.c

	Heads-up display bar

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

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

#include "QF/ui/canvas.h"
#include "QF/ui/passage.h"
#include "QF/ui/view.h"

#include "compat.h"

#include "client/hud.h"
#include "client/screen.h"
#include "client/state.h"
#include "client/world.h"

#include "gamedefs.h"

int         sb_updates;				// if >= vid.numpages, no update needed
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
static int sbar_autotrack = -1;
static int sbar_teamplay;
static int sbar_gametype;
static int sbar_active;
static int sbar_intermission;

ecs_system_t hud_psgsys;
uint32_t    hud_canvas;
int hud_sb_lines;


view_t hud_canvas_view;

static view_t hud_overlay_view;
static view_t hud_stuff_view;
static view_t hud_time_view;
static view_t hud_fps_view;
static view_t hud_ping_view;
static view_t hud_pl_view;

static view_t intermission_view;
static view_t   intermission_time;
static view_t   intermission_secr;
static view_t   intermission_kill;
static view_t hud_view;
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
static view_t spectator_view;
static view_t deathmatch_view;

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
	{&hud_overlay_view,  {  0, 0,320,200}, grav_center, &hud_canvas_view},
	{&intermission_view, {  0, 0,320,200}, grav_northwest, &hud_overlay_view},
	{0, {0, 24, 192, 24}, grav_north, &intermission_view, 1, 0, 0},
	{0, {0, 56, 160, 144}, grav_northwest, &intermission_view, 1, 0, 0},
	{0, {0, 16, 288, 24}, grav_north,  &intermission_view, 1, 0, 0},
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

	{&hud_view,           {  0, 0,320, 48}, grav_south,     &hud_canvas_view},
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
	// for rogue ctf "face"
	{0, { 1, 3, 22,  9}, grav_northwest, &sbar_face,     2,  0, 9},
	{0, { 0, 3, 24,  8}, grav_northwest, &sbar_face,     1,  0, 0},
	{0, { 0, 0, 24, 24}, grav_northwest, &sbar_armor,    4, 24, 0},
	{0, { 0, 0, 24, 24}, grav_northwest, &sbar_ammo,     4, 24, 0},
	// hipnotic and rogue have 8 item slots and no sigils, so the two extra
	// items overlap the sigils view
	{0, { 0, 0, 16, 16}, grav_northwest, &sbar_items,    8, 16, 0},
	{0, { 0, 0, 24, 16}, grav_northwest, &sbar_weapons,  7, 24, 0},
	// hipnotic adds two extra weapons that overlap the keys views (which
	// get moved for hipnotic).
	{0, { 0, 0,176, 16}, grav_northwest, &sbar_weapons,  2, 24, 0},
	{0, { 0, 0, 24, 24}, grav_northwest, &sbar_health,   3, 24, 0},
	{0, {10, 0, 24,  8}, grav_northwest, &sbar_miniammo, 4, 48, 0},

	{&spectator_view,     {  0, 0,320, 32},  grav_south,    &hud_canvas_view},
	{0, { 0, 0, 312, 8},  grav_north,    &spectator_view, 1, 0, 0},
	{0, { 0, 8, 320, 24}, grav_northwest, &spectator_view, 1, 0, 0},
	{0, { 0, 12, 112, 8}, grav_north,    &spectator_view, 1, 0, 0},
	{0, { 0, 20, 232, 8}, grav_north,    &spectator_view, 1, 0, 0},
	{&deathmatch_view,    {  0, 0,320, 200}, grav_center,   &hud_canvas_view},

	{}
};

static draw_charbuffer_t *time_buff;
static draw_charbuffer_t *fps_buff;
static draw_charbuffer_t *ping_buff;
static draw_charbuffer_t *pl_buff;

static draw_charbuffer_t *spec_buff[4];//0,1 no track, 2 lost track, 3 tracking

static draw_charbuffer_t *solo_monsters;
static draw_charbuffer_t *solo_secrets;
static draw_charbuffer_t *solo_time;
static draw_charbuffer_t *solo_name;

static ecs_system_t sbar_viewsys;

static view_t
sbar_view (int x, int y, int w, int h, grav_t gravity, view_t parent)
{
	view_t      view = View_New (sbar_viewsys, parent);
	View_SetPos (view, x, y);
	View_SetLen (view, w, h);
	View_SetGravity (view, gravity);
	View_SetVisible (view, 1);
	return view;
}

static inline void
sbar_setcomponent (view_t view, uint32_t comp, const void *data)
{
	Ent_SetComponent (view.id, cl_canvas_sys.base + comp, view.reg, data);
}

static inline int
sbar_hascomponent (view_t view, uint32_t comp)
{
	return Ent_HasComponent (view.id, cl_canvas_sys.base + comp, view.reg);
}

static inline void *
sbar_getcomponent (view_t view, uint32_t comp)
{
	return Ent_GetComponent (view.id, cl_canvas_sys.base + comp, view.reg);
}

static inline void
sbar_remcomponent (view_t view, uint32_t comp)
{
	Ent_RemoveComponent (view.id, cl_canvas_sys.base + comp, view.reg);
}

static inline void
set_update (view_t view, canvas_update_f func)
{
	sbar_setcomponent (view, canvas_updateonce, &func);
}

#define STAT_MINUS		10			// num frame for '-' stats digit

static qpic_t *sb_nums[2][11];
static qpic_t *sb_colon, *sb_slash;
static qpic_t *sb_ibar[2];
static int sb_ibar_index;
static canvas_subpic_t sb_miniammo[4];
static qpic_t *sb_sbar;
static qpic_t *sb_scorebar;

// 0 is active, 1 is owned, 2-6 are flashes
// 0-6 id, 7-9 hip (laser, mjolnir, prox), 7-11 rogue powerups
static int sb_weapon_count;
static int sb_weapon_view_count;
static int sb_game;
static canvas_subpic_t sb_weapons[7][12];
static qpic_t *sb_ammo[7];			// rogue adds 3 ammo types
static qpic_t *sb_sigil[4];
static qpic_t *sb_armor[3];
// 0 is owned, 1-5 are flashes
static int sb_item_count;
static qpic_t *sb_items[8][32];

static qpic_t *sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are alive
									// 0 is static, 1 is temporary animation
static qpic_t *sb_face_invis;
static qpic_t *sb_face_quad;
static qpic_t *sb_face_invuln;
static qpic_t *sb_face_invis_invuln;

bool sbar_showscores;
static bool sbar_showteamscores;

static int sb_lines;				// scan lines to draw

static qpic_t *rsb_teambord;		// PGM 01/19/97 - team color border

//static bool largegame = false;

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
int hud_sbar;
static cvar_t hud_sbar_cvar = {
	.name = "hud_sbar",
	.description =
		"status bar mode: 0 = hud, 1 = oldstyle",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_sbar },
};
int hud_swap;
static cvar_t hud_swap_cvar = {
	.name = "hud_swap",
	.description =
		"new HUD on left side?",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_swap },
};
int hud_fps;
static cvar_t hud_fps_cvar = {
	.name = "hud_fps",
	.description =
		"display realtime frames per second",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_fps },
};
int hud_ping;
static cvar_t hud_ping_cvar = {
	.name = "hud_ping",
	.description =
		"display current ping to server",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_ping },
};
int hud_pl;
static cvar_t hud_pl_cvar = {
	.name = "hud_pl",
	.description =
		"display current packet loss to server",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_pl },
};
int hud_time;
static cvar_t hud_time_cvar = {
	.name = "hud_time",
	.description =
		"display the current time",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_time },
};
int hud_debug;
static cvar_t hud_debug_cvar = {
	.name = "hud_debug",
	.description =
		"display hud view outlines for debugging",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &hud_debug },
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
grav_t hud_scoreboard_gravity = grav_center;
static cvar_t hud_scoreboard_gravity_cvar = {
	.name = "hud_scoreboard_gravity",
	.description =
		"control placement of scoreboard overlay: center, northwest, north, "
		"northeast, west, east, southwest, south, southeast",
	.default_value = "center",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &grav_t_type, .value = &hud_scoreboard_gravity },
};

static void
hud_scoreboard_gravity_f (void *data, const cvar_t *cvar)
{
	if (View_Valid (hud_overlay_view)) {
		View_SetGravity (hud_overlay_view, hud_scoreboard_gravity);
	}
}

static void
viewsize_f (int view_size)
{
	sb_view_size = view_size;
	if (hud_sbar) {
		SCR_SetBottomMargin (hud_sbar ? sb_lines : 0);
	}
}

static void
C_GIB_HUD_Enable_f (void)
{
	View_SetVisible (hud_canvas_view, 1);
}

static void
C_GIB_HUD_Disable_f (void)
{
	View_SetVisible (hud_canvas_view, 0);
}

static int
Sbar_ColorForMap (int m)
{
	return (bound (0, m, 13) * 16) + 8;
}

static int
write_charbuff_cl (draw_charbuffer_t *buffer, int x, int y, const char *str)
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
	return chars;
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
		sbar_remcomponent (view[x++], canvas_pic);
		digits--;
	}

	while (*ptr) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		sbar_setcomponent (view[x++], canvas_pic, &sb_nums[color][frame]);
		ptr++;
	}
}

static void
draw_smallnum (view_t view, int n, int packed, int colored)
{
	void       *comp = sbar_getcomponent (view, canvas_charbuff);
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
	if (sb_game) {
		if (sbar_stats[STAT_ITEMS] & RIT_SHELLS)
			pic = sb_ammo[0];
		else if (sbar_stats[STAT_ITEMS] & RIT_NAILS)
			pic = sb_ammo[1];
		else if (sbar_stats[STAT_ITEMS] & RIT_ROCKETS)
			pic = sb_ammo[2];
		else if (sbar_stats[STAT_ITEMS] & RIT_CELLS)
			pic = sb_ammo[3];
		else if (sbar_stats[STAT_ITEMS] & RIT_LAVA_NAILS)
			pic = sb_ammo[4];
		else if (sbar_stats[STAT_ITEMS] & RIT_MULTI_ROCKETS)
			pic = sb_ammo[5];
		else if (sbar_stats[STAT_ITEMS] & RIT_PLASMA_AMMO)
			pic = sb_ammo[6];
	} else {
		if (sbar_stats[STAT_ITEMS] & IT_SHELLS)
			pic = sb_ammo[0];
		else if (sbar_stats[STAT_ITEMS] & IT_NAILS)
			pic = sb_ammo[1];
		else if (sbar_stats[STAT_ITEMS] & IT_ROCKETS)
			pic = sb_ammo[2];
		else if (sbar_stats[STAT_ITEMS] & IT_CELLS)
			pic = sb_ammo[3];
	}

	view_t      ammo = View_GetChild (view, 0);
	if (pic) {
		sbar_setcomponent (ammo, canvas_pic, &pic);
	} else {
		sbar_remcomponent (ammo, canvas_pic);
	}

	view_t      num[3] = {
		View_GetChild (view, 1),
		View_GetChild (view, 2),
		View_GetChild (view, 3),
	};
	draw_num (num, sbar_stats[STAT_AMMO], 3, sbar_stats[STAT_AMMO] <= 10);
}

static int
calc_flashon (float time, int mask, int base)
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
	} else {
		flashon = (flashon % 5) + base;
	}
	return flashon;
}

static void
draw_weapons (view_t view)
{
	int         flashon, i;
	static byte view_map[2][12] = {
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 4 },		// id/hipnotic
		{ 0, 1, 2, 3, 4, 5, 6, 2, 3, 4, 5, 6 },	// rogue
	};
	static byte item_map[2][12] = {
		{ 0, 1, 2, 3, 4, 5, 6,	// id/hipnotic
		  HIT_LASER_CANNON_BIT, HIT_MJOLNIR_BIT, HIT_PROXIMITY_GUN_BIT },
		{ 0, 1, 2, 3, 4, 5, 6, 12, 13, 14, 15, 16 },	// rogue
	};
	static int  active_map[2][12] = {
		{ [9] = HIT_PROXIMITY_GUN },	// id/hipnotic
		{ [7] = RIT_LAVA_NAILGUN,
		  [8] = RIT_LAVA_SUPER_NAILGUN,
		  [9] = RIT_MULTI_GRENADE,
		  [10] = RIT_MULTI_ROCKET,
		  [11] = RIT_PLASMA_GUN,
		},	// rogue
	};

	for (i = 0; i < sb_weapon_count; i++) {
		view_t      weap = View_GetChild (view, view_map[sb_game][i]);
		int         mask = 1 << item_map[sb_game][i];
		float       time = sbar_item_gettime[item_map[sb_game][i]];
		int         active = active_map[sb_game][i];

		if (sbar_stats[STAT_ITEMS] & mask) {
			if ((sbar_stats[STAT_ACTIVEWEAPON] & active) == active) {
				flashon = calc_flashon (time, mask, 2);
				if (flashon > 1)
					sb_updates = 0;			// force update to remove flash
				sbar_setcomponent (weap, canvas_subpic,
								   &sb_weapons[flashon][i]);
			}
		} else {
			sbar_remcomponent (weap, canvas_subpic);
		}
	}
}

static void
draw_items (view_t view)
{
	static byte ind_map[2][8] = {
		{ 17, 18, 19, 20, 21, 22, 25, 26 },	// id/hipnotic
		{ 17, 18, 19, 20, 21, 22, 29, 30 },	// rogue
	};
	for (int i = 0; i < sb_item_count; i++) {
		view_t      item = View_GetChild (view, i);
		int         item_ind = ind_map[sb_game][i];
		if (sbar_stats[STAT_ITEMS] & (1 << item_ind)) {
			int         flashon = calc_flashon (sbar_item_gettime[item_ind],
												-1, 1);
			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
			sbar_setcomponent (item, canvas_pic, &sb_items[flashon][i]);
		} else {
			sbar_remcomponent (item, canvas_pic);
		}
	}
}

static void
draw_sigils (view_t view)
{
	for (int i = 0; i < 4; i++) {
		view_t      sigil = View_GetChild (view, i);
		if (sbar_stats[STAT_ITEMS] & (1 << (28 + i))) {
			sbar_setcomponent (sigil, canvas_pic, &sb_sigil[i]);
		} else {
			sbar_remcomponent (sigil, canvas_pic);
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
static draw_charbuffer_t *sb_team_players[MAX_PLAYERS];
static draw_charbuffer_t *sb_team_stats[MAX_PLAYERS];
static draw_charbuffer_t *sb_spectator;
int         scoreboardlines, scoreboardteams;

static void
Sbar_SortFrags (bool includespec)
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

static void
Sbar_SortTeams (void)
{
	char        t[16 + 1];
	int         i, j, k;
	player_info_t *s;

	// request new ping times every two second
	scoreboardteams = 0;


	// sort the teams
	memset (teams, 0, sizeof (teams));
	for (i = 0; i < sbar_maxplayers; i++)
		teams[i].plow = 999;

	for (i = 0; i < sbar_maxplayers; i++) {
		s = &sbar_players[i];
		if (!s->name || !s->name->value[0])
			continue;
		if (s->spectator)
			continue;

		// find his team in the list
		t[16] = 0;
		if (s->team)
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
	for (i = 0; i < sbar_maxplayers; i++) {
		team_t     *tm = teams + i;
		int         plow = tm->plow;
		if (plow < 0 || plow > 999)
			plow = 999;
		int         phigh = tm->phigh;
		if (phigh < 0 || phigh > 999)
			phigh = 999;
		int         pavg = !tm->players ? 999 : tm->ptotal / tm->players;
		if (pavg < 0 || pavg > 999)
			pavg = 999;

		write_charbuff (sb_team_stats[i], 0, 0,
						va (0, "%3i/%3i/%3i", plow, pavg, phigh));
		write_charbuff (sb_team[i], 0, 0, tm->team);
		write_charbuff (sb_team_frags[i], 0, 0, va (0, "%5d", tm->frags));
		write_charbuff (sb_team_players[i], 0, 0, va (0, "%5d", tm->players));
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

static void
draw_solo_time (void)
{
	int         minutes = sbar_time / 60;
	int         seconds = sbar_time - 60 * minutes;
	write_charbuff (solo_time, 0, 0,
					va (0, "Time :%3i:%02i", minutes, seconds));
}

static void
draw_solo (view_t view)
{
	sbar_setcomponent (sbar_solo, canvas_pic, &sb_scorebar);
	sbar_setcomponent (sbar_solo_monsters, canvas_charbuff, &solo_monsters);
	sbar_setcomponent (sbar_solo_secrets, canvas_charbuff, &solo_secrets);
	sbar_setcomponent (sbar_solo_time, canvas_charbuff, &solo_time);
	sbar_setcomponent (sbar_solo_name, canvas_charbuff, &solo_name);
}

static void
hide_solo (view_t view)
{
	sbar_remcomponent (sbar_solo, canvas_pic);
	sbar_remcomponent (sbar_solo_monsters, canvas_charbuff);
	sbar_remcomponent (sbar_solo_secrets, canvas_charbuff);
	sbar_remcomponent (sbar_solo_time, canvas_charbuff);
	sbar_remcomponent (sbar_solo_name, canvas_charbuff);
}

static void
clear_frags_bar (view_t view)
{
	sbar_remcomponent (View_GetChild (view, 0), canvas_fill);
	sbar_remcomponent (View_GetChild (view, 1), canvas_fill);
	sbar_remcomponent (View_GetChild (view, 2), canvas_charbuff);
	sbar_remcomponent (View_GetChild (view, 3), canvas_func);
}

static void
clear_minifrags_bar (view_t view)
{
	clear_frags_bar (view);
	sbar_remcomponent (View_GetChild (view, 4), canvas_charbuff);
	sbar_remcomponent (View_GetChild (view, 5), canvas_charbuff);
	sbar_remcomponent (View_GetChild (view, 6), canvas_charbuff);
}

static void
set_frags_bar (view_t view, byte top, byte bottom, draw_charbuffer_t *buff,
			   canvas_func_f func)
{
	sbar_setcomponent (View_GetChild (view, 0), canvas_fill, &top);
	sbar_setcomponent (View_GetChild (view, 1), canvas_fill, &bottom);
	sbar_setcomponent (View_GetChild (view, 2), canvas_charbuff, &buff);
	if (func) {
		sbar_setcomponent (View_GetChild (view, 3), canvas_func, &func);
	} else {
		sbar_remcomponent (View_GetChild (view, 3), canvas_func);
	}
}

static void
set_minifrags_bar (view_t view, byte top, byte bottom, draw_charbuffer_t *buff,
				   canvas_func_f func, draw_charbuffer_t *team,
				   draw_charbuffer_t *name)
{
	set_frags_bar (view, top, bottom, buff, func);
	if (team) {
		sbar_setcomponent (View_GetChild (view, 4), canvas_charbuff, &team);
		sbar_setcomponent (View_GetChild (view, 5), canvas_charbuff, &name);
		sbar_remcomponent (View_GetChild (view, 6), canvas_charbuff);
	} else {
		sbar_remcomponent (View_GetChild (view, 4), canvas_charbuff);
		sbar_remcomponent (View_GetChild (view, 5), canvas_charbuff);
		sbar_setcomponent (View_GetChild (view, 6), canvas_charbuff, &name);
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
			write_charbuff_cl (sb_team[k], 0, 0, s->team->value);
		}
		write_charbuff_cl (sb_name[k], 0, 0, s->name->value);
		draw_smallnum (View_GetChild (bar, 2), s->frags, 0, 0);
	}
	for (; i < numbars; i++) {
		clear_minifrags_bar (View_GetChild (view, i));
	}
}

static void
clear_miniteam_bar (view_t view)
{
	sbar_remcomponent (View_GetChild (view, 0), canvas_charbuff);
	sbar_remcomponent (View_GetChild (view, 1), canvas_charbuff);
	sbar_remcomponent (View_GetChild (view, 2), canvas_func);
}

static void
set_miniteam_bar (view_t view, draw_charbuffer_t *team,
				  draw_charbuffer_t *frags, canvas_func_f func)
{
	sbar_setcomponent (View_GetChild (view, 0), canvas_charbuff, &team);
	sbar_setcomponent (View_GetChild (view, 1), canvas_charbuff, &frags);
	if (func) {
		sbar_setcomponent (View_GetChild (view, 2), canvas_func, &func);
	} else {
		sbar_remcomponent (View_GetChild (view, 2), canvas_func);
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
		canvas_func_f func = 0;
		if (player_team && strnequal (player_team->value, tm->team, 16)) {
			func = frags_marker;
		}
		set_miniteam_bar (bar, sb_team[k], sb_team_frags[k], func);
		write_charbuff_cl (sb_team[k], 0, 0, s->team->value);
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

	if (sb_game && sbar_maxplayers > 1
		&& sbar_teamplay > 3 && sbar_teamplay < 7) {
		return;
	}
	if (sbar_stats[STAT_HEALTH] <= 0) {//FIXME hide_Face or hide_sbar
		sbar_remcomponent (sbar_face, canvas_pic);
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
	sbar_setcomponent (view, canvas_pic, &face);
}

static void
draw_spectator (view_t view)
{
	view_t      tracking = View_GetChild (view, 0);
	view_t      back = View_GetChild (view, 1);
	view_t      notrack[] = {
		View_GetChild (view, 2),
		View_GetChild (view, 3),
	};

	if (sbar_autotrack < 0) {
		sbar_setcomponent (back, canvas_pic, &sb_scorebar);
		sbar_setcomponent (notrack[0], canvas_charbuff, &spec_buff[0]);
		sbar_setcomponent (notrack[1], canvas_charbuff, &spec_buff[1]);
		sbar_remcomponent (tracking, canvas_charbuff);
	} else {
		sbar_remcomponent (back, canvas_pic);
		sbar_remcomponent (notrack[0], canvas_charbuff);
		sbar_remcomponent (notrack[1], canvas_charbuff);
		if (sbar_players[sbar_autotrack].name) {
			write_charbuff_cl (spec_buff[3], 0, 0,
							   va (0, "Tracking %.13s, [JUMP] for next",
								   sbar_players[sbar_autotrack].name->value));
			sbar_setcomponent (tracking, canvas_charbuff, &spec_buff[3]);
		} else {
			sbar_setcomponent (tracking, canvas_charbuff, &spec_buff[2]);
		}
	}
}

static void
hide_spectator (view_t view)
{
	for (int i = 0; i < 4; i++) {
		sbar_remcomponent (View_GetChild (view, i), canvas_charbuff);
		sbar_remcomponent (View_GetChild (view, i), canvas_pic);
	}
}

static void
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
			sbar_setcomponent (armor, canvas_pic, &sb_armor[2]);
		else if (sbar_stats[STAT_ITEMS] & IT_ARMOR2)
			sbar_setcomponent (armor, canvas_pic, &sb_armor[1]);
		else if (sbar_stats[STAT_ITEMS] & IT_ARMOR1)
			sbar_setcomponent (armor, canvas_pic, &sb_armor[0]);
		else
			sbar_remcomponent (armor, canvas_pic);
	}
}

static void
draw_health (view_t view)
{
	view_t      num[3] = {
		View_GetChild (view, 0),
		View_GetChild (view, 1),
		View_GetChild (view, 2),
	};
	draw_num (num, sbar_stats[STAT_HEALTH], 3, sbar_stats[STAT_HEALTH] <= 25);
}

static void
draw_rogue_ctf_face (view_t view)
{
	__auto_type p = &sbar_players[sbar_viewplayer];
	byte        top = Sbar_ColorForMap (p->topcolor);
	byte        bottom = Sbar_ColorForMap (p->bottomcolor);
	sbar_setcomponent (View_GetChild (view, 0), canvas_fill, &top);
	sbar_setcomponent (View_GetChild (view, 1), canvas_fill, &bottom);
	sbar_setcomponent (View_GetChild (view, 2), canvas_charbuff,
					   &sb_frags[sbar_viewplayer]);
	sbar_setcomponent (view, canvas_pic, &rsb_teambord);
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
	sbar_setcomponent (v, canvas_charbuff, &sb_spectator);
}

typedef struct dmo_def_s {
	int         width;			// in pixels
	draw_charbuffer_t **buffer;
	void      (*setup) (view_t, int);
} dmo_def_t;

static dmo_def_t ping_def      = { .width =  24, .buffer = sb_ping };
static dmo_def_t pl_def        = { .width =  24, .buffer = sb_pl };
static dmo_def_t fph_def       = { .width =  24, .buffer = sb_fph };
static dmo_def_t time_def      = { .width =  32, .buffer = sb_time };
static dmo_def_t frags_def     = { .width =  40,     .setup = setup_frags };
static dmo_def_t team_def      = { .width =  32, .buffer = sb_uid };
static dmo_def_t uid_def       = { .width =  32, .buffer = sb_uid };
static dmo_def_t name_def      = { .width = 128, .buffer = sb_name };
static dmo_def_t spectator_def = { .width = 112,     .setup = setup_spect };
static dmo_def_t spec_team_def = { .width =  32, };
static dmo_def_t team_frags_def = { .width = 40, .buffer = sb_team_frags };
static dmo_def_t team_stats_def = { .width = 88, .buffer = sb_team_stats };
static dmo_def_t team_players_def = { .width = 40, .buffer = sb_team_players };

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
	&uid_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_ping_defs[] = {
	&ping_def,
	&pl_def,
	&fph_def,
	&time_def,
	&frags_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_team_uid_ping_defs[] = {
	&uid_def,
	&pl_def,
	&spectator_def,
	&spec_team_def,
	&ping_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_team_uid_defs[] = {
	&uid_def,
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
	&uid_def,
	&name_def,
	0
};
static dmo_def_t *qw_dmo_spect_uid_defs[] = {
	&uid_def,
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
static dmo_def_t *team_overlay_defs[] = {
	&team_stats_def,
	&team_def,
	&team_frags_def,
	&team_players_def,
	0
};
static dmo_def_t **dmo_defs[] = {
	nq_dmo_defs,
	team_overlay_defs,

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
make_dmo_line (view_t parent, int player, int line_type)
{
	dmo_def_t **defs = dmo_defs[line_type];
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
				sbar_setcomponent (v, canvas_charbuff, &buff);
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

	if (total) {
		fph = (3600 * frags) / total;
		fph = bound (-999, fph, 999);
	} else {
		fph = 0;
	}

	return fph;
}

static int
dmo_line_type (void)
{
	if (!sbar_servername) {
		return 0;
	} else if (sbar_showteamscores) {
		return 1;
	} else {
		int         team = !!sbar_teamplay;
		int         spect = !!sbar_spectator;
		int         mode = bound (0, hud_scoreboard_uid, 2);

		return 2 + mode + team * 3 + spect * mode;
	}
}

static void
draw_deathmatch (view_t view)
{
	Sbar_SortFrags (0);
	Sbar_SortTeams ();

	int         y = 40;
	view_pos_t  len = View_GetLen (view);
	int         numbars = (len.y - y) / 10;
	int         count = min (scoreboardlines, numbars);
	int         line_type = dmo_line_type ();
	int         i;

	double      cur_time = sbar_intermission ? sbar_completed_time : sbar_time;
	for (i = 0; i < count; i++, y += 10) {
		int         k = fragsort[i];
		player_info_t *p = &sbar_players[k];
		if (!View_Valid (sb_views[k])) {
			sb_views[k] = make_dmo_line (view, k, line_type);
		}
		int         total = cur_time - p->entertime;
		write_charbuff (sb_fph[k], 0, 0, va (0, "%3d",
											 calc_fph (p->frags, total)));
		write_charbuff (sb_time[k], 0, 0, va (0, "%4d", total / 60));
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

static void
hide_deathmatch (view_t view)
{
	uint32_t    count = View_ChildCount (view);
	while (count-- > 0) {
		View_Delete (View_GetChild (view, count));
	}
}

static void
draw_status (void )
{
	sb_updates = 0;


	if (sbar_gametype) {
		draw_deathmatch (deathmatch_view);
	} else {
		if (sbar_spectator) {
			if (sbar_autotrack < 0) {
				return;
			}
		}
		draw_solo (sbar_solo);
	}
}

static void
hide_status (void)
{
	sb_updates = 0;
	hide_solo (sbar_solo);
	hide_deathmatch (deathmatch_view);
}

/*	 autologging of frags after a match ended
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
	write_charbuff_cl (time_buff, 0, 0, st);
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
		snprintf (st, sizeof (st), "%6.*f FPS", prec, lastfps);
	}
	write_charbuff (fps_buff, 0, 0, st);
}

/* CENTER PRINTING */
static dstring_t center_string = {&dstring_default_mem};
static passage_t center_passage = { .hierarchy = nullent };
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
	hierarchy_t *h = Ent_GetComponent (center_passage.hierarchy, ecs_hierarchy,
									   center_passage.reg);
	center_lines = h->childCount[0];
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

	hierarchy_t *h = Ent_GetComponent (center_passage.hierarchy, ecs_hierarchy,
									   center_passage.reg);
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

static void
clear_views (view_t view)
{
	sbar_remcomponent (view, canvas_cachepic);
	sbar_remcomponent (view, canvas_pic);

	for (uint32_t i = 0; i < View_ChildCount (view); i++) {
		clear_views (View_GetChild (view, i));
	}
}

static void
draw_intermission (view_t view)
{
	clear_views (view);
	const char *n;
	n = "gfx/complete.lmp";
	sbar_setcomponent (View_GetChild (view, 0), canvas_cachepic, &n);
	n = "gfx/inter.lmp";
	sbar_setcomponent (View_GetChild (view, 1), canvas_cachepic, &n);

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
	sbar_setcomponent (time_views[3], canvas_pic, &sb_colon);
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
	sbar_setcomponent (secr_views[3], canvas_pic, &sb_slash);
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
	sbar_setcomponent (kill_views[3], canvas_pic, &sb_slash);
	draw_num (kill_views + 4, sbar_stats[STAT_TOTALMONSTERS], 3, 0);
}

static void
draw_finale (view_t view)
{
	clear_views (view);

	r_data->scr_copyeverything = 1;

	const char *n = "gfx/finale.lmp";
	sbar_setcomponent (View_GetChild (view, 2), canvas_cachepic, &n);
}

static void
draw_cutscene (view_t view)
{
	clear_views (view);
}

void
Sbar_Intermission (int mode, double completed_time)
{
	static canvas_update_f intermission_funcs[2][3] = {
		{ draw_intermission, draw_finale, draw_cutscene, },
		{ draw_deathmatch, draw_finale, draw_cutscene, },
	};
	static view_t *views[2][3] = {
		{ &intermission_view, &intermission_view, &intermission_view },
		{ &deathmatch_view, &intermission_view, &intermission_view },
	};
	sbar_completed_time = completed_time;
	if ((unsigned) mode > 3) {
		mode = 0;
	}
	sbar_intermission = mode;
	set_update (intermission_view, clear_views);
	set_update (deathmatch_view, clear_views);
	if (mode > 0) {
		set_update (*views[sbar_gametype][mode - 1],
					intermission_funcs[sbar_gametype][mode - 1]);
	}
}

void
Sbar_DrawCenterPrint (void)
{
	r_data->scr_copytop = 1;

	centertime_off -= r_data->frametime;
	if (!center_passage.hierarchy
		|| (centertime_off <= 0 && !sbar_intermission))
		return;


	int         remaining = -1;
	if (sbar_intermission) {
		// the finale prints the characters one at a time
		remaining = scr_printspeed * (sbar_time - centertime_start);
	}
	Sbar_DrawCenterString (hud_overlay_view, remaining);
}

void
Sbar_Update (double time)
{
	qfZoneNamedN (sbu_zone, "Sbar_Update", true);
	fps_count++;
	sbar_time = time;
	if (!sbar_active) {
		return;
	}
	if (sbar_showscores) {
		draw_solo_time ();
	}
	if (sb_updates < r_data->vid->numpages) {
		//FIXME find a better way to support animations
		sb_updates++;
		draw_weapons (sbar_weapons);
		draw_items (sbar_items);
		draw_face (sbar_face);
	}
}

void
Sbar_UpdatePings (void)
{
	for (int i = 0; i < sbar_maxplayers; i++) {
		player_info_t *p = &sbar_players[i];
		if (!p->name || !p->name->value) {
			continue;
		}
		write_charbuff (sb_ping[i], 0, 0, va (0, "%3d", p->ping));
		write_charbuff (sb_pl[i], 0, 0, va (0, "%3d", p->pl));
	}
	write_charbuff (ping_buff, 0, 0,
					va (0, "%3d ms", sbar_players[sbar_playernum].ping));
}

void
Sbar_UpdatePL (int pl)
{
	write_charbuff (pl_buff, 0, 0, va (0, "%3d pl", pl));
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
	write_charbuff (sb_uid[playernum], 0, 0, va (0, "%4d", p->userid));
	write_charbuff_cl (sb_name[playernum], 0, 0, p->name->value);
	if (sbar_teamplay && p->team) {
		write_charbuff_cl (sb_team[playernum], 0, 0, p->team->value);
	}
	if (sb_game && sbar_maxplayers > 1
		&& sbar_teamplay > 3 && sbar_teamplay < 7) {
		set_update (sbar_face, draw_rogue_ctf_face);
	}
}

static void
update_health (int stat)
{
	set_update (sbar_health, draw_health);
	set_update (sbar_face, draw_face);
	if (sbar_stats[STAT_HEALTH] <= 0) {
		draw_status ();
	} else if (1||!sbar_showscores) {
		hide_status ();
	}
}

static void
update_frags (int stat)
{
	write_charbuff (sb_frags[sbar_playernum], 0, 0,
					va (0, "%3d", sbar_stats[stat]));
	set_update (sbar_frags, draw_frags);//FIXME
	set_update (hud_minifrags, draw_minifrags);//FIXME
	set_update (deathmatch_view, draw_deathmatch);//FIXME
	if (sbar_teamplay) {
		set_update (hud_miniteam, draw_miniteam);//FIXME
	}
}

static void
update_weapon (int stat)
{
	set_update (sbar_weapons, draw_weapons);
	sb_ibar_index = (sb_game
					 && sbar_stats[STAT_ACTIVEWEAPON] < RIT_LAVA_NAILGUN);
	if (hud_sbar) {
		// shouldn't need to sort the pics because the component is already
		// on the entity, so the position in the pool won't be affected
		sbar_setcomponent (sbar_inventory, canvas_pic, &sb_ibar[sb_ibar_index]);
	} else {
		for (int i = 0; i < 4; i++) {
			view_t      v = View_GetChild (sbar_miniammo, i);
			sb_miniammo[i].pic = sb_ibar[sb_ibar_index];
			sbar_setcomponent (v, canvas_subpic, &sb_miniammo[i]);
		}
	}
}

static void
update_ammo (int stat)
{
	set_update (sbar_ammo, draw_ammo);
}

static void
update_miniammo (int stat)
{
	set_update (sbar_miniammo, draw_miniammo);//FIXME
}

static void
update_armor (int stat)
{
	set_update (sbar_armor, draw_armor);
}

static void
update_totalsecrets (int stat)
{
	write_charbuff (solo_secrets, 14, 0,
					va (0, "%3i", sbar_stats[STAT_TOTALSECRETS]));
}

static void
update_secrets (int stat)
{
	write_charbuff (solo_secrets, 9, 0,
					va (0, "%3i", sbar_stats[STAT_SECRETS]));
}

static void
update_totalmonsters (int stat)
{
	write_charbuff (solo_monsters, 14, 0,
					va (0, "%3i", sbar_stats[STAT_TOTALMONSTERS]));
}

static void
update_monsters (int stat)
{
	write_charbuff (solo_monsters, 9, 0,
					va (0, "%3i", sbar_stats[STAT_MONSTERS]));
}

static void
update_items (int stat)
{
	set_update (sbar_items, draw_items);
	set_update (sbar_weapons, draw_weapons);
	if (sb_item_count < 7) {
		// hipnotic and rogue don't use sigils
		set_update (sbar_sigils, draw_sigils);
	}
}

typedef void (*stat_update_f) (int stat);

static stat_update_f stat_update[MAX_CL_STATS] = {
	[STAT_HEALTH] = update_health,
	[STAT_FRAGS] = update_frags,
	[STAT_AMMO] = update_ammo,
	[STAT_ARMOR] = update_armor,
	[STAT_SHELLS] = update_miniammo,
	[STAT_NAILS] = update_miniammo,
	[STAT_ROCKETS] = update_miniammo,
	[STAT_CELLS] = update_miniammo,
	[STAT_ACTIVEWEAPON] = update_weapon,
	[STAT_TOTALSECRETS] = update_totalsecrets,
	[STAT_TOTALMONSTERS] = update_totalmonsters,
	[STAT_SECRETS] = update_secrets,
	[STAT_MONSTERS] = update_monsters,
	[STAT_ITEMS] = update_items,
};

void
Sbar_UpdateStats (int stat)
{
	if ((unsigned) stat >= MAX_CL_STATS) {
		Sys_Error ("Sbar_UpdateStats: invalid stat: %d", stat);
	}
	if (stat_update[stat]) {
		stat_update[stat] (stat);
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

	if (sbar_spectator) {
		set_update (spectator_view, draw_spectator);
	} else {
		set_update (spectator_view, hide_spectator);
	}
}

void
Sbar_SetAutotrack (int autotrack)
{
	sbar_autotrack = autotrack;
	if (sbar_spectator) {
		set_update (spectator_view, draw_spectator);
	} else {
		set_update (spectator_view, hide_spectator);
	}
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
	solo_name->cursx = write_charbuff_cl (solo_name, 0, 0, sbar_levelname);

	view_pos_t len = View_GetLen (sbar_solo_name);
	len.x = 8 * solo_name->cursx;
	View_SetLen (sbar_solo_name, len.x, len.y);
	View_UpdateHierarchy (sbar_solo);
}

void
Sbar_SetTeamplay (int teamplay)
{
	sbar_teamplay = teamplay;
	if (sb_game && sbar_maxplayers > 1
		&& sbar_teamplay > 3 && sbar_teamplay < 7) {
		set_update (sbar_face, draw_rogue_ctf_face);
	}
}

void
Sbar_SetGameType (int gametype)
{
	sbar_gametype = gametype;
}

void
Sbar_SetActive (int active)
{
	sbar_active = active;
}

static void
hud_swap_f (void *data, const cvar_t *cvar)
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

		int         x = 0;
		for (int i = 0; i < sb_weapon_view_count; i++) {
			v = View_GetChild (sbar_weapons, i);
			View_SetPos (v, x, 0);
			View_SetLen (v, sb_weapons[0][i].pic->width, 16);
			x += sb_weapons[0][i].pic->width;

			if (sbar_hascomponent (v, canvas_subpic)) {
				canvas_subpic_t *subpic = sbar_getcomponent(v, canvas_subpic);
				subpic->w = subpic->pic->width;
			}
		}
		for (int i = 0; i < 4; i++) {
			v = View_GetChild (sbar_miniammo, i);
			View_SetPos (v, i * 48 + 10, 0);
			View_SetLen (v, 42, 8);
			sbar_remcomponent (v, canvas_subpic);
		}
		for (int i = 0; i < 7; i++) {
			for (int j = 0; j < sb_weapon_view_count; j++) {
				if (sb_weapons[i][j].pic) {
					sb_weapons[i][j].w = sb_weapons[i][j].pic->width;
					sb_weapons[i][j].h = sb_weapons[i][j].pic->height;
				}
			}
		}

		sbar_setcomponent (sbar_inventory, canvas_pic, &sb_ibar[sb_ibar_index]);
		sbar_setcomponent (sbar_statusbar, canvas_pic, &sb_sbar);
		sbar_setcomponent (sbar_tile[0], canvas_tile, 0);
		sbar_setcomponent (sbar_tile[1], canvas_tile, 0);
	} else {
		View_SetParent (sbar_armament, hud_view);
		View_SetPos (sbar_armament, 0, 48);
		View_SetLen (sbar_armament, 42, 44 + 16 * sb_weapon_view_count);
		View_SetGravity (sbar_armament, grav_southeast);

		View_SetLen (sbar_weapons, 24, 16 * sb_weapon_view_count);
		View_SetGravity (sbar_weapons, grav_northeast);

		View_SetLen (sbar_miniammo, 42, 44);
		View_SetGravity (sbar_miniammo, grav_southeast);

		for (int i = 0; i < sb_weapon_view_count; i++) {
			v = View_GetChild (sbar_weapons, i);
			View_SetPos (v, 0, i * 16);
			View_SetLen (v, 24, 16);

			if (sbar_hascomponent (v, canvas_subpic)) {
				canvas_subpic_t *subpic = sbar_getcomponent(v, canvas_subpic);
				subpic->w = 24;
			}
		}
		for (int i = 0; i < 4; i++) {
			v = View_GetChild (sbar_miniammo, i);
			View_SetPos (v, 0, i * 11);
			View_SetLen (v, 42, 11);
			sbar_setcomponent (v, canvas_subpic, &sb_miniammo[i]);
		}
		for (int i = 0; i < 7; i++) {
			for (int j = 0; j < sb_weapon_view_count; j++) {
				if (sb_weapons[i][j].pic) {
					sb_weapons[i][j].w = 24;
					sb_weapons[i][j].h = sb_weapons[i][j].pic->height;
				}
			}
		}

		sbar_remcomponent (sbar_inventory, canvas_pic);
		sbar_remcomponent (sbar_statusbar, canvas_pic);
		sbar_remcomponent (sbar_tile[0], canvas_tile);
		sbar_remcomponent (sbar_tile[1], canvas_tile);
	}
	Canvas_SortComponentPool (cl_canvas_sys, hud_canvas, canvas_pic);
}

static void
hud_sbar_f (void *data, const cvar_t *cvar)
{
	SCR_SetBottomMargin (hud_sbar ? hud_sb_lines : 0);
	set_hud_sbar ();
	View_UpdateHierarchy (hud_view);
}

static void
hud_fps_f (void *data, const cvar_t *cvar)
{
	if (hud_fps) {
		void       *f = draw_fps;
		sbar_setcomponent (hud_fps_view, canvas_update, &f);
		sbar_setcomponent (hud_fps_view, canvas_charbuff, &fps_buff);
	} else {
		sbar_remcomponent (hud_fps_view, canvas_update);
		sbar_remcomponent (hud_fps_view, canvas_charbuff);
	}
}

static void
hud_ping_f (void *data, const cvar_t *cvar)
{
	if (hud_ping) {
		sbar_setcomponent (hud_ping_view, canvas_charbuff, &ping_buff);
	} else {
		sbar_remcomponent (hud_ping_view, canvas_charbuff);
	}
}

static void
hud_pl_f (void *data, const cvar_t *cvar)
{
	if (hud_pl) {
		sbar_setcomponent (hud_pl_view, canvas_charbuff, &pl_buff);
	} else {
		sbar_remcomponent (hud_pl_view, canvas_charbuff);
	}
}

static void
hud_time_f (void *data, const cvar_t *cvar)
{
	if (hud_time) {
		void       *f = draw_time;
		sbar_setcomponent (hud_time_view, canvas_update, &f);
		sbar_setcomponent (hud_time_view, canvas_charbuff, &time_buff);
	} else {
		sbar_remcomponent (hud_time_view, canvas_update);
		sbar_remcomponent (hud_time_view, canvas_charbuff);
	}
}

static void
hud_add_outlines (view_t view, byte color)
{
	Ent_SetComponent (view.id, cl_canvas_sys.base + canvas_outline, view.reg,
					  &color);
	uint32_t    count = View_ChildCount (view);
	for (uint32_t i = 0; i < count; i++) {
		hud_add_outlines (View_GetChild (view, i), color);
	}
}

static void
hud_remove_outlines (view_t view)
{
	Ent_RemoveComponent (view.id, cl_canvas_sys.base + canvas_outline, view.reg);
	uint32_t    count = View_ChildCount (view);
	for (uint32_t i = 0; i < count; i++) {
		hud_remove_outlines (View_GetChild (view, i));
	}
}

static void
hud_debug_f (void *data, const cvar_t *cvar)
{
	if (!View_Valid (hud_canvas_view)) {
		return;
	}
	if (hud_debug) {
		hud_add_outlines (hud_canvas_view, 0x6f);
	} else {
		hud_remove_outlines (hud_canvas_view);
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
	view_pos_t  slen = View_GetLen (hud_canvas_view);
	view_pos_t  hlen = View_GetLen (hud_view);
	View_SetLen (hud_view, slen.x, hlen.y);
	View_SetResize (hud_view, 1, 0);

	for (int i = 0; i < 4; i++) {
		view_t      v = View_GetChild (sbar_miniammo, i);
		draw_charbuffer_t *buffer = Draw_CreateBuffer (3, 1);
		Draw_ClearBuffer (buffer);
		sbar_setcomponent (v, canvas_charbuff, &buffer);
	}

	if (r_data->vid->width > 320) {
		int         l = (r_data->vid->width - 320) / 2;
		View_SetPos (sbar_tile[0], -l, 0);
		View_SetLen (sbar_tile[0], l, 48);
		View_SetPos (sbar_tile[1], -l, 0);
		View_SetLen (sbar_tile[1], l, 48);
		sbar_setcomponent (sbar_tile[0], canvas_tile, 0);
		sbar_setcomponent (sbar_tile[1], canvas_tile, 0);
	}

	solo_monsters = Draw_CreateBuffer (17, 1);
	solo_secrets = Draw_CreateBuffer (17, 1);
	write_charbuff (solo_monsters, 0, 0, "Monsters:xxx /xxx");
	write_charbuff (solo_secrets, 0, 0, "Secrets :xxx /xxx");
	solo_time = Draw_CreateBuffer (12, 1);
	solo_name = Draw_CreateBuffer (20, 1);

	sb_item_count = 6;
	if (!strcmp (qfs_gamedir->hudtype, "hipnotic")) {
		sb_item_count = 8;
		// adjust key view locations and sizes
		for (int i = 0; i < 2; i++) {
			view_t      v = View_GetChild (sbar_items, i);
			View_SetPos (v, 16, 16 + 3 + 9 * i);
			View_SetLen (v, 16, 9);
		}
	}
	if (!strcmp (qfs_gamedir->hudtype, "rogue")) {
		sb_item_count = 8;
	}
}

static void
init_views (void)
{
	hud_stuff_view = sbar_view (0, 48, 152, 16, grav_southwest, hud_canvas_view);
	hud_time_view = sbar_view (8, 0, 64, 8, grav_northwest, hud_stuff_view);
	hud_fps_view = sbar_view (80, 0, 80, 8, grav_northwest, hud_stuff_view);
	hud_ping_view = sbar_view (0, 8, 48, 0, grav_northwest, hud_stuff_view);
	hud_pl_view = sbar_view (56, 8, 48, 0, grav_northwest, hud_stuff_view);

	Draw_ClearBuffer (time_buff = Draw_CreateBuffer (8, 1));
	Draw_ClearBuffer (fps_buff = Draw_CreateBuffer (10, 1));
	Draw_ClearBuffer (ping_buff = Draw_CreateBuffer (6, 1));
	Draw_ClearBuffer (pl_buff = Draw_CreateBuffer (6, 1));

	for (int i = 0; i < MAX_PLAYERS; i++) {
		sb_fph[i] = Draw_CreateBuffer (3, 1);
		sb_time[i] = Draw_CreateBuffer (4, 1);
		sb_frags[i] = Draw_CreateBuffer (3, 1);
		sb_ping[i] = Draw_CreateBuffer (3, 1);
		sb_pl[i] = Draw_CreateBuffer (3, 1);
		sb_uid[i] = Draw_CreateBuffer (4, 1);
		sb_name[i] = Draw_CreateBuffer (16, 1);

		sb_team[i] = Draw_CreateBuffer (4, 1);
		sb_team_stats[i] = Draw_CreateBuffer (11, 1);
		sb_team_frags[i] = Draw_CreateBuffer (5, 1);
		sb_team_players[i] = Draw_CreateBuffer (5, 1);
		fragsort[i] = -1;
	}
	sb_spectator = Draw_CreateBuffer (11, 1);
	write_charbuff (sb_spectator, 0, 0, "(spectator)");

	spec_buff[0] = Draw_CreateBuffer (14, 1);
	spec_buff[1] = Draw_CreateBuffer (29, 1);
	spec_buff[2] = Draw_CreateBuffer (28, 1);
	spec_buff[3] = Draw_CreateBuffer (39, 1);
	write_charbuff (spec_buff[0], 0, 0, "SPECTATOR MODE");
	write_charbuff (spec_buff[1], 0, 0, "Press [ATTACK] for AutoCamera");
	write_charbuff (spec_buff[2], 0, 0, "Lost player, [JUMP] for next");

	init_sbar_views ();
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

	sb_ammo[0] = r_funcs->Draw_PicFromWad ("sb_shells");
	sb_ammo[1] = r_funcs->Draw_PicFromWad ("sb_nails");
	sb_ammo[2] = r_funcs->Draw_PicFromWad ("sb_rocket");
	sb_ammo[3] = r_funcs->Draw_PicFromWad ("sb_cells");

	sb_armor[0] = r_funcs->Draw_PicFromWad ("sb_armor1");
	sb_armor[1] = r_funcs->Draw_PicFromWad ("sb_armor2");
	sb_armor[2] = r_funcs->Draw_PicFromWad ("sb_armor3");

	sb_items[0][0] = r_funcs->Draw_PicFromWad ("sb_key1");
	sb_items[0][1] = r_funcs->Draw_PicFromWad ("sb_key2");
	sb_items[0][2] = r_funcs->Draw_PicFromWad ("sb_invis");
	sb_items[0][3] = r_funcs->Draw_PicFromWad ("sb_invuln");
	sb_items[0][4] = r_funcs->Draw_PicFromWad ("sb_suit");
	sb_items[0][5] = r_funcs->Draw_PicFromWad ("sb_quad");
	for (int i = 1; i < 6; i++) {
		sb_items[i][0] = r_funcs->Draw_PicFromWad (va (0, "sba%d_key1", i));
		sb_items[i][1] = r_funcs->Draw_PicFromWad (va (0, "sba%d_key2", i));
		sb_items[i][2] = r_funcs->Draw_PicFromWad (va (0, "sba%d_invis", i));
		sb_items[i][3] = r_funcs->Draw_PicFromWad (va (0, "sba%d_invul", i));
		sb_items[i][4] = r_funcs->Draw_PicFromWad (va (0, "sba%d_suit", i));
		sb_items[i][5] = r_funcs->Draw_PicFromWad (va (0, "sba%d_quad", i));
	}

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
	sb_ibar[0] = r_funcs->Draw_PicFromWad ("ibar");
	sb_scorebar = r_funcs->Draw_PicFromWad ("scorebar");
	sb_weapon_count = 7;
	sb_weapon_view_count = 7;
	sb_game = 0;

	// MED 01/04/97 added new hipnotic weapons
	if (!strcmp (qfs_gamedir->hudtype, "hipnotic")) {
		sb_weapon_count = 10;
		sb_weapon_view_count = 9;
		sb_weapons[0][7].pic = r_funcs->Draw_PicFromWad ("inv_laser");
		sb_weapons[0][8].pic = r_funcs->Draw_PicFromWad ("inv_mjolnir");
		sb_weapons[0][9].pic = r_funcs->Draw_PicFromWad ("inv_prox_gren");
		//sb_weapons[0][3].pic = r_funcs->Draw_PicFromWad ("inv_gren_prox");
		//sb_weapons[0][4] = r_funcs->Draw_PicFromWad ("inv_prox");

		sb_weapons[1][7].pic = r_funcs->Draw_PicFromWad ("inv2_laser");
		sb_weapons[1][8].pic = r_funcs->Draw_PicFromWad ("inv2_mjolnir");
		sb_weapons[1][9].pic = r_funcs->Draw_PicFromWad ("inv2_prox_gren");
		//sb_weapons[1][3].pic = r_funcs->Draw_PicFromWad ("inv2_gren_prox");
		//sb_weapons[1][4] = r_funcs->Draw_PicFromWad ("inv2_prox");

		for (int i = 0; i < 5; i++) {
			sb_weapons[2 + i][7].pic =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_laser", i + 1));
			sb_weapons[2 + i][8].pic =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_mjolnir", i + 1));
			sb_weapons[2 + i][9].pic =
				r_funcs->Draw_PicFromWad (va (0, "inva%i_prox_gren", i + 1));
			//sb_weapons[2 + i][2] =
			//	r_funcs->Draw_PicFromWad (va (0, "inva%i_gren_prox", i + 1));
			//sb_weapons[2 + i][4] =
			//	r_funcs->Draw_PicFromWad (va (0, "inva%i_prox", i + 1));
		}

		sb_items[0][6] = r_funcs->Draw_PicFromWad ("sb_wsuit");
		sb_items[0][7] = r_funcs->Draw_PicFromWad ("sb_eshld");
	}

	// FIXME: MISSIONHUD
	if (!strcmp (qfs_gamedir->hudtype, "rogue")) {
		sb_weapon_count = 12;
		sb_game = 1;
		sb_ibar[0] = r_funcs->Draw_PicFromWad ("r_invbar1");
		sb_ibar[1] = r_funcs->Draw_PicFromWad ("r_invbar2");

		sb_weapons[0][7].pic = r_funcs->Draw_PicFromWad ("r_lava");
		sb_weapons[0][8].pic = r_funcs->Draw_PicFromWad ("r_superlava");
		sb_weapons[0][9].pic = r_funcs->Draw_PicFromWad ("r_gren");
		sb_weapons[0][10].pic = r_funcs->Draw_PicFromWad ("r_multirock");
		sb_weapons[0][11].pic = r_funcs->Draw_PicFromWad ("r_plasma");
		for (int i = 1; i < 7; i++) {
			sb_weapons[i][7].pic = sb_weapons[0][7].pic;
			sb_weapons[i][8].pic = sb_weapons[0][8].pic;
			sb_weapons[i][9].pic = sb_weapons[0][9].pic;
			sb_weapons[i][10].pic = sb_weapons[0][10].pic;
			sb_weapons[i][11].pic = sb_weapons[0][11].pic;
		}

		sb_items[0][6] = r_funcs->Draw_PicFromWad ("r_shield1");
		sb_items[0][7] = r_funcs->Draw_PicFromWad ("r_agrav1");

		// PGM 01/19/97 - team color border
		rsb_teambord = r_funcs->Draw_PicFromWad ("r_teambord");
		// PGM 01/19/97 - team color border

		// It seems the pics for plasma and multi-rockets are swapped
		sb_ammo[4] = r_funcs->Draw_PicFromWad ("r_ammolava");
		sb_ammo[5] = r_funcs->Draw_PicFromWad ("r_ammoplasma");
		sb_ammo[6] = r_funcs->Draw_PicFromWad ("r_ammomulti");
	}

	for (int i = 0; i < 7; i++) {
		for (int j = 0; j < 12; j++) {
			if (sb_weapons[i][j].pic) {
				sb_weapons[i][j].w = sb_weapons[i][j].pic->width;
				sb_weapons[i][j].h = sb_weapons[i][j].pic->height;
			}
		}
	}
	for (int i = 1; i < 6; i++) {
		for (int j = 0; j < 32; j++) {
			if (!sb_items[i][j]) {
				sb_items[i][j] = sb_items[0][j];
			}
		}
	}
	if (!sb_ibar[1]) {
		sb_ibar[1] = sb_ibar[0];
	}
	for (int i = 0; i < 4; i++) {
		sb_miniammo[i] = (canvas_subpic_t) {
			.pic = sb_ibar[0],
			.x = 3 + (i * 48),
			.y = 0,
			.w = 42,
			.h = 11,
		};
	}
}

static void
Sbar_ShowScores (void)
{
	sbar_showscores = true;
	draw_status ();
}

static void
Sbar_DontShowScores (void)
{
	if (sbar_autotrack >= 0 && sbar_stats[STAT_HEALTH] <= 0) {
		return;
	}
	sbar_showscores = false;
	hide_status ();
}

static void
Sbar_ShowTeamScores (void)
{
	if (sbar_showteamscores)
		return;

	sbar_showteamscores = true;
	sb_updates = 0;
}

static void
Sbar_DontShowTeamScores (void)
{
	sbar_showteamscores = false;
	sb_updates = 0;
}

void
Sbar_SetPlayers (player_info_t *players, int maxplayers)
{
	sbar_players = players;
	sbar_maxplayers = maxplayers;
}

void
HUD_Init (ecs_registry_t *reg)
{
	hud_psgsys = (ecs_system_t) {
		.reg = reg,
		.base = ECS_RegisterComponents (reg, passage_components,
										passage_comp_count),
	};
}

void
HUD_CreateCanvas (canvas_system_t canvas_sys)
{
	hud_canvas = Canvas_New (canvas_sys);
	hud_canvas_view = Canvas_GetRootView (canvas_sys, hud_canvas);
	View_SetPos (hud_canvas_view, 0, 0);
	View_SetLen (hud_canvas_view, viddef.width, viddef.height);
	View_SetGravity (hud_canvas_view, grav_northwest);
	View_SetVisible (hud_canvas_view, 1);
}

void
Sbar_Init (int *stats, float *item_gettime)
{
	sbar_stats = stats;
	sbar_item_gettime = item_gettime;

	sbar_viewsys = (ecs_system_t) {
		.reg = cl_canvas_sys.reg,
		.base = cl_canvas_sys.view_base,
	};

	center_passage.reg = hud_psgsys.reg;
	center_passage.comp_base = hud_psgsys.base;

	load_pics ();
	init_views ();

	View_UpdateHierarchy (sbar_main);
	set_hud_sbar ();
	View_UpdateHierarchy (sbar_main);

	Cmd_AddCommand ("+showscores", Sbar_ShowScores,
					"Display information on everyone playing");
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores,
					"Stop displaying information on everyone playing");
	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores,
					"Display information for your team");
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores,
					"Stop displaying information for your team");

	GIB_Builtin_Add ("HUD::enable", C_GIB_HUD_Enable_f);
	GIB_Builtin_Add ("HUD::disable", C_GIB_HUD_Disable_f);

	r_data->viewsize_callback = viewsize_f;

	Cvar_Register (&hud_scoreboard_gravity_cvar, hud_scoreboard_gravity_f, 0);
	Cvar_Register (&hud_scoreboard_uid_cvar, 0/*FIXME Sbar_DMO_Init_f*/, 0);
	Cvar_Register (&hud_sbar_cvar, hud_sbar_f, 0);
	Cvar_Register (&hud_swap_cvar, hud_swap_f, 0);
	Cvar_Register (&hud_fps_cvar, hud_fps_f, 0);
	Cvar_MakeAlias ("show_fps", &hud_fps_cvar);
	Cvar_Register (&hud_ping_cvar, hud_ping_f, 0);
	Cvar_Register (&hud_pl_cvar, hud_pl_f, 0);
	Cvar_Register (&hud_time_cvar, hud_time_f, 0);
	Cvar_Register (&hud_debug_cvar, hud_debug_f, 0);

	Cvar_Register (&fs_fraglog_cvar, 0, 0);
	Cvar_Register (&cl_fraglog_cvar, 0, 0);
	Cvar_Register (&scr_centertime_cvar, 0, 0);
	Cvar_Register (&scr_printspeed_cvar, 0, 0);

	// register GIB builtins
	GIB_Builtin_Add ("print::center", Sbar_GIB_Print_Center_f);
}
