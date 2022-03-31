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
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/gib.h"
#include "QF/msg.h"
#include "QF/quakefs.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "QF/plugin/console.h"

#include "QF/ui/view.h"

#include "compat.h"

#include "client/hud.h"
#include "client/world.h"

#include "qw/bothdefs.h"
#include "qw/include/cl_cam.h"
#include "qw/include/cl_parse.h"
#include "qw/include/client.h"
#include "sbar.h"

int         sb_updates;				// if >= vid.numpages, no update needed

#define STAT_MINUS		10			// num frame for '-' stats digit

qpic_t     *sb_nums[2][11];
qpic_t     *sb_colon, *sb_slash;
qpic_t     *sb_ibar;
qpic_t     *sb_sbar;
qpic_t     *sb_scorebar;

qpic_t     *sb_weapons[7][8];		// 0 is active, 1 is owned, 2-5 are flashes
qpic_t     *sb_ammo[4];
qpic_t     *sb_sigil[4];
qpic_t     *sb_armor[3];
qpic_t     *sb_items[32];

qpic_t     *sb_faces[7][2];			// 0 is gibbed, 1 is dead, 2-6 are alive
									// 0 is static, 1 is temporary animation
qpic_t     *sb_face_invis;
qpic_t     *sb_face_quad;
qpic_t     *sb_face_invuln;
qpic_t     *sb_face_invis_invuln;

qboolean    sb_showscores;
qboolean    sb_showteamscores;

int         sb_lines;				// scan lines to draw

static qboolean largegame = false;

cvar_t     *fs_fraglog;
cvar_t     *cl_fraglog;
cvar_t     *hud_scoreboard_uid;
cvar_t     *scr_centertime;
cvar_t     *scr_printspeed;

static view_t *sbar_view;
static view_t *sbar_inventory_view;
static view_t *sbar_frags_view;

static view_t *hud_view;
static view_t *hud_inventory_view;
static view_t *hud_armament_view;
static view_t *hud_frags_view;

static view_t *overlay_view;
static view_t *stuff_view;
static view_t *main_view;

static void (*Sbar_Draw_DMO_func) (view_t *view, int l, int y, int skip);

static void
hud_swap_f (cvar_t *var)
{
	if (var->int_val) {
		view_setgravity (hud_armament_view, grav_southwest);
		view_setgravity (stuff_view, grav_southeast);
	} else {
		view_setgravity (hud_armament_view, grav_southeast);
		view_setgravity (stuff_view, grav_southwest);
	}
}

static void
hud_scoreboard_gravity_f (cvar_t *var)
{
	grav_t      grav;

	if (strequal (var->string, "center"))
		grav = grav_center;
	else if (strequal (var->string, "northwest"))
		grav = grav_northwest;
	else if (strequal (var->string, "north"))
		grav = grav_north;
	else if (strequal (var->string, "northeast"))
		grav = grav_northeast;
	else if (strequal (var->string, "west"))
		grav = grav_west;
	else if (strequal (var->string, "east"))
		grav = grav_east;
	else if (strequal (var->string, "southwest"))
		grav = grav_southwest;
	else if (strequal (var->string, "south"))
		grav = grav_south;
	else if (strequal (var->string, "southeast"))
		grav = grav_southeast;
	else
		grav = grav_center;
	overlay_view->gravity = grav;
	view_move (overlay_view, overlay_view->xpos, overlay_view->ypos);
}

static void
calc_sb_lines (cvar_t *var)
{
	int        stuff_y;

	if (var->int_val >= 120) {
		sb_lines = 0;
		stuff_y = 0;
	} else if (var->int_val >= 110) {
		sb_lines = 24;
		sbar_inventory_view->visible = 0;
		hud_inventory_view->visible = 0;
		hud_armament_view->visible = 0;
		stuff_y = 32;
	} else {
		sb_lines = 48;
		sbar_inventory_view->visible = 1;
		hud_inventory_view->visible = 1;
		hud_armament_view->visible = 1;
		stuff_y = 48;
	}
	if (sb_lines) {
		sbar_view->visible = 1;
		hud_view->visible = 1;
		view_resize (sbar_view, sbar_view->xlen, sb_lines);
		view_resize (hud_view, hud_view->xlen, sb_lines);
	} else {
		sbar_view->visible = 0;
		hud_view->visible = 0;
	}
	view_move (stuff_view, stuff_view->xpos, stuff_y);
}

static void
hud_sbar_f (cvar_t *var)
{
	if (r_data->scr_viewsize)
		calc_sb_lines (r_data->scr_viewsize);
	SCR_SetBottomMargin (var->int_val ? sb_lines : 0);
	if (var->int_val) {
		view_remove (main_view, main_view->children[0]);
		view_insert (main_view, sbar_view, 0);
	} else {
		view_remove (main_view, main_view->children[0]);
		view_insert (main_view, hud_view, 0);
	}
}

static void
viewsize_f (cvar_t *var)
{
	calc_sb_lines (var);
	if (hud_sbar) {
		SCR_SetBottomMargin (hud_sbar->int_val ? sb_lines : 0);
	}
}


static int
Sbar_ColorForMap (int m)
{
	return (bound (0, m, 13) * 16) + 8;
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

static void
Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;

	sb_showscores = true;
	sb_updates = 0;
}

static void
Sbar_DontShowScores (void)
{
	sb_showscores = false;
	sb_updates = 0;
}

void
Sbar_Changed (void)
{
	sb_updates = 0;						// update next frame
}

static void
draw_pic (view_t *view, int x, int y, qpic_t *pic)
{
	r_funcs->Draw_Pic (view->xabs + x, view->yabs + y, pic);
}

static inline void
draw_cachepic (view_t *view, int x, int y, const char *name, int cent)
{
	qpic_t *pic = r_funcs->Draw_CachePic (name, true);
	if (cent)
		x += (view->xlen - pic->width) / 2;
	r_funcs->Draw_Pic (view->xabs + x, view->yabs + y, pic);
}

static inline void
draw_subpic (view_t *view, int x, int y, qpic_t *pic,
		     int srcx, int srcy, int width, int height)
{
	r_funcs->Draw_SubPic (view->xabs + x, view->yabs + y, pic,
				 srcx, srcy, width, height);
}

static inline void
draw_transpic (view_t *view, int x, int y, qpic_t *pic)
{
	r_funcs->Draw_Pic (view->xabs + x, view->yabs + y, pic);
}


// drawing routines are reletive to the status bar location

static inline void
draw_character (view_t *view, int x, int y, int c)
{
	r_funcs->Draw_Character (view->xabs + x, view->yabs + y, c);
}

static inline void
draw_string (view_t *view, int x, int y, const char *str)
{
	r_funcs->Draw_String (view->xabs + x, view->yabs + y, str);
}
#if 0
static inline void
draw_altstring (view_t *view, int x, int y, const char *str)
{
	r_funcs->Draw_AltString (view->xabs + x, view->yabs + y, str);
}
#endif
static inline void
draw_nstring (view_t *view, int x, int y, const char *str, int n)
{
	r_funcs->Draw_nString (view->xabs + x, view->yabs + y, str, n);
}

static inline void
draw_fill (view_t *view, int x, int y, int w, int h, int col)
{
	r_funcs->Draw_Fill (view->xabs + x, view->yabs + y, w, h, col);
}

static void
draw_num (view_t *view, int x, int y, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame;

	if (num > 999999999)
		num = 999999999;

	l = snprintf (str, sizeof (str), "%d", num);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr - '0';

		draw_transpic (view, x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

typedef struct {
	char        team[16 + 1];
	int         frags;
	int         players;
	int         plow, phigh, ptotal;
} team_t;

team_t      teams[MAX_CLIENTS];
int         teamsort[MAX_CLIENTS];
int         fragsort[MAX_CLIENTS]; // ZOID changed this from [MAX_SCOREBOARD]
int         scoreboardlines, scoreboardteams;

static void
Sbar_SortFrags (qboolean includespec)
{
	int         i, j, k;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name && cl.players[i].name->value[0]
			&& (!cl.players[i].spectator || includespec)) {
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
			if (cl.players[i].spectator)
				cl.players[i].frags = -999;
		}
	}

	for (i = 0; i < scoreboardlines; i++)
		for (j = 0; j < scoreboardlines - 1 - i; j++)
			if (cl.players[fragsort[j]].frags <
				cl.players[fragsort[j + 1]].frags) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
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

	if (!cl.teamplay)
		return;

	// sort the teams
	memset (teams, 0, sizeof (teams));
	for (i = 0; i < MAX_CLIENTS; i++)
		teams[i].plow = 999;

	for (i = 0; i < MAX_CLIENTS; i++) {
		s = &cl.players[i];
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
	for (i = 0; i < scoreboardteams - 1; i++)
		for (j = i + 1; j < scoreboardteams; j++)
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags) {
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}
}

static void
draw_solo (view_t *view)
{
	char        str[80];
	int         minutes, seconds;

	draw_pic (view, 0, 0, sb_scorebar);

	minutes = cl.time / 60;
	seconds = cl.time - 60 * minutes;
	snprintf (str, sizeof (str), "Time :%3i:%02i", minutes, seconds);
	draw_string (view, 184, 4, str);
}

static inline void
draw_smallnum (view_t *view, int x, int y, int n, int packed, int colored)
{
	char        num[12];

	packed = packed != 0;				// ensure 0 or 1

	if (n > 999)
		n = 999;

	snprintf (num, sizeof (num), "%3d", n);
	if (colored) {
		if (num[0] != ' ')
			num[0] = 18 + num[0] - '0';
		if (num[1] != ' ')
			num[1] = 18 + num[1] - '0';
		if (num[2] != ' ')
			num[2] = 18 + num[2] - '0';
	}
	draw_character (view, x + packed, y, num[0]);
	draw_character (view, x + 8, y, num[1]);
	draw_character (view, x + 16 - packed, y, num[2]);
}

static void
draw_tile (view_t *view)
{
	r_funcs->Draw_TileClear (view->xabs, view->yabs, view->xlen, view->ylen);
}

static void
draw_ammo_sbar (view_t *view)
{
	int         i, count;

	// ammo counts
	for (i = 0; i < 4; i++) {
		count = cl.stats[STAT_SHELLS + i];
		draw_smallnum (view, (6 * i + 1) * 8 + 2, 0, count, 0, 1);
	}
}

static void
draw_ammo_hud (view_t *view)
{
	int         i, count;

	// ammo counts
	for (i = 0; i < 4; i++) {
		count = cl.stats[STAT_SHELLS + i];
		draw_subpic (view, 0, i * 11, sb_ibar, 3 + (i * 48), 0, 42, 11);
		draw_smallnum (view, 7, i * 11, count, 0, 1);
	}
}

static int
calc_flashon (float time, int mask)
{
	int         flashon;

	flashon = (int) ((cl.time - time) * 10);
	if (flashon < 0)
		flashon = 0;
	if (flashon >= 10) {
		if (cl.stats[STAT_ACTIVEWEAPON] == mask)
			flashon = 1;
		else
			flashon = 0;
	} else
		flashon = (flashon % 5) + 2;
	return flashon;
}

static void
draw_weapons_sbar (view_t *view)
{
	int         flashon, i;

	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = calc_flashon (cl.item_gettime[i], IT_SHOTGUN << i);
			draw_pic (view, i * 24, 0, sb_weapons[flashon][i]);
			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
		}
	}
}

static void
draw_weapons_hud (view_t *view)
{
	int         flashon, i, x = 0;

	if (view->parent->gravity == grav_southeast)
		x = view->xlen - 24;

	for (i = r_data->vid->conview->ylen < 204; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = calc_flashon (cl.item_gettime[i], IT_SHOTGUN << i);
			draw_subpic (view, x, i * 16, sb_weapons[flashon][i], 0, 0, 24, 16);
			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
		}
	}
}

static void
draw_items (view_t *view)
{
	float       time;
	int         flashon = 0, i;

	for (i = 0; i < 6; i++) {
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
			time = cl.item_gettime[17 + i];
			if (time && time > cl.time - 2 && flashon) {	// Flash frame
				sb_updates = 0;
			} else {
				draw_pic (view, i * 16, 0, sb_items[i]);
			}
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
	}
}

static void
draw_sigils (view_t *view)
{
	float       time;
	int         flashon = 0, i;

	for (i = 0; i < 4; i++) {
		if (cl.stats[STAT_ITEMS] & (1 << (28 + i))) {
			time = cl.item_gettime[28 + i];
			if (time && time > cl.time - 2 && flashon) {	// flash frame
				sb_updates = 0;
			} else {
				draw_pic (view, i * 8, 0, sb_sigil[i]);
			}
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
	}
}

static void
draw_inventory_sbar (view_t *view)
{
	if (cl.spectator && autocam == CAM_TRACK) {
		if (sbar_frags_view)
			sbar_frags_view->draw (sbar_frags_view);
		return;
	}
	draw_pic (view, 0, 0, sb_ibar);
	view_draw (view);
}

static inline void
dmo_ping (view_t *view, int x, int y, player_info_t *s)
{
	int         p;

	p = s->ping;
	if (p < 0 || p > 999)
		p = 999;
	draw_smallnum (view, x + 8, y, p, 0, 0);
}

static inline void
dmo_uid (view_t *view, int x, int y, player_info_t *s)
{
	char		num[12];
	int         p;

	p = s->userid;
	snprintf (num, sizeof (num), "%4i", p);
	draw_string (view, x, y, num);
}

static inline void
dmo_pl (view_t *view, int x, int y, player_info_t *s)
{
	int         p;

	// draw pl
	p = s->pl;
	draw_smallnum (view, x, y, p, 0, p > 25);
}

static inline int
calc_fph (int frags, int total)
{
	int		fph;

	if (total != 0) {
		fph = (3600 * frags) / total;

		if (fph >= 999)
			fph = 999;
		else if (fph <= -999)
			fph = -999;
	} else {
		fph = 0;
	}

	return fph;
}

static inline void
dmo_main (view_t *view, int x, int y, player_info_t *s, int is_client)
{
	char		num[12];
	int			fph, minutes, total, top, bottom, f;

	// get time
	if (cl.intermission)
		total = cl.completed_time - s->entertime;
	else
		total = realtime - s->entertime;
	minutes = total / 60;

	// get frags
	f = s->frags;

	// draw fph
	fph = calc_fph (f, total);
	snprintf (num, sizeof (num), "%3i", fph);
	draw_string (view, x, y, num);

	//draw time
	snprintf (num, sizeof (num), "%4i", minutes);
	draw_string (view, x + 32, y, num);

	// draw background
	top = Sbar_ColorForMap (s->topcolor);
	bottom = Sbar_ColorForMap (s->bottomcolor);
	if (largegame)
		draw_fill (view, x + 72, y + 1, 40, 3, top);
	else
		draw_fill (view, x + 72, y, 40, 4, top);
	draw_fill (view, x + 72, y + 4, 40, 4, bottom);

	// draw frags
	if (!is_client) {
		snprintf (num, sizeof (num), " %3i ", f);
	} else {
		snprintf (num, sizeof (num), "\x10%3i\x11", f);
	}
	draw_nstring (view, x + 72, y, num, 5);
}

static inline void
dmo_team (view_t *view, int x, int y, player_info_t *s)
{
	draw_nstring (view, x, y, s->team->value, 4);
}

static inline void
dmo_name (view_t *view, int x, int y, player_info_t *s)
{
	draw_string (view, x, y, s->name->value);
}

static void
draw_frags (view_t *view)
{
	int         i, k, l, p = -1;
	int         top, bottom;
	int         x;
	player_info_t *s;

	Sbar_SortFrags (false);

	// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 0;

	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;
		if (s->spectator)
			continue;

		// draw background
		top = bound (0, s->topcolor, 13);
		bottom = bound (0, s->bottomcolor, 13);

		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		draw_fill (view, x + 4, 1, 28, 4, top);
		draw_fill (view, x + 4, 5, 28, 3, bottom);

		draw_smallnum (view, x + 6, 0, s->frags, 0, 0);

		if (k == cl.playernum)
			p = i;

		x += 32;
	}
	if (p != -1) {
		draw_character (view, p * 32, 0, 16);
		draw_character (view, p * 32 + 26, 0, 17);
	}
}

static void
draw_face (view_t *view)
{
	int         f, anim;

	if ((cl.stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY))
		== (IT_INVISIBILITY | IT_INVULNERABILITY)) {
		draw_pic (view, 112, 0, sb_face_invis_invuln);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		draw_pic (view, 112, 0, sb_face_quad);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		draw_pic (view, 112, 0, sb_face_invis);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		draw_pic (view, 112, 0, sb_face_invuln);
		return;
	}

	if (cl.stats[STAT_HEALTH] >= 100)
		f = 4;
	else
		f = cl.stats[STAT_HEALTH] / 20;

	if (cl.time <= cl.faceanimtime) {
		anim = 1;
		sb_updates = 0;					// make sure the anim gets drawn over
	} else
		anim = 0;
	draw_pic (view, 112, 0, sb_faces[f][anim]);
}

static void
draw_spectator (view_t *view)
{
	char        st[512];

	if (autocam != CAM_TRACK) {
		draw_string (view, 160 - 7 * 8, 4, "SPECTATOR MODE");
		draw_string (view, 160 - 14 * 8 + 4, 12,
					 "Press [ATTACK] for AutoCamera");
	} else {
//		Sbar_DrawString (160-14*8+4,4, "SPECTATOR MODE - TRACK CAMERA");
		if (cl.players[spec_track].name) {
			snprintf (st, sizeof (st), "Tracking %-.13s, [JUMP] for next",
					  cl.players[spec_track].name->value);
		} else {
			snprintf (st, sizeof (st), "Lost player, [JUMP] for next");
		}
		draw_string (view, 0, -8, st);
	}
}

static void
draw_status_bar (view_t *view)
{
	if (cl.spectator && autocam != CAM_TRACK)
		draw_pic (view, 0, 0, sb_scorebar);
	else
		draw_pic (view, 0, 0, sb_sbar);
}

static void
draw_status (view_t *view)
{
	if (cl.spectator) {
		draw_spectator (view);
		if (autocam != CAM_TRACK)
			return;
	} if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
		draw_solo (view);
		return;
	}
	// armor
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		draw_num (view, 24, 0, 666, 3, 1);
	} else {
		draw_num (view, 24, 0, cl.stats[STAT_ARMOR], 3,
				  cl.stats[STAT_ARMOR] <= 25);
		if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
			draw_pic (view, 0, 0, sb_armor[2]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
			draw_pic (view, 0, 0, sb_armor[1]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
			draw_pic (view, 0, 0, sb_armor[0]);
	}

	// face
	draw_face (view);

	// health
	draw_num (view, 136, 0, cl.stats[STAT_HEALTH], 3,
			  cl.stats[STAT_HEALTH] <= 25);

	// ammo icon
	if (cl.stats[STAT_ITEMS] & IT_SHELLS)
		draw_pic (view, 224, 0, sb_ammo[0]);
	else if (cl.stats[STAT_ITEMS] & IT_NAILS)
		draw_pic (view, 224, 0, sb_ammo[1]);
	else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
		draw_pic (view, 224, 0, sb_ammo[2]);
	else if (cl.stats[STAT_ITEMS] & IT_CELLS)
		draw_pic (view, 224, 0, sb_ammo[3]);

	draw_num (view, 248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}

/*
	Sbar_DeathmatchOverlay

	ping time frags name
*/
static void
Sbar_DeathmatchOverlay (view_t *view, int start)
{
	int			l, y;
	int			skip = 10;

	if (r_data->vid->conview->xlen < 244) // FIXME: magic number, gained through experimentation
		return;

	if (largegame)
		skip = 8;

	// request new ping times every two second
	if (realtime - cl.last_ping_request > 2.0) {
		cl.last_ping_request = realtime;
		if (!cls.demoplayback) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "pings");
		}
	}

	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	if (!start) {
		draw_cachepic (view, 0, 0, "gfx/ranking.lmp", 1);
		y = 24;
	} else
		y = start;

	// scores
	Sbar_SortFrags (true);

	// draw the text
	l = scoreboardlines;

	// func ptr, avoids absurd if testing
	Sbar_Draw_DMO_func (view, l, y, skip);

	if (y >= view->ylen - 10)		// we ran over the screen size, squish
		largegame = true;
}

/*
	Sbar_TeamOverlay

	team frags
	added by Zoid
*/
static void
Sbar_TeamOverlay (view_t *view)
{
	char        num[20];
	int         pavg, plow, phigh, i, k, x, y;
	team_t     *tm;
	info_key_t *player_team = cl.players[cl.playernum].team;

	if (!cl.teamplay) { // FIXME: if not teamplay, why teamoverlay?
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
}

static void
draw_overlay (view_t *view)
{
	if (cls.state != ca_active
		|| !((cl.stats[STAT_HEALTH] <= 0 && !cl.spectator)
			 || sb_showscores || sb_showteamscores))
		return;
	// main screen deathmatch rankings
	// if we're dead show team scores in team games
	if (cl.stats[STAT_HEALTH] <= 0 && !cl.spectator)
		if (cl.teamplay > 0 && !sb_showscores)
			Sbar_TeamOverlay (view);
		else
			Sbar_DeathmatchOverlay (view, 0);
	else if (sb_showscores)
		Sbar_DeathmatchOverlay (view, 0);
	else if (sb_showteamscores)
		Sbar_TeamOverlay (view);
}

static void
sbar_update_vis (void)
{
	qboolean    headsup;

	if (r_data->scr_copyeverything)
		Sbar_Changed ();

	sbar_view->visible = 0;

	headsup = !(hud_sbar->int_val || r_data->scr_viewsize->int_val < 100);

	if ((sb_updates >= r_data->vid->numpages) && !headsup)
		return;

	if (con_module
		&& con_module->data->console->lines == r_data->vid->conview->ylen)
		return;							// console is full screen

	if (!sb_lines)
		return;

	sbar_view->visible = 1;

	r_data->scr_copyeverything = 1;
	sb_updates++;

	if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)
		sb_updates = 0;
}

void
Sbar_Draw (void)
{
	sbar_update_vis ();
	main_view->draw (main_view);
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
Sbar_LogFrags (void)
{
	char       *name;
	char       *team;
	byte       *cp = NULL;
	QFile      *file = NULL;
	int         minutes, fph, total, d, f, i, k, l, p;
	player_info_t *s = NULL;
	const char *t = NULL;
	time_t      tt = time (NULL);

	if (!cl_fraglog->int_val)
		return;

	if ((file = QFS_Open (fs_fraglog->string, "a")) == NULL)
		return;

	t = ctime (&tt);
	if (t)
		Qwrite (file, t, strlen (t));

	Qprintf (file, "%s\n%s %s\n", cls.servername->str,
			 cl_world.worldmodel->path, cl.levelname);

	// scores
	Sbar_SortFrags (true);

	// draw the text
	l = scoreboardlines;

	if (cl.teamplay) {
		// TODO: test if the teamplay does correct output
		Qwrite (file, "pl   fph time frags team name\n",
				strlen ("pl   fph time frags team name\n"));
	} else {
		Qwrite (file, "pl   fph time frags name\n",
				strlen ("pl   fph time frags name\n"));
	}

	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

 		// draw pl
		p = s->pl;
		(void) p; //FIXME

		// get time
		if (cl.intermission)
			total = cl.completed_time - s->entertime;
		else
			total = realtime - s->entertime;
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
			if (cl.teamplay) {
				team = malloc (strlen (s->team->value) + 1);
				for (cp = (byte *) s->team->value, d = 0; *cp; cp++, d++)
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
Sbar_Draw_DMO_Team_Ping (view_t *view, int l, int y, int skip)
{
	int			i, k, x;
	player_info_t *s;

	x = 4;
//							  0    40 64   104   152  192 224
	draw_string (view, x, y, "ping pl fph time frags team name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= view->ylen - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

		dmo_ping (view, x + 0, y, s);
		dmo_pl (view, x + 32, y, s);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			dmo_name (view, x + 224, y, s);
			y += skip;
			continue;
		}

		dmo_main (view, x + 64, y, s, k == cl.playernum);
		dmo_team (view, x + 184, y, s);
		dmo_name (view, x + 224, y, s);

		y += skip;
	}
}

static void
Sbar_Draw_DMO_Team_UID (view_t *view, int l, int y, int skip)
{
	int			i, k, x;
	player_info_t *s;

	x = 4;
//							  0    40 64  104  152   192
	draw_string (view, x, y, " uid pl fph time frags team name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= view->ylen - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

		dmo_uid (view, x + 0, y, s);
		dmo_ping (view, x + 32, y, s);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			dmo_name (view, x + 224, y, s);
			y += skip;
			continue;
		}

		dmo_main (view, x + 64, y, s, k == cl.playernum);
		dmo_team (view, x + 184, y, s);
		dmo_name (view, x + 224, y, s);

		y += skip;
	}
}

static void
Sbar_Draw_DMO_Team_Ping_UID (view_t *view, int l, int y, int skip)
{
	int			i, k, x;
	player_info_t *s;

	x = 4;
//							  0    40 64  104  152   192
	draw_string (view, x, y, "ping pl fph time frags team  uid name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= view->ylen - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

		dmo_ping (view, x + 0, y, s);
		dmo_pl (view, x + 32, y, s);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			dmo_uid (view, x + 224, y, s);
			dmo_name (view, x + 264, y, s);
			y += skip;
			continue;
		}

		dmo_main (view, x + 64, y, s, k == cl.playernum);
		dmo_team (view, x + 184, y, s);
		dmo_uid (view, x + 224, y, s);
		dmo_name (view, x + 264, y, s);

		y += skip;
	}
}

static void
Sbar_Draw_DMO_Ping (view_t *view, int l, int y, int skip)
{
	int			i, k, x;
	player_info_t *s;

	x = 16;
//							  0    40 64  104  152
	draw_string (view, x, y, "ping pl fph time frags name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e"
				 "\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= view->ylen - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

		dmo_ping (view, x + 0, y, s);
		dmo_pl (view, x + 32, y, s);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			dmo_name (view, x + 184, y, s);
			y += skip;
			continue;
		}

		dmo_main (view, x + 64, y, s, k == cl.playernum);
		dmo_name (view, x + 184, y, s);

		y += skip;
	}
}

static void
Sbar_Draw_DMO_UID (view_t *view, int l, int y, int skip)
{
	int			i, k, x;
	player_info_t *s;

	x = 16;
	draw_string (view, x, y, " uid pl fph time frags name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e"
				 "\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= view->ylen - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

		dmo_uid (view, x + 0, y, s);
		dmo_pl (view, x + 32, y, s);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			dmo_name (view, x + 184, y, s);
			y += skip;
			continue;
		}

		dmo_main (view, x + 64, y, s, k == cl.playernum);
		dmo_name (view, x + 184, y, s);

		y += skip;
	}
}

static void
Sbar_Draw_DMO_Ping_UID (view_t *view, int l, int y, int skip)
{
	int			i, k, x;
	player_info_t *s;

	x = 16;
	draw_string (view, x, y, "ping pl fph time frags  uid name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e"
				 "\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= view->ylen - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

		dmo_ping (view, x + 0, y, s);
		dmo_pl (view, x + 32, y, s);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			dmo_name (view, x + 184, y, s);
			y += skip;
			continue;
		}

		dmo_main (view, x + 64, y, s, k == cl.playernum);
		dmo_uid (view, x + 184, y, s);
		dmo_name (view, x + 224, y, s);

		y += skip;
	}
}

void
Sbar_DMO_Init_f (cvar_t *var)
{
	// "var" is "hud_scoreboard_uid"
	if (!var) {
		Sbar_Draw_DMO_func = Sbar_Draw_DMO_Ping;
		return;
	}

	if (cl.teamplay)
		if (var->int_val > 1)
			Sbar_Draw_DMO_func = Sbar_Draw_DMO_Team_Ping_UID;
		else if (var->int_val > 0)
			Sbar_Draw_DMO_func = Sbar_Draw_DMO_Team_UID;
		else
			Sbar_Draw_DMO_func = Sbar_Draw_DMO_Team_Ping;
	else
		if (var->int_val > 1)
			Sbar_Draw_DMO_func = Sbar_Draw_DMO_Ping_UID;
		else if (var->int_val > 0)
			Sbar_Draw_DMO_func = Sbar_Draw_DMO_UID;
		else
			Sbar_Draw_DMO_func = Sbar_Draw_DMO_Ping;
}

/*
	draw_minifrags

	frags name
	frags team name
	displayed to right of status bar if there's room
*/
static void
draw_minifrags (view_t *view)
{
	int         numlines, top, bottom, f, i, k, x, y;
	char        num[20];
	player_info_t *s;

	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	// scores
	Sbar_SortFrags (false);

	if (!scoreboardlines)
		return;							// no one there?

	numlines = view->ylen / 8;
	if (numlines < 3)
		return;							// not enough room

	// find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.playernum)
			break;

	if (i == scoreboardlines)			// we're not there, we are probably a
										// spectator, just display top
		i = 0;
	else								// figure out start
		i = i - numlines / 2;

	if (i > scoreboardlines - numlines)
		i = scoreboardlines - numlines;
	if (i < 0)
		i = 0;

	x = 4;
	y = 0;

	for (; i < scoreboardlines && y < view->ylen - 8 + 1; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name || !s->name->value[0])
			continue;

		// draw ping
		top = s->topcolor;
		bottom = s->bottomcolor;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		draw_fill (view, x + 2, y + 1, 37, 3, top);
		draw_fill (view, x + 2, y + 4, 37, 4, bottom);

		// draw number
		f = s->frags;
		if (k != cl.playernum) {
			snprintf (num, sizeof (num), " %3i ", f);
		} else {
			snprintf (num, sizeof (num), "\x10%3i\x11", f);
		}

		draw_nstring (view, x, y, num, 5);

		// team
		if (cl.teamplay) {
			draw_nstring (view, x + 48, y, s->team->value, 4);
			draw_nstring (view, x + 48 + 40, y, s->name->value, 16);
		} else
			draw_nstring (view, x + 48, y, s->name->value, 16);
		y += 8;
	}
}

static void
draw_miniteam (view_t *view)
{
	int         i, k, x, y;
	char        num[12];
	info_key_t *player_team = cl.players[cl.playernum].team;
	team_t     *tm;

	if (!cl.teamplay)
		return;
	Sbar_SortTeams ();

	x = 0;
	y = 0;
	for (i = 0; i < scoreboardteams && y <= view->ylen; i++) {
		k = teamsort[i];
		tm = teams + k;

		// draw pings
		draw_nstring (view, x, y, tm->team, 4);
		// draw total
		snprintf (num, sizeof (num), "%5i", tm->frags);
		draw_string (view, x + 40, y, num);

		if (player_team && strnequal (player_team->value, tm->team, 16)) {
			draw_character (view, x - 8, y, 16);
			draw_character (view, x + 32, y, 17);
		}

		y += 8;
	}
}

static void
draw_time (view_t *view)
{
	struct tm  *local = 0;
	time_t      utc = 0;
	char        st[80];

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
	if (hud_time->int_val == 1) {  // Use international format
		strftime (st, sizeof (st), HOUR24":%M", local);
		draw_string (view, 8, 0, st);
	} else if (hud_time->int_val >= 2) {   // US AM/PM display
		strftime (st, sizeof (st), HOUR12":%M "PM, local);
		draw_string (view, 8, 0, st);
	}
}

static void
draw_fps (view_t *view)
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
		snprintf (st, sizeof (st), "%5.1f FPS", lastfps);
	}
	draw_string (view, 80, 8, st);
}

static void
draw_net (view_t *view)
{
	// request new ping times every two seconds
	if (!cls.demoplayback && realtime - cl.last_ping_request > 2) {
		cl.last_ping_request = realtime;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "pings");
	}
	if (hud_ping->int_val) {
		int ping = cl.players[cl.playernum].ping;

		ping = bound (0, ping, 999);
		draw_string (view, 0, 0, va (0, "%3d ms ", ping));
	}

	if (hud_ping->int_val && hud_pl->int_val)
		draw_character (view, 48, 0, '/');

	if (hud_pl->int_val) {
		int lost = CL_CalcNet ();

		lost = bound (0, lost, 999);
		draw_string (view, 56, 0, va (0, "%3d pl", lost));
	}
}

static void
draw_stuff (view_t *view)
{
	if (hud_time->int_val > 0)
		draw_time (view);
	if (hud_fps->int_val > 0)
		draw_fps (view);
	if (cls.state == ca_active && (hud_ping->int_val || hud_pl->int_val))
		draw_net (view);
}

void
Sbar_IntermissionOverlay (void)
{
	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	if (cl.teamplay > 0 && !sb_showscores)
		Sbar_TeamOverlay (overlay_view);
	else
		Sbar_DeathmatchOverlay (overlay_view, 0);
}

/* CENTER PRINTING */
static dstring_t center_string = {&dstring_default_mem};
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
	if (!str) {
		centertime_off = 0;
		dstring_clearstr (&center_string);
		return;
	}

	dstring_copystr (&center_string, str);

	centertime_off = scr_centertime->value;
	centertime_start = realtime;

	// count the number of lines for centering
	center_lines = 1;
	while (*str) {
		if (*str == '\n')
			center_lines++;
		str++;
	}
}

static void
Sbar_DrawCenterString (view_t *view, int remaining)
{
	const char *start;
	int         j, l, x, y;

	start = center_string.str;

	if (center_lines <= 4)
		y = view->yabs + view->ylen * 0.35;
	else
		y = view->yabs + 48;

	do {
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = view->xabs + (view->xlen - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8) {
			r_funcs->Draw_Character (x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;
		if (!*start)
			break;
		start++;						// skip the \n
	} while (1);
}

void
Sbar_FinaleOverlay (void)
{
	int         remaining;

	r_data->scr_copyeverything = 1;

	draw_cachepic (overlay_view, 0, 16, "gfx/finale.lmp", 1);
	// the finale prints the characters one at a time
	remaining = scr_printspeed->value * (realtime - centertime_start);
	Sbar_DrawCenterString (overlay_view, remaining);
}

void
Sbar_DrawCenterPrint (void)
{
	r_data->scr_copytop = 1;

	centertime_off -= r_data->frametime;
	if (centertime_off <= 0)
		return;

	Sbar_DrawCenterString (overlay_view, -1);
}

static void
init_sbar_views (void)
{
	view_t     *view;
	view_t     *minifrags_view = 0;
	view_t     *miniteam_view = 0;

	if (r_data->vid->conview->xlen < 512) {
		sbar_view = view_new (0, 0, 320, 48, grav_south);

		sbar_frags_view = view_new (0, 0, 130, 8, grav_northeast);
		sbar_frags_view->draw = draw_frags;
	} else if (r_data->vid->conview->xlen < 640) {
		sbar_view = view_new (0, 0, 512, 48, grav_south);
		minifrags_view = view_new (320, 0, 192, 48, grav_southwest);
		minifrags_view->draw = draw_minifrags;
		minifrags_view->resize_y = 1;

		view = view_new (0, 0, 192, 48, grav_southeast);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);
	} else {
		sbar_view = view_new (0, 0, 640, 48, grav_south);
		minifrags_view = view_new (320, 0, 192, 48, grav_southwest);
		minifrags_view->draw = draw_minifrags;
		minifrags_view->resize_y = 1;
		miniteam_view = view_new (0, 0, 96, 48, grav_southeast);
		miniteam_view->draw = draw_miniteam;
		miniteam_view->resize_y = 1;

		view = view_new (0, 0, 320, 48, grav_southeast);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);
	}

	sbar_inventory_view = view_new (0, 0, 320, 24, grav_northwest);
	sbar_inventory_view->draw = draw_inventory_sbar;

	view = view_new (0, 0, 32, 16, grav_southwest);
	view->draw = draw_weapons_sbar;
	view_add (sbar_inventory_view, view);

	view = view_new (0, 0, 32, 8, grav_northwest);
	view->draw = draw_ammo_sbar;
	view_add (sbar_inventory_view, view);

	view = view_new (32, 0, 96, 16, grav_southeast);
	view->draw = draw_items;
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
	view->draw = draw_status;
	view_add (sbar_view, view);

	if (minifrags_view)
		view_add (sbar_view, minifrags_view);
	if (miniteam_view)
		view_add (sbar_view, miniteam_view);

	if (r_data->vid->conview->xlen > 640) {
		int         l = (r_data->vid->conview->xlen - 640) / 2;

		view = view_new (-l, 0, l, 48, grav_southwest);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);

		view = view_new (-l, 0, l, 48, grav_southeast);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);
	}
}

static void
init_hud_views (void)
{
	view_t     *view;
	view_t     *minifrags_view = 0;
	view_t     *miniteam_view = 0;

	if (r_data->vid->conview->xlen < 512) {
		hud_view = view_new (0, 0, 320, 48, grav_south);

		hud_frags_view = view_new (0, 0, 130, 8, grav_northeast);
		hud_frags_view->draw = draw_frags;
	} else if (r_data->vid->conview->xlen < 640) {
		hud_view = view_new (0, 0, 512, 48, grav_south);

		minifrags_view = view_new (320, 0, 192, 48, grav_southwest);
		minifrags_view->draw = draw_minifrags;
		minifrags_view->resize_y = 1;
	} else {
		hud_view = view_new (0, 0, 640, 48, grav_south);

		minifrags_view = view_new (320, 0, 192, 48, grav_southwest);
		minifrags_view->draw = draw_minifrags;
		minifrags_view->resize_y = 1;

		miniteam_view = view_new (0, 0, 96, 48, grav_southeast);
		miniteam_view->draw = draw_miniteam;
		miniteam_view->resize_y = 1;
	}
	hud_view->resize_y = 1;

	hud_armament_view = view_new (0, 48, 42, 156, grav_southeast);

	view = view_new (0, 0, 42, 44, grav_northeast);
	view->draw = draw_weapons_hud;
	view_add (hud_armament_view, view);

	view = view_new (0, 0, 42, 44, grav_southeast);
	view->draw = draw_ammo_hud;
	view_add (hud_armament_view, view);

	hud_inventory_view = view_new (0, 0, 320, 24, grav_northwest);
	view_add (hud_view, hud_inventory_view);

	view = view_new (0, 0, 320, 24, grav_southwest);
	view->draw = draw_status;
	view_add (hud_view, view);

	view = view_new (32, 0, 96, 16, grav_southeast);
	view->draw = draw_items;
	view_add (hud_inventory_view, view);

	view = view_new (0, 0, 32, 16, grav_southeast);
	view->draw = draw_sigils;
	view_add (hud_inventory_view, view);

	if (hud_frags_view)
		view_add (hud_inventory_view, hud_frags_view);

	if (minifrags_view)
		view_add (hud_view, minifrags_view);
	if (miniteam_view)
		view_add (hud_view, miniteam_view);

	view = view_new (0, 0, r_data->vid->conview->xlen, 48, grav_south);
	view_add (view, hud_view);
	hud_view = view;

	view_add (hud_view, hud_armament_view);

	view_insert (main_view, hud_view, 0);
}

static void
init_views (void)
{
	main_view = view_new (0, 0, r_data->vid->conview->xlen,
						  r_data->vid->conview->ylen,
						  grav_northwest);
	if (con_module)
		view_insert (con_module->data->console->view, main_view, 0);
	main_view->resize_x = 1;	// get resized if the 2d view resizes
	main_view->resize_y = 1;
	main_view->visible = 0;		// but don't let the console draw our stuff
	if (r_data->vid->conview->ylen > 300)
		overlay_view = view_new (0, 0, 320, 300, grav_center);
	else
		overlay_view = view_new (0, 0, 320, r_data->vid->conview->ylen,
								 grav_center);
	overlay_view->draw = draw_overlay;
	overlay_view->visible = 0;

	stuff_view = view_new (0, 48, 152, 16, grav_southwest);
	stuff_view->draw = draw_stuff;

	view_insert (main_view, overlay_view, 0);
	view_insert (main_view, stuff_view, 0);

	init_sbar_views ();
	init_hud_views ();
}

static void
Sbar_GIB_Print_Center_f (void)
{
	if (GIB_Argc () != 2) {
		GIB_USAGE ("text");
	} else
		Sbar_CenterPrint (GIB_Argv(1));
}

void
Sbar_Init (void)
{
	int         i;

	init_views ();

	for (i = 0; i < 10; i++) {
		sb_nums[0][i] = r_funcs->Draw_PicFromWad (va (0, "num_%i", i));
		sb_nums[1][i] = r_funcs->Draw_PicFromWad (va (0, "anum_%i", i));
	}

	sb_nums[0][10] = r_funcs->Draw_PicFromWad ("num_minus");
	sb_nums[1][10] = r_funcs->Draw_PicFromWad ("anum_minus");

	sb_colon = r_funcs->Draw_PicFromWad ("num_colon");
	sb_slash = r_funcs->Draw_PicFromWad ("num_slash");

	sb_weapons[0][0] = r_funcs->Draw_PicFromWad ("inv_shotgun");
	sb_weapons[0][1] = r_funcs->Draw_PicFromWad ("inv_sshotgun");
	sb_weapons[0][2] = r_funcs->Draw_PicFromWad ("inv_nailgun");
	sb_weapons[0][3] = r_funcs->Draw_PicFromWad ("inv_snailgun");
	sb_weapons[0][4] = r_funcs->Draw_PicFromWad ("inv_rlaunch");
	sb_weapons[0][5] = r_funcs->Draw_PicFromWad ("inv_srlaunch");
	sb_weapons[0][6] = r_funcs->Draw_PicFromWad ("inv_lightng");

	sb_weapons[1][0] = r_funcs->Draw_PicFromWad ("inv2_shotgun");
	sb_weapons[1][1] = r_funcs->Draw_PicFromWad ("inv2_sshotgun");
	sb_weapons[1][2] = r_funcs->Draw_PicFromWad ("inv2_nailgun");
	sb_weapons[1][3] = r_funcs->Draw_PicFromWad ("inv2_snailgun");
	sb_weapons[1][4] = r_funcs->Draw_PicFromWad ("inv2_rlaunch");
	sb_weapons[1][5] = r_funcs->Draw_PicFromWad ("inv2_srlaunch");
	sb_weapons[1][6] = r_funcs->Draw_PicFromWad ("inv2_lightng");

	for (i = 0; i < 5; i++) {
		sb_weapons[2 + i][0] = r_funcs->Draw_PicFromWad (va (0,
															 "inva%i_shotgun",
															 i + 1));
		sb_weapons[2 + i][1] = r_funcs->Draw_PicFromWad (va (0,
															 "inva%i_sshotgun",
															 i + 1));
		sb_weapons[2 + i][2] = r_funcs->Draw_PicFromWad (va (0,
															 "inva%i_nailgun",
															 i + 1));
		sb_weapons[2 + i][3] = r_funcs->Draw_PicFromWad (va (0,
															 "inva%i_snailgun",
															 i + 1));
		sb_weapons[2 + i][4] = r_funcs->Draw_PicFromWad (va (0,
															 "inva%i_rlaunch",
															 i + 1));
		sb_weapons[2 + i][5] = r_funcs->Draw_PicFromWad (va (0,
															 "inva%i_srlaunch",
															 i + 1));
		sb_weapons[2 + i][6] = r_funcs->Draw_PicFromWad (va (0,
															 "inva%i_lightng",
															 i + 1));
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

	Cmd_AddCommand ("+showscores", Sbar_ShowScores,
					"Display information on everyone playing");
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores,
					"Stop displaying information on everyone playing");
	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores,
					"Display information for your team");
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores,
					"Stop displaying information for your team");

	sb_sbar = r_funcs->Draw_PicFromWad ("sbar");
	sb_ibar = r_funcs->Draw_PicFromWad ("ibar");
	sb_scorebar = r_funcs->Draw_PicFromWad ("scorebar");

	r_data->viewsize_callback = viewsize_f;

	hud_scoreboard_uid = Cvar_Get ("hud_scoreboard_uid", "0", CVAR_NONE,
								 Sbar_DMO_Init_f, "Set to 1 to show uid "
								 "instead of ping. Set to 2 to show both.");
	fs_fraglog = Cvar_Get ("fs_fraglog", "qw-scores.log", CVAR_ARCHIVE, NULL,
						   "Filename of the automatic frag-log.");
	cl_fraglog = Cvar_Get ("cl_fraglog", "0", CVAR_ARCHIVE, NULL,
						   "Automatic fraglogging, non-zero value will switch "
						   "it on.");
	hud_sbar = Cvar_Get ("hud_sbar", "0", CVAR_ARCHIVE, hud_sbar_f,
						 "status bar mode: 0 = hud, 1 = oldstyle");
	hud_swap = Cvar_Get ("hud_swap", "0", CVAR_ARCHIVE, hud_swap_f,
						 "new HUD on left side?");
	hud_scoreboard_gravity = Cvar_Get ("hud_scoreboard_gravity", "center",
									   CVAR_ARCHIVE, hud_scoreboard_gravity_f,
									   "control placement of scoreboard "
									   "overlay: center, northwest, north, "
									   "northeast, west, east, southwest, "
									   "south, southeast");
	scr_centertime = Cvar_Get ("scr_centertime", "2", CVAR_NONE, NULL, "How "
							   "long in seconds screen hints are displayed");
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", CVAR_NONE, NULL,
							   "How fast the text is displayed at the end of "
							   "the single player episodes");

	// register GIB builtins
	GIB_Builtin_Add ("print::center", Sbar_GIB_Print_Center_f);
}
