/*
	menu.h

	menu subsystem

	Copyright (C) 2001       Joseph Carter <knghtbrd@debian.org>

	Author: Joseph Carter <knghtbrd@debian.org>
	Date: 16 Apr 2001

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

#ifndef __MENU_H
#define __MENU_H

typedef enum {
	M_Static,
	M_Menu,
	M_Toggle,
	M_Slide,
	M_SelectStr,
	M_SelectInt,
	M_Command,
	M_Engine,
	M_Progs
} mitem_type;


typedef enum {
	MHook_None,
	MHook_Engine,
	MHook_Progs
} mhook_type;

typedef struct menuitem_s {
	char			   *title;
//	char			   *desc;		// FIXME: Implement or remove?

	mitem_type			type;
	union mitem {
		/* NOTHING */							// M_Static
		struct menu_s		   *menu;			// M_Menu
		struct cvar_s		   *var;			// M_Toggle
		struct {
			float			min, max, step;
			struct cvar_s  *var;
		}						slide;			// M_Slide
		struct {
			char		  **choices;
			char		  **strings;
			struct cvar_s   *var;
		}						selectstr;		// M_SelectStr
		struct {
			char		  **choices;
			int			   *values;
			struct cvar_s  *var;
		}						selectint;		// M_SelectInt
		char				   *cmd;			// M_Command
		void				  (*engine)(void);	// M_Engine
		char				   *progs;			// M_Progs
	};

	// engine/progs function hook for dynamic things, called at display
	mhook_type			hook;
	union mhook {
		void				  (*engine_hook)(struct menuitem_s *item);
		char				   *progs_hook;
	};

	struct menuitem_s   *prev;
	struct menuitem_s   *next;
} menuitem_t;

typedef struct menu_s {
	char		   *title;

	// for any of the screen is "reserved" for hooks
	int				lofs, rofs;			// 0-319 scale
	int				tofs, bofs;			// 0-199 scale

	// engine/progs function hook for menu title
	mhook_type		hook;
	union mthook {
		void		  (*engine_hool)(char *title);
		char		   *progs_hook;
	};
	
	menuitem_t	   *items;
} menu_t;

#endif // __MENU_H

