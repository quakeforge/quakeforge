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
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/va.h"

#include "QF/input/event.h"

#include "QF/plugin/general.h"
#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "QF/ui/canvas.h"
#include "QF/ui/inputline.h"
#include "QF/ui/view.h"

#include "cl_console.h"
#include "compat.h"

static con_buffer_t *con;

static float con_cursorspeed = 4;

static int con_scale;
static cvar_t con_scale_cvar = {
	.name = "con_scale",
	.description =
		"Pixel scale factor for the console and all 2D user interface "
		"elements",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &con_scale },
};
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

static uint32_t client_base;
static uint32_t canvas_base;
static uint32_t view_base;

static con_state_t con_state;
static bool con_hide_mouse;	// external requested state
static bool con_show_mouse;	// local (overrides con_hide_mouse)
static bool con_force_mouse_visible;// internal (overrides all for show)
static int  con_event_id;
static int  con_saved_focus;

static bool con_debuglog;
static bool chat_team;
static dstring_t *c_print_buffer;
static dstring_t *dlbar;
static dstring_t *old_gamedir = 0;


typedef struct {
	const char *prompt;
	inputline_t *input_line;
	draw_charbuffer_t *buffer;
} con_input_t;

enum {
	client_input,
	client_cursor,

	client_comp_count
};

static const component_t client_components[client_comp_count] = {
	[client_input] = {
		.size = sizeof (con_input_t *),
		.name = "input",
	},
	[client_cursor] = {
		.size = sizeof (con_input_t *),
		.name = "cursor",
	},
};

console_data_t con_data = {
	.components = client_components,
	.num_components = client_comp_count,
};

#define	MAXCMDLINE 256

static qpic_t  *conback;
static con_input_t cmd_line;
static con_input_t say_line;
static inputline_t *chat_input;
static inputline_t *team_input;

static uint32_t screen_canvas;
static view_t screen_view;
static view_t   console_view;
static view_t     buffer_view;
static view_t     command_view;
static view_t     download_view;
static view_t   notify_view;
static view_t   say_view;
static view_t   menu_view;

#define CON_BUFFER_SIZE 32768
#define CON_LINES 1024
static draw_charbuffer_t *console_buffer;
static con_buffer_t *con_main;
static int view_offset;
static draw_charbuffer_t *download_buffer;

#define	NOTIFY_LINES 4
static draw_charbuffer_t *notify_buffer;
// ring buffer holding realtime time the line was generated
static float notify_times[NOTIFY_LINES + 1];
static int notify_head;
static int notify_tail;

static bool con_initialized;

static inline void *
con_getcomponent (view_t view, uint32_t comp)
{
	return Ent_GetComponent (view.id, comp, view.reg);
}

static inline int
con_hascomponent (view_t view, uint32_t comp)
{
	return Ent_HasComponent (view.id, comp, view.reg);
}

static inline void *
con_setcomponent (view_t view, uint32_t comp, void *data)
{
	return Ent_SetComponent (view.id, comp, view.reg, data);
}

static void
con_setfunc (view_t view, uint32_t comp, canvas_update_f func)
{
	con_setcomponent (view, canvas_base + comp, &func);
}

static void
con_setinput (view_t view, con_input_t *input)
{
	con_setcomponent (view, client_base + client_input, &input);
}

static con_input_t *
con_getinput (view_t view)
{
	return *(con_input_t**)con_getcomponent (view, client_base + client_input);
}

static int
con_hasinput (view_t view)
{
	return con_hascomponent (view, client_base + client_input);
}

static void
con_setfitpic (view_t view, qpic_t *pic)
{
	con_setcomponent (view, canvas_base + canvas_fitpic, &pic);
}

static void
con_setcharbuf (view_t view, draw_charbuffer_t *buffer)
{
	con_setcomponent (view, canvas_base + canvas_charbuff, &buffer);
}

static void
con_setcursor (view_t view, con_input_t *input)
{
	con_setcomponent (view, client_base + client_cursor, &input);
}

static inline void
con_remcomponent (view_t view, uint32_t comp)
{
	Ent_RemoveComponent (view.id, comp, view.reg);
}

static void
con_remfunc (view_t view, uint32_t comp)
{
	con_remcomponent (view, canvas_base + comp);
}

static inline void
con_remcharbuf (view_t view)
{
	con_remcomponent (view, canvas_base + canvas_charbuff);
}

static inline void
con_remcursor (view_t view)
{
	con_remcomponent (view, client_base + client_cursor);
}

static void
load_conback (const char *path)
{
	qpic_t     *p;
	if (strlen (path) < 4 || strcmp (path + strlen (path) - 4, ".lmp")
		|| !(p = (qpic_t *) QFS_LoadFile (QFS_FOpenFile (path), 0))) {
		return;
	}
	conback = r_funcs->Draw_MakePic (p->width, p->height, p->data);
	free (p);
}

static void
ClearNotify (void)
{
	Draw_ClearBuffer (notify_buffer);
	notify_head = notify_tail = 0;
}

static void
con_update_mouse (void)
{
	bool show_cursor = (con_force_mouse_visible
						|| con_show_mouse
						|| !con_hide_mouse);
	VID_SetCursor (show_cursor);
	IN_UpdateGrab (show_cursor ? 0 : in_grab);
}

void
Con_Show_Mouse (bool visible)
{
	con_show_mouse = visible;
	con_update_mouse ();
}

static void
C_SetState (con_state_t state, bool hide_mouse)
{
	con_state_t old_state = con_state;
	con_state = state;
	con_hide_mouse = hide_mouse;
	if (con_state == con_inactive) {
		IE_Set_Focus (con_saved_focus);
		con_force_mouse_visible = false;
	} else if (old_state == con_inactive) {
		con_saved_focus = IE_Get_Focus ();
		IE_Set_Focus (con_event_id);
		con_force_mouse_visible = true;
	}
	con_update_mouse ();

	if (state == con_message) {
		say_line.prompt = "say:";
		say_line.input_line = chat_input;
		if (chat_team) {
			say_line.prompt = "say_team:";
			say_line.input_line = team_input;
		}
		__auto_type buffer = say_line.buffer;
		buffer->cursx = 0;
		Draw_PrintBuffer (buffer, say_line.prompt);
		say_line.input_line->width = buffer->width - buffer->cursx;
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
			C_SetState (con_active, con_hide_mouse);
			break;
		case con_active:
			C_SetState (con_inactive, con_hide_mouse);
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
	C_SetState (con_inactive, con_hide_mouse);
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
	int         mask, c;

	if (!c_print_buffer)
		c_print_buffer = dstring_new ();

	dvsprintf (c_print_buffer, fmt, args);

	// log all messages to file
	if (con_debuglog)
		Sys_DebugLog (va (0, "%s/%s/qconsole.log", qfs_userpath,//FIXME
					  qfs_gamedir->dir.def), "%s", c_print_buffer->str);

	if (!con_initialized)
		return;

	s = c_print_buffer->str;

	mask = 0;
	if (s[0] == 1 || s[0] == 2) {
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
	if ((byte)c_print_buffer->str[0] > 2)
		fputs (c_print_buffer->str, stdout);
	else if ((byte)c_print_buffer->str[0])
		fputs (c_print_buffer->str + 1, stdout);
}

/* DRAWING */

static void
DrawInputLine (int x, int y, inputline_t *il)
{
	const char *s = il->lines[il->edit_line] + il->scroll;

	if (il->scroll) {
		r_funcs->Draw_Character (x, y, '<' | 0x80);
		r_funcs->Draw_nString (x + 8, y, s + 1, il->width - 2);
	} else {
		r_funcs->Draw_nString (x, y, s, il->width - 1);
	}
	if (strlen (s) >= il->width)
		r_funcs->Draw_Character (x + ((il->width - 1) * 8), y, '>' | 0x80);

	if (il->cursor) {
		float       t = *con_data.realtime * con_cursorspeed;
		int         ch = 10 + ((int) (t) & 1);

		int         cx = (il->linepos - il->scroll) * 8;
		r_funcs->Draw_Character (x + cx, y, ch);
	}
}

void
C_DrawInputLine (inputline_t *il)
{
	DrawInputLine (il->x, il->y, il);
}

static void
draw_input_line (inputline_t *il, draw_charbuffer_t *buffer)
{
	char       *dst = buffer->chars + buffer->cursx;
	char       *src = il->lines[il->edit_line] + il->scroll + 1;
	size_t      i;

	*dst++ = il->scroll ? (char) ('<' | 0x80) : il->lines[il->edit_line][0];
	for (i = 0; i < il->width - 2 && *src; i++) {
		*dst++ = *src++;
	}
	while (i++ < il->width - 2) {
		*dst++ = ' ';
	}
	*dst++ = *src ? (char) ('<' | 0x80) : ' ';
}

static void
input_line_draw (inputline_t *il)
{
	__auto_type inp = (con_input_t *) il->user_data;
	draw_input_line (il, inp->buffer);
}

static void
resize_input (view_t view, view_pos_t len)
{
	if (!con_hasinput (view)) {
		return;
	}
	__auto_type inp = con_getinput (view);

	if (inp->buffer) {
		Draw_DestroyBuffer (inp->buffer);
	}
	inp->buffer = Draw_CreateBuffer (len.x / 8, 1);
	Draw_ClearBuffer (inp->buffer);
	Draw_PrintBuffer (inp->buffer, inp->prompt);
	if (inp->input_line) {
		inp->input_line->width = inp->buffer->width - inp->buffer->cursx;
		inp->buffer->chars[inp->buffer->cursx] = inp->input_line->prompt_char;
	}
}

static void
update_download (void)
{
	const char *text;

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
	int         i;
	for (i = 0; i < download_buffer->width && dlbar->str[i]; i++) {
		download_buffer->chars[i] = dlbar->str[i];
	}
	for (; i < download_buffer->width; i++) {
		download_buffer->chars[i] = ' ';
	}
}

static void
clear_console_text (void)
{
	Draw_ClearBuffer (console_buffer);
	console_buffer->cursy = console_buffer->height - 1;
}

static void
resize_console_text (view_t view, view_pos_t len)
{
	int			width = len.x / 8;
	int         height = len.y / 8;

	width = max (width, 1);
	height = max (height, 1);
	if (console_buffer->width != width || console_buffer->height != height) {
		con_linewidth = width;
		Draw_DestroyBuffer (console_buffer);
		console_buffer = Draw_CreateBuffer (width, height);
		con_setcharbuf (buffer_view, console_buffer);
		clear_console_text ();
	}
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
draw_cursor (view_t view)
{
	float       t = *con_data.realtime * con_cursorspeed;
	int         ch = 10 + ((int) (t) & 1);

	__auto_type inp = con_getinput (view);
	__auto_type buff = inp->buffer;
	__auto_type il = inp->input_line;
	if (!il->cursor) {
		return;
	}
	view_pos_t  pos = View_GetAbs (view);
	int         x = (buff->cursx + il->linepos - il->scroll) * 8;
	r_funcs->Draw_Character (pos.x + x, pos.y, ch);
}

static void
update_notify (void)
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
}

static void
resize_notify (view_t view, view_pos_t len)
{
	Draw_DestroyBuffer (notify_buffer);
	notify_buffer = Draw_CreateBuffer (len.x / 8, NOTIFY_LINES + 1);
	con_setcharbuf (notify_view, notify_buffer);
	ClearNotify ();
}

static void
resize_download (view_t view, view_pos_t len)
{
	if (download_buffer) {
		Draw_DestroyBuffer (download_buffer);
		download_buffer = Draw_CreateBuffer (len.x / 8, 1);
	}
}

static void
setup_console (void)
{
	float       lines = 0;
	view_pos_t  screen_len = View_GetLen (screen_view);

	switch (con_state) {
		case con_message:
		case con_menu:
		case con_inactive:
			lines = 0;
			break;
		case con_active:
			lines = screen_len.y * bound (0.2, con_size, 1);
			break;
		case con_fullscreen:
			lines = con_data.lines = screen_len.y;
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
	if (con_data.lines > screen_len.y) {
		con_data.lines = screen_len.y;
	}
	if (con_data.lines >= screen_len.y - r_data->lineadj)
		r_data->scr_copyeverything = 1;
}

static void
C_DrawConsole (void)
{
	qfZoneNamed (zone, true);
	if (con_debug) {
		Con_Debug_Draw ();
	}
	setup_console ();
	view_pos_t  screen_len = View_GetLen (screen_view);

	view_pos_t  pos = View_GetPos (console_view);
	int         ypos = con_data.lines - screen_len.y;
	if (pos.y != ypos) {
		View_SetPos (console_view, pos.x, ypos);
		View_UpdateHierarchy (console_view);
	}
	if (con_state == con_menu) {
		Menu_Draw (menu_view);
		return;
	}

	if (!con_data.lines && con_state == con_message) {
		con_setcharbuf (say_view, say_line.buffer);
		con_setcursor (say_view, &say_line);
		con_setfunc (say_view, canvas_lateupdate, draw_cursor);
	} else {
		con_remcharbuf (say_view);
		con_remcursor (say_view);
		con_remfunc (say_view, canvas_lateupdate);
	}
	if (con_data.lines) {
		con_remcharbuf (notify_view);
		con_setcharbuf (command_view, cmd_line.buffer);
		con_setcursor (command_view, &cmd_line);
		con_setfunc (command_view, canvas_lateupdate, draw_cursor);
	} else {
		con_setcharbuf (notify_view, notify_buffer);
		con_remcharbuf (command_view);
		con_remcursor (command_view);
		con_remfunc (command_view, canvas_lateupdate);
	}
	if (con_data.dl_name && *con_data.dl_name->str) {
		if (!download_buffer) {
			view_pos_t  len = View_GetLen (download_view);
			download_buffer = Draw_CreateBuffer (len.x / 8, 1);
			con_setcharbuf (download_view, download_buffer);
		}
		update_download ();
	} else if (download_buffer) {
		Draw_DestroyBuffer (download_buffer);
		con_remcharbuf (download_view);
	}

	update_notify ();
}

static void
C_NewMap (void)
{
	if (!old_gamedir || !strequal (old_gamedir->str, qfs_gamedir->gamedir))
		Menu_Load ();
	if (!old_gamedir)
		old_gamedir = dstring_newstr ();
	dstring_copystr (old_gamedir, qfs_gamedir->gamedir);
}

static void
exec_line (inputline_t *il)
{
	Con_ExecLine (il->line);
}

static int  win_xlen = -1;
static int  win_ylen = -1;

static void
con_set_size (void)
{
	int         xlen = win_xlen / con_scale;
	int         ylen = win_ylen / con_scale;
	if (xlen > 0 && ylen > 0) {
		Canvas_SetLen (*con_data.canvas_sys, screen_canvas,
					   (view_pos_t) { xlen, ylen });
	}
}

static void
con_app_window (const IE_event_t *event)
{
	if (win_xlen != event->app_window.xlen
		|| win_ylen != event->app_window.ylen) {
		win_xlen = event->app_window.xlen;
		win_ylen = event->app_window.ylen;
		con_set_size ();
	}
}

static void
con_scale_f (void *data, const cvar_t *cvar)
{
	con_scale = bound (1, con_scale, Draw_MaxScale ());
	Draw_SetScale (con_scale);
	if (con_initialized) {
		con_set_size ();
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
			il = team_input;
		} else {
			il = chat_input;
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
		if (ie_event->type == ie_app_window) {
			con_app_window (ie_event);
		}
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
	C_SetState (con_message, con_hide_mouse);
}

static void
MessageMode2_f (void)
{
	if (con_state != con_inactive)
		return;
	chat_team = true;
	C_SetState (con_message, con_hide_mouse);
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
C_InitCvars (void)
{
	Cvar_Register (&con_notifytime_cvar, 0, 0);
	Cvar_Register (&con_scale_cvar, con_scale_f, 0);
	Cvar_Register (&con_alpha_cvar, 0, 0);
	Cvar_Register (&con_size_cvar, 0, 0);
	Cvar_Register (&con_speed_cvar, 0, 0);
	Cvar_Register (&cl_conmode_cvar, 0, 0);
	Con_Debug_InitCvars ();
}

static void
C_Init (void)
{
	client_base = con_data.component_base;
	canvas_base = con_data.canvas_sys->base;
	view_base = con_data.canvas_sys->view_base;
#ifdef __QNXNTO__
	setlocale (LC_ALL, "C-TRADITIONAL");
#endif

	con_event_id = IE_Add_Handler (con_event_handler, 0);

	Menu_Init ();

	con_debuglog = COM_CheckParm ("-condebug");

	// The console will get resized, so assume initial size is 320x200
	ecs_system_t sys = { con_data.canvas_sys->reg, view_base };
	screen_canvas = Canvas_New (*con_data.canvas_sys);
	Con_Debug_Init ();

	screen_view   = Canvas_GetRootView (*con_data.canvas_sys, screen_canvas);
	console_view  = View_New (sys, screen_view);
	buffer_view   = View_New (sys, console_view);
	command_view  = View_New (sys, console_view);
	download_view = View_New (sys, console_view);
	notify_view   = View_New (sys, screen_view);
	say_view      = View_New (sys, screen_view);
	menu_view     = View_New (sys, screen_view);

	View_SetVisible (screen_view, 1);
	View_SetVisible (console_view, 1);
	View_SetVisible (buffer_view, 1);
	View_SetVisible (command_view, 1);
	View_SetVisible (download_view, 1);
	View_SetVisible (notify_view, 1);
	View_SetVisible (say_view, 1);
	View_SetVisible (menu_view, 1);

	View_SetGravity (screen_view,   grav_northwest);
	View_SetGravity (console_view,  grav_northwest);
	View_SetGravity (buffer_view,   grav_southwest);
	View_SetGravity (command_view,  grav_southwest);
	View_SetGravity (download_view, grav_southwest);
	View_SetGravity (notify_view,   grav_northwest);
	View_SetGravity (say_view,      grav_northwest);
	View_SetGravity (menu_view,     grav_center);

	View_SetResize (screen_view,   1, 1);
	View_SetResize (console_view,  1, 1);
	View_SetResize (buffer_view,   1, 1);
	View_SetResize (command_view,  1, 0);
	View_SetResize (download_view, 1, 0);
	View_SetResize (notify_view,   1, 0);
	View_SetResize (say_view,      1, 0);
	View_SetResize (menu_view,     0, 0);

	View_SetPos (screen_view,   0, 0);
	View_SetPos (console_view,  0, 0);
	View_SetPos (buffer_view,   0, 24);
	View_SetPos (command_view,  0, 0);
	View_SetPos (download_view, 0, 2);
	View_SetPos (notify_view,   8, 8);
	View_SetPos (say_view,      0, 0);

	View_SetLen (screen_view,   320, 200);
	View_SetLen (console_view,  320, 200);
	View_SetLen (buffer_view,   320, 176);
	View_SetLen (command_view,  320, 12);
	View_SetLen (download_view, 320, 8);
	View_SetLen (notify_view,   312, NOTIFY_LINES * 8);
	View_SetLen (say_view,      320, 8);
	View_SetLen (menu_view,     320, 200);

	load_conback ("gfx/conback.lmp");
	if (conback) {
		con_setfitpic (console_view, conback);
	}

	con_linewidth = 320 / 8;

	cmd_line.prompt = "";
	cmd_line.input_line = Con_CreateInputLine (32, MAXCMDLINE, ']');
	cmd_line.input_line->complete = Con_BasicCompleteCommandLine;
	cmd_line.input_line->enter = exec_line;
	cmd_line.input_line->width = con_linewidth;
	cmd_line.input_line->user_data = &cmd_line;
	cmd_line.input_line->draw = input_line_draw;

	say_line.prompt = "say:";
	chat_input = Con_CreateInputLine (32, MAXCMDLINE, ' ');
	chat_input->complete = 0;
	chat_input->enter = C_Say;
	chat_input->width = con_linewidth - 5;
	chat_input->user_data = &say_line;
	chat_input->draw = input_line_draw;

	team_input = Con_CreateInputLine (32, MAXCMDLINE, ' ');
	team_input->complete = 0;
	team_input->enter = C_SayTeam;
	team_input->width = con_linewidth - 10;
	team_input->user_data = &say_line;
	team_input->draw = input_line_draw;

	con_setinput (say_view, &say_line);
	con_setinput (command_view, &cmd_line);

	view_pos_t  len;

	len = View_GetLen (buffer_view);
	console_buffer = Draw_CreateBuffer (len.x / 8, len.y / 8);
	Draw_ClearBuffer (console_buffer);
	con_setcharbuf (buffer_view, console_buffer);
	con_main = Con_CreateBuffer (CON_BUFFER_SIZE, CON_LINES);

	len = View_GetLen (command_view);
	cmd_line.buffer = Draw_CreateBuffer (len.x / 8, len.y / 8);
	Draw_ClearBuffer (cmd_line.buffer);
	con_setcharbuf (command_view, cmd_line.buffer);

	len = View_GetLen (notify_view);
	notify_buffer = Draw_CreateBuffer (len.x / 8, NOTIFY_LINES + 1);
	Draw_ClearBuffer (notify_buffer);

	len = View_GetLen (say_view);
	say_line.buffer = Draw_CreateBuffer (len.x / 8, len.y / 8);
	Draw_ClearBuffer (say_line.buffer);

	View_UpdateHierarchy (screen_view);

	// set onresize after View_UpdateHierarchy so the callbacks are NOT called
	// during init
	View_SetOnResize (buffer_view,   resize_console_text);
	View_SetOnResize (command_view,  resize_input);
	View_SetOnResize (download_view, resize_download);
	View_SetOnResize (notify_view,   resize_notify);
	View_SetOnResize (say_view,      resize_input);

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

	con_initialized = true;
}

static void
C_shutdown (void)
{
	Con_Debug_Shutdown ();

	r_funcs->Draw_DestroyPic (conback);
	IE_Remove_Handler (con_event_id);
	Menu_Shutdown ();

	Con_DestroyInputLine (cmd_line.input_line);
	Con_DestroyInputLine (team_input);
	Con_DestroyInputLine (chat_input);
	Con_DestroyBuffer (con_main);

	if (download_buffer) {
		Draw_DestroyBuffer (download_buffer);
	}
	Draw_DestroyBuffer (console_buffer);
	Draw_DestroyBuffer (cmd_line.buffer);
	Draw_DestroyBuffer (notify_buffer);
	Draw_DestroyBuffer (say_line.buffer);

	if (c_print_buffer) {
		dstring_delete (c_print_buffer);
	}
	if (dlbar) {
		dstring_delete (dlbar);
	}
	if (old_gamedir) {
		dstring_delete (old_gamedir);
	}
}

static general_data_t plugin_info_general_data;

static general_funcs_t plugin_info_general_funcs = {
	.init = C_InitCvars,
	.shutdown = C_shutdown,
};

static console_funcs_t plugin_info_console_funcs = {
	.init = C_Init,
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
