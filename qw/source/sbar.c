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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

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
#include "QF/msg.h"
#include "QF/plugin.h"
#include "QF/quakefs.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/view.h"

#include "bothdefs.h"
#include "cl_cam.h"
#include "client.h"
#include "compat.h"
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

cvar_t     *cl_showscoresuid;
cvar_t     *fs_fraglog;
cvar_t     *cl_fraglog;
cvar_t     *cl_sbar;
cvar_t     *cl_sbar_separator;

static view_t *sbar_view;
static view_t *minifrags_view;
static view_t *miniteam_view;
static view_t *inventory_view;
static view_t *frags_view;
static view_t *status_view;

static view_t *overlay_view;

static void (*Sbar_Draw_DMO_func) (view_t *view, int l, int y, int skip);
static void Sbar_TeamOverlay (view_t *view);
static void Sbar_DeathmatchOverlay (view_t *view, int start);

static void
calc_sb_lines (cvar_t *var)
{
	if (var->int_val >= 120) {
		sb_lines = 0;
	} else if (var->int_val >= 110) {
		sb_lines = 24;
		inventory_view->visible = 0;
	} else {
		sb_lines = 24 + 16 + 8;
		inventory_view->visible = 1;
	}
	if (sb_lines) {
		sbar_view->visible = 1;
		view_resize (sbar_view, sbar_view->xlen, sb_lines);
	} else {
		sbar_view->visible = 0;
	}
}

static void
cl_sbar_f (cvar_t *var)
{
	vid.recalc_refdef = true;
	if (scr_viewsize)
		calc_sb_lines (scr_viewsize);
	r_lineadj = var->int_val ? sb_lines : 0;
}

static void
viewsize_f (cvar_t *var)
{
	calc_sb_lines (var);
	if (cl_sbar)
		r_lineadj = cl_sbar->int_val ? sb_lines : 0;
}


/*
	Sbar_ShowTeamScores

	Tab key down
*/
static void
Sbar_ShowTeamScores (void)
{
	if (sb_showteamscores)
		return;

	sb_showteamscores = true;
	sb_updates = 0;
}

/*
	Sbar_DontShowTeamScores

	Tab key up
*/
static void
Sbar_DontShowTeamScores (void)
{
	sb_showteamscores = false;
	sb_updates = 0;
}

/*
	Sbar_ShowScores

	Tab key down
*/
static void
Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;

	sb_showscores = true;
	sb_updates = 0;
}

/*
	Sbar_DontShowScores

	Tab key up
*/
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

static inline void
draw_pic (view_t *view, int x, int y, qpic_t *pic)
{
	Draw_Pic (view->xabs + x, view->yabs + y, pic);
}

static inline void
draw_cachepic (view_t *view, int x, int y, const char *name)
{
	qpic_t *pic = Draw_CachePic (name, true);
	x += (view->xlen - pic->width) / 2;
	Draw_Pic (view->xabs + x, view->yabs + y, pic);
}

static inline void
draw_subpic (view_t *view, int x, int y, qpic_t *pic,
		     int srcx, int srcy, int width, int height)
{
	Draw_SubPic (view->xabs + x, view->yabs + y, pic,
				 srcx, srcy, width, height);
}

static inline void
draw_transpic (view_t *view, int x, int y, qpic_t *pic)
{
	Draw_Pic (view->xabs + x, view->yabs + y, pic);
}


// drawing routines are reletive to the status bar location
static inline void
Sbar_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x, y + (vid.height - SBAR_HEIGHT), pic);
}

static inline void
draw_character (view_t *view, int x, int y, int c)
{
	Draw_Character (view->xabs + x, view->yabs + y, c);
}

static inline void
draw_string (view_t *view, int x, int y, const char *str)
{
	Draw_String (view->xabs + x, view->yabs + y, str);
}

static inline void
draw_nstring (view_t *view, int x, int y, const char *str, int n)
{
	Draw_nString (view->xabs + x, view->yabs + y, str, n);
}

static inline void
draw_fill (view_t *view, int x, int y, int w, int h, int col)
{
	Draw_Fill (view->xabs + x, view->yabs + y, w, h, col);
}

static void
draw_num (view_t *view, int x, int y, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame;

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
		if (cl.players[i].name[0] && (!cl.players[i].spectator ||
									  includespec)) {
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
		if (!s->name[0])
			continue;
		if (s->spectator)
			continue;

		// find his team in the list
		t[16] = 0;
		strncpy (t, s->team->value, 16);
		if (!t || !t[0])
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

static int
Sbar_ColorForMap (int m)
{
	return (bound (0, m, 13) * 16) + 8;
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

/*
static void
Sbar_DrawInventory (void)
{
	char        num[6];
	float       time;
	int         flashon, i;
	qboolean    headsup;

	headsup = !(cl_sbar->int_val || scr_viewsize->int_val < 100);

	if (!headsup)
		Sbar_DrawPic (0, -24, sb_ibar);
	// weapons
	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			time = cl.item_gettime[i];
			flashon = (int) ((cl.time - time) * 10);
			if (flashon < 0)
				flashon = 0;
			if (flashon >= 10) {
				if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
					flashon = 1;
				else
					flashon = 0;
			} else
				flashon = (flashon % 5) + 2;

			if (headsup) {
				if (i || vid.height > 200)
					Sbar_DrawSubPic (hudswap ? 0 : (vid.width - 24),
									 -68 - (7 - i) * 16,
									 sb_weapons[flashon][i], 0, 0, 24, 16);

			} else
				Sbar_DrawPic (i * 24, -16, sb_weapons[flashon][i]);

			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
		}
	}

	// ammo counts
	for (i = 0; i < 4; i++) {
		snprintf (num, sizeof (num), "%3i", min (cl.stats[STAT_SHELLS + i],
												 999));
		if (headsup) {
#define HUD_X(dist)		(hudswap ? dist : (vid.width - (42 - dist)))
#define HUD_Y(n)		(-24 - (4 - n) * 11)
			Sbar_DrawSubPic (HUD_X (0), HUD_Y (i), sb_ibar,
							 3 + (i * 48), 0, 42, 11);
			if (num[0] != ' ')
				Sbar_DrawCharacter (HUD_X (3),  HUD_Y (i), 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter (HUD_X (11), HUD_Y (i), 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter (HUD_X (19), HUD_Y (i), 18 + num[2] - '0');
#undef HUD_X
#undef HUD_Y
		} else {
#define HUD_X(n, dist)	((6 * n + dist) * 8 - 2)
			if (num[0] != ' ')
				Sbar_DrawCharacter (HUD_X(i, 1), -24, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter (HUD_X(i, 2), -24, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter (HUD_X(i, 3), -24, 18 + num[2] - '0');
#undef HUD_X
		}
	}

	flashon = 0;
	// items
	for (i = 0; i < 6; i++)
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
			time = cl.item_gettime[17 + i];
			if (time && time > cl.time - 2 && flashon) {	// flash frame
				sb_updates = 0;
			} else
				Sbar_DrawPic (192 + i * 16, -16, sb_items[i]);
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
	// sigils
	for (i = 0; i < 4; i++)
		if (cl.stats[STAT_ITEMS] & (1 << (28 + i))) {
			time = cl.item_gettime[28 + i];
			if (time && time > cl.time - 2 && flashon) {	// flash frame
				sb_updates = 0;
			} else
				Sbar_DrawPic (320 - 32 + i * 8, -16, sb_sigil[i]);
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
}
*/

static int
calc_flashon (int ind)
{
	float       time;
	int         flashon;

	time = cl.item_gettime[ind];
	flashon = (int) ((cl.time - time) * 10);
	if (flashon < 0)
		flashon = 0;
	if (flashon >= 10) {
		if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << ind))
			flashon = 1;
		else
			flashon = 0;
	} else
		flashon = (flashon % 5) + 2;
	return flashon;
}

static void
draw_weapons (view_t *view)
{
	int         flashon, i;

	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			flashon = calc_flashon (i);
			draw_pic (view, i * 24, 0, sb_weapons[flashon][i]);
			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
		}
	}
}

static void
draw_ammo (view_t *view)
{
	char        num[6];
	int         i;

	// ammo counts
#define HUD_X(n, dist)  ((6 * n + dist) * 8 + 2)
	for (i = 0; i < 4; i++) {
		snprintf (num, sizeof (num), "%3i", min (cl.stats[STAT_SHELLS + i],
												 999));
		if (num[0] != ' ')
			draw_character (view, HUD_X(i, 1), 0, 18 + num[0] - '0');
		if (num[1] != ' ')
			draw_character (view, HUD_X(i, 2), 0, 18 + num[1] - '0');
		if (num[2] != ' ')
			draw_character (view, HUD_X(i, 3), 0, 18 + num[2] - '0');
	}
#undef HUD_X
}

static void
draw_items (view_t *view)
{
	float       time;
	int         flashon = 0, i;

	for (i = 0; i < 6; i++)
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
			time = cl.item_gettime[17 + i];
			if (time && time > cl.time - 2 && flashon) {	// flash frame
				sb_updates = 0;
			} else
				draw_pic (view, 192 + i * 16, 0, sb_items[i]);
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
}

static void
draw_sigils (view_t *view)
{
	float       time;
	int         flashon = 0, i;

	for (i = 0; i < 4; i++)
		if (cl.stats[STAT_ITEMS] & (1 << (28 + i))) {
			time = cl.item_gettime[28 + i];
			if (time && time > cl.time - 2 && flashon) {	// flash frame
				sb_updates = 0;
			} else
				draw_pic (view, i * 8, 0, sb_sigil[i]);
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
}

static void
draw_inventory (view_t *view)
{
	if (cl.spectator && autocam == CAM_TRACK) {
		if (frags_view)
			view_draw (frags_view);
		return;
	}
	draw_pic (view, 0, 0, sb_ibar);

	view_draw (view);
}

static void
draw_frags (view_t *view)
{
	int         i, k, l, p = -1;
	int         top, bottom;
	int         x;
	char        num[12];
	player_info_t *s;

	Sbar_SortFrags (false);

	// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 0;

	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
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

		// draw number
		snprintf (num, sizeof (num), "%3i", s->frags);

		draw_character (view, x + 6, 0, num[0]);
		draw_character (view, x + 14, 0, num[1]);
		draw_character (view, x + 22, 0, num[2]);

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

/*
static void
Sbar_DrawNormal (void)
{
	if (cl_sbar->int_val || scr_viewsize->int_val < 100)
		Sbar_DrawPic (0, 0, sb_sbar);

	// armor
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		Sbar_DrawNum (24, 0, 666, 3, 1);
	} else {
		Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3,
					  cl.stats[STAT_ARMOR] <= 25);
		if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
			Sbar_DrawPic (0, 0, sb_armor[2]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
			Sbar_DrawPic (0, 0, sb_armor[1]);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
			Sbar_DrawPic (0, 0, sb_armor[0]);
	}

	// face
	Sbar_DrawFace ();

	// health
	Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3,
				  cl.stats[STAT_HEALTH] <= 25);

	// ammo icon
	if (cl.stats[STAT_ITEMS] & IT_SHELLS)
		Sbar_DrawPic (224, 0, sb_ammo[0]);
	else if (cl.stats[STAT_ITEMS] & IT_NAILS)
		Sbar_DrawPic (224, 0, sb_ammo[1]);
	else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
		Sbar_DrawPic (224, 0, sb_ammo[2]);
	else if (cl.stats[STAT_ITEMS] & IT_CELLS)
		Sbar_DrawPic (224, 0, sb_ammo[3]);

	Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}
*/

static void
draw_spectator (view_t *view)
{
	char        st[512];

	if (autocam != CAM_TRACK) {
		draw_pic (view, 0, 0, sb_scorebar);
		draw_string (view, 160 - 7 * 8, 4, "SPECTATOR MODE");
		draw_string (view, 160 - 14 * 8 + 4, 12,
					 "Press [ATTACK] for AutoCamera");
	} else {
//		Sbar_DrawString (160-14*8+4,4, "SPECTATOR MODE - TRACK CAMERA");
		snprintf (st, sizeof (st), "Tracking %-.13s, [JUMP] for next",
				  cl.players[spec_track].name);
		draw_string (view, 0, -8, st);
	}
}

static void
draw_status (view_t *view)
{
	draw_pic (view, 0, 0, sb_sbar);

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

static void
draw_overlay (view_t *view)
{
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
draw_tile (view_t *view)
{
	Draw_TileClear (view->xabs, view->yabs, view->xlen, view->ylen);
}

void
Sbar_Draw (void)
{
	qboolean    headsup;

	sbar_view->visible = 0;

	headsup = !(cl_sbar->int_val || scr_viewsize->int_val < 100);
	if ((sb_updates >= vid.numpages) && !headsup)
		return;

	if (scr_con_current == vid.height)
		return;

	if ((cl.stats[STAT_HEALTH] <= 0 && !cl.spectator)
		|| sb_showscores || sb_showteamscores)
		overlay_view->visible = 1;
	else
		overlay_view->visible = 0;

	if (!sb_lines)
		return;

	sbar_view->visible = 1;

	scr_copyeverything = 1;
	sb_updates++;

	if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)
		sb_updates = 0;
}

/*
	Sbar_TeamOverlay

	team frags
	added by Zoid
*/
void
Sbar_TeamOverlay (view_t *view)
{
	char        num[12];
	int         pavg, plow, phigh, i, k, l, x, y;
	team_t     *tm;
	info_key_t *player_team = cl.players[cl.playernum].team;

	if (!cl.teamplay) { // FIXME: if not teamplay, why teamoverlay?
		Sbar_DeathmatchOverlay (view, 0);
		return;
	}

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	draw_cachepic (view, 0, 0, "gfx/ranking.lmp");

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
	l = scoreboardlines;

	for (i = 0; i < scoreboardteams && y <= (int) vid.height - 10; i++) {
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
			Draw_Character (x + 104 - 8, y, 16);
			Draw_Character (x + 104 + 32, y, 17);
		}

		y += 8;
	}
	y += 8;
	Sbar_DeathmatchOverlay (view, y);
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
	char        num[512];
	char        conv[512];
	char        conv2[512];
	char       *cp = NULL;
	QFile      *file = NULL;
	int         minutes, fph, total, d, f, i, k, l, p;
	player_info_t *s = NULL;
	const char *t = NULL;
	time_t      tt = time (NULL);
	char        e_path[MAX_OSPATH];

	if (!cl_fraglog->int_val)
		return;

	memset (&e_path, 0, MAX_OSPATH);
    Qexpand_squiggle (fs_userpath->string, e_path);

	if ((file = Qopen (va ("%s/%s", e_path, fs_fraglog->string), "a")) == NULL)
		return;

	t = ctime (&tt);
	if (t)
		Qwrite (file, t, strlen (t));

	Qwrite (file, cls.servername, strlen (cls.servername));
	Qwrite (file, "\n", 1);
	Qwrite (file, cl.worldmodel->name, strlen (cl.worldmodel->name));
	Qwrite (file, " ", 1);
	Qwrite (file, cl.levelname, strlen (cl.levelname));
	Qwrite (file, "\n", 1);

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
		if (!s->name[0])
			continue;

 		// draw pl
		p = s->pl;

		// get time
		if (cl.intermission)
			total = cl.completed_time - s->entertime;
		else
			total = realtime - s->entertime;
		minutes = total / 60;

		// get frags
		f = s->frags;

		fph = calc_fph (f, total);

		memset (&conv, 0, 512);
		for (cp = (unsigned char *) s->name, d = 0; *cp; cp++, d++)
			conv[d] = sys_char_map[(unsigned int) *cp];
		
		if (s->spectator) {
			snprintf (num, sizeof (num), "%-3i%% %s (spectator)", s->pl,
					  (char *) &conv);
		} else {
			if (cl.teamplay) {
				memset (&conv2, 0, 512);
				for (cp = (unsigned char *) s->team->value, d = 0; *cp; cp++,
						 d++)
					conv2[d] = sys_char_map[(unsigned int) *cp];

				snprintf (num, sizeof (num), "%-3i%% %-3i %-4i %-3i    "
						  "%-4s %s", s->pl, fph, minutes, f, (char *) &conv2,
						  (char *) &conv);
			} else {
				snprintf (num, sizeof (num), "%-3i%% %-3i %-4i %-3i   %s",
						  s->pl, fph, minutes, f, (char *) &conv);
			}
		}
		Qwrite (file, num, strlen (num));
		Qwrite (file, "\n\n", 1);
	}
	
	Qclose (file);
}

static void
Sbar_Draw_DMO_Team_Ping (view_t *view, int l, int y, int skip)
{
	char		num[12];
	int			fph, minutes, total, top, bottom, f, i, k, p, x;
	player_info_t *s;

	x = 4;
//						0    40 64  104  152   192
	draw_string (view, x, y, "ping pl fph time frags team name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= (int) vid.height - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999)
			p = 999;
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x, y, num);

 		// draw pl
		p = s->pl;
		snprintf (num, sizeof (num), "%3i", p);
		if (p > 25)
			Draw_AltString (x + 32, y, num);
		else
			draw_string (view, x + 32, y, num);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			draw_string (view, x + 184 + 40, y, s->name);			// draw name
			y += skip;
			continue;
		}

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
		draw_string (view, x + 64, y, num);
		
		//draw time
		snprintf (num, sizeof (num), "%4i", minutes);
		draw_string (view, x + 96, y, num);

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);
		if (largegame)
			draw_fill (view, x + 136, y + 1, 40, 3, top);
		else
			draw_fill (view, x + 136, y, 40, 4, top);
		draw_fill (view, x + 136, y + 4, 40, 4, bottom);

		// draw frags
		if (k != cl.playernum) {
			snprintf (num, sizeof (num), " %3i ", f);
		} else {
			snprintf (num, sizeof (num), "\x10%3i\x11", f);
		}
		draw_nstring (view, x + 136, y, num, 5);

		draw_nstring (view, x + 184, y, s->team->value, 4);	// draw team
		draw_string (view, x + 184 + 40, y, s->name);			// draw name
		y += skip;
	}
}

static void
Sbar_Draw_DMO_Team_UID (view_t *view, int l, int y, int skip)
{
	char		num[12];
	int			fph, minutes, total, top, bottom, f, i, k, p, x;
	player_info_t *s;

	x = 4;
//						0    40 64  104  152   192
	draw_string (view, x, y, " uid pl fph time frags team name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= (int) vid.height - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		// draw userid
		p = s->userid;
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x, y, num);

 		// draw pl
		p = s->pl;
		snprintf (num, sizeof (num), "%3i", p);
		if (p > 25)
			Draw_AltString (x + 32, y, num);
		else
			draw_string (view, x + 32, y, num);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			draw_string (view, x + 184 + 40, y, s->name);			// draw name
			y += skip;
			continue;
		}

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
		draw_string (view, x + 64, y, num);
		
		//draw time
		snprintf (num, sizeof (num), "%4i", minutes);
		draw_string (view, x + 96, y, num);

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);
		if (largegame)
			draw_fill (view, x + 136, y + 1, 40, 3, top);
		else
			draw_fill (view, x + 136, y, 40, 4, top);
		draw_fill (view, x + 136, y + 4, 40, 4, bottom);

		// draw frags
		if (k != cl.playernum) {
			snprintf (num, sizeof (num), " %3i ", f);
		} else {
			snprintf (num, sizeof (num), "\x10%3i\x11", f);
		}
		draw_nstring (view, x + 136, y, num, 5);

		draw_nstring (view, x + 184, y, s->team->value, 4);	// draw team
		draw_string (view, x + 184 + 40, y, s->name);			// draw name
		y += skip;
	}
}

static void
Sbar_Draw_DMO_Team_Ping_UID (view_t *view, int l, int y, int skip)
{
	char		num[12];
	int			fph, minutes, total, top, bottom, f, i, k, p, x;
	player_info_t *s;

	x = 4;
//						0    40 64  104  152   192
	draw_string (view, x, y, "ping pl fph time frags team  uid name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= (int) vid.height - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999)
			p = 999;
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x, y, num);

 		// draw pl
		p = s->pl;
		snprintf (num, sizeof (num), "%3i", p);
		if (p > 25)
			Draw_AltString (x + 32, y, num);
		else
			draw_string (view, x + 32, y, num);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			p = s->userid;
			snprintf (num, sizeof (num), "%4i", p);
			draw_string (view, x + 184 + 40, y, num);				// draw uid
			draw_string (view, x + 184 + 80, y, s->name);			// draw name
			y += skip;
			continue;
		}

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
		draw_string (view, x + 64, y, num);
		
		//draw time
		snprintf (num, sizeof (num), "%4i", minutes);
		draw_string (view, x + 96, y, num);

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);
		if (largegame)
			draw_fill (view, x + 136, y + 1, 40, 3, top);
		else
			draw_fill (view, x + 136, y, 40, 4, top);
		draw_fill (view, x + 136, y + 4, 40, 4, bottom);

		// draw frags
		if (k != cl.playernum) {
			snprintf (num, sizeof (num), " %3i ", f);
		} else {
			snprintf (num, sizeof (num), "\x10%3i\x11", f);
		}
		draw_nstring (view, x + 136, y, num, 5);

		draw_nstring (view, x + 184, y, s->team->value, 4);	// draw team
		p = s->userid;
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x + 184 + 40, y, num);				// draw uid
		draw_string (view, x + 184 + 80, y, s->name);			// draw name
		y += skip;
	}
}

static void
Sbar_Draw_DMO_Ping (view_t *view, int l, int y, int skip)
{
	char		num[12];
	int			fph, minutes, total, top, bottom, f, i, k, p, x;
	player_info_t *s;

	x = 16;
//						0    40 64  104  152
	draw_string (view, x, y, "ping pl fph time frags name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e"
				 "\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= (int) vid.height - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999)
			p = 999;
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x, y, num);

 		// draw pl
		p = s->pl;
		snprintf (num, sizeof (num), "%3i", p);
		if (p > 25)
			Draw_AltString (x + 32, y, num);
		else
			draw_string (view, x + 32, y, num);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			draw_string (view, x + 184, y, s->name);			// draw name
			y += skip;
			continue;
		}

		// get time
		if (cl.intermission)
			total = cl.completed_time - s->entertime;
		else
			total = realtime - s->entertime;
		minutes =  total / 60;

		// get frags
		f = s->frags;

		// draw fph
		fph = calc_fph (f, total);
		snprintf (num, sizeof (num), "%3i", fph);
		draw_string (view, x + 64, y, num);
		
		//draw time
		snprintf (num, sizeof (num), "%4i", minutes);
		draw_string (view, x + 96, y, num);

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);
		if (largegame)
			draw_fill (view, x + 136, y + 1, 40, 3, top);
		else
			draw_fill (view, x + 136, y, 40, 4, top);
		draw_fill (view, x + 136, y + 4, 40, 4, bottom);

		// draw frags
		if (k != cl.playernum) {
			snprintf (num, sizeof (num), " %3i ", f);
		} else {
			snprintf (num, sizeof (num), "\x10%3i\x11", f);
		}
		draw_nstring (view, x + 136, y, num, 5);

		draw_string (view, x + 184, y, s->name);				// draw name
		y += skip;
	}
}

static void
Sbar_Draw_DMO_UID (view_t *view, int l, int y, int skip)
{
	char		num[12];
	int			fph, minutes, total, top, bottom, f, i, k, p, x;
	player_info_t *s;

	x = 16;
	draw_string (view, x, y, " uid pl fph time frags name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1e\x1e\x1e\x1e"
				 "\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= (int) vid.height - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		// draw ping
		if (cl_showscoresuid->int_val == 1) { // hack to show userid
			p = s->userid;
		} else {
			p = s->ping;
			if (p < 0 || p > 999)
				p = 999;
		}
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x, y, num);

		// draw pl
		p = s->pl;
		snprintf (num, sizeof (num), "%3i", p);
		if (p > 25)
			Draw_AltString (x + 32, y, num);
		else
			draw_string (view, x + 32, y, num);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			draw_string (view, x + 184, y, s->name);			// draw name
			y += skip;
			continue;
		}

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
		draw_string (view, x + 64, y, num);
                
		//draw time
		snprintf (num, sizeof (num), "%4i", minutes);
		draw_string (view, x + 96, y, num);

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);
		if (largegame)
			draw_fill (view, x + 136, y + 1, 40, 3, top);
		else
			draw_fill (view, x + 136, y, 40, 4, top);
		draw_fill (view, x + 136, y + 4, 40, 4, bottom);

		// draw frags
		if (k != cl.playernum) {
			snprintf (num, sizeof (num), " %3i ", f);
		} else {
			snprintf (num, sizeof (num), "\x10%3i\x11", f);
		}
		draw_nstring (view, x + 136, y, num, 5);

		draw_string (view, x + 184, y, s->name);				// draw name
		y += skip;
	}
}

static void
Sbar_Draw_DMO_Ping_UID (view_t *view, int l, int y, int skip)
{
	char		num[12];
	int			fph, minutes, total, top, bottom, f, i, k, p, x;
	player_info_t *s;

	x = 16;
	draw_string (view, x, y, "ping pl fph time frags  uid name");
	y += 8;
	draw_string (view, x, y, "\x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f "
				 "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e"
				 "\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
	y += 8;

	for (i = 0; i < l && y <= (int) vid.height - 10; i++) {
		k = fragsort[i];
		s = &cl.players[k];
		if (!s->name[0])
			continue;

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999)
			p = 999;
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x, y, num);

		// draw pl
		p = s->pl;
		snprintf (num, sizeof (num), "%3i", p);
		if (p > 25)
			Draw_AltString (x + 32, y, num);
		else
			draw_string (view, x + 32, y, num);

		if (s->spectator) {
			draw_string (view, x + 72, y, "(spectator)");
			p = s->userid;
			snprintf (num, sizeof (num), "%4i", p);
			draw_string (view, x + 184, y, num);
			draw_string (view, x + 184 + 40, y, s->name);
			y += skip;
			continue;
		}

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
		draw_string (view, x + 64, y, num);

		//draw time
		snprintf (num, sizeof (num), "%4i", minutes);
		draw_string (view, x + 96, y, num);

		// draw background
		top = Sbar_ColorForMap (s->topcolor);
		bottom = Sbar_ColorForMap (s->bottomcolor);
		if (largegame)
			draw_fill (view, x + 136, y + 1, 40, 3, top);
		else
			draw_fill (view, x + 136, y, 40, 4, top);
		draw_fill (view, x + 136, y + 4, 40, 4, bottom);

		// draw frags
		if (k != cl.playernum) {
			snprintf (num, sizeof (num), " %3i ", f);
		} else {
			snprintf (num, sizeof (num), "\x10%3i\x11", f);
		}
		draw_nstring (view, x + 136, y, num, 5);

		p = s->userid;
		snprintf (num, sizeof (num), "%4i", p);
		draw_string (view, x + 184, y, num);						// draw UID
		draw_string (view, x + 184 + 40, y, s->name);				// draw name
		y += skip;
	}
}

void
Sbar_DMO_Init_f (cvar_t *var)
{
	// "var" is "cl_showscoresuid"
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
	Sbar_DeathmatchOverlay

	ping time frags name
*/
void
Sbar_DeathmatchOverlay (view_t *view, int start)
{
	int			l, y;
	int			skip = 10;

	if (vid.width < 244) // FIXME: magic number, gained through experimentation
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

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	if (!start) {
		draw_cachepic (view, 0, 0, "gfx/ranking.lmp");
		y = 24;
	} else
		y = start;

	// scores
	Sbar_SortFrags (true);

	// draw the text
	l = scoreboardlines;

	// func ptr, avoids absurd if testing
	Sbar_Draw_DMO_func (view, l, y, skip);

	if (y >= (int) vid.height - 10)		// we ran over the screen size, squish
		largegame = true;
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
	char        num[12];
	player_info_t *s;

	if (vid.width < 512 || !sb_lines)
		return;							// not enuff room

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	// scores   
	Sbar_SortFrags (false);
	if (vid.width >= 640)
		Sbar_SortTeams ();

	if (!scoreboardlines)
		return;							// no one there?

	// draw the text
	if (view->ylen / 8 < 3)
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
		if (!s->name[0])
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
			draw_nstring (view, x + 48 + 40, y, s->name, 16);
		} else
			draw_nstring (view, x + 48, y, s->name, 16);
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
/*
	// draw separator
	x += 208;
	if (cl_sbar_separator->int_val)
		for (y = vid.height - sb_lines; y < (int) vid.height - 6; y += 2)
			Draw_Character (x, y, 14);
*/
	x = 0;
	y = 0;
	for (i = 0; i < scoreboardteams && y <= (int) vid.height; i++) {
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

void
Sbar_IntermissionOverlay (void)
{
	scr_copyeverything = 1;
	scr_fullupdate = 0;

	if (cl.teamplay > 0 && !sb_showscores)
		Sbar_TeamOverlay (overlay_view);
	else
		Sbar_DeathmatchOverlay (overlay_view, 0);
}

void
Sbar_FinaleOverlay (void)
{
	scr_copyeverything = 1;

	draw_cachepic (overlay_view, 0, 16, "gfx/finale.lmp");
}

static void
init_views (void)
{
	view_t     *view;

	if (vid.conwidth < 512) {
		sbar_view = view_new (0, 0, 320, 48, grav_south);

		frags_view = view_new (0, 0, 130, 8, grav_northeast);
		frags_view->draw = draw_frags;
	} else if (vid.conwidth < 640) {
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

	if (vid.conheight > 300)
		overlay_view = view_new (0, 0, 320, 300, grav_center);
	else
		overlay_view = view_new (0, 0, 320, vid.conheight, grav_center);
	overlay_view->draw = draw_overlay;

	inventory_view = view_new (0, 0, 320, 24, grav_northwest);
	inventory_view->draw = draw_inventory;

	view = view_new (0, 0, 32, 16, grav_southwest);
	view->draw = draw_weapons;
	view_add (inventory_view, view);

	view = view_new (0, 0, 32, 8, grav_northwest);
	view->draw = draw_ammo;
	view_add (inventory_view, view);

	view = view_new (0, 0, 96, 16, grav_southeast);
	view->draw = draw_items;
	view_add (inventory_view, view);

	view = view_new (0, 0, 32, 16, grav_southeast);
	view->draw = draw_sigils;
	view_add (inventory_view, view);

	if (frags_view)
		view_add (inventory_view, frags_view);

	status_view = view_new (0, 0, 320, 24, grav_southwest);
	status_view->draw = draw_status;

	view_add (sbar_view, inventory_view);
	view_add (sbar_view, status_view);
	if (minifrags_view)
		view_add (sbar_view, minifrags_view);
	if (miniteam_view)
		view_add (sbar_view, miniteam_view);

	if (vid.width > 640) {
		int         l = (vid.width - 640) / 2;

		view = view_new (-l, 0, l, 48, grav_southwest);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);

		view = view_new (-l, 0, l, 48, grav_southeast);
		view->draw = draw_tile;
		view->resize_y = 1;
		view_add (sbar_view, view);
	}

	if (con_module) {
		view_insert (con_module->data->console->view, overlay_view, 0);
		view_insert (con_module->data->console->view, sbar_view, 0);
	}
}

void
Sbar_Init (void)
{
	int         i;

	init_views ();

	for (i = 0; i < 10; i++) {
		sb_nums[0][i] = Draw_PicFromWad (va ("num_%i", i));
		sb_nums[1][i] = Draw_PicFromWad (va ("anum_%i", i));
	}

	sb_nums[0][10] = Draw_PicFromWad ("num_minus");
	sb_nums[1][10] = Draw_PicFromWad ("anum_minus");

	sb_colon = Draw_PicFromWad ("num_colon");
	sb_slash = Draw_PicFromWad ("num_slash");

	sb_weapons[0][0] = Draw_PicFromWad ("inv_shotgun");
	sb_weapons[0][1] = Draw_PicFromWad ("inv_sshotgun");
	sb_weapons[0][2] = Draw_PicFromWad ("inv_nailgun");
	sb_weapons[0][3] = Draw_PicFromWad ("inv_snailgun");
	sb_weapons[0][4] = Draw_PicFromWad ("inv_rlaunch");
	sb_weapons[0][5] = Draw_PicFromWad ("inv_srlaunch");
	sb_weapons[0][6] = Draw_PicFromWad ("inv_lightng");

	sb_weapons[1][0] = Draw_PicFromWad ("inv2_shotgun");
	sb_weapons[1][1] = Draw_PicFromWad ("inv2_sshotgun");
	sb_weapons[1][2] = Draw_PicFromWad ("inv2_nailgun");
	sb_weapons[1][3] = Draw_PicFromWad ("inv2_snailgun");
	sb_weapons[1][4] = Draw_PicFromWad ("inv2_rlaunch");
	sb_weapons[1][5] = Draw_PicFromWad ("inv2_srlaunch");
	sb_weapons[1][6] = Draw_PicFromWad ("inv2_lightng");

	for (i = 0; i < 5; i++) {
		sb_weapons[2 + i][0] = Draw_PicFromWad (va ("inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1] = Draw_PicFromWad (va ("inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2] = Draw_PicFromWad (va ("inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3] = Draw_PicFromWad (va ("inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4] = Draw_PicFromWad (va ("inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5] = Draw_PicFromWad (va ("inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6] = Draw_PicFromWad (va ("inva%i_lightng", i + 1));
	}

	sb_ammo[0] = Draw_PicFromWad ("sb_shells");
	sb_ammo[1] = Draw_PicFromWad ("sb_nails");
	sb_ammo[2] = Draw_PicFromWad ("sb_rocket");
	sb_ammo[3] = Draw_PicFromWad ("sb_cells");

	sb_armor[0] = Draw_PicFromWad ("sb_armor1");
	sb_armor[1] = Draw_PicFromWad ("sb_armor2");
	sb_armor[2] = Draw_PicFromWad ("sb_armor3");

	sb_items[0] = Draw_PicFromWad ("sb_key1");
	sb_items[1] = Draw_PicFromWad ("sb_key2");
	sb_items[2] = Draw_PicFromWad ("sb_invis");
	sb_items[3] = Draw_PicFromWad ("sb_invuln");
	sb_items[4] = Draw_PicFromWad ("sb_suit");
	sb_items[5] = Draw_PicFromWad ("sb_quad");

	sb_sigil[0] = Draw_PicFromWad ("sb_sigil1");
	sb_sigil[1] = Draw_PicFromWad ("sb_sigil2");
	sb_sigil[2] = Draw_PicFromWad ("sb_sigil3");
	sb_sigil[3] = Draw_PicFromWad ("sb_sigil4");

	sb_faces[4][0] = Draw_PicFromWad ("face1");
	sb_faces[4][1] = Draw_PicFromWad ("face_p1");
	sb_faces[3][0] = Draw_PicFromWad ("face2");
	sb_faces[3][1] = Draw_PicFromWad ("face_p2");
	sb_faces[2][0] = Draw_PicFromWad ("face3");
	sb_faces[2][1] = Draw_PicFromWad ("face_p3");
	sb_faces[1][0] = Draw_PicFromWad ("face4");
	sb_faces[1][1] = Draw_PicFromWad ("face_p4");
	sb_faces[0][0] = Draw_PicFromWad ("face5");
	sb_faces[0][1] = Draw_PicFromWad ("face_p5");

	sb_face_invis = Draw_PicFromWad ("face_invis");
	sb_face_invuln = Draw_PicFromWad ("face_invul2");
	sb_face_invis_invuln = Draw_PicFromWad ("face_inv2");
	sb_face_quad = Draw_PicFromWad ("face_quad");

	Cmd_AddCommand ("+showscores", Sbar_ShowScores,
					"Display information on everyone playing");
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores,
					"Stop displaying information on everyone playing");
	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores,
					"Display information for your team");
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores,
					"Stop displaying information for your team");

	sb_sbar = Draw_PicFromWad ("sbar");
	sb_ibar = Draw_PicFromWad ("ibar");
	sb_scorebar = Draw_PicFromWad ("scorebar");

	r_viewsize_callback = viewsize_f;

	cl_showscoresuid = Cvar_Get ("cl_showscoresuid", "0", CVAR_NONE,
								 Sbar_DMO_Init_f, "Set to 1 to show uid "
								 "instead of ping. Set to 2 to show both.");
	fs_fraglog = Cvar_Get ("fs_fraglog", "qw-scores.log", CVAR_ARCHIVE, NULL,
						   "Filename of the automatic frag-log.");
	cl_fraglog = Cvar_Get ("cl_fraglog", "0", CVAR_ARCHIVE, NULL,
						   "Automatic fraglogging, non-zero value will switch "
						   "it on.");
	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, cl_sbar_f,
						"status bar mode");
	cl_sbar_separator = Cvar_Get ("cl_sbar_separator", "0", CVAR_ARCHIVE, NULL,
								  "turns on status bar separator");
}
