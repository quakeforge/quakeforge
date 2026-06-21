/*
	gampad.h

	Gamepad mapping

	Copyright (C) 2026 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_input_gamepad_h
#define __QF_input_gamepad_h

#ifndef __QFCC__
#include "QF/listener.h"
#include "QF/mathlib.h"
#endif

typedef struct IE_event_s IE_event_t;

/** \defgroup input_gamepad Gamepad Mapping
	\ingroup input
*/
///@{

typedef struct in_devid_s {
	uint16_t    bustype;
	uint16_t    vendor;
	uint16_t    product;
	uint16_t    version;
} in_devid_t;

typedef struct in_gamepad_s in_gamepad_t;

void IN_Gamepad_Init (void);
in_gamepad_t *IN_Gamepad_Add (in_devid_t devid, int deviceid);
void IN_Gamepad_Remove (in_gamepad_t *gamepad);
void IN_Gamepad_Event (in_gamepad_t *gamepad, IE_event_t *ie_event);

///@}

#endif//__QF_input_binding_h
