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

#define GIB_DATA(buffer) ((gib_buffer_data_t *)(buffer->data))

typedef struct gib_buffer_data_s {
	struct dstring_s *arg_composite;
} gib_buffer_data_t;

void GIB_Buffer_Construct (struct cbuf_s *cbuf);
void GIB_Buffer_Destruct (struct cbuf_s *cbuf);
