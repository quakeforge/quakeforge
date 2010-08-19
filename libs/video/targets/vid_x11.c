/*
	vid_x11.c

	General X11 video driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  contributors of the QuakeForge project
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
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
#define _BSD

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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>

#ifdef HAVE_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "compat.h"
#include "context_x11.h"
#include "d_iface.h"
#include "dga_check.h"

int XShmGetEventBase (Display *x);	// for broken X11 headers

static Colormap x_cmap;
static GC		x_gc;

static qboolean doShm;
static XShmSegmentInfo x_shminfo[2];

static int	current_framebuffer;
static XImage *x_framebuffer[2] = { 0, 0 };

static int	verbose = 0;

int 		VID_options_items = 1;

static byte current_palette[768];

typedef unsigned char PIXEL8;
typedef unsigned short PIXEL16;
typedef unsigned int PIXEL24;

static PIXEL16	st2d_8to16table[256];
static PIXEL24	st2d_8to24table[256];
static int		shiftmask_fl = 0;
static long 	r_shift, g_shift, b_shift;
static unsigned long r_mask, g_mask, b_mask;


static void
shiftmask_init (void)
{
	unsigned long long x;

	r_mask = x_vis->red_mask;
	g_mask = x_vis->green_mask;
	b_mask = x_vis->blue_mask;

	for (r_shift = -8, x = 1; x < r_mask; x <<= 1)
		r_shift++;
	for (g_shift = -8, x = 1; x < g_mask; x <<= 1)
		g_shift++;
	for (b_shift = -8, x = 1; x < b_mask; x <<= 1)
		b_shift++;
	shiftmask_fl = 1;
}

static PIXEL16
xlib_rgb16 (int r, int g, int b)
{
	PIXEL16 	p = 0;

	if (!shiftmask_fl)
		shiftmask_init ();

	if (r_shift > 0) {
		p = (r << (r_shift)) & r_mask;
	} else {
		if (r_shift < 0) {
			p = (r >> (-r_shift)) & r_mask;
		} else {
			p |= (r & r_mask);
		}
	}

	if (g_shift > 0) {
		p |= (g << (g_shift)) & g_mask;
	} else {
		if (g_shift < 0) {
			p |= (g >> (-g_shift)) & g_mask;
		} else {
			p |= (g & g_mask);
		}
	}

	if (b_shift > 0) {
		p |= (b << (b_shift)) & b_mask;
	} else {
		if (b_shift < 0) {
			p |= (b >> (-b_shift)) & b_mask;
		} else {
			p |= (b & b_mask);
		}
	}

	return p;
}

static PIXEL24
xlib_rgb24 (int r, int g, int b)
{
	PIXEL24 	p = 0;

	if (!shiftmask_fl)
		shiftmask_init ();

	if (r_shift > 0) {
		p = (r << (r_shift)) & r_mask;
	} else {
		if (r_shift < 0) {
			p = (r >> (-r_shift)) & r_mask;
		} else {
			p |= (r & r_mask);
		}
	}

	if (g_shift > 0) {
		p |= (g << (g_shift)) & g_mask;
	} else {
		if (g_shift < 0) {
			p |= (g >> (-g_shift)) & g_mask;
		} else {
			p |= (g & g_mask);
		}
	}

	if (b_shift > 0) {
		p |= (b << (b_shift)) & b_mask;
	} else {
		if (b_shift < 0) {
			p |= (b >> (-b_shift)) & b_mask;
		} else {
			p |= (b & b_mask);
		}
	}

	return p;
}

static void
st2_fixup (XImage *framebuf, int x, int y, int width, int height)
{
	int 		xi, yi;
	unsigned char *src;
	PIXEL16 	*dest;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < (y + height); yi++) {
		src = &((byte *)vid.buffer)[yi * vid.width];
		dest = (PIXEL16 *) &framebuf->data[yi * framebuf->bytes_per_line];
		for (xi = x; xi < x + width; xi++) {
			dest[xi] = st2d_8to16table[src[xi]];
		}
	}
}

static void
st3_fixup (XImage * framebuf, int x, int y, int width, int height)
{
	int 		yi;
	unsigned char *src;
	PIXEL24 	*dest;
	register int count, n;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < (y + height); yi++) {
		src = &((byte *)vid.buffer)[yi * vid.width + x];
		dest = (PIXEL24 *) &framebuf->data[yi * framebuf->bytes_per_line + x];

		// Duff's Device
		count = width;
		n = (count + 7) / 8;

		switch (count % 8) {
			case 0:
				do {
					*dest++ = st2d_8to24table[*src++];
			case 7:
					*dest++ = st2d_8to24table[*src++];
			case 6:
					*dest++ = st2d_8to24table[*src++];
			case 5:
					*dest++ = st2d_8to24table[*src++];
			case 4:
					*dest++ = st2d_8to24table[*src++];
			case 3:
					*dest++ = st2d_8to24table[*src++];
			case 2:
					*dest++ = st2d_8to24table[*src++];
			case 1:
					*dest++ = st2d_8to24table[*src++];
				} while (--n > 0);
		}
	}
}

void
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported
}

static void
ResetFrameBuffer (void)
{
	int         mem, pwidth;
	char       *buf;

	if (x_framebuffer[0]) {
		XDestroyImage (x_framebuffer[0]);
	}

	pwidth = x_visinfo->depth / 8;

	if (pwidth == 3)
		pwidth = 4;
	mem = ((vid.width * pwidth + 7) & ~7) * vid.height;
	buf = malloc (mem);
	SYS_CHECKMEM (buf);

	// allocate new screen buffer
	x_framebuffer[0] = XCreateImage (x_disp, x_vis, x_visinfo->depth,
									 ZPixmap, 0, buf, vid.width,
									 vid.height, 32, 0);

	if (!x_framebuffer[0]) {
		Sys_Error ("VID: XCreateImage failed");
	}
}

static void
ResetSharedFrameBuffers (void)
{
	int 	size;
	int 	key;
	int 	minsize = getpagesize ();
	int 	frm;

	for (frm = 0; frm < 2; frm++) {

		// free up old frame buffer memory
		if (x_framebuffer[frm]) {
			XShmDetach (x_disp, &x_shminfo[frm]);
			free (x_framebuffer[frm]);
			shmdt (x_shminfo[frm].shmaddr);
		}
		// create the image
		x_framebuffer[frm] = XShmCreateImage (x_disp, x_vis, x_visinfo->depth,
											  ZPixmap, 0, &x_shminfo[frm],
											  vid.width, vid.height);

		// grab shared memory
		size = x_framebuffer[frm]->bytes_per_line * x_framebuffer[frm]->height;

		if (size < minsize)
			Sys_Error ("VID: Window must use at least %d bytes", minsize);

		key = random ();
		x_shminfo[frm].shmid = shmget ((key_t) key, size, IPC_CREAT | 0777);
		if (x_shminfo[frm].shmid == -1)
			Sys_Error ("VID: Could not get any shared memory (%s)",
					   strerror (errno));

		// attach to the shared memory segment
		x_shminfo[frm].shmaddr = (void *) shmat (x_shminfo[frm].shmid, 0, 0);

		Sys_DPrintf ("VID: shared memory id=%d, addr=0x%lx\n",
					 x_shminfo[frm].shmid, (long) x_shminfo[frm].shmaddr);

		x_framebuffer[frm]->data = x_shminfo[frm].shmaddr;

		// get the X server to attach to it
		if (!XShmAttach (x_disp, &x_shminfo[frm]))
			Sys_Error ("VID: XShmAttach() failed");
		XSync (x_disp, 0);
		shmctl (x_shminfo[frm].shmid, IPC_RMID, 0);

	}
}

static void
x11_init_buffers (void)
{
	if (doShm)
		ResetSharedFrameBuffers ();
	else
		ResetFrameBuffer ();

	current_framebuffer = 0;

	vid.direct = 0;
	vid.rowbytes = vid.width;
	if (x_visinfo->depth != 8) {
		if (vid.buffer)
			free (vid.buffer);
		vid.buffer = calloc (vid.width, vid.height);
		if (!vid.buffer)
			Sys_Error ("Not enough memory for video mode");
	} else {
		vid.buffer = x_framebuffer[current_framebuffer]->data;
	}
	vid.conbuffer = vid.buffer;

	vid.conrowbytes = vid.rowbytes;
	Con_CheckResize (); // Now that we have a window size, fix console
}

static void
VID_Center_f (void)
{
	X11_ForceViewPort ();
}

/*
	VID_Init

	Set up color translation tables and the window.  Takes a 256-color 8-bit
	palette.  Palette data will go away after the call, so copy it if you'll
	need it later.
*/
void
VID_Init (unsigned char *palette)
{
	int         pnum, i;
	XVisualInfo template;
	int         num_visuals;
	int         template_mask;

	Cmd_AddCommand ("vid_center", VID_Center_f, "Center the view port on the "
					"quake window in a virtual desktop.\n");

	VID_GetWindowSize (320, 200);

	vid.width = vid_width->int_val;
	vid.height = vid_height->int_val;

	vid.numpages = 2;
	vid.colormap8 = vid_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap8 + 2048));

	srandom (getpid ());

	verbose = COM_CheckParm ("-verbose");

	// open the display
	X11_OpenDisplay ();

	template_mask = 0;

	// specify a visual id
	if ((pnum = COM_CheckParm ("-visualid"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -visualid <id#>");
		template.visualid = atoi (com_argv[pnum + 1]);
		template_mask = VisualIDMask;
	} else {							// If not specified, use default
										// visual
		template.visualid =
			XVisualIDFromVisual (XDefaultVisual (x_disp, x_screen));
		template_mask = VisualIDMask;
	}

	// pick a visual -- warn if more than one was available
	x_visinfo = XGetVisualInfo (x_disp, template_mask, &template,
								&num_visuals);
	x_vis = x_visinfo->visual;

	if (num_visuals > 1) {
		Sys_DPrintf ("Found more than one visual id at depth %d:\n",
				template.depth);
		for (i = 0; i < num_visuals; i++)
			Sys_DPrintf ("    -visualid %d\n", (int) x_visinfo[i].visualid);
	} else {
		if (num_visuals == 0) {
			if (template_mask == VisualIDMask) {
				Sys_Error ("VID: Bad visual ID %ld", template.visualid);
			} else {
				Sys_Error ("VID: No visuals at depth %d", template.depth);
			}
		}
	}

	if (verbose) {
		Sys_DPrintf ("Using visualid %d:\n", (int) x_visinfo->visualid);
		Sys_DPrintf ("    class %d\n", x_visinfo->class);
		Sys_DPrintf ("    screen %d\n", x_visinfo->screen);
		Sys_DPrintf ("    depth %d\n", x_visinfo->depth);
		Sys_DPrintf ("    red_mask 0x%x\n", (int) x_visinfo->red_mask);
		Sys_DPrintf ("    green_mask 0x%x\n", (int) x_visinfo->green_mask);
		Sys_DPrintf ("    blue_mask 0x%x\n", (int) x_visinfo->blue_mask);
		Sys_DPrintf ("    colormap_size %d\n", x_visinfo->colormap_size);
		Sys_DPrintf ("    bits_per_rgb %d\n", x_visinfo->bits_per_rgb);
	}

	/* Setup attributes for main window */
	X11_SetVidMode (vid.width, vid.height);

	/* Create the main window */
	X11_CreateWindow (vid.width, vid.height);

	/* Invisible cursor */
	X11_CreateNullCursor ();

	if (x_visinfo->depth == 8) {
		/* Create and upload the palette */
		if (x_visinfo->class == PseudoColor) {
			x_cmap = XCreateColormap (x_disp, x_win, x_vis, AllocAll);
			VID_SetPalette (palette);
			XSetWindowColormap (x_disp, x_win, x_cmap);
		}
	}

	VID_InitGamma (palette);
	VID_SetPalette (vid.palette);

	// create the GC
	{
		XGCValues   xgcvalues;
		int         valuemask = GCGraphicsExposures;

		xgcvalues.graphics_exposures = False;
		x_gc = XCreateGC (x_disp, x_win, valuemask, &xgcvalues);
	}

	// even if MITSHM is available, make sure it's a local connection
	if (XShmQueryExtension (x_disp)) {
		char       *displayname;
		char       *d;

		doShm = true;

		if ((displayname = XDisplayName (NULL))) {
			if ((d = strchr (displayname, ':')))
				*d = '\0';

			if (!(!strcasecmp (displayname, "unix") || !*displayname))
				doShm = false;
		}
	}

	if (doShm) {
		x_shmeventtype = XShmGetEventBase (x_disp) + ShmCompletion;
	}

	vid.do_screen_buffer = x11_init_buffers;
	VID_InitBuffers ();

//  XSynchronize (x_disp, False);
//	X11_AddEvent (x_shmeventtype, event_shm);

	vid.initialized = true;
}

void
VID_Init_Cvars ()
{
	X11_Init_Cvars ();
}

void
VID_SetPalette (unsigned char *palette)
{
	int         i;
	XColor      colors[256];

	for (i = 0; i < 256; i++) {
		st2d_8to16table[i] = xlib_rgb16 (palette[i * 3], palette[i * 3 + 1],
										 palette[i * 3 + 2]);
		st2d_8to24table[i] = xlib_rgb24 (palette[i * 3], palette[i * 3 + 1],
										 palette[i * 3 + 2]);
	}

	if (x_visinfo->class == PseudoColor && x_visinfo->depth == 8) {
		if (palette != current_palette) {
			memcpy (current_palette, palette, 768);
		}
		for (i = 0; i < 256; i++) {
			colors[i].pixel = i;
			colors[i].flags = DoRed | DoGreen | DoBlue;
			colors[i].red = palette[(i * 3)] << 8;
			colors[i].green = palette[(i * 3) + 1] << 8;
			colors[i].blue = palette[(i * 3) + 2] << 8;
		}
		XStoreColors (x_disp, x_cmap, colors, 256);
	}
}

/*
	VID_Shutdown

	Restore video mode
*/
void
VID_Shutdown (void)
{
	Sys_DPrintf ("VID_Shutdown\n");
	X11_CloseDisplay ();
}

static int  config_notify = 0;
static int  config_notify_width;
static int  config_notify_height;

/*
	VID_Update

	Flush the given rectangles from the view buffer to the screen.
*/
void
VID_Update (vrect_t *rects)
{
	/* If the window changes dimension, skip this frame. */
	if (config_notify) {
		fprintf (stderr, "config notify\n");
		config_notify = 0;
		vid.width = config_notify_width & ~7;
		vid.height = config_notify_height;

		VID_InitBuffers ();

		vid.recalc_refdef = 1;			/* force a surface cache flush */
		Con_CheckResize ();
		return;
	}

	while (rects) {
		switch (x_visinfo->depth) {
			case 16:
				st2_fixup (x_framebuffer[current_framebuffer],
						   rects->x, rects->y, rects->width, rects->height);
				break;
			case 24:
				st3_fixup (x_framebuffer[current_framebuffer],
						   rects->x, rects->y, rects->width, rects->height);
				break;
		}
		if (doShm) {
			if (!XShmPutImage (x_disp, x_win, x_gc,
							   x_framebuffer[current_framebuffer],
							   rects->x, rects->y, rects->x, rects->y,
							   rects->width, rects->height, True)) {
				Sys_Error ("VID_Update: XShmPutImage failed");
			}
			oktodraw = false;
			while (!oktodraw)
				X11_ProcessEvent ();
			rects = rects->pnext;

			current_framebuffer = !current_framebuffer;
		} else {
			if (XPutImage (x_disp, x_win, x_gc, x_framebuffer[0],
							rects->x, rects->y, rects->x, rects->y,
							rects->width, rects->height)) {
				Sys_Error ("VID_Update: XPutImage failed");
			}
			rects = rects->pnext;
		}
	}
	XSync (x_disp, False);
	scr_fullupdate = 0;
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

		X11_SetCaption (va ("%s: %s", PACKAGE_STRING, temp));
		free (temp);
	} else {
		X11_SetCaption (va ("%s", PACKAGE_STRING));
	}
}

qboolean
VID_SetGamma (double gamma)
{
	return X11_SetGamma (gamma);
}
