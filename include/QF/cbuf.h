/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
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

#ifndef __QF_cbuf_h
#define __QF_cbuf_h

/** \defgroup cbuf Command buffer management.
	\ingroup utils
*/
///@{

#include <stdarg.h>

#include "QF/qtypes.h"

typedef struct cbuf_args_s {
	int         argc;
	struct dstring_s **argv;
	void      **argm; // Metadata (optional)
	const char **args;
	int         argv_size;
} cbuf_args_t;


typedef struct cbuf_s {
	cbuf_args_t *args;
	struct cbuf_interpreter_s *interpreter;

	struct cbuf_s *up, *down;	// The stack

	enum {
		CBUF_STATE_NORMAL = 0,	// Normal condition
		CBUF_STATE_WAIT,		// Buffer is stalled until next frame
		CBUF_STATE_BLOCKED,		// Buffer is blocked until further notice
		CBUF_STATE_ERROR,		// An unrecoverable error occured
		CBUF_STATE_STACK,		// A buffer has been added to the stack
		CBUF_STATE_JUNK			// Buffer can be freed or reused
	} state;

	int	      (*unknown_command)(void);	// handle unkown commands. !0 = handled
	qboolean    strict;			// Should we tolerate unknown commands?
	double      resumetime;		// Time when stack can be executed again

	void       *data;			// Pointer to interpreter data
} cbuf_t;

typedef struct cbuf_interpreter_s {
	void		(*construct) (struct cbuf_s *cbuf);
	void		(*destruct) (struct cbuf_s *cbuf);
	void		(*reset) (struct cbuf_s *cbuf);
	void		(*add) (struct cbuf_s *cbuf, const char *str);
	void		(*insert) (struct cbuf_s *cbuf, const char *str);
	void		(*execute) (struct cbuf_s *cbuf);
	void		(*execute_sets) (struct cbuf_s *cbuf);
	const char** (*complete) (struct cbuf_s *cbuf, const char *str);
} cbuf_interpreter_t;

extern cbuf_t *cbuf_active;

cbuf_args_t *Cbuf_ArgsNew (void);
void Cbuf_ArgsDelete (cbuf_args_t *);
void Cbuf_ArgsAdd (cbuf_args_t *args, const char *arg);

cbuf_t * Cbuf_New (cbuf_interpreter_t *interp);

void Cbuf_Delete (cbuf_t *cbuf);
void Cbuf_DeleteStack (cbuf_t *stack);
void Cbuf_Reset (cbuf_t *cbuf);
cbuf_t *Cbuf_PushStack (cbuf_interpreter_t *interp);
void Cbuf_AddText (cbuf_t *cbuf, const char *text);
void Cbuf_InsertText (cbuf_t *cbuf, const char *text);
void Cbuf_Execute (cbuf_t *cbuf);
void Cbuf_Execute_Stack (cbuf_t *cbuf);
void Cbuf_Execute_Sets (cbuf_t *cbuf);

///@}

#endif//__QF_cbuf_h
