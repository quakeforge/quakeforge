/*
	vid_win.c

	Win32 vid component

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

#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "context_win.h"
#include "d_iface.h"
#include "vid_internal.h"
#include "vid_sw.h"

static vid_internal_t vid_internal;

static byte backingbuf[48 * 24];

void
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	int         i, j, reps = 1, repshift = 0;
	vrect_t     rect;

	if (!viddef.initialized || !win_sw_context)
		return;

	if (!viddef.direct)
		return;

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			memcpy (&backingbuf[(i + j) * 24],
					viddef.direct + x + ((y << repshift) + i +
										 j) * viddef.rowbytes, width);

			memcpy (viddef.direct + x +
					((y << repshift) + i + j) * viddef.rowbytes,
					&pbitmap[(i >> repshift) * width], width);
		}
	}

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height << repshift;
	rect.next = NULL;

	win_sw_context->update (&rect);
}


void
D_EndDirectRect (int x, int y, int width, int height)
{
	int         i, j, reps = 1, repshift = 0;
	vrect_t     rect;

	if (!viddef.initialized || !win_sw_context)
		return;

	if (!viddef.direct)
		return;

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			memcpy (viddef.direct + x +
					((y << repshift) + i + j) * viddef.rowbytes,
					&backingbuf[(i + j) * 24], width);
		}
	}

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height << repshift;
	rect.next = NULL;

	win_sw_context->update (&rect);
}

static void
VID_shutdown (void *data)
{
	Sys_MaskPrintf (SYS_vid, "VID_shutdown\n");
	Win_CloseDisplay ();
}

void
VID_Init (byte *palette, byte *colormap)
{
	Sys_RegisterShutdown (VID_shutdown, 0);

	vid_internal.gl_context = Win_GL_Context;
	vid_internal.sw_context = Win_SW_Context;
#ifdef HAVE_VULKAN
	vid_internal.vulkan_context = Win_Vulkan_Context;
#endif

	R_LoadModule (&vid_internal);

	viddef.numpages = 1;
	viddef.colormap8 = colormap;
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];

	VID_GetWindowSize (640, 480);
	Win_OpenDisplay ();
	vid_internal.choose_visual ();
	Win_SetVidMode (viddef.width, viddef.height);
	Win_CreateWindow (viddef.width, viddef.height);
	vid_internal.create_context ();

	VID_InitGamma (palette);
	viddef.vid_internal->set_palette (palette);

	Sys_MaskPrintf (SYS_vid, "Video mode %dx%d initialized.\n",
					viddef.width, viddef.height);

	viddef.initialized = true;
}

void
VID_Init_Cvars (void)
{
	Win_Init_Cvars ();
#ifdef HAVE_VULKAN
	Win_Vulkan_Init_Cvars ();
#endif
	Win_GL_Init_Cvars ();
}

void
VID_LockBuffer (void)
{
}

void
VID_UnlockBuffer (void)
{
}

void
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char       *temp = strdup (text);

		Win_SetCaption (va (0, "%s: %s", PACKAGE_STRING, temp));
		free (temp);
	} else {
		Win_SetCaption (va (0, "%s", PACKAGE_STRING));
	}
}

qboolean
VID_SetGamma (double gamma)
{
	return Win_SetGamma (gamma);
}


#if 0
void
VID_Update (vrect_t *rects)
{
	vrect_t     rect;
	RECT        trect;

	if (firstupdate) {
		if (modestate == MS_WINDOWED) {
			GetWindowRect (win_mainwindow, &trect);

			if ((trect.left != vid_window_x->int_val) ||
				(trect.top != vid_window_y->int_val)) {
				if (COM_CheckParm ("-resetwinpos")) {
					Cvar_SetValue (vid_window_x, 0.0);
					Cvar_SetValue (vid_window_y, 0.0);
				}

				VID_CheckWindowXY ();
				SetWindowPos (win_mainwindow, NULL, vid_window_x->int_val,
							  vid_window_y->int_val, 0, 0,
							  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW |
							  SWP_DRAWFRAME);
			}
		}

		if ((_vid_default_mode_win->int_val != vid_default) &&
			(!startwindowed
			 || (_vid_default_mode_win->int_val < MODE_FULLSCREEN_DEFAULT))) {
			firstupdate = 0;

			if (COM_CheckParm ("-resetwinpos")) {
				Cvar_SetValue (vid_window_x, 0.0);
				Cvar_SetValue (vid_window_y, 0.0);
			}

			if ((_vid_default_mode_win->int_val < 0) ||
				(_vid_default_mode_win->int_val >= nummodes)) {
				Cvar_SetValue (_vid_default_mode_win, windowed_default);
			}

			Cvar_SetValue (vid_mode, _vid_default_mode_win->int_val);
		}
	}
	// We've drawn the frame; copy it to the screen
	FlipScreen (rects);

	// check for a driver change
	if ((vid_ddraw->int_val && !vid_usingddraw)
		|| (!vid_ddraw->int_val && vid_usingddraw)) {
		// reset the mode
		force_mode_set = true;
		VID_SetMode (vid_mode->int_val, vid_curpal);
		force_mode_set = false;

		// store back
		if (vid_usingddraw)
			Sys_Printf ("loaded DirectDraw driver\n");
		else
			Sys_Printf ("loaded GDI driver\n");
	}

	if (vid_testingmode) {
		if (Sys_DoubleTime () >= vid_testendtime) {
			VID_SetMode (vid_realmode, vid_curpal);
			vid_testingmode = 0;
		}
	} else {
		if (vid_mode->int_val != vid_realmode) {
			VID_SetMode (vid_mode->int_val, vid_curpal);
			Cvar_SetValue (vid_mode, (float) vid_modenum);
			// so if mode set fails, we don't keep on
			// trying to set that mode
			vid_realmode = vid_modenum;
		}
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED) {
		if (_windowed_mouse->int_val != windowed_mouse) {
			if (_windowed_mouse->int_val) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

			windowed_mouse = _windowed_mouse->int_val;
		}
	}
}
#endif
