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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#include <stdlib.h>
#include <string.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/csqc.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/hash.h"
#include "QF/plugin.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"

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
	func_t      func;
	func_t      cursor;
	func_t      keyevent;
	func_t      draw;
	func_t      enter_hook;
	func_t      leave_hook;
	unsigned    fadescreen:1;
	unsigned    allkeys:1;
	const char *text;
	menu_pic_t *pics;
} menu_item_t;

static cvar_t  *confirm_quit;

static progs_t  menu_pr_state;
static menu_item_t *menu;
static hashtab_t *menu_hash;
static func_t   menu_init;
static func_t   menu_quit;
static const char *top_menu;

static int
menu_resolve_globals (void)
{
	const char *sym;
	dfunction_t *f;

	if (!(f = ED_FindFunction (&menu_pr_state, sym = "menu_init")))
		goto error;
	menu_init = (func_t)(f - menu_pr_state.pr_functions);
	return 1;
error:
	Con_Printf ("%s: undefined function %s\n", menu_pr_state.progs_name, sym);
	return 0;
}

static const char *
menu_get_key (void *m, void *unused)
{
	return ((menu_item_t *)m)->text;
}

static void
menu_free (void *_m, void *unused)
{
	int         i;
	menu_item_t *m = (menu_item_t *)_m;

	if (m->text)
		free ((char*)m->text);
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
			free ((char*)p->name);
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
bi_Menu_Begin (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_STRING (pr, 2);
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
bi_Menu_FadeScreen (progs_t *pr)
{
	menu->fadescreen = P_INT (pr, 0);
}

static void
bi_Menu_Draw (progs_t *pr)
{
	menu->draw = P_FUNCTION (pr, 0);
}

static void
bi_Menu_EnterHook (progs_t *pr)
{
	menu->enter_hook = P_FUNCTION (pr, 0);
}

static void
bi_Menu_LeaveHook (progs_t *pr)
{
	menu->leave_hook = P_FUNCTION (pr, 0);
}

static void
bi_Menu_Pic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_STRING (pr, 2);

	menu_pic (x, y, name, 0, 0, -1, -1);
}

static void
bi_Menu_SubPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_STRING (pr, 2);
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	menu_pic (x, y, name, srcx, srcy, width, height);
}

static void
bi_Menu_CenterPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_STRING (pr, 2);
	qpic_t     *qpic = Draw_CachePic (name, 1);

	if (!qpic)
		return;

	menu_pic (x - qpic->width / 2, y, name, 0, 0, -1, -1);
}

static void
bi_Menu_CenterSubPic (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *name = P_STRING (pr, 2);
	qpic_t     *qpic = Draw_CachePic (name, 1);
	int         srcx = P_INT (pr, 3);
	int         srcy = P_INT (pr, 4);
	int         width = P_INT (pr, 5);
	int         height = P_INT (pr, 6);

	if (!qpic)
		return;

	menu_pic (x - qpic->width / 2, y, name, srcx, srcy, width, height);
}

static void
bi_Menu_Item (progs_t *pr)
{
	int         x = P_INT (pr, 0);
	int         y = P_INT (pr, 1);
	const char *text = P_STRING (pr, 2);
	func_t      func = P_FUNCTION (pr, 3);
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
bi_Menu_Cursor (progs_t *pr)
{
	func_t      func = P_FUNCTION (pr, 0);

	menu->cursor = func;
}

static void
bi_Menu_KeyEvent (progs_t *pr)
{
	func_t      func = P_FUNCTION (pr, 0);

	menu->keyevent = func;
}

static void
bi_Menu_End (progs_t *pr)
{
	menu = menu->parent;
}

static void
bi_Menu_TopMenu (progs_t *pr)
{
	const char *name = P_STRING (pr, 0);

	if (top_menu)
		free ((char*)top_menu);
	top_menu = strdup (name);
}

static void
bi_Menu_SelectMenu (progs_t *pr)
{
	const char *name = P_STRING (pr, 0);

	menu = 0;
	if (name && *name)
		menu = Hash_Find (menu_hash, name);
	if (menu) {
		key_dest = key_menu;
		game_target = IMT_CONSOLE;
		if (menu->enter_hook) {
			PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
		}
	} else {
		if (name && *name)
			Con_Printf ("no menu \"%s\"\n", name);
		if (con_data.force_commandline) {
			key_dest = key_console;
			game_target = IMT_CONSOLE;
		} else {
			key_dest = key_game;
			game_target = IMT_0;
		}
	}
}

static void
bi_Menu_SetQuit (progs_t *pr)
{
	func_t      func = P_FUNCTION (pr, 0);

	menu_quit = func;
}

static void
bi_Menu_Quit (progs_t *pr)
{
	if (con_data.quit)
		con_data.quit ();
	Sys_Quit ();
}

static void
bi_Menu_GetIndex (progs_t *pr)
{
	if (menu) {
		R_INT (pr) = menu->cur_item;
	} else {
		R_INT (pr) = -1;
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
	if (confirm_quit->int_val && menu_quit) {
		PR_ExecuteProgram (&menu_pr_state, menu_quit);
		if (!G_INT (&menu_pr_state, OFS_RETURN))
			return;
	}
	bi_Menu_Quit (&menu_pr_state);
}

static void *
menu_allocate_progs_mem (progs_t *pr, int size)
{
	return malloc (size);
}

static void
menu_free_progs_mem (progs_t *pr, void *mem)
{
	free (mem);
}

static void *
menu_load_file (progs_t *pr, const char *path)
{
	return QFS_LoadFile (path, 0);
}

void
Menu_Init (void)
{
	menu_pr_state.progs_name = "menu.dat";
	menu_pr_state.allocate_progs_mem = menu_allocate_progs_mem;
	menu_pr_state.free_progs_mem = menu_free_progs_mem;
	menu_pr_state.load_file = menu_load_file;

	PR_Resources_Init (&menu_pr_state);

	menu_hash = Hash_NewTable (61, menu_get_key, menu_free, 0);

	PR_AddBuiltin (&menu_pr_state, "Menu_Begin", bi_Menu_Begin, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_FadeScreen", bi_Menu_FadeScreen, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_Draw", bi_Menu_Draw, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_EnterHook", bi_Menu_EnterHook, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_LeaveHook", bi_Menu_LeaveHook, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_Pic", bi_Menu_Pic, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_SubPic", bi_Menu_SubPic, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_CenterPic", bi_Menu_CenterPic, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_CenterSubPic", bi_Menu_CenterSubPic, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_Item", bi_Menu_Item, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_Cursor", bi_Menu_Cursor, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_KeyEvent", bi_Menu_KeyEvent, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_End", bi_Menu_End, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_TopMenu", bi_Menu_TopMenu, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_SelectMenu", bi_Menu_SelectMenu, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_SetQuit", bi_Menu_SetQuit, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_Quit", bi_Menu_Quit, -1);
	PR_AddBuiltin (&menu_pr_state, "Menu_GetIndex", bi_Menu_GetIndex, -1);

	PR_Obj_Progs_Init (&menu_pr_state);

	Cbuf_Progs_Init (&menu_pr_state);
	Cmd_Progs_Init (&menu_pr_state);
	Cvar_Progs_Init (&menu_pr_state);
	File_Progs_Init (&menu_pr_state);
	InputLine_Progs_Init (&menu_pr_state);
	Key_Progs_Init (&menu_pr_state);
	QFile_Progs_Init (&menu_pr_state, 1);
	QFS_Progs_Init (&menu_pr_state);
	PR_Cmds_Init (&menu_pr_state);
	R_Progs_Init (&menu_pr_state);

	confirm_quit = Cvar_Get ("confirm_quit", "1", CVAR_ARCHIVE, NULL,
							 "confirm quit command");

	Cmd_AddCommand ("togglemenu", togglemenu_f,
					"Toggle the display of the menu");
	Cmd_AddCommand ("quit", quit_f, "Exit the program");
}

void
Menu_Load (void)
{
	int         size;
	QFile      *file;

	menu_pr_state.time = con_data.realtime;

	Hash_FlushTable (menu_hash);
	menu = 0;
	top_menu = 0;

	if ((size = QFS_FOpenFile (menu_pr_state.progs_name, &file)) != -1) {
		PR_LoadProgsFile (&menu_pr_state, file, size, 0, 256 * 1024);
		Qclose (file);

		if (!PR_RelocateBuiltins (&menu_pr_state)
			|| !PR_ResolveGlobals (&menu_pr_state)
			|| !menu_resolve_globals ()) {
			free (menu_pr_state.progs);
			menu_pr_state.progs = 0;
		} else {
			PR_LoadStrings (&menu_pr_state);
			PR_LoadDebug (&menu_pr_state);
			PR_Check_Opcodes (&menu_pr_state);
		}
	}
	if (!menu_pr_state.progs) {
		// Not a fatal error, just means no menus
		Con_SetOrMask (0x80);
		Con_Printf ("Menu_Load: could not load %s\n",
					menu_pr_state.progs_name);
		Con_SetOrMask (0x00);
		return;
	}
	PR_InitRuntime (&menu_pr_state);
	Cbuf_Progs_SetCbuf (&menu_pr_state, con_data.cbuf);
	InputLine_Progs_SetDraw (&menu_pr_state, C_DrawInputLine);
	PR_ExecuteProgram (&menu_pr_state, menu_init);
}

void
Menu_Draw (void)
{
	menu_pic_t *m_pic;
	int         i;
	menu_item_t *item;

	if (!menu)
		return;

	if (menu->fadescreen)
		Draw_FadeScreen ();

	*menu_pr_state.globals.time = *menu_pr_state.time;

	if (menu->draw) {
		PR_ExecuteProgram (&menu_pr_state, menu->draw);
		if (G_INT (&menu_pr_state, OFS_RETURN))
			return;
	}


	for (m_pic = menu->pics; m_pic; m_pic = m_pic->next) {
		qpic_t     *pic = Draw_CachePic (m_pic->name, 1);
		if (!pic)
			continue;
		if (m_pic->width > 0 && m_pic->height > 0)
			Draw_SubPic (m_pic->x, m_pic->y, pic, m_pic->srcx, m_pic->srcy,
						 m_pic->width, m_pic->height);
		else
			Draw_Pic (m_pic->x, m_pic->y, pic);
	}
	for (i = 0; i < menu->num_items; i++) {
		if (menu->items[i]->text) {
			Draw_String (menu->items[i]->x + 8, menu->items[i]->y,
						 menu->items[i]->text);
		}
	}
	if (!menu->items)
		return;
	item = menu->items[menu->cur_item];
	if (menu->cursor) {
		G_INT (&menu_pr_state, OFS_PARM0) = item->x;
		G_INT (&menu_pr_state, OFS_PARM1) = item->y;
		PR_ExecuteProgram (&menu_pr_state, menu->cursor);
	} else {
		Draw_Character (item->x, item->y,
						12 + ((int) (*con_data.realtime * 4) & 1));
	}
}

void
Menu_KeyEvent (knum_t key, short unicode, qboolean down)
{
	menu_item_t *item;

	if (!menu)
		return;
	if (menu->keyevent) {
		G_INT (&menu_pr_state, OFS_PARM0) = key;
		G_INT (&menu_pr_state, OFS_PARM1) = unicode;
		G_INT (&menu_pr_state, OFS_PARM2) = down;
		PR_ExecuteProgram (&menu_pr_state, menu->keyevent);
		if (G_INT (&menu_pr_state, OFS_RETURN))
			return;
	} else if (menu->items && menu->items[menu->cur_item]->func
			   && menu->items[menu->cur_item]->allkeys) {
		item = menu->items[menu->cur_item];
		if (item->text)
			G_INT (&menu_pr_state, OFS_PARM0) =
				PR_SetString (&menu_pr_state, item->text);
		else
			G_INT (&menu_pr_state, OFS_PARM0) = 0;
		G_INT (&menu_pr_state, OFS_PARM1) = key;
		PR_ExecuteProgram (&menu_pr_state, item->func);
		if (G_INT (&menu_pr_state, OFS_RETURN))
			return;
	}
	if (!menu || !menu->items)
		return;
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			menu->cur_item++;
			menu->cur_item %= menu->num_items;
			break;
		case QFK_UP:
		case QFM_WHEEL_UP:
			menu->cur_item += menu->num_items - 1;
			menu->cur_item %= menu->num_items;
			break;
		case QFK_RETURN:
		case QFM_BUTTON1:
			{
				item = menu->items[menu->cur_item];
				if (item->func) {
					if (item->text)
						G_INT (&menu_pr_state, OFS_PARM0) =
							PR_SetString (&menu_pr_state, item->text);
					else
						G_INT (&menu_pr_state, OFS_PARM0) = 0;
					G_INT (&menu_pr_state, OFS_PARM1) = key;
					PR_ExecuteProgram (&menu_pr_state, item->func);
				} else {
					menu = item;
					if (menu->enter_hook) {
						PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
					}
				}
			}
			break;
		default:
			break;
	}
}

void
Menu_Enter ()
{
	if (!top_menu) {
		key_dest = key_console;
		game_target = IMT_CONSOLE;
		return;
	}
	key_dest = key_menu;
	game_target = IMT_CONSOLE;
	menu = Hash_Find (menu_hash, top_menu);
	if (menu && menu->enter_hook) {
		PR_ExecuteProgram (&menu_pr_state, menu->enter_hook);
	}
}

void
Menu_Leave ()
{
	if (menu) {
		if (menu->leave_hook) {
			PR_ExecuteProgram (&menu_pr_state, menu->leave_hook);
		}
		menu = menu->parent;
		if (!menu) {
			if (con_data.force_commandline) {
				key_dest = key_console;
				game_target = IMT_CONSOLE;
			} else {
				key_dest = key_game;
				game_target = IMT_0;
			}
		}
	}
}
