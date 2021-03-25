/*
	vid_fbdev.c

	Linux FBDev video routines

	based on vid_svgalib.c

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_IO_H
# include <sys/io.h>
#elif defined(HAVE_ASM_IO_H)
# include <asm/io.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sys.h"

#include "d_iface.h"
#include "fbset.h"
#include "vid_internal.h"

#ifndef PAGE_SIZE
# define PAGE_SIZE (sysconf(_SC_PAGESIZE))
# define PAGE_MASK (~(PAGE_SIZE-1))
#endif

static struct VideoMode current_mode;
static char current_name[32];
static int num_modes;

static int fb_fd = -1;
static int tty_fd = 0;

static byte vid_current_palette[768];

static int  fbdev_inited = 0;
static int  fbdev_backgrounded = 0;

static cvar_t *vid_redrawfull;
static cvar_t *vid_waitforrefresh;

static byte *framebuffer_ptr;

static byte backingbuf[48 * 24];

void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
	int i, j, reps, repshift, offset, off;

	if (!fbdev_inited || !viddef.direct || fbdev_backgrounded)
		return;

	if (viddef.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			offset = x + ((y << repshift) + i + j)
				* viddef.rowbytes;
			off = offset % 0x10000;
			memcpy (&backingbuf[(i + j) * 24], viddef.direct + off, width);
			memcpy (viddef.direct + off,
				&pbitmap[(i >> repshift) * width], width);
		}
	}
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
	int i, j, reps, repshift, offset, off;

	if (!fbdev_inited || !viddef.direct || fbdev_backgrounded)
		return;

	if (viddef.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			offset = x + ((y << repshift) + i + j)
				* viddef.rowbytes;
			off = offset % 0x10000;
			memcpy (viddef.direct + off, &backingbuf[(i + j) * 24], width);
		}
	}
}

static int
VID_NumModes (void)
{
	struct VideoMode *vmode;
	int i = 0;

	for (vmode = VideoModes; vmode; vmode = vmode->next)
		i++;

	return i;
}

static void
VID_fbset_f (void)
{
	int i, argc;
	const char *argv[32];

	argc = Cmd_Argc();
	if (argc > 32)
		argc = 32;
	argv[0] = "vid_fbset";
	for (i = 1; i < argc; i++) {
		argv[i] = Cmd_Argv(i);
	}
	fbset_main(argc, argv);
}

static void
VID_InitModes (void)
{
	ReadModeDB();
	num_modes = VID_NumModes();
}

static const char *
get_mode (unsigned int width, unsigned int height, unsigned int depth)
{
	struct VideoMode *vmode;

	for (vmode = VideoModes; vmode; vmode = vmode->next) {
		if (vmode->xres == width
			&& vmode->yres == height
			&& vmode->depth == depth)
			return vmode->name;
	}

	Sys_Printf ("Mode %dx%d (%d bits) not supported\n",
				width, height, depth);

	return "640x480-60";
}

static unsigned char *fb_map_addr = 0;
static unsigned long fb_map_length = 0;

static struct fb_var_screeninfo orig_var;

static void
VID_shutdown (void)
{
	Sys_MaskPrintf (SYS_VID, "VID_Shutdown\n");

	if (!fbdev_inited)
		return;

	if (munmap(fb_map_addr, fb_map_length) == -1) {
		Sys_Printf("could not unmap framebuffer at %p: %s\n",
				fb_map_addr, strerror(errno));
	} else {
		if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &orig_var))
			Sys_Printf ("failed to get var screen info\n");
	}
	close(fb_fd);

	ioctl(tty_fd, KDSETMODE, KD_TEXT);
	if (write(tty_fd, "\033]R", 3) != 3)    /* reset palette */
		Sys_Printf("could not reset palette: %s\n", strerror(errno));

	fbdev_inited = 0;
}

static void
loadpalette (unsigned short *red, unsigned short *green, unsigned short *blue)
{
	struct fb_cmap cmap;

	cmap.len   = 256;
	cmap.red   = red;
	cmap.green = green;
	cmap.blue  = blue;
	cmap.transp = NULL;
	cmap.start = 0;
	if (-1 == ioctl(fb_fd, FBIOPUTCMAP, (void *)&cmap))
		Sys_Error("ioctl FBIOPUTCMAP %s", strerror(errno));
}

static int
VID_SetMode (const char *name, unsigned char *palette)
{
	struct VideoMode *vmode;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	int err;
	//unsigned long smem_start;
	unsigned long smem_offset;
	long pagesize, pagemask;

	errno = 0;
	pagesize = sysconf(_SC_PAGESIZE);
	if (errno) {
		Con_Printf("Cannot get page size: %s\n", strerror(errno));
		return 0;
	}
	pagemask = ~(pagesize-1);

	vmode = FindVideoMode(name);
	if (!vmode) {
		// Sys_Printf ("No such video mode: %s\n", name);
		return 0;
	}

	current_mode = *vmode;
	strncpy(current_name, current_mode.name, sizeof(current_name)-1);
	current_name[31] = 0;
	viddef.width = vmode->xres;
	viddef.height = vmode->yres;
	viddef.rowbytes = vmode->xres * (vmode->depth >> 3);
	viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];
	viddef.conrowbytes = viddef.rowbytes;
	viddef.numpages = 1;

	if (fb_map_addr) {
		if (munmap(fb_map_addr, fb_map_length) == -1) {
			Sys_Printf("could not unmap framebuffer at %p: %s\n",
				fb_map_addr, strerror(errno));
		}
	}

	ConvertFromVideoMode(&current_mode, &var);
	err = ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var);
	if (err)
		Sys_Error ("Video mode failed: %s", name);
	ConvertToVideoMode(&var, &current_mode);
	current_mode.name = current_name;
	viddef.set_palette (palette);

	err = ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix);
	if (err)
		Sys_Error ("Video mode failed: %s", name);
	//smem_start = (unsigned long)fix.smem_start & pagemask;
	smem_offset = (unsigned long)fix.smem_start & ~pagemask;
	fb_map_length = (smem_offset+fix.smem_len+~pagemask) & pagemask;
	fb_map_addr = mmap(0, fb_map_length, PROT_WRITE, MAP_SHARED,
							   fb_fd, 0);
	if (!fb_map_addr)
		Sys_Error ("This mode isn't hapnin'");
	viddef.direct = framebuffer_ptr = fb_map_addr;

	// alloc screen buffer, z-buffer, and surface cache
	VID_InitBuffers ();

	if (!fbdev_inited) {
		fbdev_inited = 1;
	}

	/* Force a surface cache flush */
	viddef.recalc_refdef = 1;

	return 1;
}

static void
fb_switch_handler (int sig)
{
	if (sig == SIGUSR1) {
		fbdev_backgrounded = 1;
	} else if (sig == SIGUSR2) {
		fbdev_backgrounded = 2;
	}
}

static void
fb_switch_release (void)
{
	ioctl(tty_fd, VT_RELDISP, 1);
}

static void
fb_switch_acquire (void)
{
	ioctl(tty_fd, VT_RELDISP, VT_ACKACQ);
}

static void
fb_switch_init (void)
{
	struct sigaction act;
	struct vt_mode vtmode;

	memset(&act, 0, sizeof(act));
	act.sa_handler = fb_switch_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGUSR1, &act, 0);
	sigaction(SIGUSR2, &act, 0);

	if (ioctl(tty_fd, VT_GETMODE, &vtmode)) {
		Sys_Error("ioctl VT_GETMODE: %s", strerror(errno));
	}
	vtmode.mode = VT_PROCESS;
	vtmode.waitv = 0;
	vtmode.relsig = SIGUSR1;
	vtmode.acqsig = SIGUSR2;
	if (ioctl(tty_fd, VT_SETMODE, &vtmode)) {
		Sys_Error("ioctl VT_SETMODE: %s", strerror(errno));
	}
}

static void
VID_SetPalette (const byte *palette)
{
	static unsigned short tmppalr[256], tmppalg[256], tmppalb[256];
	unsigned short i, *tpr, *tpg, *tpb;

	if (!fbdev_inited || fbdev_backgrounded || fb_fd < 0)
		return;

	memcpy (vid_current_palette, palette, sizeof (vid_current_palette));

	if (current_mode.depth == 8) {
		tpr = tmppalr;
		tpg = tmppalg;
		tpb = tmppalb;
		for (i = 0; i < 256; i++) {
			*tpr++ = (*palette++) << 8;
			*tpg++ = (*palette++) << 8;
			*tpb++ = (*palette++) << 8;
		}

		loadpalette(tmppalr, tmppalg, tmppalb);
	}
}

void
VID_Init (byte *palette, byte *colormap)
{
	struct VideoMode *vmode;
	const char *modestr;
	const char *fbname;

	// plugin_load("in_fbdev.so");

	if (fbdev_inited)
		return;

	Sys_RegisterShutdown (VID_shutdown);

	R_LoadModule (0, VID_SetPalette);

	if (COM_CheckParm ("-novideo")) {
		viddef.width = 320;
		viddef.height = 200;
		viddef.rowbytes = 320;
		viddef.aspect = ((float) viddef.height
						 / (float) viddef.width) * (4.0 / 3.0);
		viddef.colormap8 = colormap;
		viddef.fullbright = 256 - viddef.colormap8[256 * VID_GRADES];
		viddef.conrowbytes = viddef.rowbytes;
		viddef.conwidth = viddef.width;
		viddef.conheight = viddef.height;
		viddef.numpages = 1;
		viddef.basepal = palette;
		Con_CheckResize (); // Now that we have a window size, fix console
		VID_InitBuffers ();
		return;
	}

	fbname = getenv("FRAMEBUFFER");
	if (!fbname)
		fbname = "/dev/fb0";

	fb_fd = open(fbname, O_RDWR);
	if (fb_fd < 0)
		Sys_Error ("failed to open fb device");

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &orig_var))
		Sys_Error ("failed to get var screen info");

	fb_switch_init();

	VID_InitModes ();

	Cmd_AddCommand ("vid_fbset", VID_fbset_f, "No Description");

	/* Interpret command-line params */
	VID_GetWindowSize (320, 200);

	modestr = get_mode (viddef.width, viddef.height, 8);

	/* Set viddef parameters */
	vmode = FindVideoMode(modestr);
	if (!vmode)
		Sys_Error("no video mode %s", modestr);
	current_mode = *vmode;
	ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
	VID_SetMode (current_mode.name, palette);

	VID_InitGamma (palette);
	viddef.set_palette (viddef.palette);

	viddef.initialized = true;
}

void
VID_Init_Cvars ()
{
	vid_redrawfull = Cvar_Get ("vid_redrawfull", "0", CVAR_NONE, NULL,
				"Redraw entire screen each frame instead of just dirty areas");
	vid_waitforrefresh = Cvar_Get ("vid_waitforrefresh", "0", CVAR_ARCHIVE,
			NULL, "Wait for vertical retrace before drawing next frame");
}

void
VID_Update (vrect_t *rects)
{
	if (!fbdev_inited)
		return;
	if (fbdev_backgrounded) {
		if (fbdev_backgrounded == 3) {
			return;
		} else if (fbdev_backgrounded == 2) {
			fb_switch_acquire();
			fbdev_backgrounded = 0;
			viddef.set_palette (vid_current_palette);
		} else if (fbdev_backgrounded == 1) {
			fb_switch_release();
			fbdev_backgrounded = 3;
			return;
		}
	}

	if (vid_waitforrefresh->int_val) {
		// ???
	}

	if (vid_redrawfull->int_val) {
		double *d = (double *)framebuffer_ptr, *s = (double *)viddef.buffer;
		double *ends = (double *)(viddef.buffer
								  + viddef.height*viddef.rowbytes);
		while (s < ends)
			*d++ = *s++;
	} else {
		while (rects) {
			int height, width, lineskip, i, j, xoff, yoff;
			double *d, *s;

			height = rects->height;
			width = rects->width / sizeof(double);
			xoff = rects->x;
			yoff = rects->y;
			lineskip = (viddef.width - (xoff + rects->width)) / sizeof(double);
			d = (double *)(framebuffer_ptr + yoff * viddef.rowbytes + xoff);
			s = (double *)(viddef.buffer + yoff * viddef.rowbytes + xoff);
			for (i = yoff; i < height; i++) {
				for (j = xoff; j < width; j++)
					*d++ = *s++;
				d += lineskip;
				s += lineskip;
			}
			rects = rects->next;
		}
	}
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
}

qboolean
VID_SetGamma (double gamma)
{
	return false;
}
