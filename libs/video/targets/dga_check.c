/*
	dga_check.c

	Routines to check for XFree86 DGA and VidMode extensions

	Copyright (C) 2000 Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>

#ifdef HAVE_DGA
# ifdef DGA_OLD_HEADERS
#  include <X11/extensions/xf86dga.h>
#  include <X11/extensions/xf86dgastr.h>
# else
#  include <X11/extensions/Xxf86dga.h>
#  include <X11/extensions/xf86dgaproto.h>
# endif
# ifndef XDGA_MAJOR_VERSION
#  ifdef XF86DGA_MAJOR_VERSION
#   define XDGA_MAJOR_VERSION XF86DGA_MAJOR_VERSION
#  else
#   error "Neither XDGA_MAJOR_VERSION nor XF86DGA_MAJOR_VERSION found."
#  endif
# endif
#endif
#ifdef HAVE_VIDMODE
# include <X11/extensions/xf86vmode.h>
# ifdef DGA_OLD_HEADERS
#  include <X11/extensions/xf86vmstr.h>
# else
#  include <X11/extensions/xf86vmproto.h>
# endif
#endif

#include "QF/sys.h"

#include "dga_check.h"


/*
  VID_CheckDGA

  Check for the presence of the XFree86-DGA X server extension
*/
bool
VID_CheckDGA (Display * dpy, int *maj_ver, int *min_ver, int *hasvideo)
{
#ifdef HAVE_DGA
	int 	event_base, error_base, dgafeat;
	int 	dummy, dummy_major, dummy_minor, dummy_video;

	if (!XQueryExtension (dpy, XF86DGANAME, &dummy, &dummy, &dummy)) {
		return false;
	}

	if (!XF86DGAQueryExtension (dpy, &event_base, &error_base)) {
		return false;
	}

	if (!maj_ver)
		maj_ver = &dummy_major;
	if (!min_ver)
		min_ver = &dummy_minor;

	if (!XF86DGAQueryVersion (dpy, maj_ver, min_ver)) {
		return false;
	}

	if ((!maj_ver) || (*maj_ver != XDGA_MAJOR_VERSION)) {
		Sys_MaskPrintf (SYS_vid, "VID: Incorrect DGA version: %d.%d, \n",
						*maj_ver, *min_ver);
		return false;
	}
	Sys_MaskPrintf (SYS_vid, "VID: DGA version: %d.%d\n", *maj_ver, *min_ver);

	if (!hasvideo)
		hasvideo = &dummy_video;

	if (!XF86DGAQueryDirectVideo (dpy, DefaultScreen (dpy), &dgafeat)) {
		*hasvideo = 0;
	} else {
		*hasvideo = (dgafeat & XF86DGADirectGraphics);
	}

	if (!(dgafeat & (XF86DGADirectPresent | XF86DGADirectMouse))) {
		return false;
	}

	return true;
#else
	return false;
#endif // HAVE_DGA
}


/*
  VID_CheckVMode

  Check for the presence of the XFree86-VidMode X server extension
*/
bool
VID_CheckVMode (Display * dpy, int *maj_ver, int *min_ver)
{
#ifdef HAVE_VIDMODE
	int		dummy, dummy_major, dummy_minor, event_base, error_base;

	if (!XQueryExtension (dpy, XF86VIDMODENAME, &dummy, &dummy, &dummy)) {
		return false;
	}

	if (!XF86VidModeQueryExtension (dpy, &event_base, &error_base)) {
		return false;
	}

	if (!maj_ver)
		maj_ver = &dummy_major;
	if (!min_ver)
		min_ver = &dummy_minor;

	if (!XF86VidModeQueryVersion (dpy, maj_ver, min_ver))
		return false;

	if ((!maj_ver) || (*maj_ver != XF86VIDMODE_MAJOR_VERSION)) {
		Sys_MaskPrintf (SYS_vid, "VID: Incorrect VidMode version: %d.%d\n",
						*maj_ver, *min_ver);
		return false;
	}

	Sys_MaskPrintf (SYS_vid, "VID: VidMode version: %d.%d\n",
					*maj_ver, *min_ver);
	return true;
#else
	return false;
#endif // HAVE_VIDMODE
}
