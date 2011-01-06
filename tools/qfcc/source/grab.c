/*
	grab.c

	frame macro grabbing

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/06

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>

#include "QF/hash.h"

#include "expr.h"
#include "grab.h"
#include "options.h"
#include "qfcc.h"

int grab_frame;
int grab_other;

static hashtab_t *frame_tab;
static hashtab_t *grab_tab;

typedef struct frame_s {
	struct frame_s *next;
	const char *name;
	int			num;
} frame_t;

static frame_t *free_frames;

static frame_t grab_list[] = {
	{0, "cd",		0},
	{0, "origin",	0},
	{0, "base",		0},
	{0, "flags",	0},
	{0, "scale",	0},
	{0, "skin",		0},
};

static const char *
frame_get_key (void *f, void *unused)
{
	return ((frame_t*)f)->name;
}

static void
frame_free (void *_f, void *unused)
{
	frame_t    *f = (frame_t *)_f;
	f->next = free_frames;
	free_frames = f;
}

int
do_grab (char *token)
{
	static int initialized;
	frame_t *frame;

	if (!initialized) {
		size_t      i;

		initialized = 1;
		frame_tab = Hash_NewTable (1021, frame_get_key, frame_free, 0);
		grab_tab = Hash_NewTable (1021, frame_get_key, 0, 0);
		for (i = 0; i < sizeof (grab_list) / sizeof (grab_list[0]); i++)
			Hash_Add (grab_tab, &grab_list[i]);
	}
	while (isspace ((unsigned char)*++token))
		// advance over $ and leading space
		;
	if (!strcmp (token, "frame"))
		return -grab_frame;
	if (!strcmp (token, "frame_reset")) {
		clear_frame_macros ();
		return -grab_other;
	}
	if (Hash_Find (grab_tab, token))
		return -grab_other;
	frame = Hash_Find (frame_tab, token);
	if (frame)
		return frame->num;
	return 0;
}

static int frame_number;

void
add_frame_macro (char *token)
{
	frame_t *frame;

	if (Hash_Find (frame_tab, token)) {
		warning (0, "duplicate frame macro `%s'", token);
		if (options.traditional)
			frame_number++;
		return;
	}
	ALLOC (1024, frame_t, frames, frame);

	frame->name = save_string (token);
	frame->num = frame_number++;
	Hash_Add (frame_tab, frame);
}

void
clear_frame_macros (void)
{
	frame_number = 0;
	if (frame_tab)
		Hash_FlushTable (frame_tab);
}

