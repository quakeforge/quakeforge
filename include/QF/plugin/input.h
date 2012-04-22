/*
	QF/plugin/input.h

	Input plugin data and structures

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

#ifndef __QF_plugin_input_h_
#define __QF_plugin_input_h_

#include <QF/plugin.h>
#include <QF/qtypes.h>

/*
	All input plugins must export these functions
*/
typedef void (*P_IN_Commands) (void);
typedef void (*P_IN_SendKeyEvents) (void);
typedef void (*P_IN_Move) (void);
typedef void (*P_IN_ModeChanged) (void);

typedef struct input_funcs_s {
	P_IN_Commands 		pIN_Commands;
	P_IN_SendKeyEvents	pIN_SendKeyEvents;
	P_IN_Move			pIN_Move;
	P_IN_ModeChanged 	pIN_ModeChanged;
} input_funcs_t;

typedef struct input_data_s {
	int unused; /* C requires that a struct or union has at least one member */
} input_data_t;

#endif // __QF_plugin_input_h_
