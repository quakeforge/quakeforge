/*
	vid_x11_sw.c

	Software X11 video driver (8/32 bit)

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/ui/view.h"

#include "context_x11.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

int XShmGetEventBase (Display *x);	// for broken X11 headers

static GC		x_gc;

static bool doShm;
static XShmSegmentInfo x_shminfo[2];

static int	current_framebuffer;
static XImage *x_framebuffer[2] = { 0, 0 };

typedef unsigned char PIXEL8;
typedef unsigned short PIXEL16;
typedef unsigned int PIXEL24;

static PIXEL16	st2d_8to16table[256];
static PIXEL24	st2d_8to24table[256];

static byte current_palette[768];
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
x11_set_palette (sw_ctx_t *ctx, const byte *palette)
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

static void
st2_fixup (sw_ctx_t *ctx, XImage *framebuf, int x, int y, int width, int height)
{
	int 		xi, yi;
	unsigned char *src;
	PIXEL16 	*dest;
	sw_framebuffer_t *fb = ctx->framebuffer->buffer;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < (y + height); yi++) {
		src = &fb->color[yi * fb->rowbytes];
		dest = (PIXEL16 *) &framebuf->data[yi * framebuf->bytes_per_line];
		for (xi = x; xi < x + width; xi++) {
			dest[xi] = st2d_8to16table[src[xi]];
		}
	}
}

static void
st3_fixup (sw_ctx_t *ctx, XImage *framebuf, int x, int y, int width, int height)
{
	int 		yi;
	unsigned char *src;
	PIXEL24 	*dest;
	sw_framebuffer_t *fb = ctx->framebuffer->buffer;
	register int count, n;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < (y + height); yi++) {
		src = &fb->color[yi * fb->rowbytes + x];
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

static void
x11_put_image (vrect_t *rect)
{
	if (doShm) {
		if (!XShmPutImage (x_disp, x_win, x_gc,
						   x_framebuffer[current_framebuffer],
						   rect->x, rect->y, rect->x, rect->y,
						   rect->width, rect->height, True)) {
			Sys_Error ("VID_Update: XShmPutImage failed");
		}
		oktodraw = false;
		while (!oktodraw)
			X11_ProcessEvent ();

		current_framebuffer = !current_framebuffer;
	} else {
		if (XPutImage (x_disp, x_win, x_gc, x_framebuffer[0],
						rect->x, rect->y, rect->x, rect->y,
						rect->width, rect->height)) {
			Sys_Error ("VID_Update: XPutImage failed");
		}
	}
}

/*
	Flush the given rectangles from the view buffer to the screen.
*/
static void
x11_sw8_8_update (sw_ctx_t *ctx, vrect_t *rects)
{
	vrect_t     urect = *rects;
	while (rects->next) {
		rects = rects->next;
		int         minx = min (VRect_MinX (&urect), VRect_MinX (rects));
		int         miny = min (VRect_MinY (&urect), VRect_MinY (rects));
		int         maxx = max (VRect_MaxX (&urect), VRect_MaxX (rects));
		int         maxy = max (VRect_MaxY (&urect), VRect_MaxY (rects));
		urect.x = minx;
		urect.y = miny;
		urect.width = maxx - minx;
		urect.height = maxy - miny;
	}
	x11_put_image (&urect);
	XSync (x_disp, False);
	r_data->scr_fullupdate = 0;

	sw_framebuffer_t *fb = ctx->framebuffer->buffer;
	fb->color = (byte *) x_framebuffer[current_framebuffer]->data;
}

static void
x11_sw8_16_update (sw_ctx_t *ctx, vrect_t *rects)
{
	vrect_t     urect = *rects;
	st2_fixup (ctx, x_framebuffer[current_framebuffer],
			   rects->x, rects->y, rects->width, rects->height);
	while (rects->next) {
		rects = rects->next;
		st2_fixup (ctx, x_framebuffer[current_framebuffer],
				   rects->x, rects->y, rects->width, rects->height);
		int         minx = min (VRect_MinX (&urect), VRect_MinX (rects));
		int         miny = min (VRect_MinY (&urect), VRect_MinY (rects));
		int         maxx = max (VRect_MaxX (&urect), VRect_MaxX (rects));
		int         maxy = max (VRect_MaxY (&urect), VRect_MaxY (rects));
		urect.x = minx;
		urect.y = miny;
		urect.width = maxx - minx;
		urect.height = maxy - miny;
	}
	x11_put_image (&urect);
	XSync (x_disp, False);
	r_data->scr_fullupdate = 0;
}

static void
x11_sw8_24_update (sw_ctx_t *ctx, vrect_t *rects)
{
	vrect_t     urect = *rects;
	st3_fixup (ctx, x_framebuffer[current_framebuffer],
			   rects->x, rects->y, rects->width, rects->height);
	while (rects->next) {
		rects = rects->next;
		st3_fixup (ctx, x_framebuffer[current_framebuffer],
				   rects->x, rects->y, rects->width, rects->height);
		int         minx = min (VRect_MinX (&urect), VRect_MinX (rects));
		int         miny = min (VRect_MinY (&urect), VRect_MinY (rects));
		int         maxx = max (VRect_MaxX (&urect), VRect_MaxX (rects));
		int         maxy = max (VRect_MaxY (&urect), VRect_MaxY (rects));
		urect.x = minx;
		urect.y = miny;
		urect.width = maxx - minx;
		urect.height = maxy - miny;
	}
	x11_put_image (&urect);
	XSync (x_disp, False);
	r_data->scr_fullupdate = 0;
}
#if 0
static void
x11_sw16_16_update (sw_ctx_t *ctx, vrect_t *rects)
{
	uint16_t   *buffer = (uint16_t *) viddef.buffer;
	XImage     *framebuf = x_framebuffer[current_framebuffer];
	int         rowbytes = framebuf->bytes_per_line;

	while (rects) {
		int         x = rects->x;
		int         y = rects->y;
		int         width = rects->width;
		int         height = rects->height;
		for (int yi = y; yi < (y + height); yi++) {
			uint16_t    *src = buffer + yi * viddef.width + x;
			PIXEL16     *dest = (PIXEL16 *) (framebuf->data + yi * rowbytes) + x;

			for (int count = width; count-- > 0; ) {
				*dest++ = *src++;
			}
		}
		x11_put_image (rects);
		rects = rects->next;
	}
	XSync (x_disp, False);
	r_data->scr_fullupdate = 0;
}

static void
x11_sw16_32_update (sw_ctx_t *ctx, vrect_t *rects)
{
	uint16_t   *buffer = (uint16_t *) viddef.buffer;
	XImage     *framebuf = x_framebuffer[current_framebuffer];
	int         rowbytes = framebuf->bytes_per_line;

	while (rects) {
		int         x = rects->x;
		int         y = rects->y;
		int         width = rects->width;
		int         height = rects->height;
		for (int yi = y; yi < (y + height); yi++) {
			uint16_t    *src = buffer + yi * viddef.width + x;
			PIXEL24     *dest = (PIXEL24 *) (framebuf->data + yi * rowbytes) + x;

			for (int count = width; count-- > 0; ) {
				uint32_t    c = *src++;
				*dest++ = 0xff000000
						  | ((c & 0xf800) << 8)
						  | ((c & 0x07e0) << 5)
						  | ((c & 0x001f) << 3);
			}
		}
		x11_put_image (rects);
		rects = rects->next;
	}
	XSync (x_disp, False);
	r_data->scr_fullupdate = 0;
}

static void
x11_sw32_update (sw_ctx_t *ctx, vrect_t *rects)
{
	uint32_t   *buffer = (uint32_t *) viddef.buffer;
	XImage     *framebuf = x_framebuffer[current_framebuffer];
	int         rowbytes = framebuf->bytes_per_line;

	while (rects) {
		int         x = rects->x;
		int         y = rects->y;
		int         width = rects->width;
		int         height = rects->height;
		for (int yi = y; yi < (y + height); yi++) {
			uint32_t    *src = buffer + yi * viddef.width + x;
			PIXEL24     *dest = (PIXEL24 *) (framebuf->data + yi * rowbytes) + x;

			for (int count = width; count-- > 0; ) {
				*dest++ = *src++;
			}
		}
		x11_put_image (rects);
		rects = rects->next;
	}
	XSync (x_disp, False);
	r_data->scr_fullupdate = 0;
}
#endif
static void
x11_choose_visual (sw_ctx_t *ctx)
{
	qfZoneScoped (true);
	int         pnum, i;
	XVisualInfo template;
	int         num_visuals;
	int         template_mask;

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

	if (x_visinfo->depth == 8 && x_visinfo->class == PseudoColor)
		x_cmap = XCreateColormap (x_disp, x_win, x_vis, AllocAll);
	x_vis = x_visinfo->visual;

	ctx->update = x11_sw8_8_update;
	switch (x_visinfo->depth) {
		case 8:
			ctx->update = x11_sw8_8_update;
			break;
		case 16:
			ctx->update = x11_sw8_16_update;
			break;
		case 24:
			ctx->update = x11_sw8_24_update;
			break;
	}

	if (num_visuals > 1) {
		Sys_MaskPrintf (SYS_vid,
						"Found more than one visual id at depth %d:\n",
						template.depth);
		for (i = 0; i < num_visuals; i++)
			Sys_MaskPrintf (SYS_vid, "    -visualid %d\n",
							(int) x_visinfo[i].visualid);
	} else {
		if (num_visuals == 0) {
			if (template_mask == VisualIDMask) {
				Sys_Error ("VID: Bad visual ID %ld", template.visualid);
			} else {
				Sys_Error ("VID: No visuals at depth %d", template.depth);
			}
		}
	}

	Sys_MaskPrintf (SYS_vid, "Using visualid %d:\n",
					(int) x_visinfo->visualid);
	Sys_MaskPrintf (SYS_vid, "    class %d\n", x_visinfo->class);
	Sys_MaskPrintf (SYS_vid, "    screen %d\n", x_visinfo->screen);
	Sys_MaskPrintf (SYS_vid, "    depth %d\n", x_visinfo->depth);
	Sys_MaskPrintf (SYS_vid, "    red_mask 0x%x\n",
					(int) x_visinfo->red_mask);
	Sys_MaskPrintf (SYS_vid, "    green_mask 0x%x\n",
					(int) x_visinfo->green_mask);
	Sys_MaskPrintf (SYS_vid, "    blue_mask 0x%x\n",
					(int) x_visinfo->blue_mask);
	Sys_MaskPrintf (SYS_vid, "    colormap_size %d\n",
					x_visinfo->colormap_size);
	Sys_MaskPrintf (SYS_vid, "    bits_per_rgb %d\n",
					x_visinfo->bits_per_rgb);
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
	mem = ((viddef.width * pwidth + 7) & ~7) * viddef.height;
	buf = malloc (mem);
	SYS_CHECKMEM (buf);

	// allocate new screen buffer
	x_framebuffer[0] = XCreateImage (x_disp, x_vis, x_visinfo->depth,
									 ZPixmap, 0, buf, viddef.width,
									 viddef.height, 32, 0);

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
											  viddef.width, viddef.height);

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

		Sys_MaskPrintf (SYS_vid, "VID: shared memory id=%d, addr=0x%lx\n",
						x_shminfo[frm].shmid, (long) x_shminfo[frm].shmaddr);

		x_framebuffer[frm]->data = x_shminfo[frm].shmaddr;

		// get the X server to attach to it
		if (!XShmAttach (x_disp, &x_shminfo[frm]))
			Sys_Error ("VID: XShmAttach() failed");
		XSync (x_disp, 0);
		shmctl (x_shminfo[frm].shmid, IPC_RMID, 0);

	}
}

static sw_framebuffer_t swfb;
static framebuffer_t fb = { .buffer = &swfb };

static void
x11_init_buffers (void *data)
{
	sw_ctx_t   *ctx = data;

	ctx->framebuffer = &fb;

	if (doShm)
		ResetSharedFrameBuffers ();
	else
		ResetFrameBuffer ();

	current_framebuffer = 0;

	fb.width = viddef.width;
	fb.height = viddef.height;
	if (x_visinfo->depth != 8) {
		if (swfb.color)
			free (swfb.color);
		if (swfb.depth)
			free (swfb.depth);
		swfb.rowbytes = viddef.width;
		swfb.color = calloc (swfb.rowbytes, viddef.height);
		swfb.depth = 0;
		if (!swfb.color)
			Sys_Error ("Not enough memory for video mode");
	} else {
		swfb.rowbytes = x_framebuffer[current_framebuffer]->bytes_per_line;
		swfb.color = (byte *) x_framebuffer[current_framebuffer]->data;
		if (swfb.depth)
			free (swfb.depth);
		swfb.depth = 0;
	}
}

static void
x11_create_context (sw_ctx_t *ctx)
{
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

	viddef.vid_internal->init_buffers = x11_init_buffers;
//  XSynchronize (x_disp, False);
//	X11_AddEvent (x_shmeventtype, event_shm);
}

sw_ctx_t *
X11_SW_Context (vid_internal_t *vi)
{
	sw_ctx_t *ctx = calloc (1, sizeof (sw_ctx_t));
	ctx->set_palette = x11_set_palette;
	ctx->choose_visual = x11_choose_visual;
	ctx->create_context = x11_create_context;
	ctx->update = x11_sw8_8_update;

	vi->ctx = ctx;
	return ctx;
}

void
X11_SW_Init_Cvars (void)
{
}
