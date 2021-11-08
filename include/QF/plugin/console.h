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

#ifndef __QF_plugin_console_h
#define __QF_plugin_console_h

#include <stdarg.h>

#include <QF/console.h>
#include <QF/plugin.h>

typedef struct console_funcs_s {
	void (*init) (void);
	void (*print) (const char *fmt, va_list args) __attribute__((format(PRINTF, 1, 0)));
	void (*process_input) (void);
	void (*draw_console) (void);
	void (*check_resize) (void);
	void (*new_map) (void);
	void (*set_state) (con_state_t state);
} console_funcs_t;

typedef struct console_data_s {
	struct dstring_s *dl_name;
	int        *dl_percent;
	double     *realtime;
	double     *frametime;
	int         force_commandline;
	int         ormask;
	void      (*quit) (void);
	struct cbuf_s *cbuf;
	struct view_s *view;
	struct view_s *status_view;
	float       lines;
	int       (*exec_line)(void *data, const char *line);
	void       *exec_data;
} console_data_t;

#endif // __QF_plugin_console_h
