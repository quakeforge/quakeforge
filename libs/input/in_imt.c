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
#include "QF/hash.h"
#include "QF/sys.h"

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
IMT_CreateContext (void)
{
	in_context_t *ctx = DARRAY_OPEN_AT (&in_contexts, in_contexts.size, 1);
	memset (ctx, 0, sizeof (*ctx));
	ctx->imt_tail = &ctx->imts;
	return ctx - in_contexts.a;
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
IMT_ProcessAxis (int axis, int value)
{
}

void
IMT_ProcessButton (int button, int state)
{
}
