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

	$Id$
*/

#ifndef __QF_plugin_console_h_
#define __QF_plugin_console_h_

#include <stdarg.h>

#include <QF/qtypes.h>
#include <QF/plugin.h>

typedef void (QFPLUGIN *P_C_Print) (const char *fmt, va_list args);
typedef void (QFPLUGIN *P_C_ProcessInput) (void);

typedef struct console_func_s {
	P_C_Print			pC_Print;
	P_C_ProcessInput	pC_ProcessInput;
} console_funcs_t;

typedef struct console_data_s {
	const char			*dl_name;
	int					dl_percent;
	double				realtime;
	int					force_commandline;
} console_data_t;

#endif // __QF_plugin_console_h_
