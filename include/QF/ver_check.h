/*
	ver_check.h

	Version number comparison prototype

	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>

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

#ifndef __QF_ver_check_h
#define __QF_ver_check_h

/** \defgroup utils Utils
*/

/** \defgroup misc Miscellaneous functions
	\ingroup utils
*/
///@{

/*
	ver_compare

	Compare two ASCII version strings. If the first is greater than the second,
	return a positive number. If the second is greater, return a negative. If
	they are equal, return zero.
*/
int ver_compare (const char *, const char *);

///@}

#endif//__QF_ver_check_h
