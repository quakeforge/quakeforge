/*
	vid_dos.c

	@description@

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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>

#include "d_local.h"
#include "dosisms.h"
#include "vid_dos.h"

int         vid_modenum;
vmode_t    *pcurrentmode = NULL;
int         vid_testingmode, vid_realmode;
double      vid_testendtime;

cvar_t     *vid_mode;
cvar_t     *vid_wait;
cvar_t     *vid_nopageflip;
cvar_t     *_vid_wait_override;
cvar_t     *_vid_default_mode;
cvar_t     *_vid_default_mode_win;
cvar_t     *vid_config_x;
cvar_t     *vid_config_y;
cvar_t     *vid_stretch_by_2;
cvar_t     *_windowed_mouse;
cvar_t     *vid_fullscreen_mode;
cvar_t     *vid_windowed_mode;
cvar_t     *block_switch;
cvar_t     *vid_window_x;
cvar_t     *vid_window_y;

int         d_con_indirect = 0;

int         numvidmodes;
vmode_t    *pvidmodes;

static int  firstupdate = 1;

extern regs_t regs;

void        VID_TestMode_f (void);
void        VID_NumModes_f (void);
void        VID_DescribeCurrentMode_f (void);
void        VID_DescribeMode_f (void);
void        VID_DescribeModes_f (void);

byte        vid_current_palette[768];	// save for mode changes


static qboolean nomodecheck = false;

unsigned short d_8to16table[256];		// not used in 8 bpp mode
unsigned    d_8to24table[256];			// not used in 8 bpp mode

void        VID_MenuDraw (void);
void        VID_MenuKey (int key);

void
VID_InitCvars (void)
{
}


/*
================
VID_Init
================
*/
void
VID_Init (unsigned char *palette)
{
	vid_mode = Cvar_Get ("vid_mode", "0", CVAR_NONE, NULL, "None");
	vid_wait = Cvar_Get ("vid_wait", "0", CVAR_NONE, NULL, "None");
	vid_nopageflip = Cvar_Get ("vid_nopageflip", "0", CVAR_ARCHIVE, NULL,
			"None");
	_vid_wait_override =
		Cvar_Get ("_vid_wait_override", "0", CVAR_ARCHIVE, NULL, "None");
	_vid_default_mode =
		Cvar_Get ("_vid_default_mode", "0", CVAR_ARCHIVE, NULL, "None");
	_vid_default_mode_win =
		Cvar_Get ("_vid_default_mode_win", "3", CVAR_ARCHIVE, NULL, "None");
	vid_config_x = Cvar_Get ("vid_config_x", "800", CVAR_ARCHIVE, NULL, "None");
	vid_config_y = Cvar_Get ("vid_config_y", "600", CVAR_ARCHIVE, NULL, "None");
	vid_stretch_by_2 = Cvar_Get ("vid_stretch_by_2", "1", CVAR_ARCHIVE, NULL,
			"None");
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE, NULL,
			"None");
	vid_fullscreen_mode =
		Cvar_Get ("vid_fullscreen_mode", "3", CVAR_ARCHIVE, NULL, "None");
	vid_windowed_mode =
		Cvar_Get ("vid_windowed_mode", "0", CVAR_ARCHIVE, NULL, "None");
	block_switch = Cvar_Get ("block_switch", "0", CVAR_ARCHIVE, NULL, "None");

	Cmd_AddCommand ("vid_testmode", VID_TestMode_f, "No Description");
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f, "No Description");
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f,
					"No Description");
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f, "No Description");
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f, "No Description");

// set up the mode list; note that later inits link in their modes ahead of
// earlier ones, so the standard VGA modes are always first in the list. This
// is important because mode 0 must always be VGA mode 0x13
	if (!COM_CheckParm ("-stdvid"))
		VID_InitExtra ();
	VGA_Init ();

	vid_testingmode = 0;

	vid_modenum = vid_mode->int_val;

	VID_SetMode (vid_modenum, palette);

	vid_realmode = vid_modenum;

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
}


/*
=================
VID_GetModePtr
=================
*/
vmode_t    *
VID_GetModePtr (int modenum)
{
	vmode_t    *pv;

	pv = pvidmodes;
	if (!pv)
		Sys_Error ("VID_GetModePtr: empty vid mode list");

	while (modenum--) {
		pv = pv->pnext;
		if (!pv)
			Sys_Error ("VID_GetModePtr: corrupt vid mode list");
	}

	return pv;
}

/*
================
VID_NumModes
================
*/
int
VID_NumModes ()
{
	return (numvidmodes);
}


/*
================
VID_ModeInfo
================
*/
char       *
VID_ModeInfo (int modenum, char **ppheader)
{
	static char *badmodestr = "Bad mode number";
	vmode_t    *pv;

	pv = VID_GetModePtr (modenum);

	if (!pv) {
		if (ppheader)
			*ppheader = NULL;
		return badmodestr;
	} else {
		if (ppheader)
			*ppheader = pv->header;
		return pv->name;
	}
}


/*
================
VID_SetMode 
================
*/
int
VID_SetMode (int modenum, unsigned char *palette)
{
	int         stat;
	vmode_t    *pnewmode, *poldmode;

	if ((modenum >= numvidmodes) || (modenum < 0)) {
		Cvar_SetValue (vid_mode, (float) vid_modenum);

		nomodecheck = true;
		Con_Printf ("No such video mode: %d\n", modenum);
		nomodecheck = false;

		if (pcurrentmode == NULL) {
			modenum = 0;				// mode hasn't been set yet, so
			// initialize to base
			// mode since they gave us an invalid initial mode
		} else {
			return 0;
		}
	}

	pnewmode = VID_GetModePtr (modenum);

	if (pnewmode == pcurrentmode)
		return 1;						// already in the desired mode

// initialize the new mode
	poldmode = pcurrentmode;
	pcurrentmode = pnewmode;

	vid.width = pcurrentmode->width;
	vid.height = pcurrentmode->height;
	vid.aspect = pcurrentmode->aspect;
	vid.rowbytes = pcurrentmode->rowbytes;

	stat = (*pcurrentmode->setmode) (&vid, pcurrentmode);

	if (stat < 1) {
		if (stat == 0) {
			// real, hard failure that requires resetting the mode
			if (!VID_SetMode (vid_modenum, palette))	// restore prior mode
				Sys_Error ("VID_SetMode: Unable to set any mode, probably "
						   "because there's not enough memory available");
			Con_Printf ("Failed to set mode %d\n", modenum);
			return 0;
		} else if (stat == -1) {
			// not enough memory; just put things back the way they were
			pcurrentmode = poldmode;
			vid.width = pcurrentmode->width;
			vid.height = pcurrentmode->height;
			vid.aspect = pcurrentmode->aspect;
			vid.rowbytes = pcurrentmode->rowbytes;
			return 0;
		} else {
			Sys_Error ("VID_SetMode: invalid setmode return code %d");
		}
	}

	(*pcurrentmode->setpalette) (&vid, pcurrentmode, palette);

	vid_modenum = modenum;
	Cvar_SetValue (vid_mode, (float) vid_modenum);

	nomodecheck = true;
	Con_Printf ("%s\n", VID_ModeInfo (vid_modenum, NULL));
	nomodecheck = false;

	vid.recalc_refdef = 1;

	return 1;
}


/*
================
VID_SetPalette
================
*/
void
VID_SetPalette (unsigned char *palette)
{
	if (palette != vid_current_palette)
		Q_memcpy (vid_current_palette, palette, 768);
	(*pcurrentmode->setpalette) (&vid, pcurrentmode, vid_current_palette);
}


/*
================
VID_ShiftPalette
================
*/
void
VID_ShiftPalette (unsigned char *palette)
{

	VID_SetPalette (palette);
}


/*
================
VID_Shutdown
================
*/
void
VID_Shutdown (void)
{

	regs.h.ah = 0;
	regs.h.al = 0x3;
	dos_int86 (0x10);

	vid_testingmode = 0;
}


/*
================
VID_Update
================
*/
void
VID_Update (vrect_t *rects)
{
	if (firstupdate && _vid_default_mode->int_val) {
		if (_vid_default_mode->int_val >= numvidmodes)
			Cvar_SetValue (_vid_default_mode, 0);

		firstupdate = 0;
		Cvar_SetValue (vid_mode, _vid_default_mode->int_val);
	}

	(*pcurrentmode->swapbuffers) (&vid, pcurrentmode, rects);

	if (!nomodecheck) {
		if (vid_testingmode) {
			if (realtime >= vid_testendtime) {
				VID_SetMode (vid_realmode, vid_current_palette);
				vid_testingmode = 0;
			}
		} else {
			if (vid_mode->int_val != vid_realmode) {
				VID_SetMode (vid_mode->int_val, vid_current_palette);
				Cvar_SetValue (vid_mode, (float) vid_modenum);
				// so if mode set fails, we don't keep on
				// trying to set that mode
				vid_realmode = vid_modenum;
			}
		}
	}
}


/*
=================
VID_NumModes_f
=================
*/
void
VID_NumModes_f (void)
{
	int         nummodes;

	nummodes = VID_NumModes ();
	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", VID_NumModes ());
	else
		Con_Printf ("%d video modes are available\n", VID_NumModes ());
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void
VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_ModeInfo (vid_modenum, NULL));
}


/*
=================
VID_DescribeMode_f
=================
*/
void
VID_DescribeMode_f (void)
{
	int         modenum;

	modenum = Q_atoi (Cmd_Argv (1));

	Con_Printf ("%s\n", VID_ModeInfo (modenum, NULL));
}


/*
=================
VID_DescribeModes_f
=================
*/
void
VID_DescribeModes_f (void)
{
	int         i, nummodes;
	char       *pinfo, *pheader;
	vmode_t    *pv;
	qboolean    na;

	na = false;

	nummodes = VID_NumModes ();
	for (i = 0; i < nummodes; i++) {
		pv = VID_GetModePtr (i);
		pinfo = VID_ModeInfo (i, &pheader);
		if (pheader)
			Con_Printf ("\n%s\n", pheader);

		if (VGA_CheckAdequateMem (pv->width, pv->height, pv->rowbytes,
								  (pv->numpages == 1)
								  || vid_nopageflip->int_val)) {
			Con_Printf ("%2d: %s\n", i, pinfo);
		} else {
			Con_Printf ("**: %s\n", pinfo);
			na = true;
		}
	}

	if (na) {
		Con_Printf ("\n[**: not enough system RAM for mode]\n");
	}
}


/*
=================
VID_GetModeDescription
=================
*/
char       *
VID_GetModeDescription (int mode)
{
	char       *pinfo, *pheader;
	vmode_t    *pv;

	pv = VID_GetModePtr (mode);
	pinfo = VID_ModeInfo (mode, &pheader);

	if (VGA_CheckAdequateMem (pv->width, pv->height, pv->rowbytes,
							  (pv->numpages == 1) || vid_nopageflip->int_val)) {
		return pinfo;
	} else {
		return NULL;
	}
}


/*
=================
VID_TestMode_f
=================
*/
void
VID_TestMode_f (void)
{
	int         modenum;
	double      testduration;

	if (!vid_testingmode) {
		modenum = Q_atoi (Cmd_Argv (1));

		if (VID_SetMode (modenum, vid_current_palette)) {
			vid_testingmode = 1;
			testduration = Q_atof (Cmd_Argv (2));
			if (testduration == 0)
				testduration = 5.0;
			vid_testendtime = realtime + testduration;
		}
	}
}


/*
================
D_BeginDirectRect
================
*/
void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{

	if (!vid.direct || !pcurrentmode)
		return;

	if ((width > 24) || (height > 24) || (width < 1) || (height < 1))
		return;

	if (width & 0x03)
		return;

	(*pcurrentmode->begindirectrect) (&vid, pcurrentmode, x, y, pbitmap, width,
									  height);
}


/*
================
D_EndDirectRect
================
*/
void
D_EndDirectRect (int x, int y, int width, int height)
{

	if (!vid.direct || !pcurrentmode)
		return;

	if ((width > 24) || (height > 24) || (width < 1) || (height < 1))
		return;

	if ((width & 0x03) || (height & 0x03))
		return;

	(*pcurrentmode->enddirectrect) (&vid, pcurrentmode, x, y, width, height);
}


//===========================================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int  vid_line, vid_wmodes, vid_column_size;

typedef struct {
	int         modenum;
	char       *desc;
	int         iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE	11

#define MAX_MODEDESCS	(MAX_COLUMN_SIZE*3)

static modedesc_t modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void
VID_MenuDraw (void)
{
	qpic_t     *p;
	char       *ptr;
	int         nummodes, i, j, column, row, dup;
	char        temp[100];

	vid_wmodes = 0;
	nummodes = VID_NumModes ();

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ((320 - p->width) / 2, 4, p);

	for (i = 0; i < nummodes; i++) {
		if (vid_wmodes < MAX_MODEDESCS) {
			if (i != 1) {
				ptr = VID_GetModeDescription (i);

				if (ptr) {
					dup = 0;

					for (j = 0; j < vid_wmodes; j++) {
						if (!strcmp (modedescs[j].desc, ptr)) {
							if (modedescs[j].modenum != 0) {
								modedescs[j].modenum = i;
								dup = 1;

								if (i == vid_modenum)
									modedescs[j].iscur = 1;
							} else {
								dup = 1;
							}

							break;
						}
					}

					if (!dup) {
						modedescs[vid_wmodes].modenum = i;
						modedescs[vid_wmodes].desc = ptr;
						modedescs[vid_wmodes].iscur = 0;

						if (i == vid_modenum)
							modedescs[vid_wmodes].iscur = 1;

						vid_wmodes++;
					}
				}
			}
		}
	}

	vid_column_size = (vid_wmodes + 2) / 3;

	column = 16;
	row = 36;

	for (i = 0; i < vid_wmodes; i++) {
		if (modedescs[i].iscur)
			M_PrintWhite (column, row, modedescs[i].desc);
		else
			M_Print (column, row, modedescs[i].desc);

		row += 8;

		if ((i % vid_column_size) == (vid_column_size - 1)) {
			column += 13 * 8;
			row = 36;
		}
	}

// line cursor
	if (vid_testingmode) {
		snprintf (temp, sizeof (temp), "TESTING %s", modedescs[vid_line].desc);
		M_Print (13 * 8, 36 + MAX_COLUMN_SIZE * 8 + 8 * 4, temp);
		M_Print (9 * 8, 36 + MAX_COLUMN_SIZE * 8 + 8 * 6,
				 "Please wait 5 seconds...");
	} else {
		M_Print (9 * 8, 36 + MAX_COLUMN_SIZE * 8 + 8,
				 "Press Enter to set mode");
		M_Print (6 * 8, 36 + MAX_COLUMN_SIZE * 8 + 8 * 3,
				 "T to test mode for 5 seconds");
		ptr = VID_GetModeDescription (vid_modenum);
		snprintf (temp, sizeof (temp), "D to make %s the default", ptr);
		M_Print (6 * 8, 36 + MAX_COLUMN_SIZE * 8 + 8 * 5, temp);
		ptr = VID_GetModeDescription (_vid_default_mode->int_val);

		if (ptr) {
			snprintf (temp, sizeof (temp), "Current default is %s", ptr);
			M_Print (7 * 8, 36 + MAX_COLUMN_SIZE * 8 + 8 * 6, temp);
		}

		M_Print (15 * 8, 36 + MAX_COLUMN_SIZE * 8 + 8 * 8, "Esc to exit");

		row = 36 + (vid_line % vid_column_size) * 8;
		column = 8 + (vid_line / vid_column_size) * 13 * 8;

		M_DrawCharacter (column, row, 12 + ((int) (realtime * 4) & 1));
	}
}


/*
================
VID_MenuKey
================
*/
void
VID_MenuKey (int key)
{
	if (vid_testingmode)
		return;

	switch (key) {
		case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

		case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line--;

		if (vid_line < 0)
			vid_line = vid_wmodes - 1;
		break;

		case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line++;

		if (vid_line >= vid_wmodes)
			vid_line = 0;
		break;

		case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line -= vid_column_size;

		if (vid_line < 0) {
			vid_line += ((vid_wmodes + (vid_column_size - 1)) /
						 vid_column_size) * vid_column_size;

			while (vid_line >= vid_wmodes)
				vid_line -= vid_column_size;
		}
		break;

		case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		vid_line += vid_column_size;

		if (vid_line >= vid_wmodes) {
			vid_line -= ((vid_wmodes + (vid_column_size - 1)) /
						 vid_column_size) * vid_column_size;

			while (vid_line < 0)
				vid_line += vid_column_size;
		}
		break;

		case K_ENTER:
		S_LocalSound ("misc/menu1.wav");
		VID_SetMode (modedescs[vid_line].modenum, vid_current_palette);
		break;

		case 'T':
		case 't':
		S_LocalSound ("misc/menu1.wav");
		if (VID_SetMode (modedescs[vid_line].modenum, vid_current_palette)) {
			vid_testingmode = 1;
			vid_testendtime = realtime + 5.0;
		}
		break;

		case 'D':
		case 'd':
		S_LocalSound ("misc/menu1.wav");
		firstupdate = 0;
		Cvar_SetValue (_vid_default_mode, vid_modenum);
		break;

		default:
		break;
	}
}


void
VID_HandlePause (qboolean pause)
{
}
