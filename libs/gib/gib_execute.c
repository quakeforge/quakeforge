/*
	gib_execute.c

	GIB runtime execution functions

	Copyright (C) 2002 Brian Koropoff

	Author: Brian Koropoff
	Date: #DATE#

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

#include <string.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/gib.h"

#include "gib_buffer.h"
#include "gib_builtin.h"
#include "gib_function.h"
#include "gib_vars.h"
#include "gib_tree.h"
#include "gib_process.h"
#include "gib_execute.h"

static void
GIB_Execute_Generate_Composite (struct cbuf_s *cbuf)
{
	cbuf_args_t *args = cbuf->args;
	int         i;

	dstring_clearstr (GIB_DATA (cbuf)->arg_composite);
	for (i = 0; i < args->argc; i++) {
		// ->str could be moved by realloc when a dstring is resized
		// so save the offset instead of the pointer
		args->args[i] =	(const char *) strlen (GIB_DATA (cbuf)->arg_composite->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, args->argv[i]->str);
		dstring_appendstr (GIB_DATA (cbuf)->arg_composite, " ");
	}

	// Get rid of trailing space
	GIB_DATA (cbuf)->arg_composite->
		str[strlen (GIB_DATA (cbuf)->arg_composite->str) - 1] = 0;

	for (i = 0; i < args->argc; i++)
		// now that arg_composite is done we can add the pointer to the stored
		// offsets and get valid pointers. This *should* be portable.
		args->args[i] += (intptr_t) GIB_DATA (cbuf)->arg_composite->str;
}

static void
GIB_Execute_Split_Var (cbuf_t * cbuf)
{
	gib_var_t  *var;
	unsigned int i;
	unsigned int start = 0, end = ((unsigned int) ~0 >> 1);
	char       *c, *str = cbuf->args->argv[cbuf->args->argc - 1]->str + 1;
	void       *m = cbuf->args->argm[cbuf->args->argc - 1];

	i = strlen (str);
	if (i)
		i--;
	if (str[-1] == '@') {
		if (str[i] == ']')
			for (; i; i--)
				if (str[i] == '[') {
					str[i] = 0;
					start = atoi (str + i + 1);
					if ((c = strchr (str + i + 1, ':'))) {
						if (c[1] != ']')
							end = atoi (c + 1);
					} else
						end = start + 1;
					break;
				}
		cbuf->args->argc--;
		if (!(var = GIB_Var_Get_Complex (
			&GIB_DATA (cbuf)->locals,
			&GIB_DATA (cbuf)->globals,
			str, &i, false)))
				return;
		if ((int) end < 0)
			end += var->size;
		else if (end > var->size)
			end = var->size;
		if ((int) start < 0) {
			start += var->size;
			if ((int) start < 0)
				start = 0;
		} else if (start >= var->size || start >= end)
			return;
		for (i = start; i < end; i++) {
			if (var->array[i].value)
				Cbuf_ArgsAdd (cbuf->args, var->array[i].value->str);
			else
				Cbuf_ArgsAdd (cbuf->args, "");
			cbuf->args->argm[cbuf->args->argc - 1] = m;
		}
	} else {
		gib_var_t **vlist, **v;

		cbuf->args->argc--;
		if (!(var = GIB_Var_Get_Complex (
			&GIB_DATA (cbuf)->locals,
			&GIB_DATA (cbuf)->globals,
			str, &i, false)))
				return;
		if (!var->array[i].leaves)
			return;
		vlist = (gib_var_t **) Hash_GetList (var->array[i].leaves);
		for (v = vlist; *v; v++)
			Cbuf_ArgsAdd (cbuf->args, (*v)->key);
	}
}

static int
GIB_Execute_Prepare_Line (cbuf_t * cbuf, gib_tree_t * line)
{
	gib_tree_t *cur;
	cbuf_args_t *args = cbuf->args;
	unsigned int pos;

	args->argc = 0;

	for (cur = line->children; cur; cur = cur->next) {
		if (cur->flags & TREE_A_CONCAT) {
			pos = args->argv[args->argc - 1]->size - 1;
			if (cur->flags & TREE_A_EMBED) {
				GIB_Process_Embedded (cur, cbuf->args);
			} else
				dstring_appendstr (args->argv[args->argc - 1], cur->str);
		} else {
			pos = 0;
			if (cur->flags & TREE_A_EMBED) {
				Cbuf_ArgsAdd (args, "");
				GIB_Process_Embedded (cur, cbuf->args);
			} else {
				Cbuf_ArgsAdd (args, cur->str);
				args->argm[args->argc - 1] = cur;
			}
		}
		if (cur->delim == '('
			&& GIB_Process_Math (args->argv[args->argc - 1], pos))
			return -1;
		if (cur->flags & TREE_A_EXPAND)
			GIB_Execute_Split_Var (cbuf);
	}
	return 0;
}

static int
GIB_Execute_For_Next (cbuf_t * cbuf)
{
	unsigned int index;
	gib_var_t  *var;
	struct gib_dsarray_s *array = GIB_DATA (cbuf)->stack.values + GIB_DATA (cbuf)->stack.p - 1;
	if (array->size == 1) {
		GIB_Buffer_Pop_Sstack (cbuf);
		return -1;
	}
	array->size--;
	var = GIB_Var_Get_Complex (
		&GIB_DATA (cbuf)->locals,
		&GIB_DATA (cbuf)->globals, array->dstrs[0]->str,
		&index, true
	);
	dstring_clearstr (var->array[index].value);
	dstring_appendstr (var->array[index].value, array->dstrs[array->size]->str);
	return 0;
}

void
GIB_Execute (cbuf_t * cbuf)
{
	gib_buffer_data_t *g = GIB_DATA (cbuf);
	gib_builtin_t *b;
	gib_function_t *f;
	gib_object_t *obj;
	unsigned int index;
	gib_var_t *var;
	int i;
	bool super;

	static const char **mesg = NULL;
	static int maxmesg = 0;

	if (!g->program)
		return;
	g->ip = g->ip ? g->ip->next : g->program;
	while (g->ip) {
		switch (g->ip->type) {
			case TREE_T_LABEL: // Move to next instruction
				g->ip = g->ip->next;
				continue;
			case TREE_T_JUMP: // Absolute jump
				g->ip = g->ip->jump;
				continue;
			case TREE_T_FORNEXT: // Fetch next value in a for loop
				if (GIB_Execute_For_Next (cbuf))
					g->ip = g->ip->jump->next;
				else
					g->ip = g->ip->next;
				continue;
			case TREE_T_COND: // Conditional jump, move to next instruction
				if (GIB_Execute_Prepare_Line (cbuf, g->ip))
					return;
				if (g->ip->flags & TREE_L_NOT ?
					atof (cbuf->args->argv[1]->str) :
					!atof (cbuf->args->argv[1]->str))
						g->ip = g->ip->jump->next;
				else
					g->ip = g->ip->next;
				continue;
			case TREE_T_ASSIGN: // Assignment
				if (GIB_Execute_Prepare_Line (cbuf, g->ip))
					return;
				var = GIB_Var_Get_Complex (
					&g->locals,
					&g->globals,
					cbuf->args->argv[0]->str, &index, true);
				GIB_Var_Assign (var, index, cbuf->args->argv + 2, cbuf->args->argc - 2, cbuf->args->argv[0]->str[strlen (cbuf->args->argv[0]->str) - 1] != ']');
				if (g->ip->flags & TREE_L_EMBED) {
					GIB_Buffer_Push_Sstack (cbuf);
					g->waitret = true;
					if (GIB_CanReturn ())
						for (i = 2; i < cbuf->args->argc; i++)
							GIB_Return (cbuf->args->argv[i]->str);
				} else
					g->waitret = false;
				g->ip = g->ip->next;
				continue;
			case TREE_T_SEND: // Message sending
				if (GIB_Execute_Prepare_Line (cbuf, g->ip))
					return;
				if (cbuf->args->argc - 2 > maxmesg) {
					maxmesg += 32;
					mesg = realloc ((void*)mesg, sizeof (char *) * maxmesg);
				}
				for (i = 2; i < cbuf->args->argc; i++)
					mesg[i-2] = cbuf->args->argv[i]->str;

				super = false;
				if (!strcmp (cbuf->args->argv[0]->str,
							"super")) {
						if (!(obj = g->reply.obj)) {
							GIB_Error (
								"send",
								"Sending "
								"message to "
								"super not "
								"possible in "
								"this context."
							);
							return;
						}
						super = true;
				} else if (!(obj = GIB_Object_Get (cbuf->args->argv[0]->str))) {
					GIB_Error (
						"send",
						"No such object or class: %s",
						cbuf->args->argv[0]->str
					);
					return;
				}
				if (g->ip->flags & TREE_L_EMBED) {
					// Get ready for return values
					g->waitret = true;
					GIB_Buffer_Push_Sstack (cbuf);
					cbuf->state = CBUF_STATE_BLOCKED;
					i = super ? GIB_SendToMethod (obj,
							g->reply.method->parent,
							g->reply.obj, i - 2, mesg,
							GIB_Buffer_Reply_Callback,
							cbuf) : GIB_Send (obj,
							g->reply.obj, i - 2, mesg,
							GIB_Buffer_Reply_Callback,
							cbuf);
				} else {
					g->waitret = false;
					i = super ? GIB_SendToMethod (obj,
							g->reply.method->parent,
							g->reply.obj, i - 2, mesg,
							NULL, NULL) :
							GIB_Send (obj,g->reply.obj,
							i - 2, mesg, NULL,
							NULL);
				}
				if (i < 0) {
					GIB_Error (
						"send",
						"Object %s (%s) could not handle message %s",
						cbuf->args->argv[0]->str,
						obj->class->name,
						cbuf->args->argv[2]->str
					);
					return;
				}
				g->ip = g->ip->next;
				continue;
			case TREE_T_CMD: // Normal command
				if (GIB_Execute_Prepare_Line (cbuf, g->ip))
					return;
				if (g->ip->flags & TREE_L_EMBED) {
				// Get ready for return values
					g->waitret = true;
					GIB_Buffer_Push_Sstack (cbuf);
				} else
					g->waitret = false;
				if (cbuf->args->argc) {
					if ((b = GIB_Builtin_Find (cbuf->args->argv[0]->str)))
						b->func ();
					else if ((f = GIB_Function_Find (cbuf->args->argv[0]->str))) {
						cbuf_t     *new = Cbuf_PushStack (&gib_interp);
						if (GIB_Function_Execute_D
								(new, f,
								 cbuf->args->argv,
								 cbuf->args->argc))
							GIB_Error ("syntax", "not "
									"enough "
									"arguments to "
									"function '%s'",
									cbuf->args->argv[0]->str);
					} else {
						GIB_Execute_Generate_Composite (cbuf);
						if (Cmd_Command (cbuf->args))
							GIB_Error (
								"UnknownCommandError",
								"No builtin, function, or console command "
								"named '%s' was found.",
								cbuf->args->argv[0]->str
							);
					}
				}
				if (cbuf->state)
					return;
				g->ip = g->ip->next;
				continue;
			default: // We should never get here
				GIB_Error (
					"QUAKEFORGE-BUG-PLEASE-REPORT",
					"Unknown instruction type; tastes like chicken."
				);
				return;
		}
	}
	g->ip = 0;
	GIB_Tree_Unref (&g->program);
	g->program = 0;
}
