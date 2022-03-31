/*
	vid_svgalib.c

	Linux SVGALib video routines

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999-2000  David Symonds [xoxus@usa.net]
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
#ifdef HAVE_SYS_IO_H
# include <sys/io.h>
#elif defined(HAVE_ASM_IO_H)
# include <asm/io.h>
#endif

#include <stdio.h>
#include <vga.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "compat.h"
#include "d_iface.h"

static int   num_modes, current_mode;
static vga_modeinfo *modes;

static byte  vid_current_palette[768];
static byte  backingbuf[48 * 24];
static byte *framebuffer_ptr;

static int   svgalib_inited = 0;
static int   svgalib_backgrounded = 0;

static cvar_t *vid_redrawfull;
static cvar_t *vid_waitforrefresh;

int		 VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes, VGA_planar;
byte	*VGA_pagebase;
int		 VID_options_items = 0;


void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
	int			plane, reps, repshift, offset, vidpage, off, i, j, k;

	if (!svgalib_inited || !vid.direct || svgalib_backgrounded
		|| !vga_oktowrite ())
		return;

	if (vid.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	vidpage = 0;
	vga_setpage (0);

	if (VGA_planar) {
		for (plane = 0; plane < 4; plane++) {
			/* Select the correct plane for reading and writing */
			outb (0x02, 0x3C4);
			outb (1 << plane, 0x3C5);
			outb (4, 0x3CE);
			outb (plane, 0x3CF);

			for (i = 0; i < (height << repshift); i += reps) {
				for (k = 0; k < reps; k++) {
					for (j = 0; j < (width >> 2); j++) {
						backingbuf[(i + k) * 24 + (j << 2) + plane] =
							vid.direct[(y + i + k) * VGA_rowbytes +
									   (x >> 2) + j];
						vid.direct[(y + i + k) * VGA_rowbytes + (x >> 2) + j] =
							pbitmap[(i >> repshift) * 24 + (j << 2) + plane];
					}
				}
			}
		}
	} else {
		for (i = 0; i < (height << repshift); i += reps) {
			for (j = 0; j < reps; j++) {
				offset = x + ((y << repshift) + i + j) * vid.rowbytes;
				off = offset % 0x10000;
				if ((offset / 0x10000) != vidpage) {
					vidpage = offset / 0x10000;
					vga_setpage (vidpage);
				}
				memcpy (&backingbuf[(i + j) * 24], vid.direct + off, width);
				memcpy (vid.direct + off, &pbitmap[(i >> repshift) * width],
						width);
			}
		}
	}
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
	int			plane, reps, repshift, offset, vidpage, off, i, j, k;

	if (!svgalib_inited || !vid.direct || svgalib_backgrounded
		|| !vga_oktowrite ())
		return;

	if (vid.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	vidpage = 0;
	vga_setpage (0);

	if (VGA_planar) {
		for (plane = 0; plane < 4; plane++) {
			/* Select the correct plane for writing */
			outb (2, 0x3C4);
			outb (1 << plane, 0x3C5);
			outb (4, 0x3CE);
			outb (plane, 0x3CF);

			for (i = 0; i < (height << repshift); i += reps) {
				for (k = 0; k < reps; k++) {
					for (j = 0; j < (width >> 2); j++) {
						vid.direct[(y + i + k) * VGA_rowbytes + (x >> 2) + j] =
							backingbuf[(i + k) * 24 + (j << 2) + plane];
					}
				}
			}
		}
	} else {
		for (i = 0; i < (height << repshift); i += reps) {
			for (j = 0; j < reps; j++) {
				offset = x + ((y << repshift) + i + j) * vid.rowbytes;
				off = offset % 0x10000;
				if ((offset / 0x10000) != vidpage) {
					vidpage = offset / 0x10000;
					vga_setpage (vidpage);
				}
				memcpy (vid.direct + off, &backingbuf[(i + j) * 24], width);
			}
		}
	}
}

static void
VID_InitModes (void)
{
	int         i;

	/* Get complete information on all modes */
	num_modes = vga_lastmodenumber () + 1;
	modes = malloc (num_modes * sizeof (vga_modeinfo));
	SYS_CHECKMEM (modes);
	for (i = 0; i < num_modes; i++) {
		if (vga_hasmode (i)) {
			memcpy (&modes[i], vga_getmodeinfo (i), sizeof (vga_modeinfo));
		} else {
			modes[i].width = 0;			// means not available
		}
	}

	/* Filter for modes we don't support */
	for (i = 0; i < num_modes; i++) {
		if (modes[i].bytesperpixel != 1 && modes[i].colors != 256) {
			modes[i].width = 0;
		}
	}
}

static int
get_mode (int width, int height, int depth)
{
	int         i, ok, match;

	match = ((!!depth) << 2) + ((!!height) << 1) + (!!width);

	for (i = 0; i < num_modes; i++) {
		if (modes[i].width) {
			ok = ((modes[i].bytesperpixel == (depth / 8)) << 2)
				+ ((modes[i].height == height) << 1)
				+ (modes[i].width == width);
			if ((ok & match) == match)
				break;
		}
	}
	if (i == num_modes) {
		Sys_Printf ("Mode %dx%d (%d bits) not supported\n",
					width, height, depth);
		i = G320x200x256;
	}

	return i;
}

static void
VID_shutdown (void)
{
	Sys_MaskPrintf (SYS_vid, "VID_Shutdown\n");

	if (!svgalib_inited)
		return;

	vga_setmode (TEXT);
	svgalib_inited = 0;
}

void
VID_SetPalette (byte * palette)
{
	static int  tmppal[256 * 3];
	int        *tp;
	int         i;

	if (!svgalib_inited || svgalib_backgrounded)
		return;

	memcpy (vid_current_palette, palette, sizeof (vid_current_palette));

	if (vga_getcolors () == 256) {
		tp = tmppal;
		for (i = 256 * 3; i; i--) {
			*(tp++) = *(palette++) >> 2;
		}

		if (vga_oktowrite ()) {
			vga_setpalvec (0, 256, tmppal);
		}
	}
}

static int
VID_SetMode (int modenum, unsigned char *palette)
{
	int         err;

	if ((modenum >= num_modes) || (modenum < 0) || !modes[modenum].width) {
		Sys_Printf ("No such video mode: %d\n", modenum);

		return 0;
	}

	current_mode = modenum;

	vid.width = modes[current_mode].width;
	vid.height = modes[current_mode].height;

	VGA_width = modes[current_mode].width;
	VGA_height = modes[current_mode].height;
	VGA_planar = modes[current_mode].bytesperpixel == 0;
	VGA_rowbytes = modes[current_mode].linewidth;
	vid.rowbytes = modes[current_mode].linewidth;
	if (VGA_planar) {
		VGA_bufferrowbytes = modes[current_mode].linewidth * 4;
		vid.rowbytes = modes[current_mode].linewidth * 4;
	}

	vid.colormap8 = (byte *) vid_colormap;
	vid.fullbright = 256 - vid.colormap8[256 * VID_GRADES];
	vid.conrowbytes = vid.rowbytes;
	vid.numpages = 1;

	// alloc screen buffer, z-buffer, and surface cache
	VID_InitBuffers ();

	/* get goin' */
	err = vga_setmode (current_mode);
	if (err) {
		Sys_Error ("Video mode failed: %d", modenum);
	}
	VID_SetPalette (palette);

	VGA_pagebase = vid.direct = framebuffer_ptr = vga_getgraphmem ();
#if 0
	if (vga_setlinearaddressing () > 0) {
		framebuffer_ptr = (char *) vga_getgraphmem ();
	}
#endif
	if (!framebuffer_ptr) {
		Sys_Error ("This mode isn't hapnin'");
	}

	vga_setpage (0);

	svgalib_inited = 1;

	/* Force a surface cache flush */
	vid.recalc_refdef = 1;

	return 1;
}

static void
goto_background (void)
{
	svgalib_backgrounded = 1;
}

static void
comefrom_background (void)
{
	svgalib_backgrounded = 0;
}

void
VID_Init (byte *palette, byte *colormap)
{
	int         err;

	// plugin_load("in_svgalib.so");

	if (svgalib_inited)
		return;

	Sys_RegisterShutdown (VID_shutdown);

	err = vga_init ();
	if (err)
		Sys_Error ("SVGALib failed to allocate a new VC");

	if (vga_runinbackground_version () == 1) {
		Sys_Printf ("SVGALIB background support detected\n");
		vga_runinbackground (VGA_GOTOBACK, goto_background);
		vga_runinbackground (VGA_COMEFROMBACK, comefrom_background);
		vga_runinbackground (1);
	} else {
		vga_runinbackground (0);
	}

	VID_InitModes ();

	/* Interpret command-line params */
	VID_GetWindowSize (640, 480);

	current_mode = get_mode (vid.width, vid.height, 8);

	/* Set vid parameters */
	vid_colormap = colormap;
	VID_SetMode (current_mode, palette);

	VID_InitGamma (palette);
	VID_SetPalette (vid.palette);

	vid.initialized = true;
}

void
VID_Init_Cvars ()
{
	vid_redrawfull = Cvar_Get ("vid_redrawfull", "0", CVAR_NONE, NULL,
							   "Redraw entire screen each frame instead of "
							   "just dirty areas");
	vid_waitforrefresh = Cvar_Get ("vid_waitforrefresh", "0", CVAR_ARCHIVE,
								   NULL, "Wait for vertical retrace before "
								   "drawing next frame");
	vid_system_gamma = Cvar_Get ("vid_system_gamma", "1", CVAR_ARCHIVE, NULL,
								 "Use system gamma control if available");
}

void
VID_Update (vrect_t *rects)
{
	if (!svgalib_inited || svgalib_backgrounded)
		return;

	if (!vga_oktowrite ()) {
		/* Can't update screen if it's not active */
		return;
	}

	if (vid_waitforrefresh->int_val) {
		vga_waitretrace ();
	}

	if (VGA_planar) {
		VGA_UpdatePlanarScreen (vid.buffer);
	} else if (vid_redrawfull->int_val) {
		int         total = vid.rowbytes * vid.height;
		int         offset;

		for (offset = 0; offset < total; offset += 0x10000) {
			vga_setpage (offset / 0x10000);
			memcpy (framebuffer_ptr, vid.buffer + offset,
					((total - offset > 0x10000) ? 0x10000 : (total - offset)));
		}
	} else {
		int         offset, ycount;
		int         vidpage = 0;

		vga_setpage (0);

		while (rects) {
			ycount = rects->height;
			offset = rects->y * vid.rowbytes + rects->x;
			while (ycount--) {
				register int i = offset % 0x10000;

				if ((offset / 0x10000) != vidpage) {
					vidpage = offset / 0x10000;
					vga_setpage (vidpage);
				}
				if (rects->width + i > 0x10000) {
					memcpy (framebuffer_ptr + i, vid.buffer + offset,
							0x10000 - i);
					vga_setpage (++vidpage);
					memcpy (framebuffer_ptr, vid.buffer + offset + 0x10000 - i,
							rects->width - 0x10000 + i);
				} else {
					memcpy (framebuffer_ptr + i, vid.buffer + offset,
							rects->width);
				}
				offset += vid.rowbytes;
			}
			rects = rects->next;
		}
	}
}

void
VID_SetCaption (const char *text)
{
}

qboolean
VID_SetGamma (double gamma)
{
	return false;
}

#if defined(i386) && defined(__GLIBC__) && (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 2))
void
outb (unsigned char val, unsigned short port)
{
	asm ("outb %b0, %w1" : :"a"(val), "d"(port));
}
#elif defined(__FreeBSD__)
static inline void
outb (unsigned char value, unsigned short port)
{
	__asm__ __volatile__ ("outb %b0,%w1"::"a" (value), "d" (port));
}
#endif
