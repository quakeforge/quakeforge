/*
	QF/plugin/console.h

	General plugin data and structures

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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

#ifndef __QF_plugin_console_h_
#define __QF_plugin_console_h_

#include <stdarg.h>

#include <QF/keys.h>
#include <QF/plugin.h>
#include <QF/qtypes.h>

typedef void (*P_C_Print) (const char *fmt, va_list args) __attribute__((format(PRINTF, 1, 0)));
typedef void (*P_C_ProcessInput) (void);
typedef void (*P_C_KeyEvent) (knum_t key, short unicode, qboolean down);
typedef void (*P_C_DrawConsole) (void);
typedef void (*P_C_CheckResize) (void);
typedef void (*P_C_NewMap) (void);

typedef struct console_funcs_s {
	P_C_Print			pC_Print;
	P_C_ProcessInput	pC_ProcessInput;
	P_C_KeyEvent		pC_KeyEvent;
	P_C_DrawConsole		pC_DrawConsole;
	P_C_CheckResize		pC_CheckResize;
	P_C_NewMap			pC_NewMap;
} console_funcs_t;

typedef struct console_data_s {
	struct dstring_s	*dl_name;
	int					*dl_percent;
	double				*realtime;
	double				*frametime;
	int					force_commandline;
	int					ormask;
	void				(*quit)(void);
	struct cbuf_s		*cbuf;
	struct view_s		*view;
	struct view_s		*status_view;
	float				lines;
	int					(*exec_line)(void *data, const char *line);
	void				*exec_data;
} console_data_t;

#endif // __QF_plugin_console_h_
