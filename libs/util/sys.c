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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/sys.h"
#include "QF/quakefs.h"

#include "compat.h"

static void Sys_StdPrintf (const char *fmt, va_list args);
static void Sys_ErrPrintf (const char *fmt, va_list args);

cvar_t     *sys_nostdout;
cvar_t     *sys_extrasleep;
cvar_t     *sys_dead_sleep;
cvar_t     *sys_sleep;

int         sys_checksum;

static sys_printf_t sys_std_printf_function = Sys_StdPrintf;
static sys_printf_t sys_err_printf_function = Sys_ErrPrintf;

typedef struct shutdown_list_s {
	struct shutdown_list_s *next;
	void (*func)(void);
} shutdown_list_t;

static shutdown_list_t *shutdown_list;

#ifndef _WIN32 
static int  do_stdin = 1;
qboolean    stdin_ready;
#endif

/* The translation table between the graphical font and plain ASCII  --KB */
const char sys_char_map[256] = {
	'\0', '#', '#', '#', '#', '.', '#', '#',
	'#', 9, 10, '#', ' ', 13, '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',

	'<', '=', '>', '#', '#', '.', '#', '#',
	'#', '#', ' ', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

#define MAXPRINTMSG 4096


#ifndef USE_INTEL_ASM
void
Sys_MaskFPUExceptions (void)
{
}
#endif

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
	if (errno != EEXIST)
		return -1;
	return 0;
}

int
Sys_FileTime (const char *path)
{
#ifdef HAVE_ACCESS
	if (access (path, R_OK) == 0)
		return 0;
#else
# ifdef HAVE__ACCESS
	if (_access (path, R_OK) == 0)
		return 0;
# else
#  error do not know how to check access
# endif
#endif
	return -1;
}

/*
	Sys_SetPrintf

	for want of a better name, but it sets the function pointer for the
	actual implementation of Sys_Printf.
*/
void
Sys_SetStdPrintf (sys_printf_t func)
{
	sys_std_printf_function = func;
}

void
Sys_SetErrPrintf (sys_printf_t func)
{
	sys_err_printf_function = func;
}

void
Sys_Print (FILE *stream, const char *fmt, va_list args)
{
	static dstring_t *msg;
	unsigned char *p;

	if (!msg)
		msg = dstring_new ();

	dvsprintf (msg, fmt, args);

	if (stream == stderr) {
#ifdef _WIN32
		MessageBox (NULL, msg->str, "Fatal Error", 0 /* MB_OK */ );
#endif
		fputs ("Fatal Error: ", stream);
	}

	/* translate to ASCII instead of printing [xx]  --KB */
	for (p = (unsigned char *) msg->str; *p; p++)
		putc (sys_char_map[*p], stream);

	if (stream == stderr) {
		fputs ("\n", stream);
	}
	fflush (stream);
}

static void
Sys_StdPrintf (const char *fmt, va_list args)
{
	if (sys_nostdout && sys_nostdout->int_val)
		return;
	Sys_Print (stdout, fmt, args);
}

static void
Sys_ErrPrintf (const char *fmt, va_list args)
{
	Sys_Print (stderr, fmt, args);
}

void
Sys_Printf (const char *fmt, ...)
{
	va_list     args;
	va_start (args, fmt);
	sys_std_printf_function (fmt, args);
	va_end (args);
}

void
Sys_DPrintf (const char *fmt, ...)
{
	va_list     args;

	if (!developer || !developer->int_val)
		return;
	va_start (args, fmt);
	sys_std_printf_function (fmt, args);
	va_end (args);
}

double
Sys_DoubleTime (void)
{
	static qboolean first = true;
#ifdef _WIN32
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
#else
	struct timeval tp;
	struct timezone tzp;
	double now;
	static double start_time;

	gettimeofday (&tp, &tzp);

	now = tp.tv_sec + tp.tv_usec / 1e6;

	if (first) {
		first = false;
		start_time = now;
	}

	return now - start_time;
#endif
}

void
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

void
Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
#ifdef _WIN32
	DWORD       flOldProtect;

	if (!VirtualProtect
		((LPVOID) startaddr, length, PAGE_READWRITE, &flOldProtect))
		Sys_Error ("Protection change failed");
#else
# ifdef HAVE_MPROTECT
	int         r;
	unsigned long endaddr = startaddr + length;
#  ifdef HAVE__SC_PAGESIZE
	long		psize = sysconf (_SC_PAGESIZE);

	startaddr &= ~(psize - 1);
	endaddr = (endaddr + psize - 1) & ~(psize -1);
#  else
#   ifdef HAVE_GETPAGESIZE
	int         psize = getpagesize ();

	startaddr &= ~(psize - 1);
	endaddr = (endaddr + psize - 1) & ~(psize - 1);
#   endif
#  endif
	// systems with mprotect but not getpagesize (or similar) probably don't
	// need to page align the arguments to mprotect (eg, QNX)
	r = mprotect ((char *) startaddr, endaddr - startaddr,
				  PROT_EXEC | PROT_READ | PROT_WRITE);

	if (r < 0)
		Sys_Error ("Protection change failed");
# endif
#endif
}

void
Sys_Init_Cvars (void)
{
	sys_nostdout = Cvar_Get ("sys_nostdout", "0", CVAR_NONE, NULL,
							 "Set to disable std out");
	sys_extrasleep = Cvar_Get ("sys_extrasleep", "0", CVAR_NONE, NULL,
							   "Set to cause whatever amount delay in "
							   "microseconds you want. Mostly "
							   "useful to generate simulated bad "
							   "connections.");
	sys_dead_sleep = Cvar_Get ("sys_dead_sleep", "0", CVAR_NONE, NULL,
							   "When set, the server gets NO cpu if no "
							   "clients are connected and there's no other "
							   "activity. *MIGHT* cause problems with some "
							   "mods.");
	sys_sleep = Cvar_Get ("sys_sleep", "8", CVAR_NONE, NULL, "Sleep how long "
						  "in seconds between checking for connections. "
						  "Minimum is 0, maximum is 13");
}

void
Sys_Shutdown (void)
{
	shutdown_list_t *p = shutdown_list, *t;

	while (p) {
		p->func ();
		t = p;
		p = p->next;
		free (t);
	}
}

void
Sys_Quit (void)
{
	Sys_Shutdown ();

	exit (0);
}

#if defined (HAVE_VA_COPY)
# define VA_COPY(a,b) va_copy (a, b)
#elif defined (HAVE__VA_COPY)
# define VA_COPY(a,b) __va_copy (a, b)
#else
# define VA_COPY(a,b) memcpy (a, b, sizeof (a))
#endif

void
Sys_Error (const char *error, ...)
{
	va_list     args;
#ifdef VA_LIST_IS_ARRAY
	va_list     tmp_args;
#endif

	va_start (args, error);
#ifdef VA_LIST_IS_ARRAY
	VA_COPY (tmp_args, args);
#endif
	sys_err_printf_function (error, args);
	va_end (args);

	Sys_Shutdown ();

	if (sys_err_printf_function != Sys_ErrPrintf) {
		// print the message again using the default error printer to increase
		// the chances of the error being seen.
#ifdef VA_LIST_IS_ARRAY
		VA_COPY (args, tmp_args);
#endif
		Sys_ErrPrintf (error, args);
	}

	exit (1);
}

void
Sys_RegisterShutdown (void (*func) (void))
{
	shutdown_list_t *p;
	if (!func)
		return;
	p = malloc (sizeof (*p));
	if (!p)
		Sys_Error ("Sys_RegisterShutdown: insufficient memory");
	p->func = func;
	p->next = shutdown_list;
	shutdown_list = p;
}

int
Sys_TimeID (void) //FIXME I need a new name, one that doesn't make me feel 3 feet thick
{
	int val;
#ifdef _WIN32
	val = ((int) (timeGetTime () * 1000) * time (NULL)) & 0xffff;
#else
	val = ((int) (getpid () + getuid () * 1000) * time (NULL)) & 0xffff;
#endif
	return val;
}

void
Sys_PageIn (void *ptr, int size)
{
//may or may not be useful in linux #ifdef _WIN32
	byte       *x;
	int         m, n;

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

void
Sys_DebugLog (const char *file, const char *fmt, ...)
{
	va_list     args;
	static dstring_t *data;
	int         fd;

	if (!data)
		data = dstring_newstr ();

	va_start (args, fmt);
	dvsprintf (data, fmt, args);
	va_end (args);
	if ((fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0644)) >= 0) {
		if (write (fd, data->str, data->size - 1) != (ssize_t) (data->size - 1))
			Sys_Printf ("Error writing %s: %s\n", file, strerror(errno));
		close (fd);
	}
}

int
Sys_CheckInput (int idle, int net_socket)
{
	fd_set      fdset;
	int         res;
	struct timeval _timeout;
	struct timeval *timeout = 0;

#ifdef _WIN32
	int         sleep_msec;
	// Now we want to give some processing time to other applications,
	// such as qw_client, running on this machine.
	sleep_msec = sys_sleep->int_val;
	if (sleep_msec > 0) {
		if (sleep_msec > 13)
			sleep_msec = 13;
		Sleep (sleep_msec);
	}

	_timeout.tv_sec = 0;
	_timeout.tv_usec = net_socket < 0 ? 0 : 20;
#else
	_timeout.tv_sec = 0;
	_timeout.tv_usec = net_socket < 0 ? 0 : 2000;
#endif
	// select on the net socket and stdin
	// the only reason we have a timeout at all is so that if the last
	// connected client times out, the message would not otherwise
	// be printed until the next event.
	FD_ZERO (&fdset);
#ifndef _WIN32
	if (do_stdin)
		FD_SET (0, &fdset);
#endif
	if (net_socket >= 0)
		FD_SET (((unsigned) net_socket), &fdset);	// cast needed for windows

	if (!idle || !sys_dead_sleep->int_val)
		timeout = &_timeout;

	res = select (max (net_socket, 0) + 1, &fdset, NULL, NULL, timeout);
	if (res == 0 || res == -1)
		return 0;
#ifndef _WIN32
	stdin_ready = FD_ISSET (0, &fdset);
#endif
	return 1;
}

/*
	Sys_ConsoleInput

	Checks for a complete line of text typed in at the console, then forwards
	it to the host command processor
*/
const char *
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
		text[len] = 0;
		if (len == sizeof (text))
			len = 0;
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

static jmp_buf  aiee_abort;

typedef struct sh_stack_s {
	struct sh_stack_s *next;
	int (*signal_hook)(int,void*);
	void *data;
} sh_stack_t;

static sh_stack_t *sh_stack;
static sh_stack_t *free_sh;
static int (*signal_hook)(int,void*);
static void *signal_hook_data;

void
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

void
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

static void
aiee (int sig)
{
	printf ("AIEE, signal %d in shutdown code, giving up\n", sig);
	longjmp (aiee_abort, 1);
}

static void
signal_handler (int sig)
{
	int volatile recover = 0;	// volatile for longjump

	printf ("Received signal %d, exiting...\n", sig);

	switch (sig) {
		case SIGINT:
		case SIGTERM:
#ifndef _WIN32
		case SIGHUP:
			signal (SIGHUP, SIG_DFL);
#endif
			signal (SIGINT, SIG_DFL);
			signal (SIGTERM, SIG_DFL);
			Sys_Quit ();
		default:
#ifndef _WIN32
			signal (SIGQUIT, aiee);
			signal (SIGTRAP, aiee);
			signal (SIGIOT, aiee);
			signal (SIGBUS, aiee);
#endif
			signal (SIGILL, aiee);
			signal (SIGSEGV, aiee);
			signal (SIGFPE, aiee);

			if (!setjmp (aiee_abort)) {
				if (signal_hook)
					recover = signal_hook (sig, signal_hook_data);
				Sys_Shutdown ();
			}

			if (recover) {
#ifndef _WIN32
				signal (SIGQUIT, signal_handler);
				signal (SIGTRAP, signal_handler);
				signal (SIGIOT, signal_handler);
				signal (SIGBUS, signal_handler);
#endif
				signal (SIGILL, signal_handler);
				signal (SIGSEGV, signal_handler);
				signal (SIGFPE, signal_handler);
			} else {
#ifndef _WIN32
				signal (SIGQUIT, SIG_DFL);
				signal (SIGTRAP, SIG_DFL);
				signal (SIGIOT, SIG_DFL);
				signal (SIGBUS, SIG_DFL);
#endif
				signal (SIGILL, SIG_DFL);
				signal (SIGSEGV, SIG_DFL);
				signal (SIGFPE, SIG_DFL);
			}
	}
}

void
Sys_Init (void)
{
	// catch signals
#ifndef _WIN32
	signal (SIGHUP, signal_handler);
	signal (SIGQUIT, signal_handler);
	signal (SIGTRAP, signal_handler);
	signal (SIGIOT, signal_handler);
	signal (SIGBUS, signal_handler);
#endif
	signal (SIGINT, signal_handler);
	signal (SIGILL, signal_handler);
	signal (SIGSEGV, signal_handler);
	signal (SIGTERM, signal_handler);
	signal (SIGFPE, signal_handler);
}
