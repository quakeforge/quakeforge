/*
	imt.h

	Input Mapping Table management

	Copyright (C) 2001 Zephaniah E. Hull <warp@babylon.d2dc.net>
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_input_imt_h
#define __QF_input_imt_h

#ifndef __QFCC__

#include "QF/darray.h"

#include "QF/input/binding.h"

typedef enum {
	imt_button,
	imt_axis,
} imt_type;

/** Describe a region of imt bindings (axis or button)

	Each device may have a block of axis bindings and a block of button
	bindings (some devices will have only the one block). The device name is
	used instead of a pointer to the device descriptor so configs can be
	preserved even if the device is not present.

	Bindings are allocated to a device in contiguous blocks.
*/
typedef struct imt_block_s {
	const char *device;             ///< name of the owning device
	int         base;               ///< index of first binding
	int         count;              ///< number of bindings
} imt_block_t;

/**	Input Mapping Table
*/
typedef struct imt_s {
	struct imt_s *next;				///< list of tables attached to key_dest
	struct imt_s *chain;			///< fallback table if input not bound
	const char *name;				///< for user interaction
	int         written;
	struct DARRAY_TYPE (in_axisbinding_t *) axis_bindings;
	struct DARRAY_TYPE (in_buttonbinding_t *) button_bindings;
} imt_t;

typedef struct in_context_s {
	imt_t      *imts;
	imt_t     **imt_tail;
	imt_t      *active_imt;
	imt_t      *default_imt;
} in_context_t;

int IMT_GetAxisBlock (const char *device, int num_axes);
int IMT_GetButtonBlock (const char *device, int num_buttons);
int IMT_CreateContext (void);
imt_t *IMT_FindIMT (const char *name);
int IMT_CreateIMT (int context, const char *imt_name,
				   const char *chain_imt_name);
void IMT_ProcessAxis (int axis, int value);
void IMT_ProcessButton (int button, int state);

#endif

#endif//__QF_input_imt_h
