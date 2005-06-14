/*
	view.h

	console view object

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/5/5

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

	$Id$
*/

#ifndef __qf_view_h
#define __qf_view_h

typedef enum {
	grav_center,
	grav_north,
	grav_northeast,
	grav_east,
	grav_southeast,
	grav_south,
	grav_southwest,
	grav_west,
	grav_northwest,
} grav_t;

typedef struct view_s view_t;
struct view_s {
	int     xpos, ypos;
	int     xlen, ylen;
	int     xabs, yabs;
	int     xrel, yrel;
	grav_t  gravity;
	view_t *parent;
	view_t **children;
	int     num_children;
	int     max_children;
	void    (*draw)(view_t *view);
	void    (*setgeometry)(view_t *view);
	void   *data;
	unsigned visible:1;
	unsigned resize_x:1;
	unsigned resize_y:1;
};


view_t *view_new (int xp, int yp, int xl, int yl, grav_t grav);
void view_insert (view_t *par, view_t *view, int pos);
void view_add (view_t *par, view_t *view);
void view_remove (view_t *par, view_t *view);
void view_delete (view_t *view);
void view_draw (view_t *view);
void view_resize (view_t *view, int xl, int yl);
void view_move (view_t *view, int xp, int yp);

#endif//__qf_view_h
