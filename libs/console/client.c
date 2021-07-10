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

#ifdef __QNXNTO__
# include <locale.h>
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/gib.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "QF/plugin/general.h"
#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "QF/ui/view.h"

#include "QF/ui/inputline.h"

#include "compat.h"

static general_data_t plugin_info_general_data;
console_data_t con_data;

static old_console_t   con_main;
static old_console_t   con_chat;
static old_console_t  *con;

static float       con_cursorspeed = 4;

static cvar_t *con_notifytime;				// seconds
static cvar_t *con_alpha;
static cvar_t *con_size;
static cvar_t *con_speed;
static cvar_t *cl_conmode;

#define	NUM_CON_TIMES 4
static float con_times[NUM_CON_TIMES];	// realtime time the line was generated
										// for transparent notify lines

static int  con_totallines;				// total lines in console scrollback

static qboolean con_debuglog;
static qboolean chat_team;

#define		MAXCMDLINE	256
static inputline_t *input_line;
static inputline_t *say_line;
static inputline_t *say_team_line;

static view_t *console_view;
static view_t *say_view;
static view_t *notify_view;
static view_t *menu_view;
static view_t *hud_view;

static qboolean con_initialized;

static keydest_t con_keydest;

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
	Con_ClearTyping (input_line, 0);

	if (con_keydest == key_console && !con_data.force_commandline) {
		Key_SetKeyDest (key_game);
	} else {
		Key_SetKeyDest (key_console);
	}

	ClearNotify ();
}

static void
ToggleChat_f (void)
{
	Con_ClearTyping (input_line, 0);

	if (con_keydest == key_console && !con_data.force_commandline) {
		Key_SetKeyDest (key_game);
	} else {
		Key_SetKeyDest (key_console);
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
	Key_SetKeyDest (key_message);
}

static void
MessageMode2_f (void)
{
	if (con_data.force_commandline)
		return;
	chat_team = true;
	Key_SetKeyDest (key_message);
}

static void
Resize (old_console_t *con)
{
	char		tbuf[CON_TEXTSIZE];
	int			width, oldwidth, oldtotallines, numlines, numchars, i, j;

	width = (r_data->vid->conview->xlen >> 3) - 2;

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

		ClearNotify ();
	}
	say_team_line->width = con_linewidth - 9;
	say_line->width = con_linewidth - 4;
	input_line->width = con_linewidth;

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

	view_resize (con_data.view, r_data->vid->conview->xlen,
				 r_data->vid->conview->ylen);
}

static void
Condump_f (void)
{
	int         line = con->current - con->numlines;
	const char *start, *end;
	QFile      *file;
	const char *name;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("usage: condump <filename>\n");
		return;
	}

	if (strchr (Cmd_Argv (1), '/') || strchr (Cmd_Argv (1), '\\')) {
		Sys_Printf ("invalid character in filename\n");
		return;
	}
	name = va (0, "%s/%s.txt", qfs_gamedir->dir.def, Cmd_Argv (1));//FIXME

	if (!(file = QFS_WOpen (name, 0))) {
		Sys_Printf ("could not open %s for writing: %s\n", name,
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

static int
cl_exec_line_command (void *data, const char *line)
{
	Cbuf_AddText (con_data.cbuf, line);
	Cbuf_AddText (con_data.cbuf, "\n");
	return 1;
}

static int
cl_exec_line_chat (void *data, const char *line)
{
	Cbuf_AddText (con_data.cbuf, "say ");
	Cbuf_AddText (con_data.cbuf, line);
	Cbuf_AddText (con_data.cbuf, "\n");
	return 0;
}

static int
cl_exec_line_rcon (void *data, const char *line)
{
	Cbuf_AddText (con_data.cbuf, "rcon ");
	Cbuf_AddText (con_data.cbuf, line);
	Cbuf_AddText (con_data.cbuf, "\n");
	Sys_Printf ("rcon %s\n", line);
	return 0;
}

static void
cl_conmode_f (cvar_t *var)
{
	if (!strcmp (var->string, "command")) {
		con_data.exec_line = cl_exec_line_command;
	} else if (!strcmp (var->string, "chat")) {
		con_data.exec_line = cl_exec_line_chat;
	} else if (!strcmp (var->string, "rcon")) {
		con_data.exec_line = cl_exec_line_rcon;
	} else {
		Sys_Printf ("mode must be one of \"command\", \"chat\" or \"rcon\"\n");
		Sys_Printf ("    forcing \"command\"\n");
		Cvar_Set (var, "command");
	}
}

static void
C_Say (inputline_t *il)
{
	const char *line = il->line;
	if (!*line)
		return;

	Cbuf_AddText (con_data.cbuf, "say \"");
	Cbuf_AddText (con_data.cbuf, line);
	Cbuf_AddText (con_data.cbuf, "\"\n");
	Key_SetKeyDest (key_game);
}

static void
C_SayTeam (inputline_t *il)
{
	const char *line = il->line;
	if (!*line)
		return;

	Cbuf_AddText (con_data.cbuf, "say_team \"");
	Cbuf_AddText (con_data.cbuf, line);
	Cbuf_AddText (con_data.cbuf, "\"\n");
	Key_SetKeyDest (key_game);
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
static __attribute__((format(PRINTF, 1, 0))) void
C_Print (const char *fmt, va_list args)
{
	char       *s;
	static dstring_t *buffer;
	int         mask, c, l, y;
	static int  cr;

	if (!buffer)
		buffer = dstring_new ();

	dvsprintf (buffer, fmt, args);

	// log all messages to file
	if (con_debuglog)
		Sys_DebugLog (va (0, "%s/%s/qconsole.log", qfs_userpath,//FIXME
					  qfs_gamedir->dir.def), "%s", buffer->str);

	if (!con_initialized)
		return;

	s = buffer->str;

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
			if (con->current >= 0 && con_data.realtime)
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
	if ((byte)buffer->str[0] > 2)
		fputs (buffer->str, stdout);
	else if ((byte)buffer->str[0])
		fputs (buffer->str + 1, stdout);
}

static void
C_KeyEvent (knum_t key, short unicode, qboolean down, void *data)
{
	inputline_t *il;

	if (!down)
		return;

	if (con_keydest == key_menu) {
		if (Menu_KeyEvent (key, unicode, down))
			return;
	}

	if (down) {
		if (key == key_togglemenu) {
			switch (con_keydest) {
				case key_menu:
					Menu_Leave ();
					return;
				case key_message:
					if (chat_team) {
						Con_ClearTyping (say_team_line, 1);
					} else {
						Con_ClearTyping (say_line, 1);
					}
					Key_SetKeyDest (key_game);
					return;
				case key_console:
					if (!con_data.force_commandline) {
						Cbuf_AddText (con_data.cbuf, "toggleconsole\n");
						return;
					}
				case key_game:
				case key_demo:
					Menu_Enter ();
					return;
				case key_unfocused:
					return;
				case key_last:
					break;	// should not happen, so hit error
			}
			Sys_Error ("Bad con_keydest");
		} else if (key == key_toggleconsole) {
			ToggleConsole_f ();
			return;
		}
	}

	if (con_keydest == key_menu) {
		return;
	} else if (con_keydest == key_message) {
		if (chat_team) {
			il = say_team_line;
		} else {
			il = say_line;
		}
	} else {
		switch (key) {
			case QFK_PAGEUP:
				if (keydown[QFK_RCTRL] || keydown[QFK_LCTRL])
					con->display = 0;
				else
					con->display -= 10;
				if (con->display < con->current - con->numlines)
					con->display = con->current - con->numlines;
				return;
			case QFK_PAGEDOWN:
				if (keydown[QFK_RCTRL] || keydown[QFK_LCTRL])
					con->display = con->current;
				else
					con->display += 10;
				if (con->display > con->current)
					con->display = con->current;
				return;
			case QFM_WHEEL_UP:
				con->display -= 3;
				if (con->display < con->current - con->numlines)
					con->display = con->current - con->numlines;
				return;
			case QFM_WHEEL_DOWN:
				con->display += 3;
				if (con->display > con->current)
					con->display = con->current;
				return;
			default:
				break;
		}
		il = input_line;
	}
	//FIXME should this translation be here?
	if ((unicode==0x0A) && (key==QFK_RETURN)) {
		Con_ProcessInputLine (il, key);
	}
	if ((unicode==0x7F) && (key==QFK_BACKSPACE)) {
		Con_ProcessInputLine (il, key);
	}
	if (unicode!=0) {
		Con_ProcessInputLine (il, key >= 256 ? (int) key : unicode);
	} else {
		Con_ProcessInputLine (il, key);
	}
}

/* DRAWING */

static void
DrawInputLine (int x, int y, int cursor, inputline_t *il)
{
	const char *s = il->lines[il->edit_line] + il->scroll;

	if (il->scroll) {
		r_funcs->Draw_Character (x, y, '<' | 0x80);
		r_funcs->Draw_nString (x + 8, y, s + 1, il->width - 2);
	} else {
		r_funcs->Draw_nString (x, y, s, il->width - 1);
	}
	if (cursor && con_data.realtime) {
		float       t = *con_data.realtime * con_cursorspeed;
		int         ch = 10 + ((int) (t) & 1);
		r_funcs->Draw_Character (x + ((il->linepos - il->scroll) << 3), y, ch);
	}
	if (strlen (s) >= il->width)
		r_funcs->Draw_Character (x + ((il->width - 1) << 3), y, '>' | 0x80);
}

void
C_DrawInputLine (inputline_t *il)
{
	DrawInputLine (il->x, il->y, il->cursor, il);
}

static void
draw_input (view_t *view)
{
	if (con_keydest != key_console)// && !con_data.force_commandline)
		return;				// don't draw anything (always draw if not active)

	DrawInputLine (view->xabs + 8, view->yabs, 1, input_line);
}

static void
draw_download (view_t *view)
{
	char		dlbar[1024];
	const char *text;
	size_t		i, j, x, y, n;

	if (!con_data.dl_name || !*con_data.dl_name->str)
		return;

	text = QFS_SkipPath(con_data.dl_name->str);

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
	r_funcs->Draw_String (view->xabs, view->yabs, dlbar);
}

static void
draw_console_text (view_t *view)
{
	char	   *text;
	int			row, rows, i, x, y;

	rows = view->ylen >> 3;			// rows of text to draw

	x = view->xabs + 8;
	y = view->yabs + view->ylen - 8;

	// draw from the bottom up
	if (con->display != con->current) {
		// draw arrows to show the buffer is backscrolled
		for (i = 0; i < con_linewidth; i += 4)
			r_funcs->Draw_Character (x + (i << 3), y, '^');

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

		r_funcs->Draw_nString(x, y, text, con_linewidth);
	}
}

static void
draw_console (view_t *view)
{
	byte        alpha;

	if (con_data.force_commandline) {
		alpha = 255;
	} else {
		float       y = r_data->vid->conview->ylen * con_size->value;
		alpha = 255 * con_alpha->value * view->ylen / y;
		alpha = min (alpha, 255);
	}
	// draw the background
	r_funcs->Draw_ConsoleBackground (view->ylen, alpha);

	// draw everything else
	view_draw (view);
}

static void
draw_say (view_t *view)
{
	r_data->scr_copytop = 1;

	if (chat_team) {
		r_funcs->Draw_String (view->xabs + 8, view->yabs, "say_team:");
		DrawInputLine (view->xabs + 80, view->yabs, 1, say_team_line);
	} else {
		r_funcs->Draw_String (view->xabs + 8, view->yabs, "say:");
		DrawInputLine (view->xabs + 40, view->yabs, 1, say_line);
	}
}

static void
draw_notify (view_t *view)
{
	int			i, x, y;
	char	   *text;
	float		time;

	if (!con_data.realtime)
		return;

	x = view->xabs + 8;
	y = view->yabs;
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

		r_data->scr_copytop = 1;

		r_funcs->Draw_nString (x, y, text, con_linewidth);
		y += 8;
	}
}

static void
setup_console (void)
{
	float       lines;

	if (con_data.force_commandline) {
		lines = con_data.lines = r_data->vid->conview->ylen;
	} else if (con_keydest == key_console) {
		lines = r_data->vid->conview->ylen * bound (0.2, con_size->value, 1);
	} else {
		lines = 0;
	}

	if (con_speed->value) {
		if (lines < con_data.lines) {
			con_data.lines -= max (0.2, con_speed->value) * *con_data.frametime;
			con_data.lines = max (con_data.lines, lines);
		} else if (lines > con_data.lines) {
			con_data.lines += max (0.2, con_speed->value) * *con_data.frametime;
			con_data.lines = min (con_data.lines, lines);
		}
	} else {
		con_data.lines = lines;
	}
	if (con_data.lines >= r_data->vid->conview->ylen - r_data->lineadj)
		r_data->scr_copyeverything = 1;
}

static void
C_DrawConsole (void)
{
	setup_console ();

	if (console_view->ylen != con_data.lines)
		view_resize (console_view, console_view->xlen, con_data.lines);

	say_view->visible = con_keydest == key_message;
	console_view->visible = con_data.lines != 0;
	menu_view->visible = con_keydest == key_menu;

	con_data.view->draw (con_data.view);
}

static void
C_ProcessInput (void)
{
}

static void
C_NewMap (void)
{
	static dstring_t *old_gamedir = 0;

	if (!old_gamedir || !strequal (old_gamedir->str, qfs_gamedir->gamedir))
		Menu_Load ();
	if (!old_gamedir)
		old_gamedir = dstring_newstr ();
	dstring_copystr (old_gamedir, qfs_gamedir->gamedir);
}

static void
C_GIB_HUD_Enable_f (void)
{
	hud_view->visible = 1;
}

static void
C_GIB_HUD_Disable_f (void)
{
	hud_view->visible = 0;
}

static void
exec_line (inputline_t *il)
{
	Con_ExecLine (il->line);
}

static void
con_keydest_callback (keydest_t kd)
{
	// simply cache the value
	con_keydest = kd;
}

static void
C_Init (void)
{
	view_t     *view;

#ifdef __QNXNTO__
	setlocale (LC_ALL, "C-TRADITIONAL");
#endif

	Key_KeydestCallback (con_keydest_callback);
	Key_SetKeyEvent (key_message, C_KeyEvent, 0);
	Key_SetKeyEvent (key_menu, C_KeyEvent, 0);
	Key_SetKeyEvent (key_console, C_KeyEvent, 0);
	Menu_Init ();

	con_notifytime = Cvar_Get ("con_notifytime", "3", CVAR_NONE, NULL,
							   "How long in seconds messages are displayed "
							   "on screen");

	con_alpha = Cvar_Get ("con_alpha", "0.6", CVAR_ARCHIVE, NULL,
						  "alpha value for the console background");
	con_size = Cvar_Get ("con_size", "0.5", CVAR_ARCHIVE, NULL,
						 "Fraction of the screen the console covers when "
						 "down");
	con_speed = Cvar_Get ("con_speed", "300", CVAR_ARCHIVE, NULL,
						  "How quickly the console scrolls up or down");
	cl_conmode = Cvar_Get ("cl_conmode", "command", CVAR_ARCHIVE, cl_conmode_f,
						   "Set the console input mode (command, chat, rcon)");

	con_debuglog = COM_CheckParm ("-condebug");

	// The console will get resized, so assume initial size is 320x200
	con_data.view = view_new (0, 0, 320, 200, grav_northeast);
	console_view = view_new (0, 0, 320, 200, grav_northwest);
	say_view     = view_new (0, 0, 320, 8, grav_northwest);
	notify_view  = view_new (0, 8, 320, 32, grav_northwest);
	menu_view    = view_new (0, 0, 320, 200, grav_center);
	hud_view     = view_new (0, 0, 320, 200, grav_northeast);

	view_add (con_data.view, say_view);
	view_add (con_data.view, notify_view);
	view_add (con_data.view, hud_view);
	view_add (con_data.view, console_view);
	view_add (con_data.view, menu_view);

	console_view->draw = draw_console;
	console_view->visible = 0;
	console_view->resize_x = console_view->resize_y = 1;

	say_view->draw = draw_say;
	say_view->visible = 0;
	say_view->resize_x = 1;

	notify_view->draw = draw_notify;
	notify_view->resize_x = 1;

	menu_view->draw = Menu_Draw;
	menu_view->visible = 0;

	hud_view->draw = Menu_Draw_Hud;
	hud_view->visible = 0;

	view = view_new (0, 0, 320, 170, grav_northwest);
	view->draw = draw_console_text;
	view->resize_x = view->resize_y = 1;
	view_add (console_view, view);

	view = view_new (0, 12, 320, 10, grav_southwest);
	view->draw = draw_input;
	view->resize_x = 1;
	view_add (console_view, view);

	view = view_new (0, 2, 320, 11, grav_southwest);
	view->draw = draw_download;
	view->resize_x = 1;
	view_add (console_view, view);

	con = &con_main;
	con_linewidth = -1;

	input_line = Con_CreateInputLine (32, MAXCMDLINE, ']');
	input_line->complete = Con_BasicCompleteCommandLine;
	input_line->enter = exec_line;
	input_line->width = con_linewidth;
	input_line->user_data = 0;
	input_line->draw = 0;

	say_line = Con_CreateInputLine (32, MAXCMDLINE, ' ');
	say_line->complete = 0;
	say_line->enter = C_Say;
	say_line->width = con_linewidth - 5;
	say_line->user_data = 0;
	say_line->draw = 0;

	say_team_line = Con_CreateInputLine (32, MAXCMDLINE, ' ');
	say_team_line->complete = 0;
	say_team_line->enter = C_SayTeam;
	say_team_line->width = con_linewidth - 10;
	say_team_line->user_data = 0;
	say_team_line->draw = 0;

	C_CheckResize ();

	Sys_Printf ("Console initialized.\n");

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

	// register GIB builtins
	GIB_Builtin_Add ("HUD::enable", C_GIB_HUD_Enable_f);
	GIB_Builtin_Add ("HUD::disable", C_GIB_HUD_Disable_f);

	con_initialized = true;
}

static void
C_shutdown (void)
{
}

static general_funcs_t plugin_info_general_funcs = {
	.init = C_Init,
	.shutdown = C_shutdown,
};

static console_funcs_t plugin_info_console_funcs = {
	.print = C_Print,
	.process_input = C_ProcessInput,
	.draw_console = C_DrawConsole,
	.check_resize = C_CheckResize,
	.new_map = C_NewMap,
};

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.console = &plugin_info_console_funcs,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.console = &con_data,
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

PLUGIN_INFO(console, client)
{
	return &plugin_info;
}
