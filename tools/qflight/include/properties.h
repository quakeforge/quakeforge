/*
	properties.c

	Light properties handling.

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2004/01/27

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
float parse_float (const char *str);
void parse_color (const char *str, vec3_t color);
float parse_light (const char *str, vec3_t color);
int parse_attenuation (const char *arg);
int parse_noise (const char *arg);
struct plitem_s;
void set_properties (entity_t *ent, struct plitem_s *dict);

void LoadProperties (const char *filename);
