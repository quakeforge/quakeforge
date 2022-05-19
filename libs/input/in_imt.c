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
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/plist.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/input/imt.h"

#include "QF/input/binding.h"
#include "QF/input/imt.h"

#include "qfalloca.h"

/** Describe a region of imt bindings (axis or button)

	Each device may have a block of axis bindings and a block of button
	bindings (some devices will have only the one block).

	Bindings are allocated to a device in contiguous blocks.
*/
typedef struct imt_block_s {
	int         base;               ///< index of first binding
	int         count;              ///< number of bindings
} imt_block_t;

typedef struct DARRAY_TYPE (in_context_t) in_contextset_t;
typedef struct DARRAY_TYPE (imt_block_t) imt_blockset_t;
/** Binding blocks are allocated across all imts
*/
static imt_blockset_t axis_blocks = DARRAY_STATIC_INIT (8);
static imt_blockset_t button_blocks = DARRAY_STATIC_INIT (8);

static in_contextset_t in_contexts = DARRAY_STATIC_INIT (8);
static size_t   imt_current_context;

static memsuper_t *binding_mem;
static hashtab_t *recipe_tab;

static in_recipe_t *
alloc_recipe (void)
{
	return cmemalloc (binding_mem, sizeof (in_recipe_t));
}

static void
free_recipe (in_recipe_t *recipe)
{
	if (!recipe) {
		return;
	}
	cmemfree (binding_mem, recipe);
}

static void
recipe_free (void *recipe, void *data)
{
	free_recipe (recipe);
}

static uintptr_t
recipe_get_hash (const void *recipe, void *data)
{
	return Hash_Buffer (recipe, sizeof (in_recipe_t));
}

static int
recipe_compare (const void *a, const void *b, void *data)
{
	return !memcmp (a, b, sizeof (in_recipe_t));
}

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

static void
imt_reset_blocks (void)
{
	DARRAY_RESIZE (&axis_blocks, 0);
	DARRAY_RESIZE (&button_blocks, 0);
    for (size_t i = 0; i < in_contexts.size; i++) {
        for (imt_t *imt = in_contexts.a[i].imts; imt; imt = imt->next) {
			DARRAY_RESIZE (&imt->axis_bindings, 0);
			DARRAY_RESIZE (&imt->button_bindings, 0);
        }
    }
}

int
IMT_GetAxisBlock (int num_axes)
{
	int         base = imt_get_axis_block (num_axes);
	imt_block_t *block = imt_get_block (&axis_blocks);
	block->base = base;
	block->count = num_axes;
	return base;
}

int
IMT_GetButtonBlock (int num_buttons)
{
	int         base = imt_get_button_block (num_buttons);
	imt_block_t *block = imt_get_block (&button_blocks);
	block->base = base;
	block->count = num_buttons;
	return base;
}

int
IMT_CreateContext (const char *name)
{
	in_context_t *ctx = DARRAY_OPEN_AT (&in_contexts, in_contexts.size, 1);
	memset (ctx, 0, sizeof (*ctx));
	ctx->imt_tail = &ctx->imts;
	ctx->switcher_tail = &ctx->switchers;
	ctx->name = name;
	return ctx - in_contexts.a;
}

static in_context_t * __attribute__ ((pure))
imt_find_context (const char *name)
{
	if (name) {
		for (size_t i = 0; i < in_contexts.size; i++) {
			if (strcmp (name, in_contexts.a[i].name) == 0) {
				return &in_contexts.a[i];
			}
		}
	}
	return 0;
}

static void
imt_switcher_update (imt_switcher_t *switcher)
{
	int         state = 0;

	for (int i = 0; i < switcher->num_inputs; i++) {
		imt_input_t *input = &switcher->inputs[i];
		int          val = 0;
		switch (input->type) {
			case imti_button:
				val = !!(input->button->state & inb_down);
				break;
			case imti_cvar:
				//FIXME check cvar type
				val = !!*(int *) input->cvar->value.value;
				break;
		}
		state |= val << i;
	}
	switcher->context->active_imt = switcher->imts[state];
}

static void
imt_switcher_button_update (void *_switcher, const in_button_t *button)
{
	imt_switcher_update (_switcher);
}

static void
imt_switcher_cvar_update (void *_switcher, const cvar_t *cvar)
{
	imt_switcher_update (_switcher);
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
	for (imt_switcher_t *switcher = in_contexts.a[ctx].switchers; switcher;
		 switcher = switcher->next) {
		imt_switcher_update (switcher);
	}
	imt_t      *imt = in_contexts.a[imt_current_context].active_imt;
	Sys_MaskPrintf (SYS_input, "Switched to %s context, imt %s\n",
					in_contexts.a[imt_current_context].name,
					imt ? imt->name : 0);
}

void
IMT_SetContextCbuf (int ctx, cbuf_t *cbuf)
{
	if ((size_t) ctx >= in_contexts.size) {
		Sys_Error ("IMT_SetContextCbuf: invalid context %d", ctx);
	}
	in_contexts.a[ctx].cbuf = cbuf;
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

imt_t *
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

static imt_switcher_t * __attribute__((pure))
imt_find_switcher (in_context_t *ctx, const char *name)
{
	for (imt_switcher_t *switcher = ctx->switchers; switcher;
		 switcher = switcher->next) {
		if (strcasecmp (switcher->name, name) == 0) {
			return switcher;
		}
	}
	return 0;
}

imt_switcher_t *
IMT_FindSwitcher (const char *name)
{
    for (size_t i = 0; i < in_contexts.size; i++) {
		in_context_t *ctx = &in_contexts.a[i];
		imt_switcher_t *switcher = imt_find_switcher (ctx, name);
		if (switcher) {
			return switcher;
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
	if (!ctx->imts) {
		ctx->default_imt = imt;
		ctx->active_imt = imt;
	}
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
		memset (imt->button_bindings.a, 0,
				num_buttons * sizeof (in_buttonbinding_t *));
	}
	return 1;
}

int
IMT_CreateSwitcher (const char *switcher_name, int context, imt_t *default_imt,
					int num_inputs, const char **input_names)
{
	if (IMT_FindSwitcher (switcher_name)) {
		Sys_Printf ("imt switcher %s already exists\n", switcher_name);
		return 0;
	}

	if ((size_t) context >= in_contexts.size) {
		Sys_Printf ("invalid imt context %d\n", context);
		return 0;
	}
	in_context_t *ctx = &in_contexts.a[context];

	if (!imt_find_imt (ctx, default_imt->name)) {
		Sys_Printf ("default imt %s not in context %s\n",
					default_imt->name, ctx->name);
		return 0;
	}

	int         input_error = 0;
	if (num_inputs > 16) {
		Sys_Printf ("too many inputs %d\n", num_inputs);
		return 0;
	}
	for (int i = 0; i < num_inputs; i++) {
		const char *input_name = input_names[i];
		if (!input_name) {
			input_error = 1;
			continue;
		}
		if (input_name[0] == '+') {
			if (!IN_FindButton (input_name + 1)) {
				Sys_Printf ("invalid button %s\n", input_name);
				input_error = 1;
			}
		} else {
			if (!Cvar_FindVar (input_name)) {
				Sys_Printf ("invalid cvar %s\n", input_name);
				input_error = 1;
			}
		}
	}
	if (input_error) {
		return 0;
	}
	int         num_states = 1 << num_inputs;
	size_t      size = sizeof (imt_switcher_t)
						+ num_inputs * sizeof (imt_input_t)
						+ num_states * sizeof (imt_t *);
	imt_switcher_t *switcher = malloc (size);
	*ctx->switcher_tail = switcher;
	ctx->switcher_tail = &switcher->next;

	switcher->next = 0;
	switcher->name = strdup (switcher_name);
	switcher->num_inputs = num_inputs;
	switcher->inputs = (imt_input_t *) (switcher + 1);
	switcher->imts = (imt_t **) (switcher->inputs + num_inputs);
	switcher->context = ctx;
	for (int i = 0; i < num_inputs; i++) {
		imt_input_t *input = &switcher->inputs[i];
		const char *input_name = input_names[i];
		if (input_name[0] == '+') {
			input->type = imti_button;
			input->button = IN_FindButton (input_name + 1);
			IN_ButtonAddListener (input->button, imt_switcher_button_update,
								  switcher);
		} else {
			input->type = imti_cvar;
			//FIXME check cvar type
			input->cvar = Cvar_FindVar (input_name);
			Cvar_AddListener (input->cvar, imt_switcher_cvar_update, switcher);
		}
	}
	for (int i = 0; i < num_states; i++) {
		switcher->imts[i] = default_imt;
	}
	return 1;
}

void
IMT_BindAxis (imt_t *imt, int axis_num, in_axis_t *axis,
			  const in_recipe_t *recipe)
{
	if ((size_t) axis_num >= imt->axis_bindings.size)
		return;

	in_axisbinding_t **bind = &imt->axis_bindings.a[axis_num];
	free_axis_binding ((*bind));
	(*bind) = 0;
	if (axis && recipe) {
		in_axisbinding_t *a = alloc_axis_binding ();
		(*bind) = a;
		if (!(a->recipe = Hash_FindElement (recipe_tab, recipe))) {
			*(a->recipe = alloc_recipe ()) = *recipe;
			Hash_AddElement (recipe_tab, a->recipe);
		}
		a->axis = axis;
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
			in_recipe_t *recipe = a->recipe;
			int         relative = recipe->min == recipe->max;
			int         deadzone = recipe->deadzone;
			int         minval = recipe->min + recipe->minzone;
			int         maxval = recipe->max - recipe->maxzone;
			float       output;
			if (relative) {
				int         input = value;
				if (deadzone > 0) {
					if (input > deadzone) {
						input -= deadzone;
					} else if (input < -deadzone) {
						input += deadzone;
					} else {
						input = 0;
					}
				}
				output = input * recipe->scale;
				if (recipe->curve != 1) {
					output = powf (output, recipe->curve);
				}
				a->axis->rel_input += output;
			} else {
				int         input = bound (minval, value, maxval);
				int         range = maxval - minval;
				int         zero = minval;
				if (recipe->deadzone >= 0) {
					// balanced axis: -1..1
					int         center = (recipe->min + recipe->max + 1) / 2;
					minval += deadzone;
					maxval -= deadzone;
					input -= center;
					if (input < -deadzone) {
						input += deadzone;
					} else if (input > deadzone) {
						input -= deadzone;
					} else {
						input = 0;
					}
					if (center - minval > maxval - center) {
						range = center - minval;
					} else {
						range = maxval - center;
					}
					zero = 0;
				}
				output = (float)(input - zero) / (range);
				if (recipe->curve != 1) {
					output = powf (output, recipe->curve);
				}
				output *= recipe->scale;
				a->axis->abs_input = output;
			}
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

	while (imt) {
		in_buttonbinding_t *b = imt->button_bindings.a[button];
		if (b) {
			// ensure IN_ButtonAction never sees button id 0
			button += 1;
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
imt_switcher_create_f (void)
{
	int         argc = Cmd_Argc ();

	if (argc < 5) {
		Sys_Printf ("see help imt_switcher_create\n");
		return;
	}

	const char *switcher_name = Cmd_Argv (1);
	const char *context_name = Cmd_Argv (2);
	const char *default_imt_name = Cmd_Argv (3);

	in_context_t *ctx = imt_find_context (context_name);
	if (!ctx) {
		Sys_Printf ("imt error: invalid context: %s\n", context_name);
		return;
	}

	imt_t      *default_imt = imt_find_imt (ctx, default_imt_name);
	if (!default_imt) {
		Sys_Printf ("default imt %s not in context %s\n",
					default_imt_name, context_name);
		return;
	}

	int         num_inputs = argc - 4;
	const char **input_names = alloca (num_inputs * sizeof (const char*));
	for (int i = 0; i < num_inputs; i++) {
		input_names[i] = Cmd_Argv (i + 4);
	}

	IMT_CreateSwitcher (switcher_name, ctx - in_contexts.a, default_imt,
						num_inputs, input_names);
}

static void
imt_switcher_f (void)
{
	int         argc = Cmd_Argc ();

	// state/imt pairs result in an even number of arguments
	if (argc < 4 || (argc & 1)) {
		Sys_Printf ("see help imt_switcher\n");
		return;
	}
	const char *switcher_name = Cmd_Argv (1);
	imt_switcher_t *switcher = IMT_FindSwitcher (switcher_name);
	if (!switcher) {
		Sys_Printf ("switcher %s does not exist\n", switcher_name);
		return;
	}
	int         num_states = 1 << switcher->num_inputs;
	for (int i = 2; i < argc; i += 2) {
		const char *state_str = Cmd_Argv (i + 0);
		const char *imt_name = Cmd_Argv (i + 1);
		char       *end;
		int         state = strtol (state_str, &end, 0);
		imt_t      *imt = imt_find_imt (switcher->context, imt_name);
		if (*end || state < 0 || state >= num_states) {
			Sys_Printf ("invalid state: %s\n", state_str);
			continue;
		}
		if (!imt) {
			Sys_Printf ("imt %s not in context %s\n",
						imt_name, switcher->context->name);
			continue;
		}
		switcher->imts[state] = imt;
	}
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
			DARRAY_CLEAR (&imt->axis_bindings);
			DARRAY_CLEAR (&imt->button_bindings);
			free ((char *) imt->name);
			free (imt);
		}
		while (ctx->switchers) {
			imt_switcher_t *switcher = ctx->switchers;
			ctx->switchers = switcher->next;
			for (int i = 0; i < switcher->num_inputs; i++) {
				imt_input_t *input = &switcher->inputs[i];
				switch (input->type) {
					case imti_button:
						IN_ButtonRemoveListener (input->button,
												 imt_switcher_button_update,
												 switcher);
						break;
					case imti_cvar:
						Cvar_RemoveListener (input->cvar,
											 imt_switcher_cvar_update,
											 switcher);
						break;
				}
			}
			free ((char *) switcher->name);
			free (switcher);
		}
		ctx->active_imt = 0;
		ctx->default_imt = 0;
		ctx->imt_tail = &ctx->imts;
		ctx->switcher_tail = &ctx->switchers;
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
		"    imt_create <context> <imt_name> [chain_name]\n"
		"\n"
		"The new table will be attached to the specified context\n"
		"imt_name must not already exist.\n"
		"If given, chain_name must already exist and be in the context.\n"
	},
	{	"imt_switcher_create", imt_switcher_create_f,
		"create a new imt switcher:\n"
		"    imt_switcher_create <name> <context> <default_imt> <input0>"
			" [..<inputN>]\n"
		"name is the name of the switcher and must be unique across all\n"
		"contexts\n"
		"The new switcher will be attached to the specified context\n"
		"default_imt specifies the default imt to be used for all possible\n"
		"states and must exist and be in the context.\n"
		"input0..inputN specify the inputs (cvar or button) used to set the\n"
		"switcher's state. As each input forms a bit in the state index,\n"
		"there will be 2**(N+1) states (so 4 inputs will result in 16\n"
		"states, and 16 inputs will result in 65536 states). Up to 16 inputs\n"
		"are allowed\n"
		"\n"
		"Buttons are spefied as +buttonname (eg, +mlook, +strafe).\n"
		"Cvars are just the cvar name (eg, freelook, lookstrafe).\n"
		"\n"
		"Use imt_switcher to set the imt for each state.\n"
		"\n"
		"There can be multiple switchers in a context, but they can wind up\n"
		"fighting over the active imt for the context if they share inputs.\n"
	},
	{	"imt_switcher", imt_switcher_f,
		"Set the imts for states in an imt switcher:\n"
		"    imt_switcher <name> <state_index> <imt>"
			" [..<state_indexM> <imtM>]\n"
		"name is the name of the switcher to be modifed and must exist\n"
		"state_index is the state index formed by the binary number\n"
		"interpretation of the inputs with input0 being bit 0 and inputN\n"
		"being bit N.\n"
		"imt is the name of the imt to be assigned to the state and must\n"
		"exist and be in the same context as the switcher.\n"
		"\n"
		"Any number of state_index imt pairs can be specified.\n"
	},
	{	"imt_drop_all", imt_drop_all_f,
		"delete all imt tables\n"
	},
	{},
};

void
IMT_Init (void)
{
	binding_mem = new_memsuper ();
	recipe_tab = Hash_NewTable (61, 0, recipe_free, 0, 0);
	Hash_SetHashCompare (recipe_tab, recipe_get_hash, recipe_compare);
	for (imtcmd_t *cmd = imt_commands; cmd->name; cmd++) {
		Cmd_AddCommand (cmd->name, cmd->func, cmd->desc);
	}
}

void
IMT_Shutdown (void)
{
	imt_drop_all_f ();
	Hash_DelTable (recipe_tab);

	delete_memsuper (binding_mem);
	binding_mem = 0;
	DARRAY_CLEAR (&axis_blocks);
	DARRAY_CLEAR (&button_blocks);
	DARRAY_CLEAR (&in_contexts);
}

void
IMT_SaveConfig (plitem_t *config)
{
	plitem_t   *ctx_list = PL_NewArray ();
	PL_D_AddObject (config, "contexts", ctx_list);
	for (size_t i = 0; i < in_contexts.size; i++) {
		in_context_t *context = &in_contexts.a[i];
		plitem_t   *ctx = PL_NewDictionary (0);
		PL_A_AddObject (ctx_list, ctx);
		PL_D_AddObject (ctx, "name", PL_NewString (context->name));
		if (context->imts) {
			plitem_t   *imt_list = PL_NewArray ();
			PL_D_AddObject (ctx, "imts", imt_list);
			for (imt_t *imt = context->imts; imt; imt = imt->next) {
				plitem_t   *imt_cfg = PL_NewDictionary (0);
				PL_D_AddObject (imt_cfg, "name", PL_NewString (imt->name));
				if (imt->chain) {
					PL_D_AddObject (imt_cfg, "chain",
									PL_NewString (imt->chain->name));
				}
				PL_A_AddObject (imt_list, imt_cfg);
				// the bindings are not written here because they are managed
				// by IN_Binding_SaveConfig: IMT does not really know the
				// device-input structure (it cound via the blocks, but it
				// doesn't know the device names (by design))
			}
		}
		if (context->default_imt) {
			PL_D_AddObject (ctx, "default_imt",
							PL_NewString (context->default_imt->name));
		}
		if (context->switchers) {
			plitem_t   *switcher_list = PL_NewArray ();
			PL_D_AddObject (ctx, "switchers", switcher_list);
			for (imt_switcher_t *switcher = context->switchers; switcher;
				 switcher = switcher->next) {
				plitem_t   *switcher_cfg = PL_NewDictionary (0);
				PL_A_AddObject (switcher_list, switcher_cfg);

				PL_D_AddObject (switcher_cfg, "name",
								PL_NewString (switcher->name));

				plitem_t   *input_list = PL_NewArray ();
				PL_D_AddObject (switcher_cfg, "inputs", input_list);
				for (int i = 0; i < switcher->num_inputs; i++) {
					imt_input_t *input = &switcher->inputs[i];
					const char *name = 0;
					switch (input->type) {
						case imti_button:
							name = va (0, "+%s", input->button->name);
							break;
						case imti_cvar:
							name = input->cvar->name;
							break;
					}
					PL_A_AddObject (input_list, PL_NewString (name));
				}

				plitem_t   *state_list = PL_NewArray ();
				PL_D_AddObject (switcher_cfg, "imts", state_list);
				int         num_states = 1 << switcher->num_inputs;
				for (int i = 0; i < num_states; i++) {
					imt_t      *imt = switcher->imts[i];
					PL_A_AddObject (state_list, PL_NewString (imt->name));
				}
			}
		}
	}
}

void
IMT_SaveAxisConfig (plitem_t *axes, int axis_ind, int dev_axis)
{
	for (size_t i = 0; i < in_contexts.size; i++) {
		in_context_t *context = &in_contexts.a[i];
		for (imt_t *imt = context->imts; imt; imt = imt->next) {
			in_axisbinding_t *a = imt->axis_bindings.a[axis_ind];
			if (a) {
				in_recipe_t *recipe = a->recipe;
				plitem_t   *axis = PL_NewDictionary (0);
				PL_A_AddObject (axes, axis);

				PL_D_AddObject (axis, "imt", PL_NewString (imt->name));
				PL_D_AddObject (axis, "num",
								PL_NewString (va (0, "%d", dev_axis)));
				PL_D_AddObject (axis, "axis", PL_NewString (a->axis->name));
				PL_D_AddObject (axis, "min",
								PL_NewString (va (0, "%d", recipe->min)));
				PL_D_AddObject (axis, "max",
								PL_NewString (va (0, "%d", recipe->max)));
				PL_D_AddObject (axis, "minzone",
								PL_NewString (va (0, "%d", recipe->minzone)));
				PL_D_AddObject (axis, "maxzone",
								PL_NewString (va (0, "%d", recipe->maxzone)));
				PL_D_AddObject (axis, "deadzone",
								PL_NewString (va (0, "%d", recipe->deadzone)));
				PL_D_AddObject (axis, "curve",
								PL_NewString (va (0, "%.9g", recipe->curve)));
				PL_D_AddObject (axis, "scale",
								PL_NewString (va (0, "%.9g", recipe->scale)));
			}
		}
	}
}

void
IMT_SaveButtonConfig (plitem_t *buttons, int button_ind, int dev_button)
{
	for (size_t i = 0; i < in_contexts.size; i++) {
		in_context_t *context = &in_contexts.a[i];
		for (imt_t *imt = context->imts; imt; imt = imt->next) {
			in_buttonbinding_t *b = imt->button_bindings.a[button_ind];
			if (b) {
				plitem_t   *button = PL_NewDictionary (0);
				PL_A_AddObject (buttons, button);

				PL_D_AddObject (button, "imt", PL_NewString (imt->name));
				PL_D_AddObject (button, "num",
								PL_NewString (va (0, "%d", dev_button)));
				switch (b->type) {
					case inb_button:
						PL_D_AddObject (button, "binding",
										PL_NewString (va (0, "+%s",
														  b->button->name)));
						break;
					case inb_command:
						PL_D_AddObject (button, "binding",
										PL_NewString (b->command));
						break;
				}
			}
		}
	}
}

void
IMT_LoadConfig (plitem_t *config)
{
	imt_drop_all_f ();
	imt_reset_blocks ();

	plitem_t   *ctx_list = PL_ObjectForKey (config, "contexts");
	if (PL_Type (ctx_list) != QFArray) {
		Sys_Printf ("IMT_LoadConfig: contexts not an array\n");
		return;
	}
	for (int i = 0, count = PL_A_NumObjects (ctx_list); i < count; i++) {
		plitem_t   *ctx = PL_ObjectAtIndex (ctx_list, i);
		const char *name = PL_String (PL_ObjectForKey (ctx, "name"));
		in_context_t *context = imt_find_context (name);
		if (!context) {
			continue;
		}
		plitem_t   *imts = PL_ObjectForKey (ctx, "imts");
		if (!imts || PL_Type (imts) != QFArray) {
			continue;
		}
		for (int j = 0, num_imts = PL_A_NumObjects (imts); j < num_imts; j++) {
			plitem_t   *imt = PL_ObjectAtIndex (imts, j);
			const char *imt_name = PL_String (PL_ObjectForKey (imt, "name"));
			const char *imt_chain = PL_String (PL_ObjectForKey (imt, "chain"));
			if (imt_name) {
				IMT_CreateIMT (context - in_contexts.a, imt_name, imt_chain);
			}
		}
		const char *default_imt = PL_String (PL_ObjectForKey (ctx,
															  "default_imt"));
		if (default_imt) {
			context->default_imt = IMT_FindIMT (default_imt);
		}
		if (!context->default_imt) {
			context->default_imt = context->imts;
		}
		context->active_imt = context->default_imt;

		plitem_t   *switcher_list = PL_ObjectForKey (ctx, "switchers");
		if (!switcher_list || PL_Type (switcher_list) != QFArray) {
			continue;
		}
		for (int j = 0, num_switchers = PL_A_NumObjects (switcher_list);
			 j < num_switchers; j++) {
			plitem_t   *switcher = PL_ObjectAtIndex (switcher_list, j);
			const char *name = PL_String (PL_ObjectForKey (switcher, "name"));
			plitem_t   *input_list = PL_ObjectForKey (switcher, "inputs");
			if (!name || !input_list || PL_Type (input_list) != QFArray) {
				continue;
			}
			int         num_inputs = PL_A_NumObjects (input_list);
			const char **input_names = alloca (num_inputs * sizeof (char *));
			for (int k = 0; k < num_inputs; k++) {
				input_names[k] = PL_String (PL_ObjectAtIndex (input_list, k));
			}
			if (!IMT_CreateSwitcher (name, context - in_contexts.a,
									 context->default_imt,
									 num_inputs, input_names)) {
				continue;
			}
			imt_switcher_t *s = imt_find_switcher (context, name);

			plitem_t   *imt_list = PL_ObjectForKey (switcher, "imts");
			if (!imt_list || PL_Type (imt_list) != QFArray) {
				continue;
			}
			for (int k = 0, count = PL_A_NumObjects (imt_list); k < count;
				 k++) {
				const char *imt_name = PL_String(PL_ObjectAtIndex(imt_list, k));
				if (!imt_name) {
					continue;
				}
				imt_t      *imt = imt_find_imt (context, imt_name);
				if (imt) {
					s->imts[k] = imt;
				}
			}
		}
	}
}
