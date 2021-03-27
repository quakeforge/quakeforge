/*
	winquake.h

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

#ifndef _WINQUAKE_H
#define _WINQUAKE_H

#ifdef _WIN32

#include <memoryapi.h>

#ifndef __GNUC__
# pragma warning( disable : 4229 )  /* mgraph gets this */
#endif

#define byte __hide_byte
#define shutdown __hide_shutdown

#include <windows.h>

#ifdef WINNT
# ifdef HAVE_DSOUND_H
#  include <dsound.h>
# endif
#else
# include <windows.h>
# ifdef HAVE_DSOUND_H
#  include <dsound.h>
# endif
#endif

#ifdef HAVE_MGRAPH_H
# include <mgraph.h>
#endif

#undef byte
#undef shutdown
#undef LoadImage

#include "QF/qtypes.h"

#ifndef WM_MOUSEWHEEL
# define WM_MOUSEWHEEL                   0x020A
#endif

extern	HINSTANCE	global_hInstance;
extern	int			global_nCmdShow;

#ifdef HAVE_DDRAW_H
# include <ddraw.h>
extern LPDIRECTDRAW		lpDD;
extern LPDIRECTDRAWSURFACE	lpPrimary;
extern LPDIRECTDRAWSURFACE	lpFrontBuffer;
extern LPDIRECTDRAWSURFACE	lpBackBuffer;
extern LPDIRECTDRAWPALETTE	lpDDPal;
#endif

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

extern modestate_t	modestate;

extern qboolean		ActiveApp, Minimized;

extern qboolean	WinNT;

extern qboolean	winsock_lib_initialized;

extern int		window_center_x, window_center_y;
extern RECT		window_rect;

#ifdef SPLASH_SCREEN
extern HWND		hwnd_dialog;
#endif

extern HANDLE	hinput, houtput;

void S_BlockSound (void);
void S_UnblockSound (void);

DWORD *DSOUND_LockBuffer(qboolean lockit);
void DSOUND_ClearBuffer(int clear);
void DSOUND_Restore(void);

extern int (PASCAL FAR *pWSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
extern int (PASCAL FAR *pWSACleanup)(void);
extern int (PASCAL FAR *pWSAGetLastError)(void);
extern SOCKET (PASCAL FAR *psocket)(int af, int type, int protocol);
extern int (PASCAL FAR *pioctlsocket)(SOCKET s, long cmd, u_long FAR *argp);
extern int (PASCAL FAR *psetsockopt)(SOCKET s, int level, int optname, const char FAR * optval, int optlen);
extern int (PASCAL FAR *precvfrom)(SOCKET s, char FAR * buf, int len, int flags, struct sockaddr FAR *from, int FAR * fromlen);
extern int (PASCAL FAR *psendto)(SOCKET s, const char FAR * buf, int len, int flags, const struct sockaddr FAR *to, int tolen);
extern int (PASCAL FAR *pclosesocket)(SOCKET s);
extern int (PASCAL FAR *pgethostname)(char FAR * name, int namelen);
extern struct hostent FAR * (PASCAL FAR *pgethostbyname)(const char FAR * name);
extern struct hostent FAR * (PASCAL FAR *pgethostbyaddr)(const char FAR * addr, int len, int type);
extern int (PASCAL FAR *pgetsockname)(SOCKET s, struct sockaddr FAR *name, int FAR * namelen);

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);
LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate (BOOL fActive, BOOL minimize);

#undef E_POINTER

#endif /* _WIN32 */

#endif /* _WINQUAKE_H */
