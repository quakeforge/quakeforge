/*
	in_svgalib.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <vga.h>
#include <vgakeyboard.h>
#include <vgamouse.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "compat.h"

static int  UseKeyboard = 1;
static int  UseMouse = 1;
static int  in_svgalib_inited = 0;

static unsigned short scantokey[3][128];
static int  mouse_buttons;
static int  mouse_buttonstate;
static int  mouse_oldbuttonstate;

static void IN_InitKeyboard (void);
static void IN_InitMouse (void);

#define ROTL(x,n) (((x)<<(n))|(x)>>(32-n))


static void
keyhandler (int scancode, int state)
{
	int         sc, ascii, key;
	int         press = state == KEY_EVENTPRESS;
	unsigned long mask = ~1L;
	static unsigned long shifts;

	sc = scancode & 0x7f;
	key = scantokey[0][sc];
	if ((shifts & 0x0c)) {
		ascii = scantokey[2][sc];
	} else if (shifts & 3) {
		ascii = scantokey[1][sc];
	} else {
		ascii = scantokey[0][sc];
	}
	if (ascii > 255)
		ascii = 0;
	switch (key) {
		case QFK_RSHIFT:
			shifts &= mask;
			shifts |= press;
			break;
		case QFK_LSHIFT:
			shifts &= ROTL(mask, 1);
			shifts |= ROTL(press, 1);
			break;
		case QFK_RCTRL:
			shifts &= ROTL(mask, 2);
			shifts |= ROTL(press, 2);
			break;
		case QFK_LCTRL:
			shifts &= ROTL(mask, 3);
			shifts |= ROTL(press, 3);
			break;
		default:
			break;
	}
	//Sys_MaskPrintf (SYS_vid, "%d %02x %02lx %04x %c\n", sc, press, shifts,
	//			 key, ascii > 32 && ascii < 127 ? ascii : '#');
	Key_Event (key, ascii, press);
}

static void
mousehandler (int buttonstate, int dx, int dy, int dz, int drx, int dry,
			  int drz)
{
	mouse_buttonstate = buttonstate;
	in_mouse_x += dx;
	in_mouse_y += dy;
	if (drx > 0) {
		Key_Event (QFM_WHEEL_UP, 0, 1);
		Key_Event (QFM_WHEEL_UP, 0, 0);
	} else if (drx < 0) {
		Key_Event (QFM_WHEEL_DOWN, 0, 1);
		Key_Event (QFM_WHEEL_DOWN, 0, 0);
	}
}

void
IN_LL_Init (void)
{
	if (COM_CheckParm ("-nokbd"))
		UseKeyboard = 0;
	if (COM_CheckParm ("-nomouse"))
		UseMouse = 0;

	if (UseKeyboard)
		IN_InitKeyboard ();
	if (UseMouse)
		IN_InitMouse ();

	in_svgalib_inited = 1;
	return;
}

void
IN_LL_Init_Cvars (void)
{
}

static void
IN_InitKeyboard (void)
{
	int         i;

	for (i = 0; i < 128; i++) {
		scantokey[0][i] = ' ';
	}

	scantokey[0][1] = QFK_ESCAPE;
	scantokey[0][2] = '1';
	scantokey[0][3] = '2';
	scantokey[0][4] = '3';
	scantokey[0][5] = '4';
	scantokey[0][6] = '5';
	scantokey[0][7] = '6';
	scantokey[0][8] = '7';
	scantokey[0][9] = '8';
	scantokey[0][10] = '9';
	scantokey[0][11] = '0';
	scantokey[0][12] = '-';
	scantokey[0][13] = '=';
	scantokey[0][14] = QFK_BACKSPACE;
	scantokey[0][15] = QFK_TAB;
	scantokey[0][16] = 'q';
	scantokey[0][17] = 'w';
	scantokey[0][18] = 'e';
	scantokey[0][19] = 'r';
	scantokey[0][20] = 't';
	scantokey[0][21] = 'y';
	scantokey[0][22] = 'u';
	scantokey[0][23] = 'i';
	scantokey[0][24] = 'o';
	scantokey[0][25] = 'p';
	scantokey[0][26] = '[';
	scantokey[0][27] = ']';
	scantokey[0][28] = QFK_RETURN;
	scantokey[0][29] = QFK_LCTRL;			/* left */
	scantokey[0][30] = 'a';
	scantokey[0][31] = 's';
	scantokey[0][32] = 'd';
	scantokey[0][33] = 'f';
	scantokey[0][34] = 'g';
	scantokey[0][35] = 'h';
	scantokey[0][36] = 'j';
	scantokey[0][37] = 'k';
	scantokey[0][38] = 'l';
	scantokey[0][39] = ';';
	scantokey[0][40] = '\'';
	scantokey[0][41] = '`';
	scantokey[0][42] = QFK_LSHIFT;			/* left */
	scantokey[0][43] = '\\';
	scantokey[0][44] = 'z';
	scantokey[0][45] = 'x';
	scantokey[0][46] = 'c';
	scantokey[0][47] = 'v';
	scantokey[0][48] = 'b';
	scantokey[0][49] = 'n';
	scantokey[0][50] = 'm';
	scantokey[0][51] = ',';
	scantokey[0][52] = '.';
	scantokey[0][53] = '/';
	scantokey[0][54] = QFK_RSHIFT;			/* right */
	scantokey[0][55] = QFK_KP_MULTIPLY;
	scantokey[0][56] = QFK_LALT;				/* left */
	scantokey[0][57] = ' ';
	scantokey[0][58] = QFK_CAPSLOCK;
	scantokey[0][59] = QFK_F1;
	scantokey[0][60] = QFK_F2;
	scantokey[0][61] = QFK_F3;
	scantokey[0][62] = QFK_F4;
	scantokey[0][63] = QFK_F5;
	scantokey[0][64] = QFK_F6;
	scantokey[0][65] = QFK_F7;
	scantokey[0][66] = QFK_F8;
	scantokey[0][67] = QFK_F9;
	scantokey[0][68] = QFK_F10;
	scantokey[0][69] = QFK_NUMLOCK;
	scantokey[0][70] = QFK_SCROLLOCK;
	scantokey[0][71] = QFK_KP7;
	scantokey[0][72] = QFK_KP8;
	scantokey[0][73] = QFK_KP9;
	scantokey[0][74] = QFK_KP_MINUS;
	scantokey[0][75] = QFK_KP4;
	scantokey[0][76] = QFK_KP5;
	scantokey[0][77] = QFK_KP6;
	scantokey[0][79] = QFK_KP1;
	scantokey[0][78] = QFK_KP_PLUS;
	scantokey[0][80] = QFK_KP2;
	scantokey[0][81] = QFK_KP3;
	scantokey[0][82] = QFK_KP0;
	scantokey[0][83] = QFK_KP_PERIOD;
	/* 84 to 86 not used */
	scantokey[0][87] = QFK_F11;
	scantokey[0][88] = QFK_F12;
	/* 89 to 95 not used */
	scantokey[0][96] = QFK_KP_ENTER;			/* keypad enter */
	scantokey[0][97] = QFK_RCTRL;			/* right */
	scantokey[0][98] = QFK_KP_DIVIDE;
	scantokey[0][99] = QFK_PRINT;			/* print screen */
	scantokey[0][100] = QFK_RALT;			/* right */

	scantokey[0][101] = QFK_PAUSE;			/* break */
	scantokey[0][102] = QFK_HOME;
	scantokey[0][103] = QFK_UP;
	scantokey[0][104] = QFK_PAGEUP;
	scantokey[0][105] = QFK_LEFT;
	scantokey[0][106] = QFK_RIGHT;
	scantokey[0][107] = QFK_END;
	scantokey[0][108] = QFK_DOWN;
	scantokey[0][109] = QFK_PAGEDOWN;
	scantokey[0][110] = QFK_INSERT;
	scantokey[0][111] = QFK_DELETE;
	scantokey[0][119] = QFK_PAUSE;

	memcpy (scantokey[1], scantokey[0], sizeof (scantokey[1]));
	memcpy (scantokey[2], scantokey[0], sizeof (scantokey[2]));

	scantokey[1][2] = '!';
	scantokey[1][3] = '@';
	scantokey[1][4] = '#';
	scantokey[1][5] = '$';
	scantokey[1][6] = '%';
	scantokey[1][7] = '^';
	scantokey[1][8] = '&';
	scantokey[1][9] = '*';
	scantokey[1][10] = '(';
	scantokey[1][11] = ')';
	scantokey[1][12] = '_';
	scantokey[1][13] = '+';
	scantokey[1][16] = 'Q';
	scantokey[1][17] = 'W';
	scantokey[1][18] = 'E';
	scantokey[1][19] = 'R';
	scantokey[1][20] = 'T';
	scantokey[1][21] = 'Y';
	scantokey[1][22] = 'U';
	scantokey[1][23] = 'I';
	scantokey[1][24] = 'O';
	scantokey[1][25] = 'P';
	scantokey[1][26] = '[';
	scantokey[1][27] = ']';
	scantokey[1][30] = 'A';
	scantokey[1][31] = 'S';
	scantokey[1][32] = 'D';
	scantokey[1][33] = 'F';
	scantokey[1][34] = 'G';
	scantokey[1][35] = 'H';
	scantokey[1][36] = 'J';
	scantokey[1][37] = 'K';
	scantokey[1][38] = 'L';
	scantokey[1][39] = ':';
	scantokey[1][40] = '"';
	scantokey[1][41] = '~';
	scantokey[1][43] = '|';
	scantokey[1][44] = 'Z';
	scantokey[1][45] = 'X';
	scantokey[1][46] = 'C';
	scantokey[1][47] = 'V';
	scantokey[1][48] = 'B';
	scantokey[1][49] = 'N';
	scantokey[1][50] = 'M';
	scantokey[1][51] = '<';
	scantokey[1][52] = '>';
	scantokey[1][53] = '?';

	scantokey[2][7] = 30;
	scantokey[2][12] = 31;
	scantokey[2][16] = 'q' - 96;
	scantokey[2][17] = 'w' - 96;
	scantokey[2][18] = 'e' - 96;
	scantokey[2][19] = 'r' - 96;
	scantokey[2][20] = 't' - 96;
	scantokey[2][21] = 'y' - 96;
	scantokey[2][22] = 'u' - 96;
	scantokey[2][23] = 'i' - 96;
	scantokey[2][24] = 'o' - 96;
	scantokey[2][25] = 'p' - 96;
	scantokey[2][26] = 27;
	scantokey[2][27] = 29;
	scantokey[2][30] = 'a' - 96;
	scantokey[2][31] = 's' - 96;
	scantokey[2][32] = 'd' - 96;
	scantokey[2][33] = 'f' - 96;
	scantokey[2][34] = 'g' - 96;
	scantokey[2][35] = 'h' - 96;
	scantokey[2][36] = 'j' - 96;
	scantokey[2][37] = 'k' - 96;
	scantokey[2][38] = 'l' - 96;
	scantokey[2][40] = 28;
	scantokey[2][44] = 'z' - 96;
	scantokey[2][45] = 'x' - 96;
	scantokey[2][46] = 'c' - 96;
	scantokey[2][47] = 'v' - 96;
	scantokey[2][48] = 'b' - 96;
	scantokey[2][49] = 'n' - 96;
	scantokey[2][50] = 'm' - 96;

	if (keyboard_init ()) {
		Sys_Error ("keyboard_init() failed");
	}
	keyboard_seteventhandler (keyhandler);
}

static void
IN_InitMouse (void)
{
	int         mtype;
	int         mouserate = MOUSE_DEFAULTSAMPLERATE;
	const char *mousedev;

	mouse_buttons = 3;

	mtype = vga_getmousetype ();

	mousedev = "/dev/mouse";
	if (getenv ("MOUSEDEV"))
		mousedev = getenv ("MOUSEDEV");
	if (COM_CheckParm ("-mdev")) {
		mousedev = com_argv[COM_CheckParm ("-mdev") + 1];
	}

	if (getenv ("MOUSERATE"))
		mouserate = atoi (getenv ("MOUSERATE"));
	if (COM_CheckParm ("-mrate")) {
		mouserate = atoi (com_argv[COM_CheckParm ("-mrate") + 1]);
	}
#if 0
	Sys_MaskPrintf (SYS_vid, "Mouse: dev=%s,type=%s,speed=%d\n",
			mousedev, mice[mtype].name, mouserate);
#endif
	//FIXME: vga_init() opens the mouse automoatically
	//       closing it to ensure its opened how we want it
	mouse_close();
	if (mouse_init ((char *)mousedev, mtype, mouserate)) {
		Sys_MaskPrintf (SYS_vid,
						"No mouse found. Check your libvga.conf mouse settings"
						" and that the mouse\n"
						"device has appropriate permission settings.\n");
		UseMouse = 0;
	} else {
		mouse_seteventhandler ((void *) mousehandler);
	}
	in_mouse_avail = 1;
}

void
IN_LL_Shutdown (void)
{
	Sys_MaskPrintf (SYS_vid, "IN_LL_Shutdown\n");

	if (UseMouse)
		mouse_close ();
	if (UseKeyboard)
		keyboard_close ();
	in_svgalib_inited = 0;
}


void
IN_LL_ProcessEvents (void)
{
	if (!in_svgalib_inited)
		return;

	if (UseKeyboard) {
		while ((keyboard_update ()));
	}

	if (UseMouse) {
		/* Poll mouse values */
		while (mouse_update ());

		/* Perform button actions */
		if ((mouse_buttonstate & MOUSE_LEFTBUTTON) &&
			!(mouse_oldbuttonstate & MOUSE_LEFTBUTTON))
				Key_Event (QFM_BUTTON1, 0, true);
		else if (!(mouse_buttonstate & MOUSE_LEFTBUTTON) &&
				 (mouse_oldbuttonstate & MOUSE_LEFTBUTTON))
			Key_Event (QFM_BUTTON1, 0, false);

		if ((mouse_buttonstate & MOUSE_RIGHTBUTTON) &&
			!(mouse_oldbuttonstate & MOUSE_RIGHTBUTTON))
				Key_Event (QFM_BUTTON2, 0, true);
		else if (!(mouse_buttonstate & MOUSE_RIGHTBUTTON) &&
				 (mouse_oldbuttonstate & MOUSE_RIGHTBUTTON))
			Key_Event (QFM_BUTTON2, 0, false);

		if ((mouse_buttonstate & MOUSE_MIDDLEBUTTON) &&
			!(mouse_oldbuttonstate & MOUSE_MIDDLEBUTTON))
				Key_Event (QFM_BUTTON3, 0, true);
		else if (!(mouse_buttonstate & MOUSE_MIDDLEBUTTON) &&
				 (mouse_oldbuttonstate & MOUSE_MIDDLEBUTTON))
			Key_Event (QFM_BUTTON3, 0, false);

		mouse_oldbuttonstate = mouse_buttonstate;
	}
}

void
IN_LL_Grab_Input (int grab)
{
}

void
IN_LL_ClearStates (void)
{
}
