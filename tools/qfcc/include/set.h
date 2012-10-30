/*
	set.h

	Set manipulation.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/8/4

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

#ifndef set_h
#define set_h

/**	\defgroup qfcc_set Set handling
	\ingroup qfcc
*/
//@{

typedef struct set_s {
	unsigned    size;
	unsigned   *map;
	unsigned	defmap[8];
} set_t;

set_t *set_new (void);
void set_delete (set_t *set);
void set_add (set_t *set, unsigned x);
void set_remove (set_t *set, unsigned x);

set_t *set_union (set_t *dst, const set_t *src);
set_t *set_intersection (set_t *dst, const set_t *src);
set_t *set_difference (set_t *dst, const set_t *src);
set_t *set_assign (set_t *dst, const set_t *src);
set_t *set_empty (set_t *set);

int set_is_empty (const set_t *set);
int set_is_disjoint (const set_t *s1, const set_t *s2);
int set_is_intersecting (const set_t *s1, const set_t *s2);
int set_is_equivalent (const set_t *s1, const set_t *s2);
int set_is_subset (const set_t *set, const set_t *sub);
int set_is_member (const set_t *set, unsigned x);
const char *set_as_string (const set_t *set);

//@}
#endif//set_h
