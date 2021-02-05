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

*/

#ifndef __QF_sys_h
#define __QF_sys_h

/** \defgroup sys System Portability
	\ingroup utils
	Non-portable functions
*/
///@{

#include <stdio.h>
#include <stdint.h>
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

int	Sys_FileExists (const char *path);
int Sys_isdir (const char *path);
int Sys_mkdir (const char *path);

typedef void (*sys_printf_t) (const char *fmt, va_list args) __attribute__((format(printf, 1, 0)));
typedef void (*sys_error_t) (void *data);

sys_printf_t Sys_SetStdPrintf (sys_printf_t func);
sys_printf_t Sys_SetErrPrintf (sys_printf_t func);

void Sys_PushErrorHandler (sys_error_t func, void *data);
void Sys_PopErrorHandler (void);

void Sys_Print (FILE *stream, const char *fmt, va_list args) __attribute__((format(printf, 2, 0)));
void Sys_Printf (const char *fmt, ...) __attribute__((format(printf,1,2)));
void Sys_Error (const char *error, ...) __attribute__((format(printf,1,2), noreturn));
void Sys_FatalError (const char *error, ...) __attribute__((format(printf,1,2), noreturn));
void Sys_Quit (void) __attribute__((noreturn));
void Sys_Shutdown (void);
void Sys_RegisterShutdown (void (*func) (void *), void *data);
int64_t Sys_LongTime (void);
double Sys_DoubleTime (void);
void Sys_TimeOfDay(date_t *date);

void Sys_MaskPrintf (int mask, const char *fmt, ...) __attribute__((format(printf,2,3)));
#define SYS_DEV     (1|0)
#define SYS_WARN    (1|2)	// bit 0 so developer 1 will pick it up
#define SYS_VID     (1|4)
#define SYS_FS_NF   (1|8)
#define SYS_FS_F    (1|16)
#define SYS_FS      (1|32)
#define SYS_NET     (1|64)
#define SYS_RUA_OBJ (1|128)
#define SYS_RUA_MSG (1|256)
#define SYS_SND     (1|512)
#define SYS_GLT     (1|1024)
#define SYS_GLSL    (1|2048)
#define SYS_SKIN    (1|4096)
#define SYS_MODEL   (1|8192)
#define SYS_CSQC    (1|16384)

int Sys_CheckInput (int idle, int net_socket);
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
void Sys_MakeCodeWriteable (uintptr_t startaddr, size_t length);
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

/**	Create all parent directories leading to the file specified by path.

	\param path		The path to create.
	\return			0 on success, -1 on failure.

	\note No directory will be created for the name after the final
	<code>/</code>. This is to allow the same path string to be used for
	both this function and Qopen.
*/
int Sys_CreatePath (const char *path);

/**	Expand leading "~/" in \a path to the user's home directory.
	On Linux-like systems, the user's home directory is obtained from the
	system, or failing that, the \c HOME environment variable.

	On Windows systems, first the \c HOME environment variable is checked.
	If \c HOME is not set, \c WINDIR is used.

	\param path		the path to check for "~/"
	\return			the expanded path
	\note It is the caller's responsibility to free the returned string.
*/
char *Sys_ExpandSquiggle (const char *path);

///@}

#endif//__QF_sys_h
