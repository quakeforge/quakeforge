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

/** \defgroup input_imt Input Mapping Tables
	\ingroup input
*/
///@{

#ifndef __QFCC__

#include "QF/darray.h"
#include "QF/qtypes.h"

#include "QF/input/binding.h"

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

typedef enum {
	imti_button,
	imti_cvar,
} imt_input_type;

typedef struct imt_input_s {
	imt_input_type type;
	union {
		struct in_button_s *button;
		struct cvar_s *cvar;
	};
} imt_input_t;

typedef struct imt_switcher_s {
	struct imt_switcher_s *next;
	const char *name;
	int         num_inputs;
	imt_input_t *inputs;	// one per input
	imt_t     **imts;		// 2**(num_inputs)
	struct in_context_s *context;
} imt_switcher_t;

typedef struct in_context_s {
	const char *name;
	imt_t      *imts;
	imt_t     **imt_tail;
	imt_t      *active_imt;
	imt_t      *default_imt;
	imt_switcher_t *switchers;
	imt_switcher_t **switcher_tail;
	struct cbuf_s *cbuf;
} in_context_t;

int IMT_GetAxisBlock (int num_axes);
int IMT_GetButtonBlock (int num_buttons);
int IMT_CreateContext (const char *name);
int IMT_GetContext (void) __attribute__ ((pure));
void IMT_SetContext (int ctx);
void IMT_SetContextCbuf (int ctx, struct cbuf_s *cbuf);

/**	Find an Input Mapping Table by name.

	Searches through all contexts for the named imt. The search is case
	insensitive.

	\param name		The name of the imt to find. Case insensitive.
	\return			The named imt, or null if not found.
*/
imt_t *IMT_FindIMT (const char *name) __attribute__ ((pure));
imt_switcher_t *IMT_FindSwitcher (const char *name) __attribute__ ((pure));

/**	Create a new imt and attach it to the specified context.

	The name of the new imt must be unique (case insensitive) across all
	contexts. This is to simplify the in_bind command.

	If \a chain_imt_name is not null, then it species the fallback imt for when
	the input is not bound in the new imt. It must be an already existing imt
	in the specified context. This is to prevent loops and other weird
	behavior.

	\param context	The context to which the new imt will be attached.
	\param imt_name	The name for the new imt. Must be unique (case
					insensitive).
	\param chain_imt_name	The name of the fallback imt if not null. Must
					already exist on the specified context.
*/
int IMT_CreateIMT (int context, const char *imt_name,
				   const char *chain_imt_name);
int IMT_CreateSwitcher (const char *switcher_name,
						int context, imt_t *default_imt,
						int num_inputs, const char **input_names);
void IMT_BindAxis (imt_t *imt, int axis_num, in_axis_t *axis,
				   const in_recipe_t *recipe);
void IMT_BindButton (imt_t *imt, int button, const char *binding);
qboolean IMT_ProcessAxis (int axis, int value);
qboolean IMT_ProcessButton (int button, int state);
void IMT_Init (void);
struct plitem_s;
void IMT_SaveConfig (struct plitem_s *config);
void IMT_SaveAxisConfig (struct plitem_s *axes, int axis_ind, int dev_axis);
void IMT_SaveButtonConfig (struct plitem_s *buttons, int button_ind,
						   int dev_button);
void IMT_LoadConfig (struct plitem_s *config);

#endif

///@}

#endif//__QF_input_imt_h
