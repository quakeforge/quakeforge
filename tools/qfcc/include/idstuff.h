/*
	idstuff.h

	qcc compatable output stuff

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/04

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

#ifndef __idstuff_h
#define __idstuff_h

#include "QF/progs/pr_comp.h"

//XXX eww :/
void PrecacheSound (const char *, int ch);
void PrecacheModel (const char *, int ch);
void PrecacheFile (const char *, int ch);
int  WriteFiles (const char *sourcedir);
int  WriteProgdefs (dprograms_t *progs, const char *filename);

#endif//__idstuff_h
