/*
	QF/plugin/general.h

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

#ifndef __QF_plugin_general_h
#define __QF_plugin_general_h

#include <QF/plugin.h>
#include <QF/qtypes.h>

typedef struct general_funcs_s {
	void      (*init) (void);
	void      (*shutdown) (void);
} general_funcs_t;

#define PIF_GLOBAL 1

typedef struct general_data_s {
	int         flag;
} general_data_t;

#endif // __QF_plugin_general_h
