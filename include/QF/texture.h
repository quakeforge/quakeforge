/*
	texture.h

	texture definition

	Copyright (C) 2000       Bill Currie

	Author: Bill Currie
	Date: 2000-01-18

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

#ifndef __texture_h
#define __texture_h

// could not use texture_t as that is used for models.
typedef struct tex_s {
	int width;
	int height;
	unsigned char *palette;					// 0 = 32 bit, otherise 8
	unsigned char data[4];					// variable length
} tex_t;

#endif // __texture_h
