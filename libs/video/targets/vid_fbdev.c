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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_IO_H
# include <sys/io.h>
#elif defined(HAVE_ASM_IO_H)
# include <asm/io.h>
#endif
#include <stdlib.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/page.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <math.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "fbset.h"

extern void ReadModeDB(void);
extern struct VideoMode *FindVideoMode(const char *name);
void ConvertFromVideoMode(const struct VideoMode *vmode,
			 struct fb_var_screeninfo *var);
void ConvertToVideoMode(const struct fb_var_screeninfo *var,
		       struct VideoMode *vmode);

extern struct VideoMode *VideoModes;
static struct VideoMode current_mode;
static char current_name[32];
static int num_modes;

static int fb_fd = -1;
static int tty_fd = 0;

static byte vid_current_palette[768];

static int  fbdev_inited = 0;
static int  fbdev_backgrounded = 0;
static int  UseDisplay = 1;

static cvar_t *vid_mode;
static cvar_t *vid_redrawfull;
static cvar_t *vid_waitforrefresh;

static char *framebuffer_ptr;

static byte backingbuf[48 * 24];

void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
	int i, j, reps, repshift, offset, off;

	if (!fbdev_inited || !vid.direct || fbdev_backgrounded)
		return;

	if (vid.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			offset = x + ((y << repshift) + i + j)
				* vid.rowbytes;
			off = offset % 0x10000;
			memcpy (&backingbuf[(i + j) * 24], vid.direct + off, width);
			memcpy (vid.direct + off,
				&pbitmap[(i >> repshift) * width], width);
		}
	}
}


void
D_EndDirectRect (int x, int y, int width, int height)
{
	int i, j, reps, repshift, offset, off;

	if (!fbdev_inited || !vid.direct || fbdev_backgrounded)
		return;

	if (vid.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	for (i = 0; i < (height << repshift); i += reps) {
		for (j = 0; j < reps; j++) {
			offset = x + ((y << repshift) + i + j)
				* vid.rowbytes;
			off = offset % 0x10000;
			memcpy (vid.direct + off, &backingbuf[(i + j) * 24], width);
		}
	}
}


static void
VID_DescribeMode_f (void)
{
	char *modestr;
	struct VideoMode *vmode;

	modestr = Cmd_Argv(1);
	vmode = FindVideoMode(modestr);
	if (!vmode) {
		Con_Printf ("Invalid video mode: %s!\n", modestr);
		return;
	}
	Con_Printf ("%s: %d x %d - %d bpp - %5.3f Hz\n", vmode->name,
			vmode->xres, vmode->yres, vmode->depth, vmode->vrate);
}


static void
VID_DescribeModes_f (void)
{
	struct VideoMode *vmode;

	for (vmode = VideoModes; vmode; vmode = vmode->next) {
		Con_Printf ("%s: %d x %d - %d bpp - %5.3f Hz\n", vmode->name,
				vmode->xres, vmode->yres, vmode->depth, vmode->vrate);
	}
}


/*
	VID_NumModes
*/
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
VID_NumModes_f (void)
{
	Con_Printf ("%d modes\n", VID_NumModes ());
}

int VID_SetMode (char *name, unsigned char *palette);

extern void fbset_main (int argc, char **argv);

static void
VID_fbset_f (void)
{
	int i, argc;
	char *argv[32];

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
VID_Debug_f (void)
{
	Con_Printf ("mode: %s\n", current_mode.name);
	Con_Printf ("height x width: %d x %d\n", current_mode.xres, current_mode.yres);
	Con_Printf ("bpp: %d\n", current_mode.depth);
	Con_Printf ("vrate: %5.3f\n", current_mode.vrate);
	Con_Printf ("vid.aspect: %f\n", vid.aspect);
}


static void
VID_InitModes (void)
{
	ReadModeDB();
	num_modes = VID_NumModes();
}


static char *
get_mode (char *name, int width, int height, int depth)
{
	struct VideoMode *vmode;

	for (vmode = VideoModes; vmode; vmode = vmode->next) {
		if (name) {
			if (!strcmp(vmode->name, name))
				return name;
		} else {
			if (vmode->xres == width
			    && vmode->yres == height
			    && vmode->depth == depth)
				return vmode->name;
		}
	}

	Sys_Printf ("Mode %dx%d (%d bits) not supported\n",
			width, height, depth);

	return "640x480-60";
}


static unsigned char *fb_map_addr = 0;
static unsigned long fb_map_length = 0;

static struct fb_var_screeninfo orig_var;

void
VID_Shutdown (void)
{
	Sys_Printf ("VID_Shutdown\n");

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

	if (UseDisplay) {
		ioctl(tty_fd, KDSETMODE, KD_TEXT);
		write(tty_fd, "\033]R", 3);	/* reset palette */
	}

	fbdev_inited = 0;
}


void
VID_ShiftPalette (unsigned char *p)
{
	VID_SetPalette (p);
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
		Sys_Error("ioctl FBIOPUTCMAP %s\n", strerror(errno));
}

void
VID_SetPalette (byte * palette)
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

		if (UseDisplay) {
			loadpalette(tmppalr, tmppalg, tmppalb);
		}
	}
}

int
VID_SetMode (char *name, unsigned char *palette)
{
	struct VideoMode *vmode;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	int err;
	unsigned long smem_start, smem_offset;

	vmode = FindVideoMode(name);
	if (!vmode) {
		Cvar_Set (vid_mode, current_mode.name);
		// Con_Printf ("No such video mode: %s\n", name);
		return 0;
	}

	current_mode = *vmode;
	Cvar_Set (vid_mode, current_mode.name);
	strncpy(current_name, current_mode.name, sizeof(current_name)-1);
	current_name[31] = 0;
	vid.width = vmode->xres;
	vid.height = vmode->yres;
	vid.rowbytes = vmode->xres * (vmode->depth >> 3);
	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.colormap = (pixel_t *) vid_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	vid.numpages = 1;

	if (fb_map_addr) {
		if (munmap(fb_map_addr, fb_map_length) == -1) {
			Sys_Printf("could not unmap framebuffer at %p: %s\n",
				fb_map_addr, strerror(errno));
		}
	}

	ConvertFromVideoMode(&current_mode, &var);
	err = ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var);
	if (err)
		Sys_Error ("Video mode failed: %s\n", name);
	ConvertToVideoMode(&var, &current_mode);
	current_mode.name = current_name;
	VID_SetPalette (palette);

	err = ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix);
	if (err)
		Sys_Error ("Video mode failed: %s\n", name);
	smem_start = (unsigned long)fix.smem_start & PAGE_MASK;
	smem_offset = (unsigned long)fix.smem_start & ~PAGE_MASK;
	fb_map_length = (smem_offset+fix.smem_len+~PAGE_MASK) & PAGE_MASK;
	fb_map_addr = (char *)mmap(0, fb_map_length, PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if (!fb_map_addr)
		Sys_Error ("This mode isn't hapnin'\n");
	vid.direct = framebuffer_ptr = fb_map_addr;

	// alloc screen buffer, z-buffer, and surface cache
	VID_InitBuffers ();

	if (!fbdev_inited) {
		fbdev_inited = 1;
	}

	/* Force a surface cache flush */
	vid.recalc_refdef = 1;

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
		Sys_Error("ioctl VT_GETMODE: %s\n", strerror(errno));
	}
	vtmode.mode = VT_PROCESS;
	vtmode.waitv = 0;
	vtmode.relsig = SIGUSR1;
	vtmode.acqsig = SIGUSR2;
	if (ioctl(tty_fd, VT_SETMODE, &vtmode)) {
		Sys_Error("ioctl VT_SETMODE: %s\n", strerror(errno));
	}
}

void
VID_Init (unsigned char *palette)
{
	int w, h, d;
	struct VideoMode *vmode;
	char *modestr;
	char *fbname;

	// plugin_load("in_fbdev.so");

	if (fbdev_inited)
		return;

	if (UseDisplay) {
		fbname = getenv("FRAMEBUFFER");
		if (!fbname)
			fbname = "/dev/fb0";

		fb_fd = open(fbname, O_RDWR);
		if (fb_fd < 0)
			Sys_Error ("failed to open fb device\n");

		if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &orig_var))
			Sys_Error ("failed to get var screen info\n");

		fb_switch_init();

		VID_InitModes ();

		Cmd_AddCommand ("vid_nummodes", VID_NumModes_f, "No Description");
		Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f, "No Description");
		Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f, "No Description");
		Cmd_AddCommand ("vid_debug", VID_Debug_f, "No Description");
		Cmd_AddCommand ("vid_fbset", VID_fbset_f, "No Description");

		/* Interpret command-line params */
		w = h = d = 0;
		if (getenv ("GFBDEVMODE")) {
			modestr = get_mode (getenv ("GFBDEVMODE"), w, h, d);
		} else if (COM_CheckParm ("-mode")) {
			modestr = get_mode (com_argv[COM_CheckParm ("-mode") + 1], w, h, d);
		} else if (COM_CheckParm ("-w") || COM_CheckParm ("-h")
				   || COM_CheckParm ("-d")) {
			if (COM_CheckParm ("-w")) {
				w = atoi (com_argv[COM_CheckParm ("-w") + 1]);
			}
			if (COM_CheckParm ("-h")) {
				h = atoi (com_argv[COM_CheckParm ("-h") + 1]);
			}
			if (COM_CheckParm ("-d")) {
				d = atoi (com_argv[COM_CheckParm ("-d") + 1]);
			}
			modestr = get_mode (0, w, h, d);
		} else {
			modestr = "640x480-60";
		}

		/* Set vid parameters */
		vmode = FindVideoMode(modestr);
		if (!vmode)
			Sys_Error("no video mode %s\n", modestr);
		current_mode = *vmode;
		ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
		VID_SetMode (current_mode.name, palette);
		Con_CheckResize (); // Now that we have a window size, fix console

		VID_InitGamma (palette);
		VID_SetPalette (palette);
	}
}

void
VID_Init_Cvars ()
{
	vid_mode = Cvar_Get ("vid_mode", "0", CVAR_NONE, NULL,
			"Sets the video mode");
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
			VID_SetPalette(vid_current_palette);
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
		double *d = (double *)framebuffer_ptr, *s = (double *)vid.buffer;
		double *ends = (double *)(vid.buffer + vid.height*vid.rowbytes);
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
			lineskip = (vid.width - (xoff + rects->width)) / sizeof(double);
			d = (double *)(framebuffer_ptr + yoff * vid.rowbytes + xoff);
			s = (double *)(vid.buffer + yoff * vid.rowbytes + xoff);
			for (i = yoff; i < height; i++) {
				for (j = xoff; j < width; j++)
					*d++ = *s++;
				d += lineskip;
				s += lineskip;
			}
			rects = rects->pnext;
		}
	}

	if (current_mode.name && strcmp(vid_mode->string, current_mode.name)) {
		VID_SetMode (vid_mode->string, vid_current_palette);
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
VID_SetCaption (char *text)
{
}

qboolean
VID_SetGamma (double gamma)
{
	return false;
}
