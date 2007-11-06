/*
	cl_sys_win.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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
#include "winquake.h"

#include "QF/qargs.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "client.h"
#include "compat.h"
#include "host.h"
#include "netchan.h"
#include "win32/resources/resource.h"

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

static HANDLE tevent;

static void
startup (void)
{
	OSVERSIONINFO vinfo;

	// allocate named semaphore on client so front end can tell if it's alive

	// mutex will fail if semaphore already exists
	qwclsemaphore = CreateMutex (NULL,					// Security attributes
								 0,						// owner
								 "qwcl");				// Semaphore name
	if (!qwclsemaphore)
		Sys_Error ("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

	qwclsemaphore = CreateSemaphore (NULL,				// Security attributes
									 0,					// Initial count
									 1,					// Maximum count
									 "qwcl");			// Semaphore name

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
	if (tevent)
		CloseHandle (tevent);

	if (qwclsemaphore)
		CloseHandle (qwclsemaphore);
}

static void
SleepUntilInput (int time)
{
	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}

HINSTANCE   global_hInstance;
int         global_nCmdShow;
const char *argv[MAX_NUM_ARGVS];
static const char *empty_string = "";

int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
		 int nCmdShow)
{
//	MSG			msg;
	static char cwd[1024];
	double		time, oldtime, newtime;
#ifdef SPLASH_SCREEN
	RECT		rect;
#endif

	// previous instances do not exist in Win32
	if (hPrevInstance)
		return 0;

	startup ();

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

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

	COM_InitArgv (host_parms.argc, argv);
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

	tevent = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");

	Sys_Printf ("Host_Init\n");
	Host_Init ();

	Sys_RegisterShutdown (Host_Shutdown);
	Sys_RegisterShutdown (Net_LogStop);
	Sys_RegisterShutdown (shutdown);

	oldtime = Sys_DoubleTime ();

	// main window message loop
	while (1) {
		// yield CPU for a little bit when paused, minimized, or not the focus
		if ((cl.paused && (!ActiveApp)) || Minimized) {
			SleepUntilInput (PAUSE_SLEEP);
			scr_skipupdate = 1;			// no point in bothering to draw
		} else if (!ActiveApp) {
			SleepUntilInput (NOT_FOCUS_SLEEP);
		}

		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		Host_Frame (time);
		oldtime = newtime;
	}

	// return success of application
	return TRUE;
}
