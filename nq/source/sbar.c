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
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/view.h"
#include "QF/wad.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "compat.h"
#include "sbar.h"

#include "nq/include/client.h"
#include "nq/include/game.h"
#include "nq/include/server.h"

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

int         sb_lines;				// scan lines to draw
qboolean	hudswap;

qpic_t     *rsb_invbar[2];
qpic_t     *rsb_weapons[5];
qpic_t     *rsb_items[2];
qpic_t     *rsb_ammo[3];
qpic_t     *rsb_teambord;			// PGM 01/19/97 - team color border

								// MED 01/04/97 added two more weapons + 3
								// alternates for grenade launcher
qpic_t     *hsb_weapons[7][5];	// 0 is active, 1 is owned, 2-5 are flashes

								// MED 01/04/97 added array to simplify
								// weapon parsing
int         hipweapons[4] =
	{ HIT_LASER_CANNON_BIT, HIT_MJOLNIR_BIT, 4, HIT_PROXIMITY_GUN_BIT };
qpic_t     *hsb_items[2];			// MED 01/04/97 added hipnotic items array

cvar_t     *hud_sbar;
cvar_t     *hud_swap;
cvar_t     *hud_scoreboard_gravity;
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

static void
hud_swap_f (cvar_t *var)
{
	hudswap = var->int_val;
	if (var->int_val) {
		hud_armament_view->gravity = grav_southwest;
		hud_armament_view->children[0]->gravity = grav_northwest;
		hud_armament_view->children[1]->gravity = grav_southeast;
		stuff_view->gravity = grav_southeast;
	} else {
		hud_armament_view->gravity = grav_southeast;
		hud_armament_view->children[0]->gravity = grav_northeast;
		hud_armament_view->children[1]->gravity = grav_southwest;
		stuff_view->gravity = grav_southwest;
	}
	view_move (hud_armament_view, hud_armament_view->xpos,
			   hud_armament_view->ypos);
	view_move (stuff_view, stuff_view->xpos, stuff_view->ypos);
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
	r_data->vid->recalc_refdef = true;
	if (r_data->scr_viewsize)
		calc_sb_lines (r_data->scr_viewsize);
	r_data->lineadj = var->int_val ? sb_lines : 0;
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
	if (hud_sbar)
		r_data->lineadj = hud_sbar->int_val ? sb_lines : 0;
}


static int
Sbar_ColorForMap (int m)
{
	return (bound (0, m, 13) * 16) + 8;
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

static inline void
draw_altstring (view_t *view, int x, int y, const char *str)
{
	r_funcs->Draw_AltString (view->xabs + x, view->yabs + y, str);
}

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

	for (i = r_data->vid->conheight < 204; i < 7; i++) {
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
	draw_pic (view, 0, 0, sb_ibar);
	view_draw (view);
}


int         fragsort[MAX_SCOREBOARD];
char        scoreboardtext[MAX_SCOREBOARD][20];
int         scoreboardtop[MAX_SCOREBOARD];
int         scoreboardbottom[MAX_SCOREBOARD];
int         scoreboardcount[MAX_SCOREBOARD];
int         scoreboardlines;


static void
Sbar_SortFrags (void)
{
	int         i, j, k;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < cl.maxclients; i++) {
		if (cl.players[i].name->value[0]) {
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i = 0; i < scoreboardlines; i++) {
		for (j = 0; j < (scoreboardlines - 1 - i); j++) {
			if (cl.players[fragsort[j]].frags
				< cl.players[fragsort[j + 1]].frags) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}


static void
draw_solo (view_t *view)
{
	char        str[80];
	int         minutes, seconds, tens, units;
	int         l;

	draw_pic (view, 0, 0, sb_scorebar);

	snprintf (str, sizeof (str), "Monsters:%3i /%3i", cl.stats[STAT_MONSTERS],
			  cl.stats[STAT_TOTALMONSTERS]);
	draw_string (view, 8, 4, str);

	snprintf (str, sizeof (str), "Secrets :%3i /%3i", cl.stats[STAT_SECRETS],
			  cl.stats[STAT_TOTALSECRETS]);
	draw_string (view, 8, 12, str);

	// time
	minutes = cl.time / 60;
	seconds = cl.time - (60 * minutes);
	tens = seconds / 10;
	units = seconds - (10 * tens);
	snprintf (str, sizeof (str), "Time :%3i:%i%i", minutes, tens, units);
	draw_string (view, 184, 4, str);

	// draw level name
	l = strlen (cl.levelname);
	l = 232 - l * 4;
	draw_string (view, max (l, 152), 12, cl.levelname);
}

static void
draw_frags (view_t *view)
{
	int         i, k, l, p = -1;
	int         top, bottom;
	int         x;
	player_info_t *s;

	if (cl.maxclients == 1)
		return;

	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 0;

	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name->value[0])
			continue;

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);

		draw_fill (view, x + 4, 1, 28, 4, top);
		draw_fill (view, x + 4, 5, 28, 3, bottom);

		draw_smallnum (view, x + 6, 0, s->frags, 0, 0);

		if (k == cl.viewentity - 1)
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
draw_status_bar (view_t *view)
{
	draw_pic (view, 0, 0, sb_sbar);
}

static inline void
draw_armor (view_t *view)
{
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
}

static inline void
draw_health (view_t *view)
{
	draw_num (view, 136, 0, cl.stats[STAT_HEALTH], 3,
			  cl.stats[STAT_HEALTH] <= 25);
}

static inline void
draw_ammo (view_t *view)
{
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

static void
draw_status (view_t *view)
{
	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
		draw_solo (view);
		return;
	}

	draw_armor (view);
	draw_face (view);
	draw_health (view);
	draw_ammo (view);
}

static void
draw_rogue_weapons_sbar (view_t *view)
{
	int         i;

	draw_weapons_sbar (view);

	// check for powered up weapon.
	if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN) {
		for (i = 0; i < 5; i++) {
			if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i)) {
				draw_pic (view, (i + 2) * 24, 0, rsb_weapons[i]);
			}
		}
	}
}

static void
draw_rogue_weapons_hud (view_t *view)
{
	int         flashon, i, j;
	qpic_t     *pic;

	for (i = r_data->vid->conheight < 204; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = calc_flashon (cl.item_gettime[i], IT_SHOTGUN << i);
			if (i >= 2) {
				j = i - 2;
				if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << j))
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
}

static void
draw_rogue_ammo_hud (view_t *view)
{
	int         i, count;
	qpic_t     *pic;

	if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
		pic = rsb_invbar[0];
	else
		pic = rsb_invbar[1];

	for (i = 0; i < 4; i++) {
		count = cl.stats[STAT_SHELLS + i];
		draw_subpic (view, 0, i * 11, pic, 3 + (i * 48), 0, 42, 11);
		draw_smallnum (view, 7, i * 11, count, 0, 1);
	}
}

static void
draw_rogue_items (view_t *view)
{
	int         i;
	float       time;

	draw_items (view);

	for (i = 0; i < 2; i++) {
		if (cl.stats[STAT_ITEMS] & (1 << (29 + i))) {
			time = cl.item_gettime[29 + i];
			draw_pic (view, 96 + i * 16, 0, rsb_items[i]);
			if (time && time > (cl.time - 2))
				sb_updates = 0;
		}
	}
}

static void
draw_rogue_inventory_sbar (view_t *view)
{
	if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
		draw_pic (view, 0, 0, rsb_invbar[0]);
	else
		draw_pic (view, 0, 0, rsb_invbar[1]);

	view_draw (view);
}

static void
draw_rogue_face (view_t *view)
{
	int         top, bottom;
	player_info_t *s;

	// PGM 01/19/97 - team color drawing

	s = &cl.players[cl.viewentity - 1];

	top = Sbar_ColorForMap (s->topcolor);
	bottom = Sbar_ColorForMap (s->bottomcolor);

	draw_pic (view, 112, 0, rsb_teambord);
	draw_fill (view, 113, 3, 22, 9, top);
	draw_fill (view, 113, 12, 22, 9, bottom);

	draw_smallnum (view, 108, 3, s->frags, 1, top == 8);
}

static void
draw_rogue_status (view_t *view)
{
	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
		draw_solo (view);
		return;
	}

	draw_num (view, 24, 0, cl.stats[STAT_ARMOR], 3,
			  cl.stats[STAT_ARMOR] <= 25);
	if (cl.stats[STAT_ITEMS] & RIT_ARMOR3)
		draw_pic (view, 0, 0, sb_armor[2]);
	else if (cl.stats[STAT_ITEMS] & RIT_ARMOR2)
		draw_pic (view, 0, 0, sb_armor[1]);
	else if (cl.stats[STAT_ITEMS] & RIT_ARMOR1)
		draw_pic (view, 0, 0, sb_armor[0]);

	// PGM 03/02/97 - fixed so color swatch appears in only CTF modes
	if (cl.maxclients != 1 && teamplay->int_val > 3 && teamplay->int_val < 7)
		draw_rogue_face (view);
	else
		draw_face (view);

	draw_health (view);

	if (cl.stats[STAT_ITEMS] & RIT_SHELLS)
		draw_pic (view, 224, 0, sb_ammo[0]);
	else if (cl.stats[STAT_ITEMS] & RIT_NAILS)
		draw_pic (view, 224, 0, sb_ammo[1]);
	else if (cl.stats[STAT_ITEMS] & RIT_ROCKETS)
		draw_pic (view, 224, 0, sb_ammo[2]);
	else if (cl.stats[STAT_ITEMS] & RIT_CELLS)
		draw_pic (view, 224, 0, sb_ammo[3]);
	else if (cl.stats[STAT_ITEMS] & RIT_LAVA_NAILS)
		draw_pic (view, 224, 0, rsb_ammo[0]);
	else if (cl.stats[STAT_ITEMS] & RIT_PLASMA_AMMO)
		draw_pic (view, 224, 0, rsb_ammo[1]);
	else if (cl.stats[STAT_ITEMS] & RIT_MULTI_ROCKETS)
		draw_pic (view, 224, 0, rsb_ammo[2]);
	draw_num (view, 248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}

static void
draw_hipnotic_weapons_sbar (view_t *view)
{
	static int  x[] = {0, 24, 48, 72, 96, 120, 144, 176, 200, 96};
	static int  h[] = {0, 1, 3};
	int         flashon, i;
	qpic_t     *pic;
	int         mask;
	float       time;

	for (i = 0; i < 10; i++) {
		if (i < 7) {
			mask = IT_SHOTGUN << i;
			time = cl.item_gettime[i];
		} else {
			mask = 1 << hipweapons[h[i - 7]];
			time = cl.item_gettime[hipweapons[h[i - 7]]];
		}
		if (cl.stats[STAT_ITEMS] & mask) {
			flashon = calc_flashon (time, mask);

			if (i == 4 && cl.stats[STAT_ACTIVEWEAPON] == (1 << hipweapons[3]))
				continue;
			if (i == 9 && cl.stats[STAT_ACTIVEWEAPON] != (1 << hipweapons[3]))
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
}

static void
draw_hipnotic_weapons_hud (view_t *view)
{
	int         flashon, i;
	static int  y[] = {0, 16, 32, 48, 64, 96, 112, 128, 144, 80};
	static int  h[] = {0, 1, 3};
	qpic_t     *pic;
	int         mask;
	float       time;

	for (i = 0; i < 10; i++) {
		if (i < 7) {
			mask = IT_SHOTGUN << i;
			time = cl.item_gettime[i];
		} else {
			mask = 1 << hipweapons[h[i - 7]];
			time = cl.item_gettime[hipweapons[h[i - 7]]];
		}
		if (cl.stats[STAT_ITEMS] & mask) {
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
}

static void
draw_hipnotic_items (view_t *view)
{
	int         i;
	float       time;

	// items
	for (i = 2; i < 6; i++) {
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
			time = cl.item_gettime[17 + i];
			draw_pic (view, 192 + i * 16, 0, sb_items[i]);
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
	}

	// hipnotic items
	for (i = 0; i < 2; i++) {
		if (cl.stats[STAT_ITEMS] & (1 << (24 + i))) {
			time = cl.item_gettime[24 + i];
			draw_pic (view, 288 + i * 16, 0, hsb_items[i]);
			if (time && time > (cl.time - 2))
				sb_updates = 0;
		}
	}
}

static void
draw_hipnotic_inventory_sbar (view_t *view)
{
	draw_pic (view, 0, 0, sb_ibar);
	view_draw (view);
}

static void
draw_hipnotic_status (view_t *view)
{
	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
		draw_solo (view);
		return;
	}

	draw_armor (view);
	draw_face (view);
	draw_health (view);
	draw_ammo (view);

	if (cl.stats[STAT_ITEMS] & IT_KEY1)
		draw_pic (view, 209, 3, sb_items[0]);
	if (cl.stats[STAT_ITEMS] & IT_KEY2)
		draw_pic (view, 209, 12, sb_items[1]);
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

	if (con_module &&
		con_module->data->console->lines == r_data->vid->conheight)
		return;							// console is full screen

	if (cls.state == ca_active
		&& ((cl.stats[STAT_HEALTH] <= 0) || sb_showscores))
		overlay_view->visible = 1;
	else
		overlay_view->visible = 0;

	if (!sb_lines)
		return;

	sbar_view->visible = 1;

	r_data->scr_copyeverything = 1;
	sb_updates++;
}

void
Sbar_Draw (void)
{
	sbar_update_vis ();
	main_view->draw (main_view);
}

static void
Sbar_DeathmatchOverlay (view_t *view)
{
	int         i, k, l;
	int         top, bottom;
	int         x, y;
	player_info_t *s;

	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	draw_cachepic (view, 0, 0, "gfx/ranking.lmp", 1);

	// scores
	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines;

	x = 80;
	y = 40;
	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name->value[0])
			continue;

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);

		draw_fill (view, x, y, 40, 4, top);
		draw_fill (view, x, y + 4, 40, 4, bottom);

		draw_smallnum (view, x + 12, y, s->frags, 0, 0);

		if (k == cl.viewentity - 1)
			draw_character (view, x - 4, y, 12);

		// draw name
		draw_string (view, x + 64, y, s->name->value);

		y += 10;
	}
}

static void
Sbar_DrawScoreboard (void)
{
	//Sbar_SoloScoreboard ();
	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay (overlay_view);
}

static void
draw_overlay (view_t *view)
{
	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
		Sbar_DrawScoreboard ();
	}
}

static void
draw_time (view_t *view)
{
	struct tm  *local = NULL;
	time_t      utc = 0;
	const char *timefmt = NULL;
	char        st[80];		//FIXME: overflow

	// Get local time
	utc = time (NULL);
	local = localtime (&utc);

	if (hud_time->int_val == 1) {  // Use international format
		timefmt = "%k:%M";
	} else if (hud_time->int_val >= 2) {   // US AM/PM display
		timefmt = "%l:%M %P";
	}

	strftime (st, sizeof (st), timefmt, local);
	draw_string (view, 8, 0, st);
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
draw_stuff (view_t *view)
{
	if (hud_time->int_val > 0)
		draw_time (view);
	if (hud_fps->int_val > 0)
		draw_fps (view);
}

static void
draw_intermission (view_t *view)
{
	int         dig;
	int         num;

	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	draw_cachepic (view, 64, 24, "gfx/complete.lmp", 0);

	draw_cachepic (view, 0, 56, "gfx/inter.lmp", 0);

	// time
	dig = cl.completed_time / 60;
	draw_num (view, 160, 64, dig, 3, 0);
	num = cl.completed_time - dig * 60;
	draw_pic (view, 234, 64, sb_colon);
	draw_pic (view, 246, 64, sb_nums[0][num / 10]);
	draw_pic (view, 266, 64, sb_nums[0][num % 10]);

	draw_num (view, 160, 104, cl.stats[STAT_SECRETS], 3, 0);
	draw_pic (view, 232, 104, sb_slash);
	draw_num (view, 240, 104, cl.stats[STAT_TOTALSECRETS], 3, 0);

	draw_num (view, 160, 144, cl.stats[STAT_MONSTERS], 3, 0);
	draw_pic (view, 232, 144, sb_slash);
	draw_num (view, 240, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0);
}

void
Sbar_IntermissionOverlay (void)
{
	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH) {
		Sbar_DeathmatchOverlay (overlay_view);
		return;
	}
	draw_intermission (overlay_view);
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

	sbar_view = view_new (0, 0, 320, 48, grav_south);

	sbar_frags_view = view_new (0, 0, 130, 8, grav_northeast);
	sbar_frags_view->draw = draw_frags;

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

	if (r_data->vid->conwidth > 320) {
		int         l = (r_data->vid->conwidth - 320) / 2;

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

	hud_view = view_new (0, 0, 320, 48, grav_south);
	hud_frags_view = view_new (0, 0, 130, 8, grav_northeast);
	hud_frags_view->draw = draw_frags;

	hud_view->resize_y = 1;

	hud_armament_view = view_new (0, 48, 42, 156, grav_southeast);

	view = view_new (0, 0, 24, 112, grav_northeast);
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

	view = view_new (0, 0, r_data->vid->conwidth, 48, grav_south);
	view_add (view, hud_view);
	hud_view = view;

	view_add (hud_view, hud_armament_view);

	view_insert (main_view, hud_view, 0);
}

static void
init_hipnotic_sbar_views (void)
{
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

	if (r_data->vid->conwidth > 320) {
		int         l = (r_data->vid->conwidth - 320) / 2;

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
init_hipnotic_hud_views (void)
{
	view_t     *view;

	hud_view = view_new (0, 0, 320, 48, grav_south);
	hud_frags_view = view_new (0, 0, 130, 8, grav_northeast);
	hud_frags_view->draw = draw_frags;

	hud_view->resize_y = 1;

	if (r_data->vid->conheight < 252) {
		hud_armament_view = view_new (0,
									  min (r_data->vid->conheight - 160, 48),
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

	view = view_new (0, 0, r_data->vid->conwidth, 48, grav_south);
	view_add (view, hud_view);
	hud_view = view;

	view_add (hud_view, hud_armament_view);

	view_insert (main_view, hud_view, 0);
}

static void
init_rogue_sbar_views (void)
{
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

	if (r_data->vid->conwidth > 320) {
		int         l = (r_data->vid->conwidth - 320) / 2;

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
init_rogue_hud_views (void)
{
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

	view = view_new (0, 0, r_data->vid->conwidth, 48, grav_south);
	view_add (view, hud_view);
	hud_view = view;

	view_add (hud_view, hud_armament_view);

	view_insert (main_view, hud_view, 0);
}

static void
init_views (void)
{
	main_view = view_new (0, 0, r_data->vid->conwidth, r_data->vid->conheight,
						  grav_northwest);
	if (con_module)
		view_insert (con_module->data->console->view, main_view, 0);
	main_view->resize_x = 1;	// get resized if the 2d view resizes
	main_view->resize_y = 1;
	main_view->visible = 0;		// but don't let the console draw our stuff
	if (r_data->vid->conheight > 300)
		overlay_view = view_new (0, 0, 320, 300, grav_center);
	else
		overlay_view = view_new (0, 0, 320, r_data->vid->conheight,
								 grav_center);
	overlay_view->draw = draw_overlay;
	overlay_view->visible = 0;

	stuff_view = view_new (0, 48, 152, 16, grav_southwest);
	stuff_view->draw = draw_stuff;

	view_insert (main_view, overlay_view, 0);
	view_insert (main_view, stuff_view, 0);

	if (!strcmp (qfs_gamedir->hudtype, "hipnotic")) {
		init_hipnotic_sbar_views ();
		init_hipnotic_hud_views ();
	} else if (!strcmp (qfs_gamedir->hudtype, "rogue")) {
		init_rogue_sbar_views ();
		init_rogue_hud_views ();
	} else {
		init_sbar_views ();
		init_hud_views ();
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
sbar_keydest_callback (keydest_t kd)
{
	overlay_view->visible = kd == key_game;
}

void
Sbar_Init (void)
{
	int         i;

	init_views ();

	Key_KeydestCallback (sbar_keydest_callback);

	for (i = 0; i < 10; i++) {
		sb_nums[0][i] = r_funcs->Draw_PicFromWad (va ("num_%i", i));
		sb_nums[1][i] = r_funcs->Draw_PicFromWad (va ("anum_%i", i));
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
		sb_weapons[2 + i][0] =
			r_funcs->Draw_PicFromWad (va ("inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1] =
			r_funcs->Draw_PicFromWad (va ("inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2] =
			r_funcs->Draw_PicFromWad (va ("inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3] =
			r_funcs->Draw_PicFromWad (va ("inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4] =
			r_funcs->Draw_PicFromWad (va ("inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5] =
			r_funcs->Draw_PicFromWad (va ("inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6] =
			r_funcs->Draw_PicFromWad (va ("inva%i_lightng", i + 1));
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

		for (i = 0; i < 5; i++) {
			hsb_weapons[2 + i][0] =
				r_funcs->Draw_PicFromWad (va ("inva%i_laser", i + 1));
			hsb_weapons[2 + i][1] =
				r_funcs->Draw_PicFromWad (va ("inva%i_mjolnir", i + 1));
			hsb_weapons[2 + i][2] =
				r_funcs->Draw_PicFromWad (va ("inva%i_gren_prox", i + 1));
			hsb_weapons[2 + i][3] =
				r_funcs->Draw_PicFromWad (va ("inva%i_prox_gren", i + 1));
			hsb_weapons[2 + i][4] =
				r_funcs->Draw_PicFromWad (va ("inva%i_prox", i + 1));
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

	r_data->viewsize_callback = viewsize_f;
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
