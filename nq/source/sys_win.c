/*
	sys_win.c

	@description@

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "winquake.h"
#include "conproc.h"
#include "resource.h"

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/vfile.h"

#include "client.h"
#include "compat.h"
#include "game.h"
#include "host.h"

#define MINIMUM_WIN_MEMORY		0x0880000
#define MAXIMUM_WIN_MEMORY		0x1000000

#define CONSOLE_ERROR_TIMEOUT	60.0	// # of seconds to wait on Sys_Error
										// running
										// dedicated before exiting
#define PAUSE_SLEEP		50				// sleep time on pause or
										// minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

qboolean    ActiveApp, Minimized;
qboolean    WinNT;

static double pfreq;
static int  lowshift;
qboolean    isDedicated;
static qboolean sc_return_on_enter = false;
HANDLE      hinput, houtput;

static const char tracking_tag[] = "Clams & Mooses";

static HANDLE tevent;
static HANDLE hFile;
static HANDLE heventParent;
static HANDLE heventChild;

void        MaskExceptions (void);
void        Sys_PushFPCW_SetHigh (void);
void        Sys_PopFPCW (void);

volatile int sys_checksum;


void
Sys_PageIn (void *ptr, int size)
{
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
}

// FILE IO ====================================================================

#define	MAX_HANDLES		10
VFile      *sys_handles[MAX_HANDLES];

int
findhandle (void)
{
	int         i;

	for (i = 1; i < MAX_HANDLES; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

// SYSTEM IO ==================================================================

#ifndef USE_INTEL_ASM

void
Sys_SetFPCW (void)
{
}

void
Sys_PushFPCW_SetHigh (void)
{
}

void
Sys_PopFPCW (void)
{
}

void
MaskExceptions (void)
{
}

#endif

void
Sys_Init (void)
{
	LARGE_INTEGER PerformanceFreq;
	unsigned int lowpart, highpart;
	OSVERSIONINFO vinfo;

	MaskExceptions ();
	Sys_SetFPCW ();

	if (!QueryPerformanceFrequency (&PerformanceFreq))
		Sys_Error ("No hardware timer available");

	// get 32 out of the 64 time bits such that we have around
	// 1 microsecond resolution
	lowpart = (unsigned int) PerformanceFreq.LowPart;
	highpart = (unsigned int) PerformanceFreq.HighPart;
	lowshift = 0;

	while (highpart || (lowpart > 2000000.0)) {
		lowshift++;
		lowpart >>= 1;
		lowpart |= (highpart & 1) << 31;
		highpart >>= 1;
	}

	pfreq = 1.0 / (double) lowpart;

	vinfo.dwOSVersionInfoSize = sizeof (vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4) ||
		(vinfo.dwPlatformId == VER_PLATFORM_WIN32s)) {
		Sys_Error ("WinQuake requires at least Win95 or NT 4.0");
	}

	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		WinNT = true;
	else
		WinNT = false;
}

void
Sys_Error (const char *error, ...)
{
	va_list     argptr;
	char        text[1024], text2[1024];
	char       *text3 = "Press Enter to exit\n";
	char       *text4 = "***********************************\n";
	char       *text5 = "\n";
	DWORD       dummy;
	double      starttime;
	static int  in_sys_error0 = 0;
	static int  in_sys_error1 = 0;
	static int  in_sys_error2 = 0;
	static int  in_sys_error3 = 0;

	if (!in_sys_error3) {
		in_sys_error3 = 1;
		VID_ForceUnlockedAndReturnState ();
	}

	va_start (argptr, error);
	vsnprintf (text, sizeof (text), error, argptr);
	va_end (argptr);

	if (isDedicated) {
		va_start (argptr, error);
		vsnprintf (text, sizeof (text), error, argptr);
		va_end (argptr);

		snprintf (text2, sizeof (text2), "ERROR: %s\n", text);
		WriteFile (houtput, text5, strlen (text5), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);
		WriteFile (houtput, text2, strlen (text2), &dummy, NULL);
		WriteFile (houtput, text3, strlen (text3), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);

		starttime = Sys_DoubleTime ();
		sc_return_on_enter = true;		// so Enter will get us out of here

		while (!Sys_ConsoleInput () &&
			   ((Sys_DoubleTime () - starttime) < CONSOLE_ERROR_TIMEOUT)) {
		}
	} else {
		// switch to windowed so the message box is visible, unless we
		// already tried that and failed
		if (!in_sys_error0) {
			in_sys_error0 = 1;
			VID_SetDefaultMode ();
			MessageBox (NULL, text, "Quake Error",
						MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		} else {
			MessageBox (NULL, text, "Double Quake Error",
						MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
	}

	if (!in_sys_error1) {
		in_sys_error1 = 1;
		Host_Shutdown ();
	}
	// shut down QHOST hooks if necessary
	if (!in_sys_error2) {
		in_sys_error2 = 1;
		DeinitConProc ();
	}

	exit (1);
}

void
Sys_Quit (void)
{

	VID_ForceUnlockedAndReturnState ();

	Host_Shutdown ();

	if (tevent)
		CloseHandle (tevent);

	if (isDedicated)
		FreeConsole ();

	// shut down QHOST hooks if necessary
	DeinitConProc ();

	exit (0);
}

const char *
Sys_ConsoleInput (void)
{
	static char text[256];
	static int  len;
	INPUT_RECORD recs[1024];
	DWORD       dummy;
	int         ch;
	DWORD       numread;
	DWORD		numevents;

	if (!isDedicated)
		return NULL;

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
					} else if (sc_return_on_enter) {
						// special case to allow exiting from the error
						// handler on Enter
						text[0] = '\r';
						len = 0;
						return text;
					}

					break;

				case '\b':
					WriteFile (houtput, "\b \b", 3, &dummy, NULL);
					if (len) {
						len--;
					}
					break;

				default:
					if (ch >= ' ') {
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

// WINDOWS CRAP ===============================================================

void
SleepUntilInput (int time)
{
	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}

HINSTANCE   global_hInstance;
int         global_nCmdShow;
char       *argv[MAX_NUM_ARGVS];
static char *empty_string = "";
HWND        hwnd_dialog;

int         WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
		 int nCmdShow)
{
	quakeparms_t parms;
	double      time, oldtime, newtime;
	MEMORYSTATUS lpBuffer;
	static char cwd[1024];
	int         t;
	RECT        rect;

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

	parms.argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (parms.argc < MAX_NUM_ARGVS)) {
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine) {
			argv[parms.argc] = lpCmdLine;
			parms.argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine) {
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

	parms.argv = argv;

	COM_InitArgv (parms.argc, (const char**)parms.argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	isDedicated = (COM_CheckParm ("-dedicated") != 0);

	if (!isDedicated) {
		hwnd_dialog = CreateDialog (hInstance, MAKEINTRESOURCE (IDD_DIALOG1),
									NULL, NULL);

		if (hwnd_dialog) {
			if (GetWindowRect (hwnd_dialog, &rect)) {
				if (rect.left > (rect.top * 2)) {
					SetWindowPos (hwnd_dialog, 0,
								  (rect.left / 2) -
								  ((rect.right - rect.left) / 2), rect.top, 0,
								  0, SWP_NOZORDER | SWP_NOSIZE);
				}
			}

			ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (hwnd_dialog);
			SetForegroundWindow (hwnd_dialog);
		}
	}
	// take the greater of all the available memory or half the total memory,
	// but at least 8 Mb and no more than 16 Mb, unless they explicitly
	// request otherwise
	parms.memsize = lpBuffer.dwAvailPhys;

	if (parms.memsize < MINIMUM_WIN_MEMORY)
		parms.memsize = MINIMUM_WIN_MEMORY;

	if (parms.memsize < (lpBuffer.dwTotalPhys >> 1))
		parms.memsize = lpBuffer.dwTotalPhys >> 1;

	if (parms.memsize > MAXIMUM_WIN_MEMORY)
		parms.memsize = MAXIMUM_WIN_MEMORY;

	if (COM_CheckParm ("-heapsize")) {
		t = COM_CheckParm ("-heapsize") + 1;

		if (t < com_argc)
			parms.memsize = atoi (com_argv[t]) * 1024;
	}

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Not enough memory free; check disk space\n");

	Sys_PageIn (parms.membase, parms.memsize);

	tevent = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");

	if (isDedicated) {
		if (!AllocConsole ()) {
			Sys_Error ("Couldn't create dedicated server console");
		}

		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

		// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm ("-HFILE")) > 0) {
			if (t < com_argc)
				hFile = (HANDLE) atoi (com_argv[t + 1]);
		}

		if ((t = COM_CheckParm ("-HPARENT")) > 0) {
			if (t < com_argc)
				heventParent = (HANDLE) atoi (com_argv[t + 1]);
		}

		if ((t = COM_CheckParm ("-HCHILD")) > 0) {
			if (t < com_argc)
				heventChild = (HANDLE) atoi (com_argv[t + 1]);
		}

		InitConProc (hFile, heventParent, heventChild);
	}

	Sys_Init ();

// because sound is off until we become active
	//XXX S_BlockSound ();

	Sys_Printf ("Host_Init\n");
	Host_Init (&parms);

	Sys_Init_Cvars ();
	Sys_Init ();

	oldtime = Sys_DoubleTime ();

	/* main window message loop */
	while (1) {
		if (isDedicated) {
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate->value) {
				Sleep (1);
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}
		} else {
			// yield the CPU for a little while when paused, minimized, or
			// not the focus
			if ((cl.paused && (!ActiveApp && !DDActive)) || Minimized
				|| block_drawing) {
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			} else if (!ActiveApp && !DDActive) {
				SleepUntilInput (NOT_FOCUS_SLEEP);
			}

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
		}

		Host_Frame (time);
		oldtime = newtime;
	}

	/* return success of application */
	return TRUE;
}

void
Sys_DebugLog (const char *file, const char *fmt, ...)
{
}
