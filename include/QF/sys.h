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

struct dstring_s;

extern int sys_nostdout;
extern int sys_extrasleep;
extern int sys_dead_sleep;
extern int sys_sleep;

extern int developer;

extern bool sys_quake_encoding;
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

typedef void (*sys_printf_t) (const char *fmt, va_list args) __attribute__((format(PRINTF, 1, 0)));
typedef void (*sys_error_t) (void *data);

sys_printf_t Sys_SetStdPrintf (sys_printf_t func);
sys_printf_t Sys_SetErrPrintf (sys_printf_t func);

void Sys_PushErrorHandler (sys_error_t func, void *data);
void Sys_PopErrorHandler (void);

#define DFL "\e[39;49m"
#define BLK "\e[30;40m"
#define RED "\e[31;40m"
#define GRN "\e[32;40m"
#define ONG "\e[33;40m"
#define BLU "\e[34;40m"
#define MAG "\e[35;40m"
#define CYN "\e[36;40m"
#define WHT "\e[37;40m"

void Sys_Print (FILE *stream, const char *fmt, va_list args) __attribute__((format(PRINTF, 2, 0)));
void Sys_Printf (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));
void Sys_Error (const char *error, ...) __attribute__((format(PRINTF,1,2), noreturn));
void Sys_FatalError (const char *error, ...) __attribute__((format(PRINTF,1,2), noreturn));
void Sys_Quit (void) __attribute__((noreturn));
void Sys_Shutdown (void);
void Sys_RegisterShutdown (void (*func) (void *), void *data);
int64_t Sys_StartTime (void) __attribute__ ((pure));
int64_t Sys_LongTime (void);
double Sys_DoubleTime (void);
int64_t Sys_TimeBase (void) __attribute__ ((const));
double Sys_DoubleTimeBase (void) __attribute__ ((const));
void Sys_TimeOfDay(date_t *date);

#define SYS_DEVELOPER(developer) SYS_DeveloperID_##developer,
enum {
#include "QF/sys_developer.h"
};

// bit 0 so developer 1 will pick it up
#define SYS_DEVELOPER(developer) \
	SYS_##developer = (SYS_dev | (1 << (SYS_DeveloperID_##developer + 1))),
typedef enum {
	SYS_dev = 1,
#include "QF/sys_developer.h"
} sys_developer_e;

void Sys_MaskPrintf (sys_developer_e mask, const char *fmt, ...) __attribute__((format(PRINTF,2,3)));

struct qf_fd_set;
int Sys_Select (int maxfd, struct qf_fd_set *fdset, int64_t usec);
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
void Sys_PageIn (void *ptr, size_t size);
size_t Sys_PageSize (void);
void *Sys_Alloc (size_t size);
void Sys_Free (void *mem, size_t size);
int Sys_LockMemory (void *mem, size_t size);
size_t Sys_PeakRSS (void);
size_t Sys_CurrentRSS (void);

int Sys_ProcessorCount (void);

//
// system IO
//
void Sys_DebugLog(const char *file, const char *fmt, ...) __attribute__((format(PRINTF,2,3)));

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

/** Open a newly created file with a guaranteed unique name.

	Uniqueness is guaranteed by adding a numeric sequence between the \a
	prefix and \a suffix, with a minium of \a mindigits numeric characters
	(with any required leading 0s to expand the number to \a mindigits).

	The created file has read and write permissions as modified by the OS,
	and the handle can be bothe written and read.

	\param name     dstring into which the name will be generated. Any
					existing contents will be lost. If an error occurs,
					\a name will be set to the error string.
	\param prefix	This includes the path to the file and any file name
					prefix. The numeric sequence will be appended directly
					to the prefix with no directory separator.
	\param suffix	Optional tail to be appended after the numeric sequence,
					usually the file extension. A dot is not added
					automatically, it is up to the caller to supply one. NULL
					and an empty string are equivalent.
	\param mindigits	The minimum number of digits to include in the
					generated file name. The sequence number will be padded
					with 0s in order to meet this menimum. Overflow will
					simply produce longer numeric sequence sub-strings.
	\return			File handle to the newly created file, or a negative
					value if an error occured (the negative error code).
					Suitable for use with read, write, fdopen, Qdopen, etc.
	\note	It is the caller's responsibility to close the file.
*/
int Sys_UniqueFile (struct dstring_s *name, const char *prefix,
					const char *suffix, int mindigits);

typedef intptr_t sys_jmpbuf[5];
#define Sys_setjmp(jmpbuf) __builtin_setjmp(jmpbuf)
void Sys_longjmp (sys_jmpbuf jmpbuf) __attribute__((noreturn));

extern sys_jmpbuf sys_exit_jmpbuf;

///@}

#endif//__QF_sys_h
