/*
	console.c

	(description)

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdarg.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/vfs.h"

#include "compat.h"

int         con_ormask;
console_t   con_main;
console_t   con_chat;
console_t  *con;						// point to either con_main or con_chat

int         con_linewidth;				// characters across screen
int         con_totallines;				// total lines in console scrollback

float       con_cursorspeed = 4;

cvar_t     *con_notifytime;				// seconds

#define	NUM_CON_TIMES 4
float       con_times[NUM_CON_TIMES];	// realtime time the line was generated
										// for transparent notify lines

int         con_vislines;
int         con_notifylines;			// scan lines to clear for notify lines

qboolean    con_debuglog;

#define		MAXCMDLINE	256

extern char key_lines[32][MAXCMDLINE];
extern int  edit_line;
extern int  key_linepos;

qboolean    con_initialized;


void
Con_ToggleConsole_f (void)
{
	Key_ClearTyping ();
/*XXX
	if (key_dest == key_console) {
		if (cls.state == ca_active)
			key_dest = key_game;
	} else
		key_dest = key_console;
*/
	Con_ClearNotify ();
}


void
Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (key_dest == key_console) {
/*XXX
		if (cls.state == ca_active)
*/
			key_dest = key_game;
			game_target = IMT_DEFAULT;
	} else {
		key_dest = key_console;
		game_target = IMT_CONSOLE;
	}

	Con_ClearNotify ();
}


void
Con_Clear_f (void)
{
	con_main.numlines = 0;
	con_chat.numlines = 0;
	memset (con_main.text, ' ', CON_TEXTSIZE);
	memset (con_chat.text, ' ', CON_TEXTSIZE);
	con_main.display = con_main.current;
}


void
Con_ClearNotify (void)
{
	int         i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}


void
Con_MessageMode_f (void)
{
/*XXX
	if (cls.state != ca_active)
		return;
*/
	chat_team = false;
	key_dest = key_message;
}


void
Con_MessageMode2_f (void)
{
/*XXX
	if (cls.state != ca_active)
		return;
*/
	chat_team = true;
	key_dest = key_message;
}


void
Con_Resize (console_t *con)
{
	int         i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char        tbuf[CON_TEXTSIZE];

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1) {					// video hasn't been initialized yet
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con->text, ' ', CON_TEXTSIZE);
	} else {
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy (tbuf, con->text, CON_TEXTSIZE);
		memset (con->text, ' ', CON_TEXTSIZE);

		for (i = 0; i < numlines; i++) {
			for (j = 0; j < numchars; j++) {
				con->text[(con_totallines - 1 - i) * con_linewidth + j] =
					tbuf[((con->current - i + oldtotallines) %
						  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con->current = con_totallines - 1;
	con->display = con->current;
}


/*
	Con_CheckResize

	If the line width has changed, reformat the buffer.
*/
void
Con_CheckResize (void)
{
	Con_Resize (&con_main);
	Con_Resize (&con_chat);
}


static void
C_Init (void)
{
	con_debuglog = COM_CheckParm ("-condebug");

	con = &con_main;
	con_linewidth = -1;
	Con_CheckResize ();

	Con_Printf ("Console initialized.\n");

	// register our commands
	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f,
					"Toggle the console up and down");
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f,
					"Toggle the console up and down");
	Cmd_AddCommand ("messagemode", Con_MessageMode_f,
					"Prompt to send a message to everyone");
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f,
					"Prompt to send a message to only people on your team");
	Cmd_AddCommand ("clear", Con_Clear_f, "Clear the console");
	con_initialized = true;
}

static void
C_Shutdown (void)
{
}


void
Con_Init_Cvars (void)
{
	con_notifytime =
		Cvar_Get ("con_notifytime", "3", CVAR_NONE, NULL,
				  "How long in seconds messages are displayed on screen");
}


void
Con_Linefeed (void)
{
	con->x = 0;
	if (con->display == con->current)
		con->display++;
	con->current++;
	if (con->numlines < con_totallines)
		con->numlines++;
	memset (&con->text[(con->current % con_totallines) * con_linewidth],
			' ', con_linewidth);
}


/*
	C_Print

	Handles cursor positioning, line wrapping, etc
	All console printing must go through this in order to be logged to disk
	If no console is visible, the notify window will pop up.
*/
void
C_Print (const char *fmt, va_list args)
{
	static char *buffer;
	static int  buffer_size;
	int         size;
	int         y;
	int         c, l;
	static int  cr;
	int         mask;
	const char *txt;

	size = vsnprintf (buffer, buffer_size, fmt, args);
	if (size + 1 > buffer_size) {
		buffer_size = (size + 1 + 1024) % 1024; // 1k multiples
		buffer = realloc (buffer, buffer_size);
		if (!buffer)
			Sys_Error ("console: could not allocate %d bytes\n",
					   buffer_size);
		vsnprintf (buffer, buffer_size, fmt, args);
	}
	txt = buffer;

	// echo to debugging console
	Sys_Printf ("%s", buffer);

	// log all messages to file
	if (con_debuglog)
		Sys_DebugLog (va ("%s/qconsole.log", com_gamedir), "%s", txt);

	if (!con_initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2) {
		mask = 128;						// go to colored text
		txt++;
	} else
		mask = 0;

	while ((c = *txt)) {
		// count word length
		for (l = 0; l < con_linewidth; l++)
			if (txt[l] <= ' ')
				break;

		// word wrap
		if (l != con_linewidth && (con->x + l > con_linewidth))
			con->x = 0;

		txt++;

		if (cr) {
			con->current--;
			cr = false;
		}


		if (!con->x) {
			Con_Linefeed ();
			// mark time for transparent overlay
/*XXX
			if (con->current >= 0)
				con_times[con->current % NUM_CON_TIMES] = realtime;
*/
		}

		switch (c) {
			case '\n':
				con->x = 0;
				break;

			case '\r':
				con->x = 0;
				cr = 1;
				break;

			default:					// display character and advance
				y = con->current % con_totallines;
				con->text[y * con_linewidth + con->x] = c | mask | con_ormask;
				con->x++;
				if (con->x >= con_linewidth)
					con->x = 0;
				break;
		}

	}
}


/*
	DRAWING
*/


/*
	Con_DrawInput

	The input line scrolls horizontally if typing goes beyond the right edge
*/
void
Con_DrawInput (void)
{
	int         y;
	int         i;
	char       *text;
	char        temp[MAXCMDLINE];

/*XXX
	if (key_dest != key_console && cls.state == ca_active)
		return;				// don't draw anything (always draw if not active)
*/

	text = strcpy (temp, key_lines[edit_line]);

	// fill out remainder with spaces
	for (i = strlen (text); i < MAXCMDLINE; i++)
		text[i] = ' ';

	// add the cursor frame
/*XXX
	if ((int) (realtime * con_cursorspeed) & 1)
		text[key_linepos] = 11;
*/

	//  prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	y = con_vislines - 22;

	for (i = 0; i < con_linewidth; i++)
		Draw_Character8 ((i + 1) << 3, con_vislines - 22, text[i]);
}


/*
	Con_DrawNotify

	Draws the last few lines of output transparently over the game top
*/
void
Con_DrawNotify (void)
{
	int         x, v;
	char       *text;
	int         i;
	float       time;
	char       *s;
	int         skip;

	v = 0;
	for (i = con->current - NUM_CON_TIMES + 1; i <= con->current; i++) {
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
/*XXX
		time = realtime - time;
*/
		if (time > con_notifytime->value)
			continue;
		text = con->text + (i % con_totallines) * con_linewidth;

		clearnotify = 0;
		scr_copytop = 1;

		for (x = 0; x < con_linewidth; x++)
			Draw_Character8 ((x + 1) << 3, v, text[x]);

		v += 8;
	}

	if (key_dest == key_message) {
		clearnotify = 0;
		scr_copytop = 1;

		if (chat_team) {
			Draw_String8 (8, v, "say_team:");
			skip = 11;
		} else {
			Draw_String8 (8, v, "say:");
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (vid.width >> 3) - (skip + 1))
			s += chat_bufferlen - ((vid.width >> 3) - (skip + 1));
		x = 0;
		while (s[x]) {
			Draw_Character8 ((x + skip) << 3, v, s[x]);
			x++;
		}
/*XXX
		Draw_Character8 ((x + skip) << 3, v,
						 10 + ((int) (realtime * con_cursorspeed) & 1));
*/
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}


/*
	Con_DrawConsole

	Draws the console with the solid background
*/
void
Con_DrawConsole (int lines)
{
	int         i, x, y;
	int         rows;
	char       *text;
	int         row;

	if (lines <= 0)
		return;

	// draw the background
	Draw_ConsoleBackground (lines);

	// draw the text
	con_vislines = lines;

	// changed to line things up better
	rows = (lines - 22) >> 3;			// rows of text to draw

	y = lines - 30;

	// draw from the bottom up
	if (con->display != con->current) {
		// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con_linewidth; x += 4)
			Draw_Character8 ((x + 1) << 3, y, '^');

		y -= 8;
		rows--;
	}
	
	row = con->display;
	for (i = 0; i < rows; i++, y -= 8, row--) {
		if (row < 0)
			break;
		if (con->current - row >= con_totallines)
			break;						// past scrollback wrap point

		text = con->text + (row % con_totallines) * con_linewidth;

		for (x = 0; x < con_linewidth; x++)
			Draw_Character8 ((x + 1) << 3, y, text[x]);
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}


void
Con_DrawDownload (int lines)
{
	int         i, j, x, y, n = 0; //XXX n
	const char *text = 0;//XXX
	char        dlbar[1024];

/*XXX
	if (!cls.download)
		return;

	text = COM_SkipPath(cls.downloadname);
*/

	x = con_linewidth - ((con_linewidth * 7) / 40);
	y = x - strlen (text) - 8;
	i = con_linewidth / 3;
	if (strlen (text) > i) {
		y = x - i - 11;
		strncpy (dlbar, text, i);
		dlbar[i] = 0;
		strncat (dlbar, "...", sizeof (dlbar) - strlen (dlbar));
	} else
		strncpy (dlbar, text, sizeof (dlbar));
	strncat (dlbar, ": ", sizeof (dlbar) - strlen (dlbar));
	i = strlen (dlbar);
	dlbar[i++] = '\x80';
	// where's the dot go?
/*XXX
	if (cls.downloadpercent == 0)
		n = 0;
	else
		n = y * cls.downloadpercent / 100;
*/
	for (j = 0; j < y; j++)
		if (j == n)
			dlbar[i++] = '\x83';
		else
			dlbar[i++] = '\x81';
	dlbar[i++] = '\x82';
	dlbar[i] = 0;

/*XXX
	snprintf (dlbar + strlen (dlbar), sizeof (dlbar) - strlen (dlbar),
			  " %02d%%", cls.downloadpercent);
*/
	// draw it
	y = lines - 22 + 8;
	for (i = 0; i < strlen (dlbar); i++)
		Draw_Character8 ((i + 1) << 3, y, dlbar[i]);
}

static general_funcs_t plugin_info_general_funcs = {
	C_Init,
	C_Shutdown,
};
static general_data_t plugin_info_general_data;

static console_funcs_t plugin_info_console_funcs = {
	C_Print,
};
static console_data_t plugin_info_console_data;

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	0,
	&plugin_info_console_funcs,
};

static plugin_data_t plugin_info_data = {
	&plugin_info_general_data,
	0,
	0,
	0,
	&plugin_info_console_data,
};

static plugin_t plugin_info = {
	qfp_console,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"client console driver",
	"Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge"
		" project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

plugin_t *
PluginInfo (void)
{
	return &plugin_info;
}
