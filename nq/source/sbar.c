
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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cmd.h"
#include "compat.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/screen.h"
#include "QF/vid.h"
#include "QF/va.h"
#include "QF/wad.h"

#include "sbar.h"
#include "game.h"
#include "client.h"
#include "server.h"

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

// FIXME: MISSIONHUD (rsb_*, hsb_*)
//qpic_t     *rsb_invbar[2];
//qpic_t     *rsb_weapons[5];
//qpic_t     *rsb_items[2];
//qpic_t     *rsb_ammo[3];
//qpic_t     *rsb_teambord;			// PGM 01/19/97 - team color border

								// MED 01/04/97 added two more weapons + 3
								// alternates for grenade launcher
//qpic_t     *hsb_weapons[7][5];	// 0 is active, 1 is owned, 2-5 are flashes

								// MED 01/04/97 added array to simplify
								// weapon parsing
//int         hipweapons[4] =
//	{ HIT_LASER_CANNON_BIT, HIT_MJOLNIR_BIT, 4, HIT_PROXIMITY_GUN_BIT };
//qpic_t     *hsb_items[2];			// MED 01/04/97 added hipnotic items array

qboolean    headsup;
qboolean    sbar_centered;

void        Sbar_MiniDeathmatchOverlay (void);
void        Sbar_DeathmatchOverlay (void);


/*
	Sbar_ColorForMap

	I'm not exactly sure why this exists, but I'm not going to change it yet.
*/
int
Sbar_ColorForMap (int m)
{
	return m + 8;						// FIXME: Might want this to be
	// return (bound (0, m, 13) * 16) +
	// 8;
}


/*
	Sbar_ShowScores

	Tab key has been pressed, inform sbar it needs to show scores
*/
void
Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;
	sb_showscores = true;
	sb_updates = 0;
}


/*
	Sbar_DontShowScores

	Tab key up, show normal sbar again
*/
void
Sbar_DontShowScores (void)
{
	if (!sb_showscores)
		return;
	sb_showscores = false;
	sb_updates = 0;
}


/*
	Sbar_Changed

	Call this to signal sbar to redraw next frame.
*/
void
Sbar_Changed (void)
{
	sb_updates = 0;						// update next frame
}


/*
	Drawing Routines

	Sbar_Draw* routines are relative to the location of the status bar.
*/


/*
	Sbar_DrawPic

	Draw a texture.
*/
void
Sbar_DrawPic (int x, int y, qpic_t *pic)
{
	if (sbar_centered)
		Draw_Pic (x + ((vid.width - 320) >> 1), y + (vid.height - SBAR_HEIGHT),
				  pic);
	else
		Draw_Pic (x, y + (vid.height - SBAR_HEIGHT), pic);
}


/*
	Sbar_DrawSubPic

	Draw a _portion_ of a texture.
*/
void
Sbar_DrawSubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
				 int height)
{
	if (sbar_centered)
		Draw_SubPic (x + ((vid.width - 320) >> 1),
					 y + (vid.height - SBAR_HEIGHT), pic, srcx, srcy, width,
					 height);
	else
		Draw_SubPic (x, y + (vid.height - SBAR_HEIGHT), pic, srcx, srcy, width,
					 height);
}


/*
	Sbar_DrawTransPic

	Draw a transparent pic?
*/
void
Sbar_DrawTransPic (int x, int y, qpic_t *pic)
{
	if (sbar_centered)
		Draw_Pic (x + ((vid.width - 320) >> 1),
					   y + (vid.height - SBAR_HEIGHT), pic);
	else
		Draw_Pic (x, y + (vid.height - SBAR_HEIGHT), pic);
}


/*
	Sbar_DrawCharacter

	Draw one solid graphics character
*/
void
Sbar_DrawCharacter (int x, int y, int num)
{
	if (sbar_centered)
		Draw_Character8 (x + ((vid.width - 320) >> 1) + 4,
						 y + vid.height - SBAR_HEIGHT, num);
	else
		Draw_Character8 (x + 4, y + vid.height - SBAR_HEIGHT, num);
}


/*
	Sbar_DrawString

	Draw a string
*/
void
Sbar_DrawString (int x, int y, char *str)
{
	if (sbar_centered)
		Draw_String8 (x + ((vid.width - 320) >> 1),
					  y + vid.height - SBAR_HEIGHT, str);
	else
		Draw_String8 (x, y + vid.height - SBAR_HEIGHT, str);
}


/*
	Sbar_itoa

	Convert an int to ascii
*/
int
Sbar_itoa (int num, char *buf)
{
	char       *str;
	int         pow10;
	int         dig;

	str = buf;

	if (num < 0) {
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10; num >= pow10; pow10 *= 10);

	do {
		pow10 /= 10;
		dig = num / pow10;
		*str++ = '0' + dig;
		num -= dig * pow10;
	} while (pow10 != 1);

	*str = 0;

	return str - buf;
}


void
Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame;

	l = Sbar_itoa (num, str);
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

		Sbar_DrawTransPic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}


int         fragsort[MAX_SCOREBOARD];
char        scoreboardtext[MAX_SCOREBOARD][20];
int         scoreboardtop[MAX_SCOREBOARD];
int         scoreboardbottom[MAX_SCOREBOARD];
int         scoreboardcount[MAX_SCOREBOARD];
int         scoreboardlines;


void
Sbar_SortFrags (void)
{
	int         i, j, k;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < cl.maxclients; i++) {
		if (cl.scores[i].name[0]) {
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i = 0; i < scoreboardlines; i++) {
		for (j = 0; j < (scoreboardlines - 1 - i); j++) {
			if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j + 1]].frags) {
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}


void
Sbar_UpdateScoreboard (void)
{
	int         i, k;
	int         top, bottom;
	scoreboard_t *s;

	Sbar_SortFrags ();

	// draw the text
	memset (scoreboardtext, 0, sizeof (scoreboardtext));

	for (i = 0; i < scoreboardlines; i++) {
		k = fragsort[i];
		s = &cl.scores[k];
		snprintf (&scoreboardtext[i][1], sizeof (&scoreboardtext[i][1]),
				  "%3i %s", s->frags, s->name);

		top = s->colors & 0xf0;
		bottom = (s->colors & 15) << 4;
		scoreboardtop[i] = Sbar_ColorForMap (top);
		scoreboardbottom[i] = Sbar_ColorForMap (bottom);
	}
}


void
Sbar_SoloScoreboard (void)
{
	char        str[80];
	int         minutes, seconds, tens, units;
	int         l;

	if (!headsup)
		Sbar_DrawPic (0, 0, sb_scorebar);

	snprintf (str, sizeof (str), "Monsters:%3i /%3i", cl.stats[STAT_MONSTERS],
			  cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (8, 4, str);

	snprintf (str, sizeof (str), "Secrets :%3i /%3i", cl.stats[STAT_SECRETS],
			  cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (8, 12, str);

	// time
	minutes = cl.time / 60;
	seconds = cl.time - (60 * minutes);
	tens = seconds / 10;
	units = seconds - (10 * tens);
	snprintf (str, sizeof (str), "Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

	// draw level name
	l = strlen (cl.levelname);
	Sbar_DrawString (232 - l * 4, 12, cl.levelname);
}


void
Sbar_DrawScoreboard (void)
{
	Sbar_SoloScoreboard ();
	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay ();
}


void
Sbar_DrawInventory (void)
{
	int         i;
	char        num[6];
	float       time;
	int         flashon;

	// FIXME: MISSIONHUD
//	if (rogue) {
//		if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
//			Sbar_DrawPic (0, -24, rsb_invbar[0]);
//		else
//			Sbar_DrawPic (0, -24, rsb_invbar[1]);
//	} else {
		if (!headsup)
			Sbar_DrawPic (0, -24, sb_ibar);
//	}

	// weapons
	for (i = 0; i < 7; i++) {
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i)) {
			time = cl.item_gettime[i];
			flashon = (int) ((cl.time - time) * 10);
			flashon = max (0, flashon);

			if (flashon >= 10) {
				if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
					flashon = 1;
				else
					flashon = 0;
			} else {
				flashon = (flashon % 5) + 2;
			}

			if (headsup) {
				if (i || vid.height > 200)
					Sbar_DrawSubPic ((cl_hudswap->int_val) ? 0
									 : (vid.width - 24), -68 - (7 - i) * 16,
									 sb_weapons[flashon][i], 0, 0, 24, 16);
			} else {
				Sbar_DrawPic (i * 24, -16, sb_weapons[flashon][i]);
			}

			if (flashon > 1)
				sb_updates = 0;			// force update to remove flash
		}
	}

	// FIXME: MISSIONHUD
//	// hipnotic weapons
//	if (hipnotic) {
//		int         grenadeflashing = 0;
//
//		for (i = 0; i < 4; i++) {
//			if (cl.stats[STAT_ITEMS] & (1 << hipweapons[i])) {
//				time = cl.item_gettime[hipweapons[i]];
//				flashon = (int) ((cl.time - time) * 10);
//				flashon = max (0, flashon);
//
//				if (flashon >= 10) {
//					if (cl.stats[STAT_ACTIVEWEAPON] == (1 << hipweapons[i]))
//						flashon = 1;
//					else
//						flashon = 0;
//				} else {
//					flashon = (flashon % 5) + 2;
//				}
//
//				// check grenade launcher
//				switch (i) {
//					case 2:
//					if (cl.stats[STAT_ITEMS] & HIT_PROXIMITY_GUN) {
//						if (flashon) {
//							grenadeflashing = 1;
//							Sbar_DrawPic (96, -16, hsb_weapons[flashon][2]);
//						}
//					}
//					break;
//					case 3:
//					if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << 4)) {
//						if (flashon && !grenadeflashing) {
//							Sbar_DrawPic (96, -16, hsb_weapons[flashon][3]);
//						} else if (!grenadeflashing) {
//							Sbar_DrawPic (96, -16, hsb_weapons[0][3]);
//						}
//					} else {
//						Sbar_DrawPic (96, -16, hsb_weapons[flashon][4]);
//					}
//					break;
//					default:
//					Sbar_DrawPic (176 + (i * 24), -16, hsb_weapons[flashon][i]);
//					break;
//				}
//				if (flashon > 1)
//					sb_updates = 0;		// force update to remove flash
//			}
//		}
//	}

	// FIXME: MISSIONHUD
//	if (rogue) {
//		// check for powered up weapon.
//		if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN) {
//			for (i = 0; i < 5; i++) {
//				if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i)) {
//					Sbar_DrawPic ((i + 2) * 24, -16, rsb_weapons[i]);
//				}
//			}
//		}
//	}
	// ammo counts
	for (i = 0; i < 4; i++) {
		snprintf (num, sizeof (num), "%3i", cl.stats[STAT_SHELLS + i]);
		if (headsup) {
			Sbar_DrawSubPic ((cl_hudswap->int_val) ? 0 : (vid.width - 42),
							 -24 - (4 - i) * 11, sb_ibar, 3 + (i * 48), 0, 42,
							 11);
			if (num[0] != ' ')
				Sbar_DrawCharacter ((cl_hudswap->int_val) ? 3
									: (vid.width - 39), -24 - (4 - i) * 11,
									18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter ((cl_hudswap->int_val) ? 11
									: (vid.width - 31), -24 - (4 - i) * 11,
									18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter ((cl_hudswap->int_val) ? 19
									: (vid.width - 23), -24 - (4 - i) * 11,
									18 + num[2] - '0');
		} else {
			if (num[0] != ' ')
				Sbar_DrawCharacter ((6 * i + 1) * 8 - 2, -24,
									18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter ((6 * i + 2) * 8 - 2, -24,
									18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter ((6 * i + 3) * 8 - 2, -24,
									18 + num[2] - '0');
		}
	}

	flashon = 0;

	// items
	for (i = 0; i < 6; i++) {
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i))) {
			time = cl.item_gettime[17 + i];
			if (time && time > (cl.time - 2) && flashon) {	// Flash frame
				sb_updates = 0;
			} else {
				// FIXME: MISSIONHUD (this should have been !hip && maybe?)
				if (/*!hipnotic ||*/ (i > 1)) {
					Sbar_DrawPic (192 + i * 16, -16, sb_items[i]);
				}
			}
			if (time && time > cl.time - 2)
				sb_updates = 0;
		}
	}

	// FIXME: MISSIONHUD
//	// hipnotic items
//	if (hipnotic) {
//		for (i = 0; i < 2; i++) {
//			if (cl.stats[STAT_ITEMS] & (1 << (24 + i))) {
//				time = cl.item_gettime[24 + i];
//				if (time && time > cl.time - 2 && flashon) {	// flash
//					// frame
//					sb_updates = 0;
//				} else {
//					Sbar_DrawPic (288 + i * 16, -16, hsb_items[i]);
//				}
//				if (time && time > (cl.time - 2))
//					sb_updates = 0;
//			}
//		}
//	}

	// FIXME: MISSIONHUD
//	if (rogue) {						// new rogue items
//		for (i = 0; i < 2; i++) {
//			if (cl.stats[STAT_ITEMS] & (1 << (29 + i))) {
//				time = cl.item_gettime[29 + i];
//
//				if (time && time > (cl.time - 2) && flashon) {	// flash
//					// frame
//					sb_updates = 0;
//				} else {
//					Sbar_DrawPic (288 + i * 16, -16, rsb_items[i]);
//				}
//
//				if (time && time > (cl.time - 2))
//					sb_updates = 0;
//			}
//		}
//	} else {
		// sigils
		for (i = 0; i < 4; i++) {
			if (cl.stats[STAT_ITEMS] & (1 << (28 + i))) {
				time = cl.item_gettime[28 + i];
				if (time && time > cl.time - 2 && flashon) {	// flash
					// frame
					sb_updates = 0;
				} else {
					Sbar_DrawPic (320 - 32 + i * 8, -16, sb_sigil[i]);
				}
				if (time && time > cl.time - 2)
					sb_updates = 0;
			}
		}
//	}
}


void
Sbar_DrawFrags (void)
{
	int         i, k, l;
	int         top, bottom;
	int         x, y, f;
	int         xofs;
	char        num[12];
	scoreboard_t *s;

	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23;

	if (sbar_centered)
		xofs = (vid.width - 320) >> 1;
	else
		xofs = 0;

	y = vid.height - SBAR_HEIGHT - 23;

	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15) << 4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill (xofs + x * 8 + 10, y, 28, 4, top);
		Draw_Fill (xofs + x * 8 + 10, y + 4, 28, 3, bottom);

		// draw number
		f = s->frags;
		snprintf (num, sizeof (num), "%3i", f);

		Sbar_DrawCharacter ((x + 1) * 8, -24, num[0]);
		Sbar_DrawCharacter ((x + 2) * 8, -24, num[1]);
		Sbar_DrawCharacter ((x + 3) * 8, -24, num[2]);

		if (k == cl.viewentity - 1) {
			Sbar_DrawCharacter (x * 8 + 2, -24, 16);
			Sbar_DrawCharacter ((x + 4) * 8 - 4, -24, 17);
		}
		x += 4;
	}
}


void
Sbar_DrawFace (void)
{
	int         f, anim;

	// FIXME: MISSIONHUD
//	// PGM 01/19/97 - team color drawing
//	// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
//	if (rogue && (cl.maxclients != 1)
//		&& (teamplay->int_val > 3) && (teamplay->int_val < 7)) {
//
//		int         top, bottom;
//		int         xofs;
//		char        num[12];
//		scoreboard_t *s;
//
//		s = &cl.scores[cl.viewentity - 1];
//
//		// draw background
//		top = (s->colors & 0xf0);
//		bottom = ((s->colors & 15) << 4);
//		top = Sbar_ColorForMap (top);
//		bottom = Sbar_ColorForMap (bottom);
//
//		if (sbar_centered)
//			xofs = ((vid.width - 320) >> 1) + 113;
//		else
//			xofs = 113;
//
//		Sbar_DrawPic (112, 0, rsb_teambord);
//		Draw_Fill (xofs, vid.height - SBAR_HEIGHT + 3, 22, 9, top);
//		Draw_Fill (xofs, vid.height - SBAR_HEIGHT + 12, 22, 9, bottom);
//
//		// draw number
//		f = s->frags;
//		snprintf (num, sizeof (num), "%3i", f);
//
//		if (top == 8) {
//			if (num[0] != ' ')
//				Sbar_DrawCharacter (109, 3, 18 + num[0] - '0');
//			if (num[1] != ' ')
//				Sbar_DrawCharacter (116, 3, 18 + num[1] - '0');
//			if (num[2] != ' ')
//				Sbar_DrawCharacter (123, 3, 18 + num[2] - '0');
//		} else {
//			Sbar_DrawCharacter (109, 3, num[0]);
//			Sbar_DrawCharacter (116, 3, num[1]);
//			Sbar_DrawCharacter (123, 3, num[2]);
//		}
//
//		return;
//	}
	// PGM 01/19/97 - team color drawing

	if ((cl.stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY))
		== (IT_INVISIBILITY | IT_INVULNERABILITY)) {
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
		return;
	}

	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		Sbar_DrawPic (112, 0, sb_face_quad);
		return;
	}

	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		Sbar_DrawPic (112, 0, sb_face_invis);
		return;
	}

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		Sbar_DrawPic (112, 0, sb_face_invuln);
		return;
	}

	if (cl.stats[STAT_HEALTH] >= 100)
		f = 4;
	else
		f = cl.stats[STAT_HEALTH] / 20;

	if (cl.time <= cl.faceanimtime) {
		anim = 1;
		sb_updates = 0;					// make sure the anim gets drawn over
	} else {
		anim = 0;
	}
	Sbar_DrawPic (112, 0, sb_faces[f][anim]);
}


void
Sbar_DrawNormal (void)
{
	if (!headsup)
		Sbar_DrawPic (0, 0, sb_sbar);

	// FIXME: MISSIONHUD
//	if (hipnotic) {
//		if (cl.stats[STAT_ITEMS] & IT_KEY1)
//			Sbar_DrawPic (209, 3, sb_items[0]);
//		if (cl.stats[STAT_ITEMS] & IT_KEY2)
//			Sbar_DrawPic (209, 12, sb_items[1]);
//	}
	// armor
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		Sbar_DrawNum (24, 0, 666, 3, 1);
	} else {
		// FIXME: MISSIONHUD
//		if (rogue) {
//			Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3,
//						  cl.stats[STAT_ARMOR] <= 25);
//			if (cl.stats[STAT_ITEMS] & RIT_ARMOR3)
//				Sbar_DrawPic (0, 0, sb_armor[2]);
//			else if (cl.stats[STAT_ITEMS] & RIT_ARMOR2)
//				Sbar_DrawPic (0, 0, sb_armor[1]);
//			else if (cl.stats[STAT_ITEMS] & RIT_ARMOR1)
//				Sbar_DrawPic (0, 0, sb_armor[0]);
//		} else {
			Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3,
						  cl.stats[STAT_ARMOR] <= 25);
			if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
				Sbar_DrawPic (0, 0, sb_armor[2]);
			else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
				Sbar_DrawPic (0, 0, sb_armor[1]);
			else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
				Sbar_DrawPic (0, 0, sb_armor[0]);
//		}
	}

	// face
	Sbar_DrawFace ();

	// health
	Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3,
				  cl.stats[STAT_HEALTH] <= 25);

	// FIXME: MISSIONHUD
//	// ammo icon
//	if (rogue) {
//		if (cl.stats[STAT_ITEMS] & RIT_SHELLS)
//			Sbar_DrawPic (224, 0, sb_ammo[0]);
//		else if (cl.stats[STAT_ITEMS] & RIT_NAILS)
//			Sbar_DrawPic (224, 0, sb_ammo[1]);
//		else if (cl.stats[STAT_ITEMS] & RIT_ROCKETS)
//			Sbar_DrawPic (224, 0, sb_ammo[2]);
//		else if (cl.stats[STAT_ITEMS] & RIT_CELLS)
//			Sbar_DrawPic (224, 0, sb_ammo[3]);
//		else if (cl.stats[STAT_ITEMS] & RIT_LAVA_NAILS)
//			Sbar_DrawPic (224, 0, rsb_ammo[0]);
//		else if (cl.stats[STAT_ITEMS] & RIT_PLASMA_AMMO)
//			Sbar_DrawPic (224, 0, rsb_ammo[1]);
//		else if (cl.stats[STAT_ITEMS] & RIT_MULTI_ROCKETS)
//			Sbar_DrawPic (224, 0, rsb_ammo[2]);
//	} else {
		if (cl.stats[STAT_ITEMS] & IT_SHELLS)
			Sbar_DrawPic (224, 0, sb_ammo[0]);
		else if (cl.stats[STAT_ITEMS] & IT_NAILS)
			Sbar_DrawPic (224, 0, sb_ammo[1]);
		else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
			Sbar_DrawPic (224, 0, sb_ammo[2]);
		else if (cl.stats[STAT_ITEMS] & IT_CELLS)
			Sbar_DrawPic (224, 0, sb_ammo[3]);
//	}
	Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}


void
Sbar_Draw (int swap)
{
	// FIXME: MISSIONHUD
//	if (hipnotic || rogue) {
//		if (!cl_sbar->int_val) {
//			Cvar_SetValue (cl_sbar, 1);
//		}
//	}

	headsup = !(cl_sbar->int_val || scr_viewsize->int_val < 100);
	sbar_centered = (!headsup && !cl.gametype == GAME_DEATHMATCH);

	if ((sb_updates >= vid.numpages) && !headsup)
		return;

	if (scr_con_current == vid.height)
		return;							// console is full screen

	scr_copyeverything = 1;

	sb_updates++;

	// top line
	if (sb_lines > 24) {
		Sbar_DrawInventory ();
		if (cl.maxclients != 1)
			Sbar_DrawFrags ();
	}
	// main area
	if (sb_lines > 0) {
		if (sb_showscores || cl.stats[STAT_HEALTH] <= 0) {
			Sbar_DrawScoreboard ();
		} else {
			Sbar_DrawNormal ();
		}
	}

	if (!headsup && sbar_centered && sb_lines && vid.width > 320) {
		Draw_TileClear (0, vid.height - sb_lines, (vid.width - 320) >> 1,
						sb_lines);
		Draw_TileClear ((vid.width + 320) >> 1, vid.height - sb_lines,
						(vid.width - 320) >> 1, sb_lines);
	} else {
		if ((!headsup) && (!sbar_centered)) {
			Draw_TileClear (320, vid.height - sb_lines, vid.width - 320,
							sb_lines);
		}
	}
}


void
Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{
	char        str[12];
	char       *ptr;
	int         l, frame;

	l = Sbar_itoa (num, str);
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

		Draw_Pic (x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}


void
Sbar_DeathmatchOverlay (void)
{
	qpic_t     *pic;
	int         i, k, l;
	int         top, bottom;
	int         x, y, f;
	char        num[12];
	scoreboard_t *s;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	pic = Draw_CachePic ("gfx/ranking.lmp", true);
	Draw_Pic (160 - pic->width / 2, 0, pic);

	// scores
	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines;

	x = 80;
	y = 40;
	for (i = 0; i < l; i++) {
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15) << 4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill (x, y, 40, 4, top);
		Draw_Fill (x, y + 4, 40, 4, bottom);

		// draw number
		f = s->frags;
		snprintf (num, sizeof (num), "%3i", f);

		Draw_Character8 (x + 8, y, num[0]);
		Draw_Character8 (x + 16, y, num[1]);
		Draw_Character8 (x + 24, y, num[2]);

		if (k == cl.viewentity - 1)
			Draw_Character8 (x - 8, y, 12);

		// draw name
		Draw_String8 (x + 64, y, s->name);

		y += 10;
	}
}


void
Sbar_MiniDeathmatchOverlay (void)
{
	int         i, k, l;
	int         top, bottom;
	int         x, y, f;
	char        num[12];
	scoreboard_t *s;
	int         numlines;

	if (vid.width < 512 || !sb_lines)
		return;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	// scores
	Sbar_SortFrags ();

	// draw the text
	l = scoreboardlines;
	y = vid.height - sb_lines;
	numlines = sb_lines / 8;
	if (numlines < 3)
		return;

	// find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.viewentity - 1)
			break;

	if (i == scoreboardlines)			// we're not there
		i = 0;
	else								// figure out start
		i = i - numlines / 2;

	if (i > scoreboardlines - numlines)
		i = scoreboardlines - numlines;
	i = max (i, 0);

	x = 324;
	for (; i < scoreboardlines && y < vid.height - 8; i++) {
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15) << 4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill (x, y + 1, 40, 3, top);
		Draw_Fill (x, y + 4, 40, 4, bottom);

		// draw number
		f = s->frags;
		snprintf (num, sizeof (num), "%3i", f);

		Draw_Character8 (x + 8, y, num[0]);
		Draw_Character8 (x + 16, y, num[1]);
		Draw_Character8 (x + 24, y, num[2]);

		if (k == cl.viewentity - 1) {
			Draw_Character8 (x, y, 16);
			Draw_Character8 (x + 32, y, 17);
		}
		// draw name
		Draw_String8 (x + 48, y, s->name);

		y += 8;
	}
}


void
Sbar_IntermissionOverlay (void)
{
	qpic_t     *pic;
	int         dig;
	int         num;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH) {
		Sbar_DeathmatchOverlay ();
		return;
	}

	pic = Draw_CachePic ("gfx/complete.lmp", true);
	Draw_Pic (64, 24, pic);

	pic = Draw_CachePic ("gfx/inter.lmp", true);
	Draw_Pic (0, 56, pic);

	// time
	dig = cl.completed_time / 60;
	Sbar_IntermissionNumber (160, 64, dig, 3, 0);
	num = cl.completed_time - dig * 60;
	Draw_Pic (234, 64, sb_colon);
	Draw_Pic (246, 64, sb_nums[0][num / 10]);
	Draw_Pic (266, 64, sb_nums[0][num % 10]);

	Sbar_IntermissionNumber (160, 104, cl.stats[STAT_SECRETS], 3, 0);
	Draw_Pic (232, 104, sb_slash);
	Sbar_IntermissionNumber (240, 104, cl.stats[STAT_TOTALSECRETS], 3, 0);

	Sbar_IntermissionNumber (160, 144, cl.stats[STAT_MONSTERS], 3, 0);
	Draw_Pic (232, 144, sb_slash);
	Sbar_IntermissionNumber (240, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0);
}


void
Sbar_FinaleOverlay (void)
{
	qpic_t     *pic;

	scr_copyeverything = 1;

	pic = Draw_CachePic ("gfx/finale.lmp", true);
	Draw_Pic ((vid.width - pic->width) / 2, 16, pic);
}


/*
	Sbar_Init

	Initialize the status bar's data
*/
void
Sbar_Init (void)
{
	int         i;

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

	Cmd_AddCommand ("+showscores", Sbar_ShowScores, "No Description");
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores, "No Description");

	sb_sbar = Draw_PicFromWad ("sbar");
	sb_ibar = Draw_PicFromWad ("ibar");
	sb_scorebar = Draw_PicFromWad ("scorebar");

	// FIXME: MISSIONHUD
//	// MED 01/04/97 added new hipnotic weapons
//	if (hipnotic) {
//		hsb_weapons[0][0] = Draw_PicFromWad ("inv_laser");
//		hsb_weapons[0][1] = Draw_PicFromWad ("inv_mjolnir");
//		hsb_weapons[0][2] = Draw_PicFromWad ("inv_gren_prox");
//		hsb_weapons[0][3] = Draw_PicFromWad ("inv_prox_gren");
//		hsb_weapons[0][4] = Draw_PicFromWad ("inv_prox");
//
//		hsb_weapons[1][0] = Draw_PicFromWad ("inv2_laser");
//		hsb_weapons[1][1] = Draw_PicFromWad ("inv2_mjolnir");
//		hsb_weapons[1][2] = Draw_PicFromWad ("inv2_gren_prox");
//		hsb_weapons[1][3] = Draw_PicFromWad ("inv2_prox_gren");
//		hsb_weapons[1][4] = Draw_PicFromWad ("inv2_prox");
//
//		for (i = 0; i < 5; i++) {
//			hsb_weapons[2 + i][0] =
//				Draw_PicFromWad (va ("inva%i_laser", i + 1));
//			hsb_weapons[2 + i][1] =
//				Draw_PicFromWad (va ("inva%i_mjolnir", i + 1));
//			hsb_weapons[2 + i][2] =
//				Draw_PicFromWad (va ("inva%i_gren_prox", i + 1));
//			hsb_weapons[2 + i][3] =
//				Draw_PicFromWad (va ("inva%i_prox_gren", i + 1));
//			hsb_weapons[2 + i][4] = Draw_PicFromWad (va ("inva%i_prox", i + 1));
//		}
//
//		hsb_items[0] = Draw_PicFromWad ("sb_wsuit");
//		hsb_items[1] = Draw_PicFromWad ("sb_eshld");
//	}

	// FIXME: MISSIONHUD
//	if (rogue) {
//		rsb_invbar[0] = Draw_PicFromWad ("r_invbar1");
//		rsb_invbar[1] = Draw_PicFromWad ("r_invbar2");
//
//		rsb_weapons[0] = Draw_PicFromWad ("r_lava");
//		rsb_weapons[1] = Draw_PicFromWad ("r_superlava");
//		rsb_weapons[2] = Draw_PicFromWad ("r_gren");
//		rsb_weapons[3] = Draw_PicFromWad ("r_multirock");
//		rsb_weapons[4] = Draw_PicFromWad ("r_plasma");
//
//		rsb_items[0] = Draw_PicFromWad ("r_shield1");
//		rsb_items[1] = Draw_PicFromWad ("r_agrav1");
//
//		// PGM 01/19/97 - team color border
//		rsb_teambord = Draw_PicFromWad ("r_teambord");
//		// PGM 01/19/97 - team color border
//
//		rsb_ammo[0] = Draw_PicFromWad ("r_ammolava");
//		rsb_ammo[1] = Draw_PicFromWad ("r_ammomulti");
//		rsb_ammo[2] = Draw_PicFromWad ("r_ammoplasma");
//	}
}
