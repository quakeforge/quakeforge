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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "winquake.h"
#include "win32/resources/resource.h"

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "nq/include/client.h"
#include "nq/include/host.h"

qboolean    isDedicated = false;

#define MINIMUM_WIN_MEMORY		0x0880000
#define MAXIMUM_WIN_MEMORY		0x1000000

#define CONSOLE_ERROR_TIMEOUT	60.0	// # of seconds to wait on Sys_Error
										// running dedicated before exiting
#define PAUSE_SLEEP		50				// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

qboolean    ActiveApp, Minimized;
qboolean    WinNT;

static double pfreq;
static int  lowshift;
HANDLE      hinput, houtput;

static HANDLE tevent;

// SYSTEM IO ==================================================================

static void
startup (void)
{
	LARGE_INTEGER PerformanceFreq;
	unsigned int lowpart, highpart;
	OSVERSIONINFO vinfo;

	Sys_MaskFPUExceptions ();

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

static void
shutdown_f (void *data)
{
	if (tevent)
		CloseHandle (tevent);
}

// WINDOWS CRAP ===============================================================

static void
SleepUntilInput (int time)
{
	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}

HINSTANCE   global_hInstance;
int         global_nCmdShow;
const char *argv[MAX_NUM_ARGVS];
static const char *empty_string = "";
HWND        hwnd_dialog;

static void
init_handles (HINSTANCE hInstance)
{
	RECT        rect;

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

	tevent = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");
}

int         WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
		 int nCmdShow)
{
	double      time, oldtime, newtime;
	MEMORYSTATUS lpBuffer;
	int         argc;

	// previous instances do not exist in Win32
	if (hPrevInstance)
		return 0;

	startup ();

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	lpBuffer.dwLength = sizeof (MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS)) {
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine) {
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine) {
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

	memset (&host_parms, 0, sizeof (host_parms));

	COM_InitArgv (argc, argv);
	host_parms.argc = com_argc;
	host_parms.argv = com_argv;

	isDedicated = (COM_CheckParm ("-dedicated") != 0);

	if (!isDedicated)
		init_handles (hInstance);

	Sys_RegisterShutdown (Host_Shutdown, 0);
	Sys_RegisterShutdown (shutdown_f, 0);

	Host_Init ();

	oldtime = Sys_DoubleTime () - 0.1;
	while (1) {							// Main message loop
		if (!isDedicated) {
			// yield the CPU for a little while when paused, minimized, or
			// not the focus
			if ((cl.paused && !ActiveApp) || Minimized) {
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			} else if (!ActiveApp) {
				SleepUntilInput (NOT_FOCUS_SLEEP);
			}
		}
		// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		if (net_is_dedicated) {	// play vcrfiles at max speed
			if (time < sys_ticrate && (!vcrFile || recording)) {
				Sleep (1);
				continue;			// not time to run a server-only tic yet
			}
			time = sys_ticrate;
		}

		if (time > sys_ticrate * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame (time);
	}
}
