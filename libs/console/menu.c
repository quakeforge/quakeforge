/*
	menu.c

	Menu support code and interface to QC

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie
	Date: 2002/1/18

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

#include <stdlib.h>
#include <string.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/csqc.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/hash.h"
#include "QF/keys.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/ruamoko.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/input/event.h"
#include "QF/ui/view.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "cl_console.h"

typedef struct menu_pic_s {
	struct menu_pic_s *next;
	int         x, y;
	int         srcx, srcy, width, height;
	const char *name;
} menu_pic_t;

typedef struct menu_item_s {
	struct menu_item_s *parent;
	struct menu_item_s **items;
	int         num_items;
	int         max_items;
	int         cur_item;
	int         x, y;
	pr_func_t   func;
	pr_func_t   cursor;
	pr_func_t   keyevent;
	pr_func_t   draw;
	pr_func_t   enter_hook;
	pr_func_t   leave_hook;
	unsigned    fadescreen:1;
	unsigned    allkeys:1;
	const char *text;
	menu_pic_t *pics;
} menu_item_t;

static int confirm_quit;
static cvar_t confirm_quit_cvar = {
	.name = "confirm_quit",
	.description =
		"confirm quit command",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &confirm_quit },
};

static progs_t  menu_pr_state;
static menu_item_t *menu;
//static keydest_t menu_keydest;
static hashtab_t *menu_hash;
static pr_func_t menu_init;
static pr_func_t menu_quit;
static pr_func_t menu_draw_hud;
static pr_func_t menu_pre;
static pr_func_t menu_post;
static char *top_menu;

typedef struct menu_func_s {
	const char *name;
	pr_func_t  *func;
} menu_func_t;

static menu_func_t menu_functions[] = {
	{"menu_init", &menu_init},
	{"menu_draw_hud", &menu_draw_hud},
	{"menu_pre", &menu_pre},
	{"menu_post", &menu_post},
};

static void
run_menu_pre (void)
{
	qfZoneScoped (true);
	PR_ExecuteProgram (&menu_pr_state, menu_pre);
}

static void
run_menu_post (void)
{
	qfZoneScoped (true);
	PR_ExecuteProgram (&menu_pr_state, menu_post);
}

static int
menu_resolve_globals (progs_t *pr)
{
	const char *sym;
	pr_def_t   *def;
	dfunction_t *f;
	size_t      i;

	for (i = 0;
		 i < sizeof (menu_functions) / sizeof (menu_functions[0]); i++) {
		sym = menu_functions[i].name;
		if (!(f = PR_FindFunction (pr, sym)))
			goto error;
		*menu_functions[i].func = (pr_func_t) (f - menu_pr_state.pr_functions);
	}

	if (!(def = PR_FindGlobal (pr, sym = "time")))
		goto error;
	menu_pr_state.globals.ftime = &G_FLOAT (pr, def->ofs);//FIXME double time
	return 1;
error:
	Sys_Printf ("%s: undefined symbol %s\n", pr->progs_name, sym);
	return 0;
}

static const char *
menu_get_key (const void *m, void *unused)
{
	return ((menu_item_t *) m)->text;
}

static void
menu_free (void *_m, void *unused)
{
	int         i;
	menu_item_t *m = (menu_item_t *) _m;

	if (m->text)
		free ((char *) m->text);
	if (m->parent) {
		// remove self from parent list to avoid double frees
		for (i = 0; i < m->parent->num_items; i++)
			if (m->parent->items[i] == m)
				m->parent->items[i] = 0;
	}
	if (m->items) {
		for (i = 0; i < m->num_items; i++)
			if (m->items[i]) {
				m->items[i]->parent = 0;
				if (m->items[i]->text)
					Hash_Del (menu_hash, m->items[i]->text);
				menu_free (m->items[i], 0);
			}
		free (m->items);
	}
	while (m->pics) {
		menu_pic_t *p = m->pics;
		m->pics = p->next;
		if (p->name)
			free ((char *) p->name);
		free (p);
	}
	free (m);
}

static void
menu_add_item (menu_item_t *m, menu_item_t *i)
{
	if (m->num_items == m->max_items) {
		m->items = realloc (m->items,
							(m->max_items + 8) * sizeof (menu_item_t *));
		m->max_items += 8;
	}
	i->parent = m;
	m->items[m->num_items++] = i;
}

static void
menu_pic (int x, int y, const char *name,
		  int srcx, int srcy, int width, int height)
{
	menu_pic_t *pic = malloc (sizeof (menu_pic_t));

	pic->x = x;
	pic->y = y;
	pic->name = strdup (name);
	pic->srcx = srcx;
	pic->srcy = srcy;
	pic->width = width;
	pic->height = height;

	pic->next = menu->pics;
	menu->pics = pic;
}

static void
bi_Menu_Begin (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);
	menu_item_t *m = calloc (sizeof (menu_item_t), 1);

	m->x = x;
	m->y = y;
	m->text = text && text[0] ? strdup (text) : 0;
	if (menu)
		menu_add_item (menu, m);
	menu = m;
	if (m->text)
		Hash_Add (menu_hash, m);
}

static void
bi_Menu_FadeScreen (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu->fadescreen = P_INT (pr, 0);
}

static void
bi_Menu_Draw (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu->draw = P_FUNCTION (pr, 0);
}

static void
bi_Menu_EnterHook (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu->enter_hook = P_FUNCTION (pr, 0);
}

static void
bi_Menu_LeaveHook (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu->leave_hook = P_FUNCTION (pr, 0);
}

static void
bi_Menu_Pic (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);

	menu_pic (x, y, name, 0, 0, -1, -1);
}

static void
bi_Menu_SubPic (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	menu_pic (x, y, name, srcx, srcy, width, height);
}

static void
bi_Menu_CenterPic (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);
	qpic_t     *qpic = r_funcs->Draw_CachePic (name, 1);

	if (!qpic)
		return;

	menu_pic (x - qpic->width / 2, y, name, 0, 0, -1, -1);
}

static void
bi_Menu_CenterSubPic (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_GSTRING (pr, 2);
	qpic_t     *qpic = r_funcs->Draw_CachePic (name, 1);
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	if (!qpic)
		return;

	menu_pic (x - qpic->width / 2, y, name, srcx, srcy, width, height);
}

static void
bi_Menu_Item (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_GSTRING (pr, 2);
	pr_func_t   func = P_FUNCTION (pr, 3);
	int         allkeys = P_INT (pr, 4);
	menu_item_t *mi = calloc (sizeof (menu_item_t), 1);

	mi->x = x;
	mi->y = y;
	mi->text = text && text[0] ? strdup (text) : 0;
	mi->func = func;
	mi->parent = menu;
	mi->allkeys = allkeys;
	menu_add_item (menu, mi);
}

static void
bi_Menu_Cursor (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	pr_func_t   func = P_FUNCTION (pr, 0);

	menu->cursor = func;
}

static void
bi_Menu_KeyEvent (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	pr_func_t   func = P_FUNCTION (pr, 0);

	menu->keyevent = func;
}

static void
bi_Menu_End (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu = menu->parent;
}

static void
bi_Menu_TopMenu (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *name = P_GSTRING (pr, 0);

	if (top_menu)
		free (top_menu);
	top_menu = strdup (name);
}

static void
bi_Menu_SelectMenu (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	const char *name = P_GSTRING (pr, 0);

	menu = 0;
	if (name && *name)
		menu = Hash_Find (menu_hash, name);
	if (menu) {
		Con_SetState (con_menu, true);
		if (menu->enter_hook) {
			run_menu_pre ();
			PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
			run_menu_post ();
		}
	} else {
		if (name && *name)
			Sys_Printf ("no menu \"%s\"\n", name);
		Con_SetState (con_inactive, true);
	}
}

static void
bi_Menu_SetQuit (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	pr_func_t   func = P_FUNCTION (pr, 0);

	menu_quit = func;
}

static void
bi_Menu_Quit (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	if (con_data.quit)
		con_data.quit ();
	Sys_Quit ();
}

static void
bi_Menu_GetIndex (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	if (menu) {
		R_INT (pr) = menu->cur_item;
	} else {
		R_INT (pr) = -1;
	}
}

static void
bi_Menu_Next (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu->cur_item++;
	menu->cur_item %= menu->num_items;
}

static void
bi_Menu_Prev (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu->cur_item += menu->num_items - 1;
	menu->cur_item %= menu->num_items;
}

static void
bi_Menu_Enter (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	menu_item_t *item;

	if (!menu)
		return;

	item = menu->items[menu->cur_item];
	if (item->func) {
		run_menu_pre ();
		PR_PushFrame (&menu_pr_state);
		PR_SetupParams (pr, 2, 1);
		P_STRING (&menu_pr_state, 0) =
			PR_SetTempString (&menu_pr_state, item->text);
		P_INT (&menu_pr_state, 1) = 0;
		PR_ExecuteProgram (&menu_pr_state, item->func);
		PR_PopFrame (&menu_pr_state);
		run_menu_post ();
	} else {
		menu = item;
		if (menu->enter_hook) {
			run_menu_pre ();
			PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
			run_menu_post ();
		}
	}
}

static void
bi_Menu_Leave (progs_t *pr, void *data)
{
	qfZoneScoped (true);
	if (menu) {
		if (menu->leave_hook) {
			run_menu_pre ();
			PR_ExecuteProgram (&menu_pr_state, menu->leave_hook);
			run_menu_post ();
		}
		menu = menu->parent;
		if (!menu) {
			Con_SetState (con_inactive, true);
		}
	}
}

static void
togglemenu_f (void)
{
	if (menu)
		Menu_Leave ();
	else
		Menu_Enter ();
}

static void
quit_f (void)
{
	int         ret;

	if (confirm_quit && menu_quit) {
		run_menu_pre ();
		PR_ExecuteProgram (&menu_pr_state, menu_quit);
		ret = R_INT (&menu_pr_state);
		run_menu_post ();
		if (!ret)
			return;
	}
	bi_Menu_Quit (&menu_pr_state, 0);
}

static void *
menu_allocate_progs_mem (progs_t *pr, int size)
{
	size = (size + 63) & ~63;
#ifdef _WIN32
	return _aligned_malloc (size, 64);
#else
	return aligned_alloc (64, size);
#endif
}

static void
menu_free_progs_mem (progs_t *pr, void *mem)
{
#ifdef _WIN32
	_aligned_free (mem);
#else
	free (mem);
#endif
}

static void *
menu_load_file (progs_t *pr, const char *path, off_t *size)
{
	void *data = QFS_LoadFile (QFS_FOpenFile (path), 0);
	*size = qfs_filesize;
	return data;
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(Menu_Begin,        3, p(int), p(int), p(string)),
	bi(Menu_FadeScreen,   1, p(int)),
	bi(Menu_Draw,         2, p(int), p(int)),
	bi(Menu_EnterHook,    1, p(func)),
	bi(Menu_LeaveHook,    1, p(func)),
	bi(Menu_Pic,          3, p(int), p(int), p(string)),
	bi(Menu_SubPic,       7, p(int), p(int), p(string),
	                         p(int), p(int), p(int), p(int)),
	bi(Menu_CenterPic,    3, p(int), p(int), p(string)),
	bi(Menu_CenterSubPic, 7, p(int), p(int), p(string),
	                         p(int), p(int), p(int), p(int)),
	bi(Menu_Item,         5, p(int), p(int), p(string), p(func), p(int)),
	bi(Menu_Cursor,       1, p(func)),
	bi(Menu_KeyEvent,     1, p(func)),
	bi(Menu_End,          0),
	bi(Menu_TopMenu,      1, p(string)),
	bi(Menu_SelectMenu,   1, p(string)),
	bi(Menu_SetQuit,      1, p(func)),
	bi(Menu_Quit,         0),
	bi(Menu_GetIndex,     0),
	bi(Menu_Next,         0),
	bi(Menu_Prev,         0),
	bi(Menu_Enter,        0),
	bi(Menu_Leave,        0),
	{0},
};

static int//FIXME reimplement users properly (or remove?)
Menu_KeyEvent (int key, int unicode, int pressed)
{
	IE_event_t  event = {
		.type = ie_key,
		.when = Sys_LongTime (),
		.key = {
			.code = key,
			.unicode = unicode,
		}
	};
	return IE_Send_Event (&event);
}

void
Menu_Enter_f (void)
{
	if (!Menu_KeyEvent(QFK_RETURN, '\0', true))
		Menu_KeyEvent('y', 'y', true);
}

void
Menu_Leave_f (void)
{
	Menu_Leave ();
}

void
Menu_Prev_f (void)
{
	Menu_KeyEvent (QFK_UP, '\0', true);
}

void
Menu_Next_f (void)
{
	Menu_KeyEvent (QFK_DOWN, '\0', true);
}

void
Menu_Shutdown (void)
{
	PR_Shutdown (&menu_pr_state);
	Hash_DelTable (menu_hash);
	free (top_menu);
}

void
Menu_Init (void)
{
	menu_pr_state.progs_name = "menu.dat";
	menu_pr_state.allocate_progs_mem = menu_allocate_progs_mem;
	menu_pr_state.free_progs_mem = menu_free_progs_mem;
	menu_pr_state.load_file = menu_load_file;
	menu_pr_state.resolve = menu_resolve_globals;

	menu_pr_state.max_edicts = 0;
	menu_pr_state.zone_size = 1024 * 1024;
	menu_pr_state.stack_size = 64 * 1024;

	PR_Init (&menu_pr_state);

	menu_hash = Hash_NewTable (61, menu_get_key, menu_free, 0, 0);

	PR_RegisterBuiltins (&menu_pr_state, builtins, 0);

	RUA_Init (&menu_pr_state, 3);

	InputLine_Progs_Init (&menu_pr_state);
	RUA_Game_Init (&menu_pr_state, 1);
	GIB_Progs_Init (&menu_pr_state);
	PR_Cmds_Init (&menu_pr_state);
	R_Progs_Init (&menu_pr_state);
	S_Progs_Init (&menu_pr_state);

	Cvar_Register (&confirm_quit_cvar, 0, 0);

	Cmd_AddCommand ("togglemenu", togglemenu_f,
					"Toggle the display of the menu");
	Cmd_RemoveCommand ("quit");
	Cmd_AddCommand ("quit", quit_f, "Exit the program");
	Cmd_AddCommand ("Menu_Enter", Menu_Enter_f, "Do menu action/move up in the menu tree.");
	Cmd_AddCommand ("Menu_Leave", Menu_Leave_f, "Move down in the menu tree.");
	Cmd_AddCommand ("Menu_Prev",  Menu_Prev_f, "Move cursor up.");
	Cmd_AddCommand ("Menu_Next",  Menu_Next_f, "Move cursor up.");
}

void
Menu_Load (void)
{
	qfZoneScoped (true);
	int         size;
	QFile      *file;

	Hash_FlushTable (menu_hash);
	menu = 0;
	top_menu = 0;

	menu_pr_state.progs = 0;
	if ((file = QFS_FOpenFile (menu_pr_state.progs_name))) {
		size = Qfilesize (file);
		PR_LoadProgsFile (&menu_pr_state, file, size);
		Qclose (file);

		if (!PR_RunLoadFuncs (&menu_pr_state)
			|| !PR_RunPostLoadFuncs (&menu_pr_state)) {
			free (menu_pr_state.progs);
			menu_pr_state.progs = 0;
		}
	}
	if (!menu_pr_state.progs) {
		// Not a fatal error, just means no menus
		Con_SetOrMask (0x80);
		Sys_Printf ("Menu_Load: could not load %s\n",
					menu_pr_state.progs_name);
		Con_SetOrMask (0x00);
		return;
	}
	run_menu_pre ();
	RUA_Cbuf_SetCbuf (&menu_pr_state, con_data.cbuf);
	InputLine_Progs_SetDraw (&menu_pr_state, C_DrawInputLine);
	PR_ExecuteProgram (&menu_pr_state, menu_init);
	run_menu_post ();
}

void
Menu_Draw (view_t view)
{
	menu_pic_t *m_pic;
	int         i, x, y;
	menu_item_t *item;

	if (!menu)
		return;

	view_pos_t  abs = View_GetAbs (view);
	x = abs.x;
	y = abs.y;

	if (menu->fadescreen)
		r_funcs->Draw_FadeScreen ();

	*menu_pr_state.globals.ftime = *con_data.realtime;//FIXME double time

	if (menu->draw) {
		int         ret;

		run_menu_pre ();
		PR_PushFrame (&menu_pr_state);
		PR_SetupParams (&menu_pr_state, 2, 1);
		P_INT (&menu_pr_state, 0) = x;
		P_INT (&menu_pr_state, 1) = y;
		PR_ExecuteProgram (&menu_pr_state, menu->draw);
		ret = R_INT (&menu_pr_state);
		PR_PopFrame (&menu_pr_state);
		run_menu_post ();
		if (!ret)
			return;
	}


	for (m_pic = menu->pics; m_pic; m_pic = m_pic->next) {
		qpic_t     *pic = r_funcs->Draw_CachePic (m_pic->name, 1);
		if (!pic)
			continue;
		if (m_pic->width > 0 && m_pic->height > 0)
			r_funcs->Draw_SubPic (x + m_pic->x, y + m_pic->y, pic,
						 m_pic->srcx, m_pic->srcy,
						 m_pic->width, m_pic->height);
		else
			r_funcs->Draw_Pic (x + m_pic->x, y + m_pic->y, pic);
	}
	for (i = 0; i < menu->num_items; i++) {
		if (menu->items[i]->text) {
			r_funcs->Draw_String (x + menu->items[i]->x + 8,
								  y + menu->items[i]->y,
						 menu->items[i]->text);
		}
	}
	if (!menu->items)
		return;
	item = menu->items[menu->cur_item];
	if (menu->cursor) {
		run_menu_pre ();
		PR_PushFrame (&menu_pr_state);
		PR_SetupParams (&menu_pr_state, 2, 1);
		P_INT (&menu_pr_state, 0) = x + item->x;
		P_INT (&menu_pr_state, 1) = y + item->y;
		PR_ExecuteProgram (&menu_pr_state, menu->cursor);
		PR_PopFrame (&menu_pr_state);
		run_menu_post ();
	} else {
		r_funcs->Draw_Character (x + item->x, y + item->y,
								 12 + ((int) (*con_data.realtime * 4) & 1));
	}
}

void
Menu_Draw_Hud (view_t view)
{
	run_menu_pre ();
	*menu_pr_state.globals.ftime = *con_data.realtime;//FIXME double time

	PR_ExecuteProgram (&menu_pr_state, menu_draw_hud);
	run_menu_post ();
}

static int
menu_key_event (const IE_event_t *ie_event)
{
	menu_item_t *item;
	int         ret;
	__auto_type key = ie_event->key;

	if (!menu)
		return 0;
	if (menu->keyevent) {
		run_menu_pre ();
		PR_PushFrame (&menu_pr_state);
		PR_SetupParams (&menu_pr_state, 3, 1);
		P_INT (&menu_pr_state, 0) = key.code;
		P_INT (&menu_pr_state, 1) = key.unicode;
		P_INT (&menu_pr_state, 2) = 1;	//FIXME only presses now
		PR_ExecuteProgram (&menu_pr_state, menu->keyevent);
		ret = R_INT (&menu_pr_state);
		PR_PopFrame (&menu_pr_state);
		run_menu_post ();
		if (ret)
			return 1;
	} else if (menu->items && menu->items[menu->cur_item]->func
			   && menu->items[menu->cur_item]->allkeys) {
		run_menu_pre ();
		PR_PushFrame (&menu_pr_state);
		item = menu->items[menu->cur_item];
		PR_SetupParams (&menu_pr_state, 2, 1);
		P_STRING (&menu_pr_state, 0) = PR_SetTempString (&menu_pr_state,
														 item->text);
		P_INT (&menu_pr_state, 1) = key.code;
		PR_ExecuteProgram (&menu_pr_state, item->func);
		ret = R_INT (&menu_pr_state);
		PR_PopFrame (&menu_pr_state);
		run_menu_post ();
		if (ret)
			return 1;
	}
	if (key.code == QFK_ESCAPE) {
		Menu_Leave ();
		return 1;
	}
	if (!menu || !menu->items)
		return 0;
	switch (key.code) {
		case QFK_ESCAPE:
			break;
		case QFK_DOWN:
//		case QFM_WHEEL_DOWN:
			bi_Menu_Next (&menu_pr_state, 0);
			break;
		case QFK_UP:
//		case QFM_WHEEL_UP:
			bi_Menu_Prev (&menu_pr_state, 0);
			break;
		case QFK_RETURN:
//		case QFM_BUTTON1:
			bi_Menu_Enter (&menu_pr_state, 0);
			break;
		default:
			break;
	}
	return 1;
}

static int
menu_mouse_event (const IE_event_t *ie_event)
{
	return 0;
}

int
Menu_EventHandler (const IE_event_t *ie_event)
{
	static int (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_key] = menu_key_event,
		[ie_mouse] = menu_mouse_event,
	};
	if ((unsigned) ie_event->type >= ie_event_count
		|| !handlers[ie_event->type]) {
		return 0;
	}
	return handlers[ie_event->type] (ie_event);
}

void
Menu_Enter ()
{
	if (!top_menu) {
		Con_SetState (con_active, true);
		return;
	}
	if (!menu) {
		menu = Hash_Find (menu_hash, top_menu);
	}
	if (menu) {
		Con_SetState (con_menu, true);
		if (menu->enter_hook) {
			run_menu_pre ();
			PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
			run_menu_post ();
		}
	}
}

void
Menu_Leave ()
{
	if (menu) {
		if (menu->leave_hook) {
			run_menu_pre ();
			PR_ExecuteProgram (&menu_pr_state, menu->leave_hook);
			run_menu_post ();
		}
		menu = menu->parent;
		if (!menu) {
			Con_SetState (con_inactive, true);
		}
	}
}
