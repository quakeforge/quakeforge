/*
	va.h

	Definitions common to client and server.

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000  Marcus Sundberg <mackan@stacken.kth.se>

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

#ifndef __va_h
#define __va_h

/** \addtogroup misc
	Formatted printing.
*/
///@{

// does a varargs printf into a temp buffer
char	*va(const char *format, ...) __attribute__((format(printf,1,2)));
// does a varargs printf into a malloced buffer
char	*nva(const char *format, ...) __attribute__((format(printf,1,2)));

///@}

#endif // __va_h
