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

*/
static const char rcsid[] = 
	"$Id$";

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
#include <errno.h>

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

static general_data_t plugin_info_general_data;
static console_data_t plugin_info_console_data;
#define con_data plugin_info_console_data

static old_console_t   con_main;
static old_console_t   con_chat;
static old_console_t  *con;

static float       con_cursorspeed = 4;

static cvar_t *con_notifytime;				// seconds

#define	NUM_CON_TIMES 4
static float con_times[NUM_CON_TIMES];	// realtime time the line was generated
										// for transparent notify lines

static int  con_totallines;				// total lines in console scrollback
static int  con_vislines;
static int  con_notifylines;			// scan lines to clear for notify lines

static qboolean con_debuglog;

#define		MAXCMDLINE	256
static inputline_t *input_line;

static qboolean con_initialized;


static void
ClearNotify (void)
{
	int			i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}


static void
ToggleConsole_f (void)
{
	Con_ClearTyping (input_line);

	if (key_dest == key_console && !con_data.force_commandline) {
		key_dest = key_game;
		game_target = IMT_0;
	} else {
		key_dest = key_console;
		game_target = IMT_CONSOLE;
	}

	ClearNotify ();
}

static void
ToggleChat_f (void)
{
	Con_ClearTyping (input_line);

	if (key_dest == key_console && !con_data.force_commandline) {
		key_dest = key_game;
		game_target = IMT_0;
	} else {
		key_dest = key_console;
		game_target = IMT_CONSOLE;
	}

	ClearNotify ();
}

static void
Clear_f (void)
{
	con_main.numlines = 0;
	con_chat.numlines = 0;
	memset (con_main.text, ' ', CON_TEXTSIZE);
	memset (con_chat.text, ' ', CON_TEXTSIZE);
	con_main.display = con_main.current;
}

static void
MessageMode_f (void)
{
	if (con_data.force_commandline)
		return;
	chat_team = false;
	key_dest = key_message;
}

static void
MessageMode2_f (void)
{
	if (con_data.force_commandline)
		return;
	chat_team = true;
	key_dest = key_message;
}

static void
Resize (old_console_t *con)
{
	char		tbuf[CON_TEXTSIZE];
	int			width, oldwidth, oldtotallines, numlines, numchars, i, j;

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1) {					// video hasn't been initialized yet
		width = 38;
		con_linewidth = width;
		input_line->width = con_linewidth;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con->text, ' ', CON_TEXTSIZE);
	} else {
		oldwidth = con_linewidth;
		con_linewidth = width;
		input_line->width = con_linewidth;
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

		ClearNotify ();
	}

	con->current = con_totallines - 1;
	con->display = con->current;
}

/*
	C_CheckResize

	If the line width has changed, reformat the buffer.
*/
static void
C_CheckResize (void)
{
	Resize (&con_main);
	Resize (&con_chat);
}

static void
Condump_f (void)
{
	int         line = con->current - con->numlines;
	const char *start, *end;
	VFile      *file;
	char        name[MAX_OSPATH];

	if (Cmd_Argc () != 2) {
		Con_Printf ("usage: condump <filename>\n");
		return;
	}

	if (strchr (Cmd_Argv (1), '/') || strchr (Cmd_Argv (1), '\\')) {
		Con_Printf ("invalid character in filename\n");
		return;
	}
	snprintf (name, sizeof (name), "%s/%s.txt", com_gamedir, Cmd_Argv (1));

	if (!(file = Qopen (name, "wt"))) {
		Con_Printf ("could not open %s for writing: %s\n", name,
					strerror (errno));
		return;
	}

	while (line < con->current) {
		start = &con->text[(line % con_totallines) * con_linewidth];
		end = start + con_linewidth;
		while (end > start && end[-1] != ' ')
			end--;
		Qprintf (file, "%.*s\n", (int)(end - start), start);
		line++;
	}

	Qclose (file);
}

static void
C_ExecLine (const char *line)
{
	if (line[0] == '/')
		line++; 
	Cbuf_AddText (line);
}


static void
C_Init (void)
{
	con_notifytime = Cvar_Get ("con_notifytime", "3", CVAR_NONE, NULL,
							   "How long in seconds messages are displayed "
							   "on screen");

	con_debuglog = COM_CheckParm ("-condebug");

	con = &con_main;
	con_linewidth = -1;

	input_line = Con_CreateInputLine (32, MAXCMDLINE, ']');
	input_line->complete = Con_BasicCompleteCommandLine;
	input_line->enter = C_ExecLine;
	input_line->width = con_linewidth;
	input_line->user_data = 0;
	input_line->draw = 0;//C_DrawInput;

	C_CheckResize ();

	Con_Printf ("Console initialized.\n");

	// register our commands
	Cmd_AddCommand ("toggleconsole", ToggleConsole_f,
					"Toggle the console up and down");
	Cmd_AddCommand ("togglechat", ToggleChat_f,
					"Toggle the console up and down");
	Cmd_AddCommand ("messagemode", MessageMode_f,
					"Prompt to send a message to everyone");
	Cmd_AddCommand ("messagemode2", MessageMode2_f,
					"Prompt to send a message to only people on your team");
	Cmd_AddCommand ("clear", Clear_f, "Clear the console");
	Cmd_AddCommand ("condump", Condump_f, "dump the console text to a "
					"file");
	con_initialized = true;
}

static void
C_Shutdown (void)
{
}

static void
Linefeed (void)
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
	char       *s;
	static char	*buffer;
	int         mask, size, c, l, y;
	static int  buffer_size, cr;

	size = vsnprintf (buffer, buffer_size, fmt, args);
	if (size + 1 > buffer_size) {
		buffer_size = (size + 1 + 1024) % 1024; // 1k multiples
		buffer = realloc (buffer, buffer_size);
		if (!buffer)
			Sys_Error ("console: could not allocate %d bytes\n",
					   buffer_size);
		vsnprintf (buffer, buffer_size, fmt, args);
	}

	// log all messages to file
	if (con_debuglog)
		Sys_DebugLog (va ("%s/qconsole.log", com_gamedir), "%s", buffer);

	if (!con_initialized)
		return;

	s = buffer;

	if (s[0] == 1 || s[0] == 2) {
		mask = 128;						// go to colored text
		s++;
	} else
		mask = 0;

	while ((c = (byte)*s)) {
		// count word length
		for (l = 0; l < con_linewidth; l++)
			if (s[l] <= ' ')
				break;

		// word wrap
		if (l != con_linewidth && (con->x + l > con_linewidth))
			con->x = 0;

		*s++ = sys_char_map[c];

		if (cr) {
			con->current--;
			cr = false;
		}

		if (!con->x) {
			Linefeed ();
			// mark time for transparent overlay
			if (con->current >= 0)
				con_times[con->current % NUM_CON_TIMES] = *con_data.realtime;
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
				con->text[y * con_linewidth + con->x] = c | mask | con_data.ormask;
				con->x++;
				if (con->x >= con_linewidth)
					con->x = 0;
				break;
		}

	}

	// echo to debugging console
	if ((byte)buffer[0] > 2)
		fputs (buffer, stdout);
	else if ((byte)buffer[0])
		fputs (buffer + 1, stdout);
}

static void
C_KeyEvent (key_t key, short unicode, qboolean down)
{
	if (!down)
		return;
	Con_ProcessInputLine (input_line, key);
}

/* DRAWING */

/*
	DrawInput

	The input line scrolls horizontally if typing goes beyond the right edge
*/
static void
DrawInput (void)
{
	int			i, y;
	char		temp[MAXCMDLINE];
	char	   *text;

	if (key_dest != key_console && !con_data.force_commandline)
		return;				// don't draw anything (always draw if not active)

	text = strcpy (temp, input_line->lines[input_line->edit_line]
						 + input_line->scroll);

	// fill out remainder with spaces
	for (i = strlen (text); i < MAXCMDLINE; i++)
		text[i] = ' ';

	// add the cursor frame
	if ((int) (*con_data.realtime * con_cursorspeed) & 1)
		text[input_line->linepos - input_line->scroll] = 11;

	// draw it
	y = con_vislines - 22;

	Draw_nString (8, y, text, con_linewidth);
}

/*
	DrawNotify

	Draws the last few lines of output transparently over the game top
*/
static void
DrawNotify (void)
{
	int			skip, i, v, x;
	char	   *text, *s;
	float		time;

	v = 0;
	for (i = con->current - NUM_CON_TIMES + 1; i <= con->current; i++) {
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = *con_data.realtime - time;
		if (time > con_notifytime->value)
			continue;
		text = con->text + (i % con_totallines) * con_linewidth;

		clearnotify = 0;
		scr_copytop = 1;

		Draw_nString (8, v, text, con_linewidth);
		v += 8;
	}

	if (key_dest == key_message) {
		clearnotify = 0;
		scr_copytop = 1;

		if (chat_team) {
			Draw_String (8, v, "say_team:");
			skip = 11;
		} else {
			Draw_String (8, v, "say:");
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (vid.width >> 3) - (skip + 1))
			s += chat_bufferlen - ((vid.width >> 3) - (skip + 1));
		x = 0;
		Draw_String (skip << 3, v, s);
		Draw_Character ((strlen(s) + skip) << 3, v,
						10 + ((int) (*con_data.realtime
									 * con_cursorspeed) & 1));
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

/*
	DrawConsole

	Draws the console with the solid background
*/
static void
DrawConsole (int lines)
{
	char	   *text;
	int			row, rows, i, x, y;

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
			Draw_Character ((x + 1) << 3, y, '^');

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

		Draw_nString(8, y, text, con_linewidth);
	}

	// draw the input prompt, user text, and cursor if desired
	DrawInput ();
}

static void
DrawDownload (int lines)
{
	char		dlbar[1024];
	const char *text;
	int			i, j, x, y, n;

	if (!con_data.dl_name || !*con_data.dl_name)
		return;

	text = COM_SkipPath(con_data.dl_name);

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
	if (con_data.dl_percent == 0)
		n = 0;
	else
		n = y * *con_data.dl_percent / 100;
	for (j = 0; j < y; j++)
		if (j == n)
			dlbar[i++] = '\x83';
		else
			dlbar[i++] = '\x81';
	dlbar[i++] = '\x82';
	dlbar[i] = 0;

	snprintf (dlbar + strlen (dlbar), sizeof (dlbar) - strlen (dlbar),
			  " %02d%%", *con_data.dl_percent);

	// draw it
	y = lines - 22 + 8;
	for (i = 0; i < strlen (dlbar); i++)
		Draw_Character ((i + 1) << 3, y, dlbar[i]);
}

static void
C_DrawConsole (int lines)
{
	if (lines) {
		DrawConsole (lines);
		DrawDownload (lines);
	} else {
		if (key_dest == key_game || key_dest == key_message)
			DrawNotify ();				// only draw notify in game
	}
}

static void
C_ProcessInput (void)
{
}

static general_funcs_t plugin_info_general_funcs = {
	C_Init,
	C_Shutdown,
};

static console_funcs_t plugin_info_console_funcs = {
	C_Print,
	C_ProcessInput,
	C_KeyEvent,
	C_DrawConsole,
	C_CheckResize,
};

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	&plugin_info_console_funcs,
};

static plugin_data_t plugin_info_data = {
	&plugin_info_general_data,
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
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
		"project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

QFPLUGIN plugin_t *
console_client_PluginInfo (void)
{
	return &plugin_info;
}
