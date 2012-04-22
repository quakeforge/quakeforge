/*
	uint32.h

	Definitions for portable (?) unsigned int

	Copyright (C) 2000       Jeff Teunissen <d2deek@pmail.net>

	Author: Jeff Teunissen	<d2deek@pmail.net>
	Date: 01 Jan 2000

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

#ifndef __uint32_h
#define __uint32_h

#ifndef int32
# if (SIZEOF_INT == 4)
#  define int32 int
# elif (SIZEOF_LONG == 4)
#  define int32 long
# elif (SIZEOF_SHORT == 4)
#  define int32 short
# else
/* I hope this works */
#  define int32 int
#  define LARGE_INT32
# endif
#endif

#ifndef uint32
# define uint32 unsigned int32
#endif

#endif	// __uint32_h
