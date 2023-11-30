/*
	sys.c

	virtual filesystem functions

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#ifdef HAVE_CONIO_H
# include <conio.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_WINDOWS_H
# include "winquake.h"
# include "shlobj.h"
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifdef HAVE_PWD_H
# include <pwd.h>
#endif

#include <inttypes.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <unwind.h>

#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif

#include "qfalloca.h"

#include "QF/alloc.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/msg.h"
#include "QF/sys.h"
#include "QF/quakefs.h"
#include "QF/va.h"

#include "compat.h"
#define IMPLEMENT_QFSELECT_Funcs
#include "qfselect.h"

static void Sys_StdPrintf (const char *fmt, va_list args) __attribute__((format(PRINTF, 1, 0)));
static void Sys_ErrPrintf (const char *fmt, va_list args) __attribute__((format(PRINTF, 1, 0)));

VISIBLE int sys_nostdout;
static cvar_t sys_nostdout_cvar = {
	.name = "sys_nostdout",
	.description =
		"Set to disable std out",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sys_nostdout },
};
VISIBLE int sys_extrasleep;
static cvar_t sys_extrasleep_cvar = {
	.name = "sys_extrasleep",
	.description =
		"Set to cause whatever amount delay in microseconds you want. Mostly "
		"useful to generate simulated bad connections.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sys_extrasleep },
};
int sys_dead_sleep;
static cvar_t sys_dead_sleep_cvar = {
	.name = "sys_dead_sleep",
	.description =
		"When set, the server gets NO cpu if no clients are connected and "
		"there's no other activity. *MIGHT* cause problems with some mods.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sys_dead_sleep },
};
int sys_sleep;
static cvar_t sys_sleep_cvar = {
	.name = "sys_sleep",
	.description =
		"Sleep how long in seconds between checking for connections. Minimum "
		"is 0, maximum is 13",
	.default_value = "8",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &sys_sleep },
};

int         sys_checksum;

static __thread sys_printf_t sys_std_printf_function = Sys_StdPrintf;
static __thread sys_printf_t sys_err_printf_function = Sys_ErrPrintf;

typedef struct shutdown_list_s {
	struct shutdown_list_s *next;
	void      (*func) (void *);
	void       *data;
} shutdown_list_t;

typedef struct error_handler_s {
	struct error_handler_s *next;
	sys_error_t func;
	void       *data;
} error_handler_t;

static __thread shutdown_list_t *shutdown_list;

static __thread error_handler_t *error_handler_freelist;
static __thread error_handler_t *error_handler;

#ifndef _WIN32
static int  do_stdin = 1;
bool        stdin_ready;
#endif

/* The translation table between the graphical font and plain ASCII  --KB */
VISIBLE bool sys_quake_encoding;
VISIBLE const char sys_char_map[256] = {
      0, '#', '#', '#', '#', '.', '#', '#',
	'#',   9,  10, '#', ' ',  13, '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&','\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[','\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',

	'<', '=', '>', '#', '#', '.', '#', '#',
	'#', '#', ' ', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&','\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[','\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

#ifndef USE_INTEL_ASM
void
Sys_MaskFPUExceptions (void)
{
}
#endif

int
Sys_isdir (const char *path)
{
	int         res;
#ifdef _WIN32
	struct _stat st;
	res = _stat (path, &st);
#else
	struct stat st;
	res = stat (path, &st);
#endif
	if (res < 0) {
		// assume any error means path does not refer to a directory. certainly
		// true if errno == ENOENT
		return 0;
	}
	return S_ISDIR (st.st_mode);
}

int
Sys_mkdir (const char *path)
{
#ifdef HAVE_MKDIR
# ifdef _WIN32
	if (mkdir (path) == 0)
		return 0;
# else
	if (mkdir (path, 0777) == 0)
		return 0;
# endif
#else
# ifdef HAVE__MKDIR
	if (_mkdir (path) == 0)
		return 0;
# else
#  error do not know how to make directories
# endif
#endif
	return -1;
}

VISIBLE int
Sys_FileExists (const char *path)
{
#ifdef HAVE_ACCESS
	if (access (path, R_OK) == 0)
		return 0;
#else
# ifdef HAVE__ACCESS
	if (_access (path, R_OK) == 0)
		return 0;
# else
	int         fd;

	if ((fd = open (path, O_RDONLY)) >= 0) {
		close (fd);
		return 0;
	}
# endif
#endif
	return -1;
}

/*
	Sys_SetPrintf

	for want of a better name, but it sets the function pointer for the
	actual implementation of Sys_Printf.
*/
VISIBLE sys_printf_t
Sys_SetStdPrintf (sys_printf_t func)
{
	sys_printf_t prev = sys_std_printf_function;
	if (!func) {
		func = Sys_StdPrintf;
	}
	sys_std_printf_function = func;
	return prev;
}

VISIBLE sys_printf_t
Sys_SetErrPrintf (sys_printf_t func)
{
	sys_printf_t prev = sys_err_printf_function;
	if (!func) {
		func = Sys_ErrPrintf;
	}
	sys_err_printf_function = func;
	return prev;
}

static __thread dstring_t *sys_print_msg;
void
Sys_Print (FILE *stream, const char *fmt, va_list args)
{
	unsigned char *p;

	if (!sys_print_msg)
		sys_print_msg = dstring_new ();

	dvsprintf (sys_print_msg, fmt, args);

	if (stream == stderr) {
#ifdef _WIN32
		MessageBox (NULL, sys_print_msg->str, "Fatal Error", 0 /* MB_OK */ );
#endif
		fputs ("Fatal Error: ", stream);
	}

	if (sys_quake_encoding) {
		/* translate to ASCII instead of printing [xx]  --KB */
		for (p = (unsigned char *) sys_print_msg->str; *p; p++)
			putc (sys_char_map[*p], stream);
	} else {
		fputs (sys_print_msg->str, stream);
	}

	if (stream == stderr) {
		fputs ("\n", stream);
	}
	fflush (stream);
}

static void
Sys_StdPrintf (const char *fmt, va_list args)
{
	if (sys_nostdout)
		return;
	Sys_Print (stdout, fmt, args);
}

static void
Sys_ErrPrintf (const char *fmt, va_list args)
{
	Sys_Print (stderr, fmt, args);
}

VISIBLE void
Sys_Printf (const char *fmt, ...)
{
	va_list     args;
	va_start (args, fmt);
	sys_std_printf_function (fmt, args);
	va_end (args);
}

VISIBLE void
Sys_MaskPrintf (sys_developer_e mask, const char *fmt, ...)
{
	va_list     args;

	if (!(developer & mask))
		return;
	va_start (args, fmt);
	sys_std_printf_function (fmt, args);
	va_end (args);
}

static int64_t sys_starttime = -1;

VISIBLE int64_t
Sys_StartTime (void)
{
	return sys_starttime;
}

VISIBLE int64_t
Sys_LongTime (void)
{
	static bool first = true;
#ifdef _WIN32
# if 0
	static DWORD starttime;
	DWORD       now;

	now = timeGetTime ();

	if (first) {
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime)				// wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
# else
	// MH's solution combining timeGetTime for stability and
	// QueryPerformanceCounter for precision.
	static int64_t qpcfreq = 0;
	static int64_t currqpccount = 0;
	static int64_t lastqpccount = 0;
	static int64_t qpcfudge = 0;
	int64_t currtime = 0;
	static int64_t lasttime = 0;

	if (first) {
		timeBeginPeriod (1);
		sys_starttime = lasttime = timeGetTime () * 1000;
		QueryPerformanceFrequency ((LARGE_INTEGER *) &qpcfreq);
		QueryPerformanceCounter ((LARGE_INTEGER *) &lastqpccount);
		first = false;
		return 0;
	}

	// get the current time from both counters
	currtime = timeGetTime () * 1000;
    QueryPerformanceCounter ((LARGE_INTEGER *) &currqpccount);

	if (currtime != lasttime)  {
		// requery the frequency in case it changes (which it can on multicore
		// machines)
		QueryPerformanceFrequency ((LARGE_INTEGER *) &qpcfreq);

		// store back times and calc a fudge factor as timeGetTime can
		// overshoot on a sub-millisecond scale
		qpcfudge = (( (currqpccount - lastqpccount) * 1000000 / qpcfreq))
				- (currtime - lasttime);
		lastqpccount = currqpccount;
		lasttime = currtime;
	} else {
		qpcfudge = 0;
	}

	// the final time is the base from timeGetTime plus an addition from QPC
	return ((currtime - sys_starttime)
			+ ((currqpccount - lastqpccount) * 1000000 / qpcfreq) + qpcfudge);
# endif
#else
	int64_t now;
#ifdef CLOCK_BOOTTIME
	struct timespec tp;

	clock_gettime (CLOCK_BOOTTIME, &tp);

	now = tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
#else
	struct timeval tp;
	struct timezone tzp;

	gettimeofday (&tp, &tzp);

	now = tp.tv_sec * 1000000 + tp.tv_usec;
#endif

	if (first) {
		first = false;
		sys_starttime = now;
	}

	return now - sys_starttime;
#endif
}

VISIBLE int64_t
Sys_TimeBase (void)
{
	return INT64_C (4294967296000000);
}

VISIBLE double
Sys_DoubleTime (void)
{
	return (Sys_TimeBase () + Sys_LongTime ()) / 1e6;
}

VISIBLE double
Sys_DoubleTimeBase (void)
{
	return Sys_TimeBase () / 1e6;
}

VISIBLE void
Sys_TimeOfDay (date_t *date)
{
	struct tm  *newtime;
	time_t      long_time;

	time (&long_time);
	newtime = localtime (&long_time);

	date->day = newtime->tm_mday;
	date->mon = newtime->tm_mon;
	date->year = newtime->tm_year + 1900;
	date->hour = newtime->tm_hour;
	date->min = newtime->tm_min;
	date->sec = newtime->tm_sec;
	strftime (date->str, 128, "%a %b %d, %H:%M %Y", newtime);
}

VISIBLE void
Sys_MakeCodeWriteable (uintptr_t startaddr, size_t length)
{
#ifdef _WIN32
	DWORD       flOldProtect;

	if (!VirtualProtect
		((LPVOID) startaddr, length, PAGE_EXECUTE_READWRITE, &flOldProtect))
		Sys_Error ("Protection change failed");
#else
# ifdef HAVE_MPROTECT
	int         r;
	long        psize = Sys_PageSize ();
	unsigned long endaddr = startaddr + length;
	startaddr &= ~(psize - 1);
	endaddr = (endaddr + psize - 1) & ~(psize - 1);
	// systems with mprotect but not getpagesize (or similar) probably don't
	// need to page align the arguments to mprotect (eg, QNX)
	r = mprotect ((char *) startaddr, endaddr - startaddr,
				  PROT_EXEC | PROT_READ | PROT_WRITE);

	if (r < 0)
		Sys_Error ("Protection change failed");
# endif
#endif
}

VISIBLE void
Sys_Init_Cvars (void)
{
	Cvar_Register (&sys_nostdout_cvar, 0, 0);
	Cvar_Register (&sys_extrasleep_cvar, 0, 0);
	Cvar_Register (&sys_dead_sleep_cvar, 0, 0);
	Cvar_Register (&sys_sleep_cvar, 0, 0);
}

VISIBLE void
Sys_Quit (void)
{
	Sys_Shutdown ();

	exit (0);
}

VISIBLE void
Sys_PushErrorHandler (sys_error_t func, void *data)
{
	error_handler_t *eh;
	if (error_handler_freelist) {
		eh = error_handler_freelist;
	} else {
		eh = malloc (sizeof (error_handler_t));
		eh->next = 0;
	}
	eh->func = func;
	eh->data = data;
	error_handler_freelist = eh->next;
	eh->next = error_handler;
	error_handler = eh;
}

VISIBLE void
Sys_PopErrorHandler (void)
{
	error_handler_t *eh;

	if (!error_handler) {
		Sys_Error ("Sys_PopErrorHandler: no handler to pop");
	}
	eh = error_handler;
	error_handler = eh->next;
	eh->next = error_handler_freelist;
	error_handler_freelist = eh;
}


VISIBLE void
Sys_Error (const char *error, ...)
{
	va_list     args;
	va_list     tmp_args;
	static int  in_sys_error = 0;

	if (in_sys_error) {
		ssize_t     cnt;
		const char *msg = "\nSys_Error: recursive error condition\n";
		cnt = write (2, msg, strlen (msg));
		if (cnt < 0)
			perror ("write failed");
		abort ();
	}
	in_sys_error = 1;

	va_start (args, error);
	va_copy (tmp_args, args);
	sys_err_printf_function (error, args);
	va_end (args);

	if (error_handler) {
		error_handler->func (error_handler->data);
	}

	Sys_Shutdown ();

	if (sys_err_printf_function != Sys_ErrPrintf) {
		// print the message again using the default error printer to increase
		// the chances of the error being seen.
		va_copy (args, tmp_args);
		Sys_ErrPrintf (error, args);
	}

	exit (1);
}

VISIBLE void
Sys_RegisterShutdown (void (*func) (void *), void *data)
{
	shutdown_list_t *p;
	if (!func)
		return;
	p = malloc (sizeof (*p));
	if (!p)
		Sys_Error ("Sys_RegisterShutdown: insufficient memory");
	p->func = func;
	p->data = data;
	p->next = shutdown_list;
	shutdown_list = p;
}

VISIBLE int
Sys_TimeID (void) //FIXME I need a new name, one that doesn't make me feel 3 feet thick
{
	int val;
#ifdef HAVE_GETUID
	val = ((int) (getpid () + getuid () * 1000) * time (NULL)) & 0xffff;
#else
	val = ((int) (Sys_DoubleTime () * 1000) * time (NULL)) & 0xffff;
#endif
	return val;
}

VISIBLE void
Sys_PageIn (void *ptr, size_t size)
{
//may or may not be useful in linux #ifdef _WIN32
	byte       *x;
	size_t      m, n;

	// touch all the memory to make sure it's there. The 16-page skip is to
	// keep Win 95 from thinking we're trying to page ourselves in (we are
	// doing that, of course, but there's no reason we shouldn't)
	x = (byte *) ptr;

	for (n = 0; n < 4; n++) {
		for (m = 0; m < (size - 16 * 0x1000); m += 4) {
			sys_checksum += *(int *) &x[m];
			sys_checksum += *(int *) &x[m + 16 * 0x1000];
		}
	}
//#endif
}

#if defined(_WIN32) && !defined(_WIN64)
// this is a hack to make memory allocations 16-byte aligned on 32-bit
// systems (in particular for this case, windows) as vectors and
// matrices require 16-byte alignment but system malloc (etc) provide only
// 8-byte alignment.
void *__cdecl
calloc (size_t nume, size_t sizee)
{
	size_t size = nume * sizee;
	void *mem = _aligned_malloc (size, 16);
	memset (mem, 0, size);
	return mem;
}

void __cdecl
free (void *mem)
{
	_aligned_free (mem);
}

void *__cdecl
malloc (size_t size)
{
	return _aligned_malloc (size, 16);
}

void *__cdecl
realloc (void *mem, size_t size)
{
	return _aligned_realloc (mem, size, 16);
}

char *__cdecl
strdup(const char *src)
{
	size_t      len = strlen (src);
	char       *dup = malloc (len + 1);
	strcpy (dup, src);
	return dup;
}
#endif

VISIBLE size_t
Sys_PageSize (void)
{
#ifdef _WIN32
	SYSTEM_INFO si;
	GetSystemInfo (&si);
	return si.dwPageSize;
#else
# ifdef HAVE__SC_PAGESIZE
	return sysconf (_SC_PAGESIZE);
# else
#  ifdef HAVE_GETPAGESIZE
	return getpagesize ();
#  endif
# endif
#endif
}

VISIBLE void *
Sys_Alloc (size_t size)
{
	size_t      page_size = Sys_PageSize ();
	size_t      page_mask = page_size - 1;
	size = (size + page_mask) & ~page_mask;
#ifdef _WIN32
	return VirtualAlloc (0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	return mmap (0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
				 -1, 0);
#endif
}

VISIBLE void
Sys_Free (void *mem, size_t size)
{
	size_t      page_size = Sys_PageSize ();
	size_t      page_mask = page_size - 1;
	size = (size + page_mask) & ~page_mask;
#ifdef _WIN32
	VirtualFree (mem, 0, MEM_RELEASE | MEM_DECOMMIT);
#else
	munmap (mem, size);
#endif
}

VISIBLE int
Sys_LockMemory (void *mem, size_t size)
{
	size_t      page_size = Sys_PageSize ();
	size_t      page_mask = page_size - 1;
	size = (size + page_mask) & ~page_mask;
#ifdef _WIN32
	return VirtualLock (mem, size) != 0;
#else
	return mlock (mem, size) == 0;
#endif
}

VISIBLE int
Sys_ProcessorCount (void)
{
	int         cpus = 1;
#if defined (_SC_NPROCESSORS_ONLN)
	cpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined (_WIN32)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	cpus = sysinfo.dwNumberOfProcessors;
#endif
	if (cpus < 1) {
		cpus = 1;
	}
	return cpus;
}

static __thread dstring_t *sys_debuglog_data;
VISIBLE void
Sys_DebugLog (const char *file, const char *fmt, ...)
{
	va_list     args;
	int         fd;

	if (!sys_debuglog_data)
		sys_debuglog_data = dstring_newstr ();

	va_start (args, fmt);
	dvsprintf (sys_debuglog_data, fmt, args);
	va_end (args);
	if ((fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644)) >= 0) {
		if (write (fd, sys_debuglog_data->str, sys_debuglog_data->size - 1)
			!= (ssize_t) (sys_debuglog_data->size - 1))
			Sys_Printf ("Error writing %s: %s\n", file, strerror(errno));
		close (fd);
	}
}

VISIBLE int
Sys_Select (int maxfd, qf_fd_set *fdset, int64_t usec)
{
	struct timeval _timeout;
	struct timeval *timeout = 0;

	if (usec >= 0) {
		timeout = &_timeout;
		if (usec < 1000000) {
			_timeout.tv_sec = 0;
			_timeout.tv_usec = usec;
		} else {
			_timeout.tv_sec = usec / 1000000;
			_timeout.tv_usec = usec % 1000000;
		}
	}

	return select (maxfd + 1, &fdset->fdset, NULL, NULL, timeout);
}

VISIBLE int
Sys_CheckInput (int idle, int net_socket)
{
	qf_fd_set   fdset;
	int         res;
	int64_t     usec;

#ifdef _WIN32
	int         sleep_msec;
	// Now we want to give some processing time to other applications,
	// such as qw_client, running on this machine.
	sleep_msec = sys_sleep;
	if (sleep_msec > 0) {
		if (sleep_msec > 13)
			sleep_msec = 13;
		Sleep (sleep_msec);
	}

	usec = net_socket < 0 ? 0 : 20;
#else
	usec = net_socket < 0 ? 0 : 2000;
#endif
	// select on the net socket and stdin
	// the only reason we have a timeout at all is so that if the last
	// connected client times out, the message would not otherwise
	// be printed until the next event.
	QF_FD_ZERO (&fdset);
#ifndef _WIN32
	if (do_stdin)
		QF_FD_SET (0, &fdset);
#endif
	if (net_socket >= 0)
		QF_FD_SET (((unsigned) net_socket), &fdset);// cast needed for windows

	if (idle && sys_dead_sleep)
		usec = -1;

	res = Sys_Select (max (net_socket, 0), &fdset, usec);
	if (res == 0 || res == -1)
		return 0;
#ifndef _WIN32
	stdin_ready = QF_FD_ISSET (0, &fdset);
#endif
	return 1;
}

/*
	Sys_ConsoleInput

	Checks for a complete line of text typed in at the console, then forwards
	it to the host command processor
*/
VISIBLE const char *
Sys_ConsoleInput (void)
{
	static char text[256];
	static int len = 0;

#ifdef _WIN32
	int         c;

	// read a line out
	while (_kbhit ()) {
		c = _getch ();
		putch (c);
		if (c == '\r') {
			text[len] = 0;
			putch ('\n');
			len = 0;
			return text;
		}
		if (c == 8) {
			if (len) {
				putch (' ');
				putch (c);
				len--;
				text[len] = 0;
			}
			continue;
		}
		text[len] = c;
		len++;
		if (len < (int) sizeof (text))
			text[len] = 0;
		else {
		// buffer is full
			len = 0;
			text[sizeof (text) - 1] = 0;
			return text;
		}
	}

	return NULL;
#else
	if (!stdin_ready || !do_stdin)
		return NULL;					// the select didn't say it was ready
	stdin_ready = false;

	len = read (0, text, sizeof (text));
	if (len == 0) {
		// end of file
		do_stdin = 0;
		return NULL;
	}
	if (len < 1)
		return NULL;
	text[len - 1] = 0;					// rip off the \n and terminate

	return text;
#endif
}

#ifdef _WIN32
static __thread jmp_buf  aiee_abort;
#else
static __thread sigjmp_buf aiee_abort;
#endif

typedef struct sh_stack_s {
	struct sh_stack_s *next;
	int (*signal_hook)(int,void*);
	void *data;
} sh_stack_t;

static __thread sh_stack_t *sh_stack;
static __thread sh_stack_t *free_sh;
static __thread int (*signal_hook)(int,void*);
static __thread void *signal_hook_data;

VISIBLE void
Sys_PushSignalHook (int (*hook)(int, void *), void *data)
{
	sh_stack_t *s;

	if (free_sh) {
		s = free_sh;
	} else {
		s = malloc (sizeof (sh_stack_t));
		s->next = 0;
	}
	s->signal_hook = signal_hook;
	s->data = signal_hook_data;
	signal_hook = hook;
	signal_hook_data = data;

	free_sh = s->next;
	s->next = sh_stack;
	sh_stack = s;
}

VISIBLE void
Sys_PopSignalHook (void)
{
	if (sh_stack) {
		sh_stack_t *s;

		signal_hook = sh_stack->signal_hook;
		signal_hook_data = sh_stack->data;

		s = sh_stack->next;
		sh_stack->next = free_sh;
		free_sh = sh_stack;
		sh_stack = s;
	}
}

static void __attribute__((noreturn))
aiee (int sig)
{
	printf ("AIEE, signal %d in shutdown code, giving up\n", sig);
#ifdef _WIN32
	longjmp (aiee_abort, 1);
#else
	siglongjmp (aiee_abort, 1);
#endif
}

#ifdef _WIN32
static void
signal_handler (int sig)
{
	int volatile recover = 0;	// volatile for longjump
	static volatile int in_signal_handler = 0;

	if (in_signal_handler) {
		aiee (sig);
	}
	printf ("Received signal %d, exiting...\n", sig);

	switch (sig) {
		case SIGINT:
		case SIGTERM:
			signal (SIGINT,  SIG_DFL);
			signal (SIGTERM, SIG_DFL);
			exit(1);
		default:
			if (!setjmp (aiee_abort)) {
				if (signal_hook)
					recover = signal_hook (sig, signal_hook_data);
				Sys_Shutdown ();
			}

			if (!recover) {
				signal (SIGILL,  SIG_DFL);
				signal (SIGSEGV, SIG_DFL);
				signal (SIGFPE,  SIG_DFL);
			}
	}
}

static void
hook_signals (void)
{
	// catch signals
	signal (SIGINT,  signal_handler);
	signal (SIGILL,  signal_handler);
	signal (SIGSEGV, signal_handler);
	signal (SIGTERM, signal_handler);
	signal (SIGFPE,  signal_handler);
}
#else

static __thread struct sigaction save_hup;
static __thread struct sigaction save_quit;
static __thread struct sigaction save_trap;
static __thread struct sigaction save_iot;
static __thread struct sigaction save_bus;
static __thread struct sigaction save_int;
static __thread struct sigaction save_ill;
static __thread struct sigaction save_segv;
static __thread struct sigaction save_term;
static __thread struct sigaction save_fpe;

static void
signal_handler (int sig, siginfo_t *info, void *ucontext)
{
	int volatile recover = 0;	// volatile for longjump
	static volatile int in_signal_handler = 0;

	if (in_signal_handler) {
		aiee (sig);
	}
	printf ("Received signal %d, exiting...\n", sig);

	switch (sig) {
		case SIGINT:
		case SIGTERM:
		case SIGHUP:
		case SIGQUIT:
			sigaction (SIGHUP,  &save_hup,  0);
			sigaction (SIGINT,  &save_int,  0);
			sigaction (SIGTERM, &save_term, 0);
			sigaction (SIGQUIT, &save_quit, 0);
			exit(1);
		default:
			if (!sigsetjmp (aiee_abort, 1)) {
				if (signal_hook)
					recover = signal_hook (sig, signal_hook_data);
				Sys_Shutdown ();
			}

			if (!recover) {
				sigaction (SIGTRAP, &save_trap, 0);
				sigaction (SIGIOT,  &save_iot,  0);
				sigaction (SIGBUS,  &save_bus,  0);
				sigaction (SIGILL,  &save_ill,  0);
				sigaction (SIGSEGV, &save_segv, 0);
				sigaction (SIGFPE,  &save_fpe,  0);
			}
	}
}

static void
hook_signals (void)
{
	// catch signals
	struct sigaction action = {};
	action.sa_flags = SA_SIGINFO;
	action.sa_sigaction = signal_handler;
#ifndef _WIN32
	sigaction (SIGHUP,  &action, &save_hup);
	sigaction (SIGQUIT, &action, &save_quit);
	sigaction (SIGTRAP, &action, &save_trap);
	sigaction (SIGIOT,  &action, &save_iot);
	sigaction (SIGBUS,  &action, &save_bus);
#endif
	sigaction (SIGINT,  &action, &save_int);
	sigaction (SIGILL,  &action, &save_ill);
	sigaction (SIGSEGV, &action, &save_segv);
	sigaction (SIGTERM, &action, &save_term);
	sigaction (SIGFPE,  &action, &save_fpe);
}
#endif

VISIBLE void
Sys_Init (void)
{
	hook_signals ();

	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Cvar_Init ();
	Sys_Init_Cvars ();

	Cmd_Init ();
}

VISIBLE int
Sys_CreatePath (const char *path)
{
	char       *ofs;
	char       *e_path = alloca (strlen (path) + 1);

	strcpy (e_path, path);
	for (ofs = e_path + 1; *ofs; ofs++) {
		if (*ofs == '/') {
			*ofs = 0;
			if (!Sys_isdir (e_path)) {
				// create the directory
				if (Sys_mkdir (e_path) == -1)
					return -1;
			}
			*ofs = '/';
		}
	}
	return 0;
}

char *
Sys_ExpandSquiggle (const char *path)
{
	const char *home = 0;
#ifdef _WIN32
	char        userpath[MAX_PATH];	// sigh, why can't windows give the size?
#else
# ifdef HAVE_GETUID
	struct passwd *pwd_ent;
# endif
#endif

	if (strncmp (path, "~/", 2) != 0) {
		return strdup (path);
	}

#ifdef _WIN32
	if (SHGetFolderPathA (0, CSIDL_LOCAL_APPDATA, 0, 0, userpath) == S_OK) {
		home = userpath;
	}
	// LordHavoc: first check HOME to duplicate previous version behavior
	// (also handy if someone wants it elsewhere than their windows directory)
	if (!home)
		home = getenv ("HOME");
	if (!home || !home[0])
		home = getenv ("USERPROFILE");
	if (!home || !home[0])
		home = 0;
#else
# ifdef HAVE_GETUID
	if ((pwd_ent = getpwuid (getuid ()))) {
		home = pwd_ent->pw_dir;
	} else
		home = getenv ("HOME");
# endif
#endif

	if (!home)
		home = ".";
	return nva ("%s%s", home, path + 1);	// skip leading ~
}

VISIBLE int
Sys_UniqueFile (dstring_t *name, const char *prefix, const char *suffix,
				int mindigits)
{
	const int   flags = O_CREAT | O_EXCL | O_RDWR;
	const int   mode = 0644;
	int64_t     seq = 0;	// it should take a while to run out

	if (!suffix) {
		suffix = "";
	}
	while (1) {
		dsprintf (name, "%s%0*"PRIi64"%s", prefix, mindigits, seq, suffix);
		int         fd = open (name->str, flags, mode);
		if (fd >= 0) {
			return fd;
		}
		int         err = errno;
		if (err != EEXIST) {
			dsprintf (name, "%s", strerror (err));
			return -err;
		}
		seq++;
	}
}

void
Sys_Shutdown (void)
{
	shutdown_list_t *t;

	while (shutdown_list) {
		void      (*func) (void *) = shutdown_list->func;
		void       *data = shutdown_list->data;
		t = shutdown_list;
		shutdown_list = shutdown_list->next;
		free (t);

		func (data);
	}
	while (free_sh) {
		__auto_type t = free_sh->next;
		free (free_sh);
		free_sh = t;
	}
	while (sh_stack) {
		__auto_type t = sh_stack->next;
		free (sh_stack);
		sh_stack = t;
	}
	while (error_handler_freelist) {
		__auto_type t = error_handler_freelist->next;
		free (error_handler_freelist);
		error_handler_freelist = t;
	}
	while (error_handler) {
		__auto_type t = error_handler->next;
		free (error_handler);
		error_handler = t;
	}
	if (sys_print_msg) {
		dstring_delete (sys_print_msg);
		sys_print_msg = 0;
	}
	if (sys_debuglog_data) {
		dstring_delete (sys_debuglog_data);
		sys_debuglog_data = 0;
	}
}

#ifndef _WIN32
#define DW_EH_PE_absptr         0x00
#define DW_EH_PE_omit           0xff

#define DW_EH_PE_uleb128        0x01
#define DW_EH_PE_udata2         0x02
#define DW_EH_PE_udata4         0x03
#define DW_EH_PE_udata8         0x04
#define DW_EH_PE_sleb128        0x09
#define DW_EH_PE_sdata2         0x0A
#define DW_EH_PE_sdata4         0x0B
#define DW_EH_PE_sdata8         0x0C
#define DW_EH_PE_signed         0x08

#define DW_EH_PE_pcrel          0x10
#define DW_EH_PE_textrel        0x20
#define DW_EH_PE_datarel        0x30
#define DW_EH_PE_funcrel        0x40
#define DW_EH_PE_aligned        0x50


typedef struct {
	uintptr_t   start;
	uintptr_t   lpstart;
	uintptr_t   ttype_base;
	const byte *ttype;
	const byte *action_table;
	byte        ttype_encoding;
	byte        cs_encoding;
} lsd_header_t;

static uintptr_t
read_value (byte enc, qmsg_t *msg)
{
	if (enc == DW_EH_PE_aligned) {
		Sys_Error ("unexpected DW_EH_PE_aligned\n");
	}
	uintptr_t res = 0;
	switch (enc & 0x0f) {
		case DW_EH_PE_uleb128:
			res = MSG_ReadUleb128 (msg);
			break;
		case DW_EH_PE_sleb128:
			res = MSG_ReadSleb128 (msg);
			break;
		default:
			Sys_Error ("unexpected dwarf encoding: %d\n", enc);
			break;
	}
	if (res && enc & 0xf0) {
		Sys_Error ("unexpected dwarf encoding: %d\n", enc);
	}
	return res;
}

static lsd_header_t
read_lsd_header (qmsg_t *msg, uintptr_t start)
{
	lsd_header_t hdr;
	uintptr_t   tmp;
	byte        enc = MSG_ReadByte (msg);
	hdr.start = start;
	if (enc == DW_EH_PE_omit) {
		hdr.lpstart = start;
	} else {
		hdr.lpstart = read_value (enc, msg);
	}
	hdr.ttype_encoding = MSG_ReadByte (msg);
	if (hdr.ttype_encoding == DW_EH_PE_omit) {
		hdr.ttype = 0;
	} else {
		tmp = MSG_ReadUleb128 (msg);
		hdr.ttype = msg->message->data + msg->readcount + tmp;
	}
	hdr.cs_encoding = MSG_ReadByte (msg);;
	tmp = MSG_ReadUleb128 (msg);
	hdr.action_table = msg->message->data + msg->readcount + tmp;
	return hdr;
}

#endif

static _Unwind_Reason_Code
sys_stop (int version, _Unwind_Action actions,
		  _Unwind_Exception_Class excls,
		  struct _Unwind_Exception *exobj,
		  struct _Unwind_Context *context, void *_jmpbuf)
{
	if (version != 1) {
		Sys_Error ("unknown unwind version");
	}
	intptr_t   *jmpbuf = _jmpbuf;
#ifdef _WIN32
	if (!context) {
		// if we get here, then attempting to delete the exception contextuuu
		// can fail
		__builtin_longjmp (jmpbuf, 1);
	}
	uintptr_t   target_cfa = jmpbuf[2];
	uintptr_t   cfa = _Unwind_GetCFA (context);
	if (target_cfa < cfa) {
		// we've gone past the target frame (there were no intervening
		// cleanup points) so just jump out to avoid cleaning up something
		// we shouldn't touch
		__builtin_longjmp (jmpbuf, 1);
	}
	// These are not meant to be read, let alone written, but this is
	// the only way to communicate the target frame/ip to RtlUnwindEx
	exobj->private_[1] = target_cfa;
	exobj->private_[2] = jmpbuf[1];	// target ip
	return _URC_NO_REASON;
#else
	if ((actions & _UA_END_OF_STACK)) {
		Sys_Error ("Sys_longjmp called outside a Sys_setjmp context");
	}
	uintptr_t   target_cfa = jmpbuf[2];
	uintptr_t   cfa = _Unwind_GetCFA (context);
	int         no_dec_ip;
	uintptr_t   ip = _Unwind_GetIPInfo (context, &no_dec_ip);
	if (!ip) {
		Sys_Error ("null ip\n");
	}
	if (cfa != target_cfa) {
		return _URC_NO_REASON;
	}
	ip -= !no_dec_ip;
	byte       *lsd = _Unwind_GetLanguageSpecificData (context);
	if (lsd) {
		sizebuf_t   message = { .data = lsd, .cursize = -1 };
		qmsg_t      msg = { .message = &message };
		auto        region_start = _Unwind_GetRegionStart (context);
		lsd_header_t hdr = read_lsd_header (&msg, region_start);
		message.cursize = hdr.action_table - message.data;
		bool match = false;
		bool cleanup = false;
		while (!match && msg.readcount < message.cursize) {
			auto cs_start = read_value (hdr.cs_encoding, &msg);
			auto cs_len = read_value (hdr.cs_encoding, &msg);
			auto cs_lp = read_value (hdr.cs_encoding, &msg);
			/*auto cs_action =*/ read_value (hdr.cs_encoding, &msg);
			cs_start += hdr.start;
			match = ip >= cs_start && ip < cs_start + cs_len;
			cleanup = cs_lp;
		}
		if (!match) {
			Sys_Error ("could not find ip in call site list");
		}
		if (cleanup) {
			// the call site has cleanup associated with it, so return to
			// let the unwind system deal with the cleanup (we'll get called
			// again with the same cfa)
			return _URC_NO_REASON;
		}
	}
	_Unwind_DeleteException (exobj);
	__builtin_longjmp (jmpbuf, 1);
#endif
}

void
Sys_longjmp (sys_jmpbuf jmpbuf)
{
	static struct _Unwind_Exception sys_exception;
	auto res = _Unwind_ForcedUnwind (&sys_exception, sys_stop, jmpbuf);
	Sys_Error ("_Unwind_ForcedUnwind returned %d", res);
}
