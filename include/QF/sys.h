/*
	sys.h

	non-portable functions

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef __sys_h
#define __sys_h

/** \addtogroup utils */
//@{

/** \defgroup sys Portability
	Non-portable functions
*/
//@{

#include <stdio.h>
#include <stdarg.h>

extern	struct cvar_s	*sys_nostdout;
extern	struct cvar_s	*sys_extrasleep;
extern	struct cvar_s	*sys_dead_sleep;
extern	struct cvar_s	*sys_sleep;

extern struct cvar_s *developer;


extern const char sys_char_map[256];

typedef struct date_s {
	int         sec;
	int         min;
	int         hour;
	int         day;
	int         mon;
	int         year;
	char        str[128];
} date_t;

int	Sys_FileTime (const char *path);
int Sys_mkdir (const char *path);

typedef void (*sys_printf_t) (const char *fmt, va_list args);

void Sys_SetStdPrintf (sys_printf_t func);
void Sys_SetErrPrintf (sys_printf_t func);

void Sys_Print (FILE *stream, const char *fmt, va_list args);
void Sys_Printf (const char *fmt, ...) __attribute__((format(printf,1,2)));
void Sys_DPrintf (const char *fmt, ...) __attribute__((format(printf,1,2)));
void Sys_Error (const char *error, ...) __attribute__((format(printf,1,2), noreturn));
void Sys_Quit (void) __attribute__((noreturn));
void Sys_Shutdown (void);
void Sys_RegisterShutdown (void (*func) (void));
double Sys_DoubleTime (void);
void Sys_TimeOfDay(date_t *date);

int Sys_CheckInput (int idle, unsigned int net_socket);
const char *Sys_ConsoleInput (void);

void Sys_Sleep (void);

int Sys_TimeID (void);
// called to yield for a little bit so as
// not to hog cpu when paused or debugging

void Sys_MaskFPUExceptions (void);
void Sys_PushSignalHook (int (*hook)(int, void*), void *data);
void Sys_PopSignalHook (void);

// send text to the console

void Sys_Init (void);
void Sys_Init_Cvars (void);

//
// memory protection
//
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length);
void Sys_PageIn (void *ptr, int size);

//
// system IO
//
void Sys_DebugLog(const char *file, const char *fmt, ...) __attribute__((format(printf,2,3)));

#define SYS_CHECKMEM(x) 												\
	do {																\
		if (!(x))														\
			Sys_Error ("%s: Failed to allocate memory.", __FUNCTION__);	\
	} while (0)

//@}
//@}

#endif // __sys_h
