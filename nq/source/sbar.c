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
#include "QF/wad.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "QF/ui/view.h"

#include "compat.h"
#include "sbar.h"

#include "client/hud.h"

#include "nq/include/client.h"
#include "nq/include/game.h"
#include "nq/include/server.h"

int         sb_updates;				// if >= vid.numpages, no update needed
static int sb_update_flags;
static int sb_view_size;

static view_t sbar_main;
static view_t   sbar_inventory;
static view_t     sbar_frags;
static view_t     sbar_sigils;
static view_t     sbar_items;
static view_t     sbar_armament;
static view_t       sbar_weapons;
static view_t       sbar_miniammo;
static view_t   sbar_statusbar;
static view_t     sbar_armor;
static view_t     sbar_face;
static view_t     sbar_health;
static view_t     sbar_ammo;
//static view_t   sbar_status;
static view_t sbar_tile[2];

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
sbar_setcomponent (view_t view, uint32_t comp, void *data)
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
Sbar_Changed (sbar_changed change)
{
	sb_update_flags |= 1 << change;
	sb_updates = 0;						// update next frame
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
draw_smallnum (view_t view, int x, int y, int n, int packed, int colored)
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
		count = cl.stats[STAT_SHELLS + i];
		draw_smallnum (v, (6 * i + 1) * 8 + 2, 0, count, 0, 1);
	}
}

static void
draw_ammo (view_t view)
{
	qpic_t     *pic = 0;
	if (cl.stats[STAT_ITEMS] & IT_SHELLS)
		pic = sb_ammo[0];
	else if (cl.stats[STAT_ITEMS] & IT_NAILS)
		pic = sb_ammo[1];
	else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
		pic = sb_ammo[2];
	else if (cl.stats[STAT_ITEMS] & IT_CELLS)
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
	draw_num (num, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
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
draw_weapons_sbar (view_t view)
{
	int         flashon, i;

	for (i = 0; i < 7; i++) {
		view_t      weap = View_GetChild (view, i);
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = calc_flashon (cl.item_gettime[i], IT_SHOTGUN << i);
			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
			sbar_setcomponent (weap, hud_pic, &sb_weapons[flashon][i]);
		} else {
			sbar_remcomponent (weap, hud_pic);
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
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
#if 0
			time = cl.item_gettime[17 + i];
			if (time && time > cl.time - 2 && flashon) {	// Flash frame
				sb_updates = 0;
			} else {
				draw_pic (view, i * 16, 0, sb_items[i]);
			}
			if (time && time > cl.time - 2)
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
		if (cl.stats[STAT_ITEMS] & (1 << (28 + i))) {
#if 0
			time = cl.item_gettime[28 + i];
			if (time && time > cl.time - 2 && flashon) {	// flash frame
				sb_updates = 0;
			} else {
				draw_pic (view, i * 8, 0, sb_sigil[i]);
			}
			if (time && time > cl.time - 2)
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

team_t      teams[MAX_CLIENTS];
int         teamsort[MAX_CLIENTS];
int         fragsort[MAX_SCOREBOARD];
char        scoreboardtext[MAX_SCOREBOARD][20];
int         scoreboardtop[MAX_SCOREBOARD];
int         scoreboardbottom[MAX_SCOREBOARD];
int         scoreboardcount[MAX_SCOREBOARD];
int         scoreboardlines, scoreboardteams;


static void __attribute__((used))
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
		for (j = 0; j < scoreboardlines - 1 - i; j++) {
			if (cl.players[fragsort[j]].frags
				< cl.players[fragsort[j + 1]].frags) {
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

static void __attribute__((used))
draw_solo (view_t *view)
{
#if 0
	char        str[80];
	int         minutes, seconds;
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
	seconds = cl.time - 60 * minutes;
	snprintf (str, sizeof (str), "Time :%3i:%02i", minutes, seconds);
	draw_string (view, 184, 4, str);

	// draw level name
	l = strlen (cl.levelname);
	l = 232 - l * 4;
	draw_string (view, max (l, 152), 12, cl.levelname);
#endif
}

static void
draw_frags (view_t view)
{
#if 0
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
		if (!s->name || !s->name->value[0])
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
#endif
}

static void
draw_face (view_t view)
{
#if 0
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
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		draw_num (num, 666, 3, 1);
	} else {
		draw_num (num, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
		if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
			sbar_setcomponent (armor, hud_pic, &sb_armor[2]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
			sbar_setcomponent (armor, hud_pic, &sb_armor[1]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
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
	draw_num (num, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
}
#if 0
static void
draw_status (view_t *view)
{
	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
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
	if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN) {
		for (i = 0; i < 5; i++) {
			if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i)) {
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
#endif
}

static void __attribute__((used))
draw_rogue_ammo_hud (view_t *view)
{
#if 0
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
		if (cl.stats[STAT_ITEMS] & (1 << (29 + i))) {
			time = cl.item_gettime[29 + i];
			draw_pic (view, 96 + i * 16, 0, rsb_items[i]);
			if (time && time > (cl.time - 2))
				sb_updates = 0;
		}
	}
#endif
}

static void __attribute__((used))
draw_rogue_inventory_sbar (view_t *view)
{
#if 0
	if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
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

	s = &cl.players[cl.viewentity - 1];

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
	if (cl.maxclients != 1 && teamplay > 3 && teamplay < 7)
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
#endif
}

static void
sbar_update_vis (void)
{
#if 0
	qboolean    headsup;

	if (r_data->scr_copyeverything)
		Sbar_Changed ();

	sbar_view->visible = 0;

	headsup = !(hud_sbar || sb_view_size < 100);

	if ((sb_updates >= r_data->vid->numpages) && !headsup)
		return;

	if (con_module &&
		con_module->data->console->lines == cl_screen_view->ylen)
		return;							// console is full screen

	if (cls.state == ca_active
		&& ((cl.stats[STAT_HEALTH] <= 0) || sb_showscores))
		hud_overlay_view->visible = 1;
	else
		hud_overlay_view->visible = 0;

	if (!sb_lines)
		return;

	sbar_view->visible = 1;

	r_data->scr_copyeverything = 1;
	sb_updates++;
#endif
}

static void
Sbar_DeathmatchOverlay (view_t *view)
{
#if 0
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
	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
		Sbar_DrawScoreboard ();
	}
}

static void
draw_time (view_t *view)
{
#if 0
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
		draw_string (view, 8, 0, st);
	} else if (hud_time >= 2) {   // US AM/PM display
		strftime (st, sizeof (st), HOUR12":%M "PM, local);
		draw_string (view, 8, 0, st);
	}
#endif
}

static void
draw_fps (view_t *view)
{
#if 0
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
#endif
}

static void
draw_stuff (view_t *view)
{
	if (hud_time > 0)
		draw_time (view);
	if (hud_fps > 0)
		draw_fps (view);
}

static void
draw_intermission (view_t *view)
{
#if 0
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
#endif
}

void
Sbar_IntermissionOverlay (void)
{
#if 0
	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH) {
		Sbar_DeathmatchOverlay (hud_overlay_view);
		return;
	}
	draw_intermission (hud_overlay_view);
#endif
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

	centertime_off = scr_centertime;
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
Sbar_DrawCenterString (view_t view, int remaining)
{
#if 0
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
#endif
}

void
Sbar_FinaleOverlay (void)
{
#if 0
	int         remaining;

	r_data->scr_copyeverything = 1;

	draw_cachepic (hud_overlay_view, 0, 16, "gfx/finale.lmp", 1);
	// the finale prints the characters one at a time
	remaining = scr_printspeed * (realtime - centertime_start);
	Sbar_DrawCenterString (hud_overlay_view, remaining);
#endif
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

/*
	draw_minifrags

	frags name
	frags team name
	displayed to right of status bar if there's room
*/
static void
draw_minifrags (view_t *view)
{
#if 0
	int         numlines, top, bottom, f, i, k, x, y;
	char        num[20];
	player_info_t *s;

	r_data->scr_copyeverything = 1;
	r_data->scr_fullupdate = 0;

	// scores
	Sbar_SortFrags ();

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
#endif
}

static void
draw_miniteam (view_t *view)
{
#if 0
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
#endif
}

void
Sbar_Draw (void)
{
	if (cls.state != ca_active) {
		return;
	}
	sb_update_flags = ~0;
	if (sb_update_flags & (1 << sbc_ammo)) {
		draw_miniammo (sbar_miniammo);
		draw_ammo (sbar_ammo);
	}
	if (sb_update_flags & (1 << sbc_armor)) draw_armor (sbar_armor);
	if (sb_update_flags & (1 << sbc_frags)) draw_frags (sbar_frags);
	if (sb_update_flags & (1 << sbc_health)) draw_health (sbar_health);
	if (sb_update_flags & (1 << sbc_info)) {
		draw_miniteam (0);
		draw_minifrags (0);
		draw_stuff (0);
		draw_overlay (0);
		draw_intermission (0);
		Sbar_DeathmatchOverlay (0);
		sbar_update_vis ();
	}
	if (sb_update_flags & (1 << sbc_items)) {
		draw_items (sbar_items);
		draw_sigils (sbar_items);
	}
	if (sb_update_flags & (1 << sbc_weapon)) draw_weapons_sbar (sbar_weapons);
	if (sb_update_flags & ((1 << sbc_health) | (1 << sbc_items))) {
		draw_face (sbar_face);
	}
#if 0
	sbar_update_vis ();
	hud_main_view->draw (hud_main_view);
#endif
	sb_update_flags = 0;
}

static void
sbar_hud_swap_f (void *data, const cvar_t *cvar)
{
	if (hud_sbar) {
		return;
	}
}

static void
sbar_hud_sbar_f (void *data, const cvar_t *cvar)
{
	view_t      v;
	if (hud_swap) {
		View_SetParent (sbar_armament, hud_view);
		View_SetPos (sbar_armament, 0, 48);
		View_SetLen (sbar_armament, 42, 156);
		View_SetGravity (sbar_armament,
						 hud_swap ? grav_southwest : grav_southeast);

		View_SetLen (sbar_weapons, 24, 112);
		View_SetGravity (sbar_weapons,
						 hud_swap ? grav_northwest : grav_northeast);

		View_SetLen (sbar_miniammo, 42, 44);
		View_SetGravity (sbar_weapons, grav_southeast);

		for (int i = 0; i < 7; i++) {
			v = View_GetChild (sbar_weapons, i);
			View_SetPos (v, 0, i * 16);
			View_SetLen (v, 24, 16);
			if (sbar_hascomponent (v, hud_subpic)) {
				hud_subpic_t subpic = {
					sbar_getcomponent (v, hud_pic), 0, 0, 24, 16
				};
				sbar_setcomponent (v, hud_subpic, &subpic);
				sbar_remcomponent (v, hud_pic);
			}
		}
		for (int i = 0; i < 4; i++) {
			v = View_GetChild (sbar_miniammo, i);
			View_SetPos (v, 0, i * 11);
			View_SetLen (v, 42, 11);
			hud_subpic_t subpic = { sb_ibar, 3 + (i * 48), 0, 42, 11 };
			sbar_setcomponent (v, hud_subpic, &subpic);
		}
	} else {
		View_SetParent (sbar_armament, sbar_inventory);
		View_SetPos (sbar_armament, 0, 0);
		View_SetLen (sbar_armament, 202, 24);
		View_SetGravity (sbar_armament, grav_northwest);

		View_SetLen (sbar_weapons, 32, 192);
		View_SetGravity (sbar_weapons, grav_southwest);

		View_SetLen (sbar_miniammo, 42, 44);
		View_SetGravity (sbar_weapons, grav_southeast);

		for (int i = 0; i < 7; i++) {
			v = View_GetChild (sbar_weapons, i);
			View_SetPos (v, i * 24, 0);
			View_SetLen (v, 24 + 24 * (i == 6), 16);

			if (sbar_hascomponent (v, hud_subpic)) {
				hud_subpic_t *subpic = sbar_getcomponent(v, hud_subpic);
				sbar_setcomponent (v, hud_pic, &subpic->pic);
				sbar_remcomponent (v, hud_subpic);
			}
		}
	}
}

static void
init_sbar_views (void)
{
	sbar_main      = sbar_view ( 0, 0, 320, 48, grav_south, cl_screen_view);
	sbar_inventory = sbar_view ( 0, 0, 320, 24, grav_northwest, sbar_main);
	sbar_frags     = sbar_view ( 0, 0, 130,  8, grav_northeast, sbar_inventory);
	sbar_sigils    = sbar_view ( 0, 0,  32, 16, grav_southeast, sbar_inventory);
	sbar_items     = sbar_view (32, 0,  96, 16, grav_southeast, sbar_inventory);
	sbar_armament  = sbar_view ( 0, 0, 202, 24, grav_northwest, sbar_inventory);
	sbar_weapons   = sbar_view ( 0, 0, 192, 16, grav_southwest, sbar_armament);
	sbar_miniammo  = sbar_view ( 0, 0,  32,  8, grav_northwest, sbar_armament);
	sbar_statusbar = sbar_view ( 0, 0, 320, 24, grav_southwest, sbar_main);
	sbar_armor     = sbar_view (  0, 0, 96, 24, grav_northwest, sbar_statusbar);
	sbar_face      = sbar_view (112, 0, 24, 24, grav_northwest, sbar_statusbar);
	sbar_health    = sbar_view (136, 0, 72, 24, grav_northwest, sbar_statusbar);
	sbar_ammo      = sbar_view (224, 0, 96, 24, grav_northwest, sbar_statusbar);
	sbar_tile[0]   = sbar_view ( 0, 0,   0, 48, grav_southwest, sbar_main);
	sbar_tile[1]   = sbar_view ( 0, 0,   0, 48, grav_southeast, sbar_main);

	for (int i = 0; i < 4; i++) {
		sbar_view (i * 8, 0, 8, 16, grav_northwest, sbar_sigils);

		sbar_view (i * 24, 0, 24, 24, grav_northwest, sbar_armor);
		sbar_view (i * 24, 0, 24, 24, grav_northwest, sbar_ammo);
	}
	for (int i = 0; i < 6; i++) {
		sbar_view (i * 16, 0, 16, 16, grav_northwest, sbar_items);
	}
	for (int i = 0; i < 7; i++) {
		sbar_view (i * 24, 0, 24, 16, grav_northwest, sbar_weapons);
	}
	for (int i = 0; i < 4; i++) {
		sbar_view (i * 24, 0, 24, 16, grav_northwest, sbar_health);
	}
	for (int i = 0; i < 4; i++) {
		view_t v = sbar_view (i * 48 + 10, 0, 8, 8, grav_northwest,
							  sbar_miniammo);
		draw_charbuffer_t *buffer = Draw_CreateBuffer (3, 1);
		sbar_setcomponent (v, hud_charbuff, &buffer);
	}

	sbar_setcomponent (sbar_inventory, hud_pic, &sb_ibar);
	sbar_setcomponent (sbar_statusbar, hud_pic, &sb_sbar);

	if (r_data->vid->width > 320) {
		int         l = (r_data->vid->width - 320) / 2;
		View_SetPos (sbar_tile[0], -l, 0);
		View_SetLen (sbar_tile[0], l, 48);
		View_SetPos (sbar_tile[1], -l, 0);
		View_SetLen (sbar_tile[1], l, 48);
		sbar_setcomponent (sbar_tile[0], hud_tile, 0);
		sbar_setcomponent (sbar_tile[1], hud_tile, 0);
	}
}

static void
init_hud_views (void)
{
#if 0
	view_t     *view;
	view_t     *minifrags_view = 0;
	view_t     *miniteam_view = 0;

	if (cl_screen_view->xlen < 512) {
		hud_view = view_new (0, 0, 320, 48, grav_south);

		hud_frags_view = view_new (0, 0, 130, 8, grav_northeast);
		hud_frags_view->draw = draw_frags;
	} else if (cl_screen_view->xlen < 640) {
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

	view = view_new (0, 0, 42, 112, grav_northeast);
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

	view = view_new (0, 0, cl_screen_view->xlen, 48, grav_south);
	view->resize_x = 1;
	view_add (view, hud_view);
	hud_view = view;

	view_add (hud_view, hud_armament_view);

	view_insert (hud_main_view, hud_view, 0);
#endif
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
#if 0
	hud_main_view = view_new (0, 0, cl_screen_view->xlen, cl_screen_view->ylen,
							  grav_northwest);
	view_insert (cl_screen_view, hud_main_view, 0);
	hud_main_view->resize_x = 1;	// get resized if the 2d view resizes
	hud_main_view->resize_y = 1;
	hud_main_view->visible = 0;		// but don't let the console draw our stuff
	if (cl_screen_view->ylen > 300)
		hud_overlay_view = view_new (0, 0, 320, 300, grav_center);
	else
		hud_overlay_view = view_new (0, 0, 320, cl_screen_view->ylen,
									 grav_center);
	hud_overlay_view->draw = draw_overlay;
	hud_overlay_view->visible = 0;

	hud_stuff_view = view_new (0, 48, 152, 16, grav_southwest);
	hud_stuff_view->draw = draw_stuff;

	view_insert (hud_main_view, hud_overlay_view, 0);
	view_insert (hud_main_view, hud_stuff_view, 0);
#endif
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

	for (int i = 0; i < 5; i++) {
		sb_weapons[2 + i][0] =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1] =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2] =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3] =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4] =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5] =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6] =
			r_funcs->Draw_PicFromWad (va (0, "inva%i_lightng", i + 1));
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

void
Sbar_Init (void)
{
	HUD_Init_Cvars ();
	Cvar_AddListener (Cvar_FindVar ("hud_sbar"), sbar_hud_sbar_f, 0);
	Cvar_AddListener (Cvar_FindVar ("hud_swap"), sbar_hud_swap_f, 0);

	load_pics ();
	init_views ();
	View_UpdateHierarchy (sbar_main);

	r_data->viewsize_callback = viewsize_f;
	Cvar_Register (&scr_centertime_cvar, 0, 0);
	Cvar_Register (&scr_printspeed_cvar, 0, 0);

	// register GIB builtins
	GIB_Builtin_Add ("print::center", Sbar_GIB_Print_Center_f);
}
