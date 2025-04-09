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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/quakeio.h"

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/grab.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/strpool.h"

int grab_frame;
int grab_other;
int grab_write;

static hashtab_t *frame_tab;
static hashtab_t *grab_tab;

typedef struct frame_s {
	struct frame_s *next;
	const char *name;
	int			num;
} frame_t;

ALLOC_STATE (frame_t, frames);
static frame_t *frame_list;
static frame_t **frame_tail = &frame_list;

static frame_t grab_list[] = {
	{0, "modelname",		0},
	{0, "base",				0},
	{0, "cd",				0},
	{0, "sync",				0},
	{0, "origin",			0},
	{0, "eyeposition",		0},
	{0, "scale",			0},
	{0, "flags",			0},
	{0, "skin",				0},
	{0, "framegroupstart",	0},
	{0, "skingroupstart",	0},
};

static const char *
frame_get_key (const void *f, void *unused)
{
	return ((frame_t*)f)->name;
}

static void
frame_free (void *_f, void *unused)
{
	frame_t    *f = (frame_t *)_f;
	FREE (frames, f);
}

int
do_grab (const char *token)
{
	static int initialized;
	frame_t *frame;

	if (!initialized) {
		size_t      i;

		initialized = 1;
		frame_tab = Hash_NewTable (1021, frame_get_key, frame_free, 0, 0);
		grab_tab = Hash_NewTable (1021, frame_get_key, 0, 0, 0);
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
	if (!strcmp (token, "frame_write"))
		return -grab_write;
	if (Hash_Find (grab_tab, token))
		return -grab_other;
	frame = Hash_Find (frame_tab, token);
	if (frame)
		return frame->num;
	return 0;
}

static int frame_number;

void
add_frame_macro (const char *token)
{
	frame_t *frame;

	if (Hash_Find (frame_tab, token)) {
		warning (0, "duplicate frame macro `%s'", token);
		if (options.traditional)
			frame_number++;
		return;
	}
	ALLOC (1024, frame_t, frames, frame);

	*frame_tail = frame;
	frame_tail = &frame->next;
	frame->name = save_string (token);
	frame->num = frame_number++;
	Hash_Add (frame_tab, frame);
}

void
clear_frame_macros (void)
{
	frame_number = 0;
	frame_tail = &frame_list;
	frame_list = 0;
	if (frame_tab)
		Hash_FlushTable (frame_tab);
}

void
write_frame_macros (const char *filename)
{
	frame_t *frame;
	QFile      *file;

	if (!frame_list)
		return;
	file = Qopen (filename, "wt");
	for (frame = frame_list; frame; frame = frame->next)
		Qprintf (file, "%s %d\n", frame->name, frame->num);
	Qclose (file);
}
