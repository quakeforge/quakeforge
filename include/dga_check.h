/*
	dga_check.h

	Definitions for XFree86 DGA and VidMode support

	Copyright (C) 2000	Jeff Teunissen <deek@dusknet.dhs.org>
	Copyright (C) 2000	Marcus Sundberg [mackan@stacken.kth.se]

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

#ifndef __dga_check_h_
#define __dga_check_h_

#include <X11/Xlib.h>

#include "QF/qtypes.h"

/*
  VID_CheckDGA

  Check for the presence of the XFree86-DGA support in the X server
*/
qboolean VID_CheckDGA (Display *, int *, int *, int *)
#ifndef HAVE_DGA	// FIXME
	__attribute__((const))
#endif
	;


/*
  VID_CheckVMode

  Check for the presence of the XFree86-VMode X server extension
*/
qboolean VID_CheckVMode (Display *, int *, int *)
#ifndef HAVE_DGA	// FIXME
	__attribute__((const))
#endif
	;

#endif	// __dga_check_h_
