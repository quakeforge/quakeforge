/*
	vid_ggi.c

	general LibGGI video driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999       Marcus Sundberg [mackan@stacken.kth.se]

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

#define _BSD

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <ggi/ggi.h>

#include "client.h"
#include "cmd.h"
#include "console.h"
#include "cvar.h"
#include "d_local.h"
#include "draw.h"
#include "host.h"
#include "qargs.h"
#include "qendian.h"
#include "sys.h"

extern viddef_t vid;					// global video state
unsigned short d_8to16table[256];

/* Unused */
int         VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes, VGA_planar;
byte       *VGA_pagebase;

cvar_t     *m_filter;
cvar_t     *_windowed_mouse;
#define NUM_STDBUTTONS	3
#define NUM_BUTTONS	10

static ggi_visual_t ggivis = NULL;
static ggi_mode mode;
static const ggi_directbuffer *dbuf1 = NULL, *dbuf2 = NULL;

static uint8 *drawptr = NULL;
static void *frameptr[2] = { NULL, NULL };
static void *oneline = NULL;
static void *palette = NULL;
static int  curframe = 0;

static int  realwidth, realheight;
static int  doublebuffer;
static int  scale;
static int  stride, drawstride;
static int  pixelsize;
static int  usedbuf, havedbuf;

int         VID_options_items = 1;

static void
do_scale8 (int xsize, int ysize, uint8 * dest, uint8 * src)
{
	int         i, j, destinc = stride * 2 - xsize * 2;

	for (j = 0; j < ysize; j++) {
		for (i = 0; i < xsize; /* i is incremented below */ ) {
			register uint32 pix1 = src[i++], pix2 = src[i++];

#ifdef GGI_LITTLE_ENDIAN
			*((uint32 *) (dest + stride))
				= *((uint32 *) dest)
				= (pix1 | (pix1 << 8)
				   | (pix2 << 16) | (pix2 << 24));
#else
			*((uint32 *) (dest + stride))
				= *((uint32 *) dest)
				= (pix2 | (pix2 << 8)
				   | (pix1 << 16) | (pix1 << 24));
#endif
			dest += 4;
		}
		dest += destinc;
		src += xsize;
	}
}

static void
do_scale16 (int xsize, int ysize, uint8 * dest, uint8 * src)
{
	int         i, j, destinc = stride * 2 - xsize * 4;
	uint16     *palptr = palette;

	for (j = 0; j < ysize; j++) {
		for (i = 0; i < xsize; /* i is incremented below */ ) {
			register uint32 pixel = palptr[src[i++]];

			*((uint32 *) (dest + stride))
				= *((uint32 *) dest)
				= pixel | (pixel << 16);
			dest += 4;
		}
		dest += destinc;
		src += xsize;
	}
}

static void
do_scale32 (int xsize, int ysize, uint8 * dest, uint8 * src)
{
	int         i, j, destinc = stride * 2 - xsize * 8;
	uint32     *palptr = palette;

	for (j = 0; j < ysize; j++) {
		for (i = 0; i < xsize; /* i is incremented below */ ) {
			register uint32 pixel = palptr[src[i++]];

			*((uint32 *) (dest + stride))
				= *((uint32 *) (dest)) = pixel;
			dest += 4;
			*((uint32 *) (dest + stride))
				= *((uint32 *) (dest)) = pixel;
			dest += 4;
		}
		dest += destinc;
		src += xsize;
	}
}

static void
do_copy8 (int xsize, int ysize, uint8 * dest, uint8 * src)
{
	int         i, j;
	uint8      *palptr = palette;

	for (j = 0; j < ysize; j++) {
		for (i = 0; i < xsize; i++) {
			dest[i] = palptr[src[i]];
		}
		dest += stride;
		src += xsize;
	}
}

static void
do_copy16 (int xsize, int ysize, void *destptr, uint8 * src)
{
	int         i, j, destinc = (stride / 2 - xsize) / 2;
	uint16     *palptr = palette;
	uint32     *dest = destptr;

	for (j = 0; j < ysize; j++) {
		for (i = 0; i < xsize; /* i is incremented below */ ) {
			register uint32 pixel = palptr[src[i++]];

#ifdef GGI_LITTLE_ENDIAN
			*(dest++) = pixel | (palptr[src[i++]] << 16);
#else
			*(dest++) = (palptr[src[i++]] << 16) | pixel;
#endif
		}
		dest += destinc;
		src += xsize;
	}
}

static void
do_copy32 (int xsize, int ysize, uint32 * dest, uint8 * src)
{
	int         i, j, destinc = stride / 4;
	uint32     *palptr = palette;

	for (j = 0; j < ysize; j++) {
		for (i = 0; i < xsize; i++) {
			dest[i] = palptr[src[i]];
		}
		dest += destinc;
		src += xsize;
	}
}

void
ResetFrameBuffer (void)
{
	int         tbuffersize, tcachesize;
	void       *vid_surfcache;

	// Calculate the sizes we want first
	tbuffersize = vid.width * vid.height * sizeof (*d_pzbuffer);
	tcachesize = D_SurfaceCacheForRes (vid.width, vid.height);

	// Free the old z-buffer
	if (d_pzbuffer) {
		free (d_pzbuffer);
		d_pzbuffer = NULL;
	}
	// Free the old surface cache
	vid_surfcache = D_SurfaceCacheAddress ();
	if (vid_surfcache) {
		D_FlushCaches ();
		free (vid_surfcache);
		vid_surfcache = NULL;
	}
	// Allocate the new z-buffer
	d_pzbuffer = calloc (tbuffersize, 1);
	if (!d_pzbuffer) {
		Sys_Error ("Not enough memory for video mode\n");
	}
	// Allocate the new surface cache; free the z-buffer if we fail
	vid_surfcache = calloc (tcachesize, 1);
	if (!vid_surfcache) {
		free (d_pzbuffer);
		d_pzbuffer = NULL;
		Sys_Error ("Not enough memory for video mode\n");
	}

	D_InitCaches (vid_surfcache, tcachesize);
}


// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void
VID_Init (unsigned char *pal)
{
	int         pnum;

	vid.width = GGI_AUTO;
	vid.height = GGI_AUTO;

	srandom (getpid ());

	if (ggiInit () < 0) {
		Sys_Error ("VID: Unable to init LibGGI\n");
	}
	ggivis = ggiOpen (NULL);
	if (!ggivis) {
		Sys_Error ("VID: Unable to open default visual\n");
	}

	/* Go into async mode */
	ggiSetFlags (ggivis, GGIFLAG_ASYNC);

	VID_GetWindowSize (320, 200);

	scale = COM_CheckParm ("-scale");

	/* specify a LibGGI mode */
	if ((pnum = COM_CheckParm ("-ggimode"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -ggimode <mode>\n");
		ggiParseMode (com_argv[pnum + 1], &mode);
	} else {
		/* This will give the default mode */
		ggiParseMode ("", &mode);
		/* Now put in any parameters given above */
		mode.visible.x = vid.width;
		mode.visible.y = vid.height;
	}

	if (scale) {
		mode.visible.x *= 2;
		mode.visible.y *= 2;
	}

	/* We prefer 8 bit mode unless otherwise specified */
	if (mode.graphtype == GT_AUTO)
		mode.graphtype = GT_8BIT;

	/* We want double buffering if possible */
	if (mode.frames == GGI_AUTO) {
		ggi_mode    tmpmode = mode;

		tmpmode.frames = 2;
		if (ggiCheckMode (ggivis, &tmpmode) == 0) {
			mode = tmpmode;
		} else {
			tmpmode.frames = 2;
			if (ggiCheckMode (ggivis, &tmpmode) == 0) {
				mode = tmpmode;
			}
		}
	}

	if (ggiSetMode (ggivis, &mode) != 0) {
		/* Try again with suggested mode */
		if (ggiSetMode (ggivis, &mode) != 0) {
			Sys_Error ("VID: LibGGI can't set any modes!\n");
		}
	}

	/* Pixel size must be 1, 2 or 4 bytes */
	if (GT_SIZE (mode.graphtype) != 8 &&
		GT_SIZE (mode.graphtype) != 16 && GT_SIZE (mode.graphtype) != 32) {
		if (GT_SIZE (mode.graphtype) == 24) {
			Sys_Error
				("VID: 24 bits per pixel not supported - try using the palemu target.\n");
		} else {
			Sys_Error ("VID: %d bits per pixel not supported by GGI Quake.\n",
					   GT_SIZE (mode.graphtype));
		}
	}

	realwidth = mode.visible.x;
	realheight = mode.visible.y;
	if (scale) {
		vid.width = realwidth / 2;
		vid.height = realheight / 2;
	} else {
		vid.width = realwidth;
		vid.height = realheight;
	}
	Con_CheckResize (); // Now that we have a window size, fix console

	if (mode.frames >= 2)
		doublebuffer = 1;
	else
		doublebuffer = 0;

	pixelsize = (GT_SIZE (mode.graphtype) + 7) / 8;
	if (mode.graphtype != GT_8BIT) {
		if ((palette = malloc (pixelsize * 256)) == NULL) {
			Sys_Error ("VID: Unable to allocate palette table\n");
		}
	}

	VID_SetPalette (pal);

	usedbuf = havedbuf = 0;
	drawstride = vid.width;
	stride = realwidth * pixelsize;
	if ((dbuf1 = ggiDBGetBuffer (ggivis, 0)) != NULL &&
		(dbuf1->type & GGI_DB_SIMPLE_PLB)) {
		havedbuf = 1;
		stride = dbuf1->buffer.plb.stride;
		if (doublebuffer) {
			if ((dbuf2 = ggiDBGetBuffer (ggivis, 1)) == NULL ||
				!(dbuf2->type & GGI_DB_SIMPLE_PLB)) {
				/* Only one DB? No double buffering then */
				doublebuffer = 0;
			}
		}
		if (doublebuffer) {
			fprintf (stderr, "VID: Got two DirectBuffers\n");
		} else {
			fprintf (stderr, "VID: Got one DirectBuffer\n");
		}
		if (doublebuffer && !scale && !palette) {
			usedbuf = 1;
			drawstride = stride;
			frameptr[0] = dbuf1->write;
			if (doublebuffer) {
				frameptr[1] = dbuf2->write;
			} else {
				frameptr[1] = frameptr[0];
			}
			drawptr = frameptr[0];
			fprintf (stderr, "VID: Drawing into DirectBuffer\n");
		}
	}

	if (!usedbuf) {
		if ((drawptr = malloc (vid.width * vid.height)) == NULL) {
			Sys_Error ("VID: Unable to allocate draw buffer\n");
		}
		if (!havedbuf && (scale || palette)) {
			int         linesize = pixelsize * realwidth;

			if (scale)
				linesize *= 4;
			if ((oneline = malloc (linesize)) == NULL) {
				Sys_Error ("VID: Unable to allocate line buffer\n");
			}
		}
		fprintf (stderr, "VID: Drawing into offscreen memory\n");
	}

	ResetFrameBuffer ();

	curframe = 0;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = doublebuffer ? 2 : 1;
	vid.colormap = host_colormap;
	vid.buffer = drawptr;
	vid.rowbytes = drawstride;
	vid.direct = drawptr;
	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));
}

void
VID_ShiftPalette (unsigned char *pal)
{
	VID_SetPalette (pal);
}


void
VID_SetPalette (unsigned char *pal)
{
	int         i;
	ggi_color   colors[256];

	for (i = 0; i < 256; i++) {
		colors[i].r = pal[i * 3] * 257;
		colors[i].g = pal[i * 3 + 1] * 257;
		colors[i].b = pal[i * 3 + 2] * 257;
	}
	if (palette) {
		ggiPackColors (ggivis, palette, colors, 256);
	} else {
		ggiSetPalette (ggivis, 0, 256, colors);
	}
}

// Called at shutdown
void
VID_Shutdown (void)
{
	Con_Printf ("VID_Shutdown\n");

	if (!usedbuf) {
		free (drawptr);
		drawptr = NULL;
	}
	if (oneline) {
		free (oneline);
		oneline = NULL;
	}
	if (palette) {
		free (palette);
		palette = NULL;
	}
	if (ggivis) {
		ggiClose (ggivis);
		ggivis = NULL;
	}
	ggiExit ();
}


// flushes the given rectangles from the view buffer to the screen

void
VID_Update (vrect_t *rects)
{
	int         height = 0;

#if 0
// if the window changes dimension, skip this frame

	if (config_notify) {
		fprintf (stderr, "config notify\n");
		config_notify = 0;
		vid.width = config_notify_width & ~7;
		vid.height = config_notify_height;
		if (doShm)
			ResetSharedFrameBuffers ();
		else
			ResetFrameBuffer ();
		vid.rowbytes = x_framebuffer[0]->bytes_per_line;
		vid.buffer = x_framebuffer[curframe]->data;
		vid.conbuffer = vid.buffer;
		vid.conwidth = vid.width;
		vid.conheight = vid.height;
		vid.conrowbytes = vid.rowbytes;
		vid.recalc_refdef = 1;			// force a surface cache flush
		Con_CheckResize ();
		Con_Clear_f ();
		return;
	}
	// force full update if not 8bit
	if (x_visinfo->depth != 8) {
		extern int  scr_fullupdate;

		scr_fullupdate = 0;
	}
#endif

	while (rects) {
		int         y = rects->y + rects->height;

		if (y > height)
			height = y;
		rects = rects->pnext;
	}

	if (!usedbuf) {
		int         i;

		if (havedbuf) {
			if (ggiResourceAcquire (dbuf1->resource,
									GGI_ACTYPE_WRITE) != 0 ||
				(doublebuffer ?
				 ggiResourceAcquire (dbuf2->resource,
									 GGI_ACTYPE_WRITE) != 0 : 0)) {
				ggiPanic ("Unable to acquire DirectBuffer!\n");
			}
			/* ->write is allowed to change at acquire time */
			frameptr[0] = dbuf1->write;
			if (doublebuffer) {
				frameptr[1] = dbuf2->write;
			} else {
				frameptr[1] = frameptr[0];
			}
		}
		if (scale) {
			switch (pixelsize) {
				case 1:
					if (havedbuf) {
						do_scale8 (vid.width, height,
								   frameptr[curframe], drawptr);
					} else {
						uint8      *buf = drawptr;

						for (i = 0; i < height; i++) {
							do_scale8 (vid.width, 1, oneline, buf);
							ggiPutBox (ggivis, 0, i * 2, realwidth, 2, oneline);
							buf += vid.width;
						}
					}
					break;
				case 2:
					if (havedbuf) {
						do_scale16 (vid.width, height,
									frameptr[curframe], drawptr);
					} else {
						uint8      *buf = drawptr;

						for (i = 0; i < height; i++) {
							do_scale16 (vid.width, 1, oneline, buf);
							ggiPutBox (ggivis, 0, i * 2, realwidth, 2, oneline);
							buf += vid.width;
						}
					}
					break;
				case 4:
					if (havedbuf) {
						do_scale32 (vid.width, height,
									frameptr[curframe], drawptr);
					} else {
						uint8      *buf = drawptr;

						for (i = 0; i < height; i++) {
							do_scale32 (vid.width, 1, oneline, buf);
							ggiPutBox (ggivis, 0, i * 2, realwidth, 2, oneline);
							buf += vid.width;
						}
					}
					break;
			}
		} else if (palette) {
			switch (pixelsize) {
				case 1:
					if (havedbuf) {
						do_copy8 (vid.width, height,
								  frameptr[curframe], drawptr);
					} else {
						uint8      *buf = drawptr;

						for (i = 0; i < height; i++) {
							do_copy8 (vid.width, 1, oneline, buf);
							ggiPutBox (ggivis, 0, i, realwidth, 1, oneline);
							buf += vid.width;
						}
					}
					break;
				case 2:
					if (havedbuf) {
						do_copy16 (vid.width, height,
								   frameptr[curframe], drawptr);
					} else {
						uint8      *buf = drawptr;

						for (i = 0; i < height; i++) {
							do_copy16 (vid.width, 1, oneline, buf);
							ggiPutBox (ggivis, 0, i, realwidth, 1, oneline);
							buf += vid.width;
						}
					}
					break;
				case 4:
					if (havedbuf) {
						do_copy32 (vid.width, height,
								   frameptr[curframe], drawptr);
					} else {
						uint8      *buf = drawptr;

						for (i = 0; i < height; i++) {
							do_copy32 (vid.width, 1, oneline, buf);
							ggiPutBox (ggivis, 0, i, realwidth, 1, oneline);
							buf += vid.width;
						}
					}
					break;
			}
		} else {
			ggiPutBox (ggivis, 0, 0, vid.width, height, drawptr);
		}
		if (havedbuf) {
			ggiResourceRelease (dbuf1->resource);
			if (doublebuffer) {
				ggiResourceRelease (dbuf2->resource);
			}
		}

	}

	if (doublebuffer) {
		ggiSetDisplayFrame (ggivis, curframe);
		curframe = !curframe;
		if (usedbuf) {
			vid.buffer = vid.conbuffer = vid.direct
				= drawptr = frameptr[curframe];
		}
		ggiSetWriteFrame (ggivis, curframe);
	}
#if 0
	if (GT_SIZE (mode.graphtype) == 16) {
		do_copy16 (vid.width, height, (uint16 *) frameptr, drawptr);
	} else if (GT_SIZE (mode.graphtype) == 32) {
		do_copy32 (vid.width, height, (uint32 *) frameptr, drawptr);
	}
#endif

	ggiFlush (ggivis);
}

void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void
VID_Init_Cvars (void)
{
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
