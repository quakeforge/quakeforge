/*
        borland.c

        borland support routines

	Copyright (C) 1996-1997  Id Software, Inc.
        Copyright (C) 1999,2000  Jukka Sorjonen.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#ifdef __BORLANDC__

#include <stdio.h>
#include <stdarg.h>

// replacement funcs for missing asms - not good but who cares
#ifndef USE_INTEL_ASM
void Sys_HighFPPrecision(void)
{
	return;
}

void Sys_LowFPPrecision(void)
{
return;
}

void MaskExceptions(void)
{
return;
}

void Sys_SetFPCW(void)
{
return;
}
#endif

#if (__BORLANDC__ < 0x550)

void vsnprintf(char *buffer, size_t t,char *format, va_list argptr)
{
        vsprintf(buffer,format,argptr);
}

void snprintf(char * buffer, size_t n, const char * format, ...)
{
        va_list argptr;
        va_start(argptr,format);
        vsprintf(buffer,format,argptr);
        va_end(argptr);
        return;
}
#endif

#endif


