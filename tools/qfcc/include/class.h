/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

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

	$Id$
*/

#ifndef __class_h
#define __class_h

#include "method.h"

typedef struct class_s {
	const char *name;
	struct class_s *base;
	methodlist_t methods;
} class_t;

typedef struct protocol_s {
	const char *name;
	methodlist_t methods;
} protocol_t;

typedef struct category_s {
	const char *name;
	class_t    *klass;
	methodlist_t methods;
} category_t;

class_t *new_class (const char *name, class_t *base);
protocol_t *new_protocol (const char *name);
category_t *new_category (const char *name, class_t *klass);
class_t *find_class (const char *name);
protocol_t *find_protocol (const char *name);

#endif//__class_h
