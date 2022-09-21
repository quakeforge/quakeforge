/*
	client.c

	Client console routines

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

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/gib.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/va.h"

#include "QF/input/event.h"

#include "QF/plugin/general.h"
#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "QF/ui/inputline.h"
#include "QF/ui/view.h"

#include "compat.h"

static general_data_t plugin_info_general_data;
console_data_t con_data;

static con_buffer_t *con;

static float con_cursorspeed = 4;

static float con_notifytime;
static cvar_t con_notifytime_cvar = {
	.name = "con_notifytime",
	.description =
		"How long in seconds messages are displayed on screen",
	.default_value = "3",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &con_notifytime },
};
static float con_alpha;
static cvar_t con_alpha_cvar = {
	.name = "con_alpha",
	.description =
		"alpha value for the console background",
	.default_value = "0.6",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &con_alpha },
};
static float con_size;
static cvar_t con_size_cvar = {
	.name = "con_size",
	.description =
		"Fraction of the screen the console covers when down",
	.default_value = "0.5",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &con_size },
};
static float con_speed;
static cvar_t con_speed_cvar = {
	.name = "con_speed",
	.description =
		"How quickly the console scrolls up or down",
	.default_value = "300",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &con_speed },
};
static exprenum_t cl_conmode_enum;
static exprtype_t cl_conmode_type = {
	.name = "cl_conmode",
	.size = sizeof (con_data.exec_line),
	.data = &cl_conmode_enum,
	.get_string = cexpr_enum_get_string,
};
static int cl_exec_line_command (void *data, const char *line);
static int cl_exec_line_chat (void *data, const char *line);
static int cl_exec_line_rcon (void *data, const char *line);
static int (*cl_conmode_values[])(void *, const char *) = {
	cl_exec_line_command,
	cl_exec_line_chat,
	cl_exec_line_rcon,
};
static exprsym_t cl_conmode_symbols[] = {
	{"command", &cl_conmode_type, cl_conmode_values + 0},
	{"chat", &cl_conmode_type, cl_conmode_values + 1},
	{"rcon", &cl_conmode_type, cl_conmode_values + 1},
	{}
};
static exprtab_t cl_conmode_symtab = {
	cl_conmode_symbols,
};
static exprenum_t cl_conmode_enum = {
	&cl_conmode_type,
	&cl_conmode_symtab,
};
static cvar_t cl_conmode_cvar = {
	.name = "cl_conmode",
	.description =
		"Set the console input mode (command, chat, rcon)",
	.default_value = "command",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cl_conmode_type, .value = &con_data.exec_line },
};

static con_state_t con_state;
static int  con_event_id;
static int  con_saved_focos;

static qboolean con_debuglog;
static qboolean chat_team;

typedef struct {
	const char *prompt;
	inputline_t *input_line;
	view_t     *view;
	draw_charbuffer_t *buffer;
} con_input_t;

#define	MAXCMDLINE 256

static con_input_t cmd_line;
static con_input_t say_line;
static con_input_t team_line;

#define CON_BUFFER_SIZE 32768
#define CON_LINES 1024
static view_t *console_view;
static draw_charbuffer_t *console_buffer;
static con_buffer_t *con_main;
static int view_offset;

#define	NOTIFY_LINES 4
static view_t *notify_view;
static draw_charbuffer_t *notify_buffer;
// ring buffer holding realtime time the line was generated
static float notify_times[NOTIFY_LINES + 1];
static int notify_head;
static int notify_tail;

static view_t *menu_view;
static view_t *hud_view;

static qboolean con_initialized;

static void
ClearNotify (void)
{
	Draw_ClearBuffer (notify_buffer);
	notify_head = notify_tail = 0;
}

static void
C_SetState (con_state_t state)
{
	con_state_t old_state = con_state;
	con_state = state;
	if (con_state == con_inactive) {
		IE_Set_Focus (con_saved_focos);
	} else if (old_state == con_inactive) {
		con_saved_focos = IE_Get_Focus ();
		IE_Set_Focus (con_event_id);
	}

	if (state == con_message) {
		say_line.prompt = "say:";
		if (chat_team) {
			say_line.prompt = "say_team:";
		}
		say_line.buffer->cursx = 0;
		Draw_PrintBuffer (say_line.buffer, say_line.prompt);
	}

	if (con_state == con_menu && old_state != con_menu) {
		Menu_Enter ();
	}
}

static void
ToggleConsole_f (void)
{
	switch (con_state) {
		case con_menu:
		case con_message:
			return;
		case con_inactive:
			C_SetState (con_active);
			break;
		case con_active:
			C_SetState (con_inactive);
			break;
		case con_fullscreen:
			break;
	}
	Con_ClearTyping (cmd_line.input_line, 0);

	ClearNotify ();
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
con_end_message (inputline_t *line)
{
	Con_ClearTyping (line, 1);
	C_SetState (con_inactive);
}

static void
C_Say (inputline_t *il)
{
	const char *line = il->line;
	if (*line) {
		Cbuf_AddText (con_data.cbuf, "say \"");
		Cbuf_AddText (con_data.cbuf, line);
		Cbuf_AddText (con_data.cbuf, "\"\n");
	}
	con_end_message (il);
}

static void
C_SayTeam (inputline_t *il)
{
	const char *line = il->line;

	if (*line) {
		Cbuf_AddText (con_data.cbuf, "say_team \"");
		Cbuf_AddText (con_data.cbuf, line);
		Cbuf_AddText (con_data.cbuf, "\"\n");
	}
	con_end_message (il);
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
	int         mask, c;

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

	mask = 0;
	if (buffer->str[0] == 1 || buffer->str[0] == 2) {
		mask = 128;						// go to colored text
		s++;
	}
	if (mask || con_data.ormask) {
		for (char *m = s; *m; m++) {
			if (*m >= ' ') {
				*m |= mask | con_data.ormask;
			}
		}
	}

	if (con_data.realtime) {
		c = Draw_PrintBuffer (notify_buffer, s);
		while (c-- > 0) {
			notify_times[notify_head++] = *con_data.realtime;
			if (notify_head >= NOTIFY_LINES + 1) {
				notify_head -= NOTIFY_LINES + 1;
			}
			if (notify_head == notify_tail) {
				notify_tail++;
				if (notify_tail >= NOTIFY_LINES + 1) {
					notify_tail -= NOTIFY_LINES + 1;
				}
			}
		}
	}
	Con_BufferAddText (con_main, s);
	if (!view_offset) {
		Draw_PrintBuffer (console_buffer, s);
	}

	while (*s) {
		*s = sys_char_map[(byte) *s];
		s++;
	}

	// echo to debugging console
	// but don't print the highchars flag (leading \x01)
	if ((byte)buffer->str[0] > 2)
		fputs (buffer->str, stdout);
	else if ((byte)buffer->str[0])
		fputs (buffer->str + 1, stdout);
}

/* DRAWING */

static void
draw_cursor (int x, int y, inputline_t *il)
{
	if (!con_data.realtime) {
		return;
	}

	float       t = *con_data.realtime * con_cursorspeed;
	int         ch = 10 + ((int) (t) & 1);
	r_funcs->Draw_Character (x + ((il->linepos - il->scroll) * 8), y, ch);
}

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
	if (cursor) {
		draw_cursor (x, y, il);
	}
	if (strlen (s) >= il->width)
		r_funcs->Draw_Character (x + ((il->width - 1) * 8), y, '>' | 0x80);
}

void
C_DrawInputLine (inputline_t *il)
{
	DrawInputLine (il->x, il->y, il->cursor, il);
}

static void
draw_input_line (inputline_t *il, draw_charbuffer_t *buffer)
{
	char       *dst = buffer->chars + buffer->cursx;
	char       *src = il->lines[il->edit_line] + il->scroll + 1;
	size_t      i;

	*dst++ = il->scroll ? '<' | 0x80 : il->lines[il->edit_line][0];
	for (i = 0; i < il->width - 2 && *src; i++) {
		*dst++ = *src++;
	}
	while (i++ < il->width - 2) {
		*dst++ = ' ';
	}
	*dst++ = *src ? '>' | 0x80 : ' ';
}

static void
draw_input (view_t *view)
{
	__auto_type inp = (con_input_t *) view->data;

	Draw_CharBuffer (view->xabs, view->yabs, inp->buffer);
	draw_cursor (view->xabs + inp->buffer->cursx * 8, view->yabs,
				 inp->input_line);
}

static void
input_line_draw (inputline_t *il)
{
	__auto_type inp = (con_input_t *) il->user_data;
	draw_input_line (il, inp->buffer);
}

static void
resize_input (view_t *view)
{
	__auto_type inp = (con_input_t *) view->data;

	if (inp->buffer) {
		Draw_DestroyBuffer (inp->buffer);
	}
	inp->buffer = Draw_CreateBuffer (view->xlen / 8, 1);
	Draw_ClearBuffer (inp->buffer);
	Draw_PrintBuffer (inp->buffer, inp->prompt);
	inp->buffer->chars[inp->buffer->cursx] = inp->input_line->prompt_char;
	inp->input_line->width = inp->buffer->width - inp->buffer->cursx;
}

static void
draw_download (view_t *view)
{
	static dstring_t *dlbar;
	const char *text;

	if (!con_data.dl_name || !*con_data.dl_name->str)
		return;

	if (!dlbar) {
		dlbar = dstring_new ();
	}

	text = QFS_SkipPath(con_data.dl_name->str);

	int         line_size = con_linewidth - ((con_linewidth * 7) / 40);
	int         dot_space = line_size - strlen (text) - 8;
	const char *ellipsis = "";
	int         txt_size = con_linewidth / 3;
	if (strlen (text) > (size_t) txt_size) {
		ellipsis = "...";
		dot_space = line_size - txt_size - 11;
	} else {
		txt_size = strlen (text);
	}
	// where's the dot go?
	int         n = 0;
	if (con_data.dl_percent) {
		n = dot_space * *con_data.dl_percent / 100;
	}
	char       *dots = alloca (dot_space + 1);
	dots[dot_space] = 0;
	for (int j = 0; j < dot_space; j++) {
		if (j == n) {
			dots[j++] = '\x83';
		} else {
			dots[j++] = '\x81';
		}
	}

	dsprintf (dlbar, "%.*s%s: \x80%s\x82 %02d%%",
			  txt_size, text, ellipsis, dots, *con_data.dl_percent);

	// draw it
	r_funcs->Draw_String (view->xabs, view->yabs, dlbar->str);
}

static void
draw_console_text (view_t *view)
{
	Draw_CharBuffer (view->xabs, view->yabs, console_buffer);
}

static void
clear_console_text (void)
{
	Draw_ClearBuffer (console_buffer);
	console_buffer->cursy = console_buffer->height - 1;
}

static void
resize_console_text (view_t *view)
{
	int			width = view->xlen / 8;
	int         height = view->ylen / 8;

	ClearNotify ();

	con_linewidth = width;
	Draw_DestroyBuffer (console_buffer);
	console_buffer = Draw_CreateBuffer (width, height);
	clear_console_text ();
}

static void
draw_con_scrollback (void)
{
	__auto_type cb = console_buffer;
	char       *dst = cb->chars + (cb->height - 1) * cb->width;
	int         rows = cb->height;
	int         cur_line = con_main->line_head - 1 + view_offset;

	if (view_offset) {
		// draw arrows to show the buffer is backscrolled
		memset (dst, '^' | 0x80, cb->width);
		dst -= cb->width;
		rows--;
	}
	for (int i = 0; i < rows; i++) {
		con_line_t *l = Con_BufferLine (con_main, cur_line - i);
		int         len = max (min ((int) l->len, cb->width + 1) - 1, 0);
		memcpy (dst, con_main->buffer + l->text, len);
		memset (dst + len, ' ', cb->width - len);
		dst -= cb->width;
	}
}

static void
draw_console (view_t *view)
{
	byte        alpha;

	if (con_state == con_fullscreen) {
		alpha = 255;
	} else {
		float       y = r_data->vid->conview->ylen * con_size;
		alpha = 255 * con_alpha * view->ylen / y;
		alpha = min (alpha, 255);
	}
	// draw the background
	r_funcs->Draw_ConsoleBackground (view->ylen, alpha);

	// draw everything else
	view_draw (view);
}

static void
draw_notify (view_t *view)
{
	if (con_data.realtime && notify_tail != notify_head
		&& *con_data.realtime - notify_times[notify_tail] > con_notifytime) {
		Draw_ScrollBuffer (notify_buffer, 1);
		notify_buffer->cursy--;
		notify_tail++;
		if (notify_tail >= (NOTIFY_LINES + 1)) {
			notify_tail -= NOTIFY_LINES + 1;
		}
	}
	Draw_CharBuffer (view->xabs, view->yabs, notify_buffer);
}

static void
resize_notify (view_t *view)
{
	Draw_DestroyBuffer (notify_buffer);
	notify_buffer = Draw_CreateBuffer (view->xlen / 8, NOTIFY_LINES + 1);
	ClearNotify ();
}

static void
setup_console (void)
{
	float       lines = 0;

	switch (con_state) {
		case con_message:
		case con_menu:
		case con_inactive:
			lines = 0;
			break;
		case con_active:
			lines = r_data->vid->conview->ylen * bound (0.2, con_size,
														1);
			break;
		case con_fullscreen:
			lines = con_data.lines = r_data->vid->conview->ylen;
			break;
	}

	if (con_speed) {
		if (lines < con_data.lines) {
			con_data.lines -= max (0.2, con_speed) * *con_data.frametime;
			con_data.lines = max (con_data.lines, lines);
		} else if (lines > con_data.lines) {
			con_data.lines += max (0.2, con_speed) * *con_data.frametime;
			con_data.lines = min (con_data.lines, lines);
		}
	} else {
		con_data.lines = lines;
	}
	if (con_data.lines > r_data->vid->conview->ylen) {
		con_data.lines = r_data->vid->conview->ylen;
	}
	if (con_data.lines >= r_data->vid->conview->ylen - r_data->lineadj)
		r_data->scr_copyeverything = 1;
}

static void
C_DrawConsole (void)
{
	setup_console ();

	say_line.view->visible = con_state == con_message ? !chat_team : 0;
	team_line.view->visible = con_state == con_message ? chat_team : 0;
	console_view->visible = con_data.lines != 0;
	menu_view->visible = con_state == con_menu;

	con_data.view->draw (con_data.view);
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
con_app_window (const IE_event_t *event)
{
	static int  old_xlen;
	static int  old_ylen;
	if (old_xlen != event->app_window.xlen
		|| old_ylen != event->app_window.ylen) {
		old_xlen = event->app_window.xlen;
		old_ylen = event->app_window.ylen;

		view_resize (con_data.view, r_data->vid->conview->xlen,
					 r_data->vid->conview->ylen);
	}
}

static void
con_key_event (const IE_event_t *event)
{
	inputline_t *il;
	__auto_type key = &event->key;
	int         old_view_offset = view_offset;

#if 0
	if (con_curr_keydest == key_menu) {
		Menu_KeyEvent (key, unicode, down);
		return;
	}

	if (down) {
		if (key == key_toggleconsole) {
			ToggleConsole_f ();
			return;
		}
	}
#endif
	if (con_state == con_message) {
		if (chat_team) {
			il = team_line.input_line;
		} else {
			il = say_line.input_line;
		}
		if (key->code == QFK_ESCAPE) {
			con_end_message (il);
			return;
		}
	} else {
		con_buffer_t *cb = con_main;
		int         num_lines = (cb->line_head - cb->line_tail
								 + cb->max_lines) % cb->max_lines;
		int         con_lines = con_data.lines / 8;
		switch (key->code) {
			case QFK_ESCAPE:
				ToggleConsole_f ();
				break;
			case QFK_PAGEUP:
				view_offset -= 10;
				if (view_offset <= -(num_lines - con_lines)) {
					view_offset = -(num_lines - con_lines) + 1;
				}
				break;
			case QFK_PAGEDOWN:
				view_offset += 10;
				if (view_offset > 0) {
					view_offset = 0;
				}
				break;
#if 0
			case QFM_WHEEL_UP:
				break;
			case QFM_WHEEL_DOWN:
				break;
#endif
			default:
				break;
		}
		il = cmd_line.input_line;
	}
	if (old_view_offset != view_offset) {
		draw_con_scrollback ();
	}

	if (key->unicode) {
		Con_ProcessInputLine (il, key->code >= 256 ? (int) key->code
												   : key->unicode);
	} else {
		Con_ProcessInputLine (il, key->code);
	}
}

static void
con_mouse_event (const IE_event_t *event)
{
}

static int
con_event_handler (const IE_event_t *ie_event, void *data)
{
	if (con_state == con_menu) {
		return Menu_EventHandler (ie_event);
	}
	static void (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_app_window] = con_app_window,
		[ie_key] = con_key_event,
		[ie_mouse] = con_mouse_event,
	};
	if ((unsigned) ie_event->type >= ie_event_count
		|| !handlers[ie_event->type]) {
		return 0;
	}
	handlers[ie_event->type] (ie_event);
	return 1;
}

static void
Clear_f (void)
{
	Con_ClearBuffer (con_main);
	clear_console_text ();
}

static void
MessageMode_f (void)
{
	if (con_state != con_inactive)
		return;
	chat_team = false;
	C_SetState (con_message);
}

static void
MessageMode2_f (void)
{
	if (con_state != con_inactive)
		return;
	chat_team = true;
	C_SetState (con_message);
}

static void
Condump_f (void)
{
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

	for (uint32_t line_ind = con->line_tail; line_ind != con->line_head;
		 line_ind = (line_ind + 1) % con->max_lines) {
		con_line_t *line = &con->lines[line_ind];
		Qwrite (file, con->buffer + line->text, line->len);
	}

	Qclose (file);
}

static void
C_Init (void)
{
	view_t     *view;

#ifdef __QNXNTO__
	setlocale (LC_ALL, "C-TRADITIONAL");
#endif

	con_event_id = IE_Add_Handler (con_event_handler, 0);

	Menu_Init ();

	Cvar_Register (&con_notifytime_cvar, 0, 0);

	Cvar_Register (&con_alpha_cvar, 0, 0);
	Cvar_Register (&con_size_cvar, 0, 0);
	Cvar_Register (&con_speed_cvar, 0, 0);
	Cvar_Register (&cl_conmode_cvar, 0, 0);

	con_debuglog = COM_CheckParm ("-condebug");

	// The console will get resized, so assume initial size is 320x200
	con_data.view = view_new (0, 0, 320, 200, grav_northeast);
	console_view = view_new (0, 0, 320, 200, grav_northwest);
	notify_view  = view_new (8, 8, 312, NOTIFY_LINES * 8, grav_northwest);
	menu_view    = view_new (0, 0, 320, 200, grav_center);
	hud_view     = view_new (0, 0, 320, 200, grav_northeast);

	cmd_line.prompt = "";
	cmd_line.input_line = Con_CreateInputLine (32, MAXCMDLINE, ']');
	cmd_line.input_line->complete = Con_BasicCompleteCommandLine;
	cmd_line.input_line->enter = exec_line;
	cmd_line.input_line->width = con_linewidth;
	cmd_line.input_line->user_data = &cmd_line;
	cmd_line.input_line->draw = input_line_draw;
	cmd_line.view = view_new_data (0, 12, 320, 10, grav_southwest, &cmd_line);
	cmd_line.view->draw = draw_input;
	cmd_line.view->setgeometry = resize_input;
	cmd_line.view->resize_x = 1;
	view_add (console_view, cmd_line.view);

	say_line.prompt = "say:";
	say_line.input_line = Con_CreateInputLine (32, MAXCMDLINE, ' ');
	say_line.input_line->complete = 0;
	say_line.input_line->enter = C_Say;
	say_line.input_line->width = con_linewidth - 5;
	say_line.input_line->user_data = &say_line;
	say_line.input_line->draw = input_line_draw;
	say_line.view = view_new_data (0, 0, 320, 8, grav_northwest, &say_line);
	say_line.view->draw = draw_input;
	say_line.view->setgeometry = resize_input;
	say_line.view->visible = 0;
	say_line.view->resize_x = 1;
	view_add (con_data.view, say_line.view);

	team_line.prompt = "say_team:";
	team_line.input_line = Con_CreateInputLine (32, MAXCMDLINE, ' ');
	team_line.input_line->complete = 0;
	team_line.input_line->enter = C_SayTeam;
	team_line.input_line->width = con_linewidth - 10;
	team_line.input_line->user_data = &team_line;
	team_line.input_line->draw = input_line_draw;
	team_line.view = view_new_data (0, 0, 320, 8, grav_northwest, &team_line);
	team_line.view->draw = draw_input;
	team_line.view->setgeometry = resize_input;
	team_line.view->visible = 0;
	team_line.view->resize_x = 1;
	view_add (con_data.view, team_line.view);

	view_add (con_data.view, notify_view);
	view_add (con_data.view, hud_view);
	view_add (con_data.view, console_view);
	view_add (con_data.view, menu_view);

	console_buffer = Draw_CreateBuffer (console_view->xlen / 8,
										console_view->ylen / 8);
	Draw_ClearBuffer (console_buffer);
	con_main = Con_CreateBuffer (CON_BUFFER_SIZE, CON_LINES);
	console_view->draw = draw_console;
	console_view->visible = 0;
	console_view->resize_x = console_view->resize_y = 1;


	notify_buffer = Draw_CreateBuffer (notify_view->xlen / 8, NOTIFY_LINES + 1);
	Draw_ClearBuffer (notify_buffer);
	notify_view->draw = draw_notify;
	notify_view->setgeometry = resize_notify;
	notify_view->resize_x = 1;

	menu_view->draw = Menu_Draw;
	menu_view->visible = 0;

	hud_view->draw = Menu_Draw_Hud;
	hud_view->visible = 0;

	view = view_new (8, 16, 320, 168, grav_southwest);
	view->draw = draw_console_text;
	view->setgeometry = resize_console_text;
	view->resize_x = view->resize_y = 1;
	view_add (console_view, view);

	view = view_new (0, 2, 320, 11, grav_southwest);
	view->draw = draw_download;
	view->resize_x = 1;
	view_add (console_view, view);

	con = con_main;
	con_linewidth = -1;




	Sys_Printf ("Console initialized.\n");

	// register our commands
	Cmd_AddCommand ("toggleconsole", ToggleConsole_f,
					"Toggle the console up and down");
	Cmd_AddCommand ("togglechat", ToggleConsole_f,
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
	IE_Remove_Handler (con_event_id);
}

static general_funcs_t plugin_info_general_funcs = {
	.init = C_Init,
	.shutdown = C_shutdown,
};

static console_funcs_t plugin_info_console_funcs = {
	.print = C_Print,
	.draw_console = C_DrawConsole,
	.new_map = C_NewMap,
	.set_state = C_SetState,
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
