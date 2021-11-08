/*
	in_imt.c

	Input Mapping Table management

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/10/30

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/cmem.h"
#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/input/imt.h"

#include "QF/input/binding.h"
#include "QF/input/imt.h"

typedef struct DARRAY_TYPE (in_context_t) in_contextset_t;
typedef struct DARRAY_TYPE (imt_block_t) imt_blockset_t;
/** Binding blocks are allocated across all imts
*/
static imt_blockset_t axis_blocks = DARRAY_STATIC_INIT (8);
static imt_blockset_t button_blocks = DARRAY_STATIC_INIT (8);

static in_contextset_t in_contexts = DARRAY_STATIC_INIT (8);
static size_t   imt_current_context;

static memsuper_t *binding_mem;

static in_axisbinding_t *
alloc_axis_binding (void)
{
	return cmemalloc (binding_mem, sizeof (in_axisbinding_t));
}

static void
free_axis_binding (in_axisbinding_t *binding)
{
	if (!binding) {
		return;
	}
	cmemfree (binding_mem, binding);
}

static in_buttonbinding_t *
alloc_button_binding (void)
{
	return cmemalloc (binding_mem, sizeof (in_buttonbinding_t));
}

static void
free_button_binding (in_buttonbinding_t *binding)
{
	if (!binding) {
		return;
	}
	switch (binding->type) {
		case inb_button:
			break;
		case inb_command:
			free (binding->command);
			break;
	}
	cmemfree (binding_mem, binding);
}

static imt_block_t * __attribute__((pure))
imt_find_block (imt_blockset_t *blockset, const char *device)
{
    for (size_t i = 0; i < blockset->size; i++) {
        if (strcmp (blockset->a[i].device, device) == 0) {
            return &blockset->a[i];
        }
    }
    return 0;
}

static imt_block_t *
imt_get_block (imt_blockset_t *blockset)
{
    return DARRAY_OPEN_AT (blockset, blockset->size, 1);
}

static int
imt_get_next_base (imt_blockset_t *blockset)
{
    if (!blockset->size) {
        return 0;
    }
    imt_block_t *b = &blockset->a[blockset->size - 1];
    return b->base + b->count;
}

static int
imt_get_axis_block (int count)
{
    int         base = imt_get_next_base (&axis_blocks);

    for (size_t i = 0; i < in_contexts.size; i++) {
        for (imt_t *imt = in_contexts.a[i].imts; imt; imt = imt->next) {
            in_axisbinding_t **binding;
            binding = DARRAY_OPEN_AT (&imt->axis_bindings, base, count);
            memset (binding, 0, count * sizeof (binding));
        }
    }
    return base;
}

static int
imt_get_button_block (int count)
{
    int         base = imt_get_next_base (&button_blocks);

    for (size_t i = 0; i < in_contexts.size; i++) {
        for (imt_t *imt = in_contexts.a[i].imts; imt; imt = imt->next) {
            in_buttonbinding_t **binding;
            binding = DARRAY_OPEN_AT (&imt->button_bindings, base, count);
            memset (binding, 0, count * sizeof (binding));
        }
    }
    return base;
}

int
IMT_GetAxisBlock (const char *device, int num_axes)
{
    imt_block_t *block;
    if (!(block = imt_find_block (&axis_blocks, device))) {
        block = imt_get_block (&axis_blocks);
        block->device = device;
        block->base = imt_get_axis_block (num_axes);
        block->count = num_axes;
    }
    return block - axis_blocks.a;
}

int
IMT_GetButtonBlock (const char *device, int num_buttons)
{
    imt_block_t *block;
    if (!(block = imt_find_block (&button_blocks, device))) {
        block = imt_get_block (&button_blocks);
        block->device = device;
        block->base = imt_get_button_block (num_buttons);
        block->count = num_buttons;
    }
    return block - button_blocks.a;
}

int
IMT_CreateContext (const char *name)
{
	in_context_t *ctx = DARRAY_OPEN_AT (&in_contexts, in_contexts.size, 1);
	memset (ctx, 0, sizeof (*ctx));
	ctx->imt_tail = &ctx->imts;
	ctx->name = name;
	return ctx - in_contexts.a;
}

static in_context_t *
imt_find_context (const char *name)
{
	for (size_t i = 0; i < in_contexts.size; i++) {
		if (strcmp (name, in_contexts.a[i].name) == 0) {
			return &in_contexts.a[i];
		}
	}
	return 0;
}

int
IMT_GetContext (void)
{
	return imt_current_context;
}

void
IMT_SetContext (int ctx)
{
	if ((size_t) ctx >= in_contexts.size) {
		Sys_Error ("IMT_SetContext: invalid context %d", ctx);
	}
	imt_current_context = ctx;
}

void
IMT_SetContextCbuf (int ctx, cbuf_t *cbuf)
{
	if ((size_t) ctx >= in_contexts.size) {
		Sys_Error ("IMT_SetContextCbuf: invalid context %d", ctx);
	}
	in_contexts.a[imt_current_context].cbuf = cbuf;
}

static imt_t * __attribute__ ((pure))
imt_find_imt (in_context_t *ctx, const char *name)
{
	for (imt_t *imt = ctx->imts; imt; imt = imt->next) {
		if (strcasecmp (imt->name, name) == 0) {
			return imt;
		}
	}
	return 0;
}

imt_t * __attribute__ ((pure))
IMT_FindIMT (const char *name)
{
    for (size_t i = 0; i < in_contexts.size; i++) {
		in_context_t *ctx = &in_contexts.a[i];
		imt_t      *imt = imt_find_imt (ctx, name);
		if (imt) {
			return imt;
		}
	}
	return 0;
}

int
IMT_CreateIMT (int context, const char *imt_name, const char *chain_imt_name)
{
	in_context_t *ctx = &in_contexts.a[context];
	imt_t      *imt;
	imt_t      *chain_imt = 0;

	if ((size_t) context >= in_contexts.size) {
		Sys_Printf ("invalid imt context %d\n", context);
		return 0;
	}

	if (IMT_FindIMT (imt_name)) {
		Sys_Printf ("imt %s already exists\n", imt_name);
		return 0;
	}
	if (chain_imt_name) {
		chain_imt = IMT_FindIMT (chain_imt_name);
		if (!chain_imt) {
			Sys_Printf ("chain imt %s does not exist\n", chain_imt_name);
			return 0;
		}
		chain_imt = imt_find_imt (ctx, chain_imt_name);
		if (!chain_imt) {
			Sys_Printf ("chain imt %s not in target context\n",
						chain_imt_name);
			return 0;
		}
	}
	imt = malloc (sizeof (imt_t));
	*ctx->imt_tail = imt;
	ctx->imt_tail = &imt->next;

	imt->next = 0;
	imt->chain = chain_imt;
	imt->name = strdup (imt_name);
	imt->written = 0;
	DARRAY_INIT (&imt->axis_bindings, 8);
	DARRAY_INIT (&imt->button_bindings, 8);
	int         num_axes = imt_get_next_base (&axis_blocks);
	int         num_buttons = imt_get_next_base (&button_blocks);
	DARRAY_RESIZE (&imt->axis_bindings, num_axes);
	DARRAY_RESIZE (&imt->button_bindings, num_buttons);
	if (num_axes) {
		memset (imt->axis_bindings.a, 0,
				num_axes * sizeof (in_axisbinding_t *));
	}
	if (num_buttons) {
		memset (imt->axis_bindings.a, 0,
				num_buttons * sizeof (in_buttonbinding_t *));
	}
	return 1;
}

void
IMT_BindAxis (imt_t *imt, int axis, const char *binding)
{
	if ((size_t) axis >= imt->axis_bindings.size)
		return;

	in_axisbinding_t **bind = &imt->axis_bindings.a[axis];
	free_axis_binding ((*bind));
	(*bind) = 0;
	if (binding) {
		in_axisbinding_t *a = alloc_axis_binding ();
		(*bind) = a;
		*a = (in_axisbinding_t) {};
	}
}

void
IMT_BindButton (imt_t *imt, int button, const char *binding)
{
	if ((size_t) button >= imt->button_bindings.size)
		return;

	in_buttonbinding_t **bind = &imt->button_bindings.a[button];
	free_button_binding ((*bind));
	(*bind) = 0;
	if (binding) {
		in_buttonbinding_t *b = alloc_button_binding ();
		(*bind) = b;
		in_button_t *button;
		if (binding[0] == '+' && (button = IN_FindButton (binding + 1))) {
			b->type = inb_button;
			b->button = button;
		} else {
			b->type = inb_command;
			b->command = strdup(binding);
		}
	}
}

qboolean
IMT_ProcessAxis (int axis, int value)
{
	imt_t      *imt = in_contexts.a[imt_current_context].active_imt;

	while (imt) {
		in_axisbinding_t *a = imt->axis_bindings.a[axis];
		if (a) {
			return true;
		}
		imt = imt->chain;
	}
	return false;
}

static void
process_binding (int button, int state, const char *cmd)
{
	cbuf_t     *cbuf = in_contexts.a[imt_current_context].cbuf;

	if (!cbuf) {
		return;
	}

	if (cmd[0] == '+') {
		if (state) {
			Cbuf_AddText (cbuf, va (0, "%s %d\n", cmd, button));
		} else {
			Cbuf_AddText (cbuf, va (0, "-%s %d\n", cmd + 1, button));
		}
	} else {
		if (state) {
			Cbuf_AddText (cbuf, va (0, "%s\n", cmd));
		}
	}
}

qboolean
IMT_ProcessButton (int button, int state)
{
	imt_t      *imt = in_contexts.a[imt_current_context].active_imt;

	Sys_Printf ("IMT_ProcessButton: %d %d\n", button, state);
	while (imt) {
		in_buttonbinding_t *b = imt->button_bindings.a[button];
		if (b) {
			switch (b->type) {
				case inb_button:
					IN_ButtonAction (b->button, button, state);
					break;
				case inb_command:
					//FIXME avoid repeat
					process_binding (button, state, b->command);
					break;
			}
			return true;
		}
		imt = imt->chain;
	}
	return false;
}

static void
imt_f (void)
{
	int         c;
	imt_t      *imt;
	const char *imt_name = 0;
	const char *context_name;

	c = Cmd_Argc ();
	switch (c) {
		case 3:
			imt_name = Cmd_Argv (2);
		case 2:
			context_name = Cmd_Argv (1);
			break;
		default:
			return;
	}
	in_context_t *ctx = imt_find_context (context_name);
	if (!ctx) {
		Sys_Printf ("imt error: invalid context: %s\n", context_name);
		return;
	}

	if (!imt_name) {
		Sys_Printf ("Current imt is %s\n", ctx->active_imt->name);
		Sys_Printf ("imt <imt> : set to a specific input mapping table\n");
		return;
	}

	imt = imt_find_imt (ctx, imt_name);
	if (!imt) {
		Sys_Printf ("\"%s\" is not an imt in %s\n", imt_name, ctx->name);
		return;
	}

	ctx->active_imt = imt;
}

static void
imt_list_f (void)
{
    for (size_t i = 0; i < in_contexts.size; i++) {
		in_context_t *ctx = &in_contexts.a[i];
		Sys_Printf ("context: %s\n", ctx->name);
		for (imt_t *imt = ctx->imts; imt; imt = imt->next) {
			if (imt->chain) {
				Sys_Printf ("    %s -> %s\n", imt->name, imt->chain->name);
			} else {
				Sys_Printf ("    %s\n", imt->name);
			}
		}
	}
}

static void
imt_create_f (void)
{
	const char *context_name;
	const char *imt_name;
	const char *chain_imt_name = 0;

	if (Cmd_Argc () < 3 || Cmd_Argc () > 4) {
		Sys_Printf ("see help imt_create\n");
		return;
	}
	context_name = Cmd_Argv (1);
	imt_name = Cmd_Argv (2);
	if (Cmd_Argc () == 4) {
		chain_imt_name = Cmd_Argv (3);
	}
	in_context_t *ctx = imt_find_context (context_name);
	if (!ctx) {
		Sys_Printf ("imt error: invalid context: %s\n", context_name);
		return;
	}
	IMT_CreateIMT (ctx - in_contexts.a, imt_name, chain_imt_name);
}

static void
imt_drop_all_f (void)
{
	for (size_t i = 0; i < in_contexts.size; i++) {
		in_context_t *ctx = &in_contexts.a[i];
		while (ctx->imts) {
			imt_t      *imt = ctx->imts;
			ctx->imts = imt->next;
			for (size_t i = 0; i < imt->axis_bindings.size; i++) {
				free_axis_binding (imt->axis_bindings.a[i]);
			}
			for (size_t i = 0; i < imt->button_bindings.size; i++) {
				free_button_binding (imt->button_bindings.a[i]);
			}
			free ((char *) imt->name);
			free (imt);
		}
		ctx->active_imt = 0;
	}
}

typedef struct {
	const char *name;
	xcommand_t  func;
	const char *desc;
} imtcmd_t;

static imtcmd_t imt_commands[] = {
	{	"imt", imt_f,
		"Set the active imt of the specified context"
	},
	{	"imt_list", imt_list_f,
		"List the available input mapping tables"
	},
	{	"imt_create", imt_create_f,
		"create a new imt table:\n"
		"    imt_create <keydest> <imt_name> [chain_name]\n"
		"\n"
		"The new table will be attached to the specified keydest\n"
		"imt_name must not already exist.\n"
		"If given, chain_name must already exist and be on keydest.\n"
	},
	{	"imt_drop_all", imt_drop_all_f,
		"delete all imt tables\n"
	},
};

void
IMT_Init (void)
{
	binding_mem = new_memsuper ();
	for (imtcmd_t *cmd = imt_commands; cmd->name; cmd++) {
		Cmd_AddCommand (cmd->name, cmd->func, cmd->desc);
	}
}
