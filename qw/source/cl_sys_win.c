/*
	sys_win.c

	(description)

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_CONIO_H
# include <conio.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <windows.h>

#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "client.h"
#include "compat.h"
#include "host.h"
#include "net.h"
#include "resource.h"

#define MAXIMUM_WIN_MEMORY	0x1000000
#define MINIMUM_WIN_MEMORY	0x0c00000

#define PAUSE_SLEEP		50				// sleep time on pause or
										// minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

char       *svs_info;

qboolean    ActiveApp, Minimized, WinNT;
qboolean    is_server = false;

HWND        hwnd_dialog;				// startup dialog box

HANDLE      qwclsemaphore;

static HANDLE hinput, houtput, tevent;

void        MaskExceptions (void);
void        Sys_PopFPCW (void);
void        Sys_PushFPCW_SetHigh (void);
void        Sys_InitFloatTime (void);


void
Sys_Init (void)
{
	OSVERSIONINFO vinfo;

	// allocate named semaphore on client so front end can tell if it's alive

	// mutex will fail if semephore allready exists
	qwclsemaphore = CreateMutex (NULL, /* Security attributes */
								 0,	/* owner */
								 "qwcl"); /* Semaphore name */
	if (!qwclsemaphore)
		Sys_Error ("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

	qwclsemaphore = CreateSemaphore (NULL, /* Security attributes */
									 0,	/* Initial count */
									 1,	/* Maximum count */
									 "qwcl"); /* Semaphore name */

#ifdef USE_INTEL_ASM
	MaskExceptions ();
	Sys_SetFPCW ();
#endif

	// make sure the timer is high precision, otherwise NT gets 18ms resolution
	timeBeginPeriod (1);

	vinfo.dwOSVersionInfoSize = sizeof (vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4) ||
		(vinfo.dwPlatformId == VER_PLATFORM_WIN32s)) {
		Sys_Error ("This version of " PROGRAM
				   " requires a full Win32 implementation.");
	}

	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		WinNT = true;
	else
		WinNT = false;
}

static void
shutdown (void)
{
	VID_ForceUnlockedAndReturnState ();

	if (tevent)
		CloseHandle (tevent);

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);
}

void
Sys_DebugLog (const char *file, const char *fmt, ...)
{
	static char data[1024];
	int         fd;
	va_list     argptr;

	va_start (argptr, fmt);
	vsnprintf (data, sizeof (data), fmt, argptr);
	va_end (argptr);
	fd = open (file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	write (fd, data, strlen (data));
	close (fd);
};

/*
	Sys_ConsoleInput

	Checks for a complete line of text typed in at the console, then forwards
	it to the host command processor
*/
const char *
Sys_ConsoleInput (void)
{
	char        *clipText, *textCopied;
	static char  text[256];
	int          ch, i;										// , count;
	static int   len;
	DWORD        numread, numevents, dummy;
	HANDLE       th;
	INPUT_RECORD recs[1024];

	for (;;) {
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput (hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT) {
			if (!recs[0].Event.KeyEvent.bKeyDown) {
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch) {
					case '\r':
						WriteFile (houtput, "\r\n", 2, &dummy, NULL);

						if (len) {
							text[len] = 0;
							len = 0;
							return text;
						}
						break;

					case '\b':
						WriteFile (houtput, "\b \b", 3, &dummy, NULL);
						if (len) {
							len--;
							putch ('\b');
						}
						break;

					default:
						Con_Printf ("Stupid: %ld\n",
									recs[0].Event.KeyEvent.dwControlKeyState);
						if (
							((ch == 'V' || ch == 'v')
							 && (recs[0].Event.KeyEvent.
								 dwControlKeyState & (LEFT_CTRL_PRESSED |
													  RIGHT_CTRL_PRESSED)))
							||
							((recs
							  [0].Event.KeyEvent.
							  dwControlKeyState & SHIFT_PRESSED)
							 && (recs[0].Event.KeyEvent.wVirtualKeyCode ==
								 VK_INSERT))) {
							if (OpenClipboard (NULL)) {
								th = GetClipboardData (CF_TEXT);
								if (th) {
									clipText = GlobalLock (th);
									if (clipText) {
										textCopied =
											malloc (GlobalSize (th) + 1);
										strcpy (textCopied, clipText);
										// Substitutes a NULL for every token
											strtok (textCopied, "\n\r\b");
										i = strlen (textCopied);
										if (i + len >= 256)
											i = 256 - len;
										if (i > 0) {
											textCopied[i] = 0;
											text[len] = 0;
											strncat (text, textCopied,
													 sizeof (text) -
													 strlen (text));
											len += dummy;
											WriteFile (houtput, textCopied, i,
													   &dummy, NULL);
										}
										free (textCopied);
									}
									GlobalUnlock (th);
								}
								CloseClipboard ();
							}
						} else if (ch >= ' ') {
							WriteFile (houtput, &ch, 1, &dummy, NULL);
							text[len] = ch;
							len = (len + 1) & 0xff;
						}

						break;

				}
			}
		}
	}
	return NULL;
}

void
SleepUntilInput (int time)
{
	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}

HINSTANCE   global_hInstance;
int         global_nCmdShow;
char       *argv[MAX_NUM_ARGVS];
static char *empty_string = "";

int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
		 int nCmdShow)
{
//	MSG               msg;
	static char cwd[1024];
	double      time, oldtime, newtime;
	int         t;
	MEMORYSTATUS lpBuffer;
#ifdef SPLASH_SCREEN
	RECT        rect;
#endif

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	lpBuffer.dwLength = sizeof (MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	if (!GetCurrentDirectory (sizeof (cwd), cwd))
		Sys_Error ("Couldn't determine current directory");

	if (cwd[strlen (cwd) - 1] == '/')
		cwd[strlen (cwd) - 1] = 0;

	host_parms.argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (host_parms.argc < MAX_NUM_ARGVS)) {
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine) {
			argv[host_parms.argc] = lpCmdLine;
			host_parms.argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine) {
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

	host_parms.argv = argv;

	COM_InitArgv (host_parms.argc, (const char**)host_parms.argv);

	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

#ifdef SPLASH_SCREEN
	hwnd_dialog = CreateDialog (hInstance, MAKEINTRESOURCE (IDD_DIALOG1),
								NULL, NULL);

	if (hwnd_dialog) {
		if (GetWindowRect (hwnd_dialog, &rect)) {
			if (rect.left > (rect.top * 2)) {
				SetWindowPos (hwnd_dialog, 0,
							  (rect.left / 2) - ((rect.right - rect.left) / 2),
							  rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			}
		}

		ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
		UpdateWindow (hwnd_dialog);
		SetForegroundWindow (hwnd_dialog);
	}
#endif

	// take the greater of all the available memory or half the total memory,
	// but at least 8 Mb and no more than 16 Mb, unless they explicitly
	// request otherwise
	host_parms.memsize = lpBuffer.dwAvailPhys;

	if (host_parms.memsize < MINIMUM_WIN_MEMORY)
		host_parms.memsize = MINIMUM_WIN_MEMORY;

	if (host_parms.memsize < (lpBuffer.dwTotalPhys >> 1))
		host_parms.memsize = lpBuffer.dwTotalPhys >> 1;

	if (host_parms.memsize > MAXIMUM_WIN_MEMORY)
		host_parms.memsize = MAXIMUM_WIN_MEMORY;

	if (COM_CheckParm ("-heapsize")) {
		t = COM_CheckParm ("-heapsize") + 1;

		if (t < com_argc)
			host_parms.memsize = atoi (com_argv[t]) * 1024;
	}

	host_parms.membase = malloc (host_parms.memsize);

	if (!host_parms.membase)
		Sys_Error ("Not enough memory free; check disk space\n");

	tevent = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");

	// because sound is off until we become active
	// XXX S_BlockSound ();

	Con_Printf ("Host_Init\n");
	Host_Init ();

	Sys_RegisterShutdown (Host_Shutdown);
	Sys_RegisterShutdown (Net_LogStop);
	Sys_RegisterShutdown (shutdown);

	oldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1) {
		// yield CPU for a little bit when paused, minimized, or not the focus
		if ((cl.paused && (!ActiveApp && !DDActive)) || Minimized
			|| block_drawing) {
			SleepUntilInput (PAUSE_SLEEP);
			scr_skipupdate = 1;			// no point in bothering to draw
		} else if (!ActiveApp && !DDActive) {
			SleepUntilInput (NOT_FOCUS_SLEEP);
		}

		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		Host_Frame (time);
		oldtime = newtime;
	}

	/* return success of application */
	return TRUE;
}
