/*
	gl_draw.c

	Draw functions for chars, textures, etc

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/texture.h"
#include "QF/tga.h"
#include "QF/va.h"
#include "QF/vfs.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_screen.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"
#include "QF/GL/types.h"

#include "r_cvar.h"
#include "r_shared.h"
#include "sbar.h"

byte *draw_chars;						// 8*8 graphic characters

qpic_t     *draw_backtile;

static int  translate_texture;
static int  char_texture;
static int  cs_texture;					// crosshair texturea

static byte color_0_8[4] = { 204, 204, 204, 255 };

static byte cs_data[64] = {
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

typedef struct {
	int         texnum;
} glpic_t;

typedef struct cachepic_s {
	char        name[MAX_QPATH];
	qboolean    dirty;
	qpic_t      pic;
	byte        padding[32];			// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
static cachepic_t cachepics[MAX_CACHED_PICS];
static int     numcachepics;

static byte    menuplyr_pixels[4096];

qpic_t *
Draw_PicFromWad (const char *name)
{
	glpic_t    *gl;
	qpic_t     *p;
	char        filename[MAX_QPATH + 4];
	VFile      *f;
	tex_t      *targa;

	p = W_GetLumpName (name);
	gl = (glpic_t *) p->data;

	snprintf (filename, sizeof (filename), "%s.tga", name);
	COM_FOpenFile (filename, &f);
	if (f) {
		targa = LoadTGA (f);
		Qclose (f);
		if (targa->format < 4)
			gl->texnum = GL_LoadTexture (name, targa->width,
				targa->height, targa->data, false, false, 3);
		else
			gl->texnum = GL_LoadTexture (name, targa->width,
				targa->height, targa->data, false, true, 4);
		return p;
	}
	gl->texnum = GL_LoadTexture (name, p->width, p->height,
					p->data, false, true, 1);
	return p;
}

void
Draw_ClearCache (void)
{
	cachepic_t *pic;
	int         i;

	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		pic->dirty = true;
}

qpic_t *
Draw_CachePic (const char *path, qboolean alpha)
{
	cachepic_t *pic;
	int         i;
	glpic_t    *gl;
	qpic_t     *dat;
	char        filename[MAX_QPATH + 4];
	VFile      *f;
	tex_t      *targa;

	// First, check if its cached..
	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		if ((!strcmp (path, pic->name)) && !pic->dirty)
			return &pic->pic;

	// Its not cached, lets make sure we have space in the cache..
	if (numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");

	// Load the picture..
	dat = (qpic_t *) COM_LoadTempFile (path);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);

	// Adjust for endian..
	SwapPic (dat);

	// Ok, the image is here, lets load it up into the cache..

	// First the image name..
	strncpy (pic->name, path, sizeof (pic->name));

	// Now the width and height.
	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	// Now feed it to the GL stuff and get a texture number..
	gl = (glpic_t *) pic->pic.data;

	snprintf (filename, sizeof (filename), "%s.tga", path);
	COM_FOpenFile (filename, &f);
	if (f) {
		targa = LoadTGA (f);
		Qclose (f);
		if (targa->format < 4)
			gl->texnum = GL_LoadTexture ("", targa->width, targa->height,
				targa->data, false, alpha, 3);
		else
			gl->texnum = GL_LoadTexture ("", targa->width, targa->height,
				targa->data, false, alpha, 4);
	}
	else
		gl->texnum = GL_LoadTexture ("", dat->width, dat->height, dat->data,
								 false, alpha, 1);

	// Now lets mark this cache entry as used..
	pic->dirty = false;
	numcachepics++;

	// FIXME: A really ugly kluge, keep a specific image in memory
	//  for the menu system. Some days I really dislike legacy support..
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);

	// And now we are done, return what was asked for..
	return &pic->pic;
}

void
Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
	int         cx, cy, n;
	qpic_t     *p;

	// draw left side
	color_white[3] = alpha;
	qfglColor4ubv (color_white);
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp", true);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp", true);
	Draw_Pic (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp", true);
		Draw_Pic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp", true);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp", true);
			Draw_Pic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp", true);
		Draw_Pic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp", true);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp", true);
	Draw_Pic (cx, cy + 8, p);
	qfglColor3ubv (color_white);
}


void
Draw_Init (void)
{
	int         i;
	GLint		texSize;

	// Some cards have a texture size limit.
	qfglGetIntegerv(GL_MAX_TEXTURE_SIZE, &texSize);
	Cvar_Set (gl_max_size, va("%d", texSize));

	Cmd_AddCommand ("gl_texturemode", &GL_TextureMode_f,
					"Texture mipmap quality.");

	// load the console background and the charset by hand, because we need to
	// write the version string into the background before turning it into a
	// texture
	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;		// proper transparent color

	// now turn them into textures
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false,
								   true, 1);
	cs_texture = GL_LoadTexture ("crosshair", 8, 8, cs_data, false, true, 1);

	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// get the other pics we need
	draw_backtile = Draw_PicFromWad ("backtile");

	// LordHavoc: call init code for other GL renderer modules;
	glrmain_init ();
	glrsurf_init ();
}

#define CELL_SIZE 0.0625

/*
	Draw_Character

	Draws one 8*8 graphics character with 0 being transparent.
	It can be clipped to the top of the screen to allow the console to be
	smoothly scrolled off.
*/
void
Draw_Character (int x, int y, unsigned int num)
{
	float       frow, fcol;

	if (num == 32)
		return;							// space

	if (y <= -8)
		return;							// totally off screen

	num &= 255;

	frow = (num >> 4) * CELL_SIZE;
	fcol = (num & 15) * CELL_SIZE;

	qfglBindTexture (GL_TEXTURE_2D, char_texture);

	qfglBegin (GL_QUADS);
	qfglTexCoord2f (fcol, frow);
	qfglVertex2f (x, y);
	qfglTexCoord2f (fcol + CELL_SIZE, frow);
	qfglVertex2f (x + 8, y);
	qfglTexCoord2f (fcol + CELL_SIZE, frow + CELL_SIZE);
	qfglVertex2f (x + 8, y + 8);
	qfglTexCoord2f (fcol, frow + CELL_SIZE);
	qfglVertex2f (x, y + 8);
	qfglEnd ();
}

void
Draw_String (int x, int y, const char *str)
{
	unsigned char	num;
	float       	frow, fcol;

	if (!str || !str[0])
		return;
	if (y <= -8)
		return;							// totally off screen

	qfglBindTexture (GL_TEXTURE_2D, char_texture);
	qfglBegin (GL_QUADS);

	while (*str) {
		if ((num = *str++) != 32) // Don't render spaces
		{
			frow = (num >> 4) * CELL_SIZE;
			fcol = (num & 15) * CELL_SIZE;

			qfglTexCoord2f (fcol, frow);
			qfglVertex2f (x, y);
			qfglTexCoord2f (fcol + CELL_SIZE, frow);
			qfglVertex2f (x + 8, y);
			qfglTexCoord2f (fcol + CELL_SIZE, frow + CELL_SIZE);
			qfglVertex2f (x + 8, y + 8);
			qfglTexCoord2f (fcol, frow + CELL_SIZE);
			qfglVertex2f (x, y + 8);
		}
		x += 8;
	}

	qfglEnd ();
}

void
Draw_nString (int x, int y, const char *str, int count)
{
	unsigned char   num;
	float           frow, fcol;
	
	if (!str || !str[0])
		return;
	if (y <= -8)
		return;                         // totally off screen

	qfglBindTexture (GL_TEXTURE_2D, char_texture);
	qfglBegin (GL_QUADS);

	while (count-- && *str) {
		if ((num = *str++) != 32) {		// Don't render spaces
			frow = (num >> 4) * CELL_SIZE;
			fcol = (num & 15) * CELL_SIZE;

			qfglTexCoord2f (fcol, frow);
			qfglVertex2f (x, y);
			qfglTexCoord2f (fcol + CELL_SIZE, frow);
			qfglVertex2f (x + 8, y);
			qfglTexCoord2f (fcol + CELL_SIZE, frow + CELL_SIZE);
			qfglVertex2f (x + 8, y + 8);
			qfglTexCoord2f (fcol, frow + CELL_SIZE);
			qfglVertex2f (x, y + 8);
		}
		x += 8;
	}
    qfglEnd ();
}

void
Draw_AltString (int x, int y, const char *str)
{
	unsigned char	num;
	float       	frow, fcol;

	if (!str || !str[0])
		return;
	if (y <= -8)
		return;							// totally off screen

	qfglBindTexture (GL_TEXTURE_2D, char_texture);
	qfglBegin (GL_QUADS);

	while (*str) {
		if ((num = *str++ | 0x80) != (0x80 | 32)) // Don't render spaces
		{
			frow = (num >> 4) * CELL_SIZE;
			fcol = (num & 15) * CELL_SIZE;

			qfglTexCoord2f (fcol, frow);
			qfglVertex2f (x, y);
			qfglTexCoord2f (fcol + CELL_SIZE, frow);
			qfglVertex2f (x + 8, y);
			qfglTexCoord2f (fcol + CELL_SIZE, frow + CELL_SIZE);
			qfglVertex2f (x + 8, y + 8);
			qfglTexCoord2f (fcol, frow + CELL_SIZE);
			qfglVertex2f (x, y + 8);
		}
		x += 8;
	}

	qfglEnd ();
}

void
Draw_Crosshair (void)
{
	unsigned char *pColor;
	int            x, y;

	switch (crosshair->int_val) {
		case 0:
			break;
		case 1:
		default:
			Draw_Character (scr_vrect.x + scr_vrect.width / 2 - 4 +
							cl_crossx->int_val,
							scr_vrect.y + scr_vrect.height / 2 - 4 +
							cl_crossy->int_val, '+');
			break;
		case 2:
			x = scr_vrect.x + scr_vrect.width / 2 - 3 + cl_crossx->int_val;
			y = scr_vrect.y + scr_vrect.height / 2 - 3 + cl_crossy->int_val;

			pColor = (unsigned char *) &d_8to24table[crosshaircolor->int_val];
			qfglColor4ubv (pColor);
			qfglBindTexture (GL_TEXTURE_2D, cs_texture);

			qfglBegin (GL_QUADS);
			qfglTexCoord2f (0, 0);
			qfglVertex2f (x - 4, y - 4);
			qfglTexCoord2f (1, 0);
			qfglVertex2f (x + 12, y - 4);
			qfglTexCoord2f (1, 1);
			qfglVertex2f (x + 12, y + 12);
			qfglTexCoord2f (0, 1);
			qfglVertex2f (x - 4, y + 12);
			qfglEnd ();
			qfglColor3ubv (color_white);
			break;
	}
}

void
Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t    *gl;

	gl = (glpic_t *) pic->data;

	qfglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qfglBegin (GL_QUADS);
	qfglTexCoord2f (0, 0);
	qfglVertex2f (x, y);
	qfglTexCoord2f (1, 0);
	qfglVertex2f (x + pic->width, y);
	qfglTexCoord2f (1, 1);
	qfglVertex2f (x + pic->width, y + pic->height);
	qfglTexCoord2f (0, 1);
	qfglVertex2f (x, y + pic->height);
	qfglEnd ();
}

void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
			 int height)
{
	float       newsl, newtl, newsh, newth;
	glpic_t    *gl;

	gl = (glpic_t *) pic->data;

	newsl = (float) srcx / (float) pic->width;
	newsh = newsl + (float) width / (float) pic->width;

	newtl = (float) srcy / (float) pic->height;
	newth = newtl + (float) height / (float) pic->height;

	qfglColor3ubv (color_0_8);
	qfglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qfglBegin (GL_QUADS);
	qfglTexCoord2f (newsl, newtl);
	qfglVertex2f (x, y);
	qfglTexCoord2f (newsh, newtl);
	qfglVertex2f (x + width, y);
	qfglTexCoord2f (newsh, newth);
	qfglVertex2f (x + width, y + height);
	qfglTexCoord2f (newsl, newth);
	qfglVertex2f (x, y + height);
	qfglEnd ();
	qfglColor3ubv (color_white);
}

/*
	Draw_TransPicTranslate

	Only used for the player color selection menu
*/
void
Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte * translation)
{
	byte        *src;
	int          c, p, u, v;
	unsigned int trans[64 * 64], *dest;

	qfglBindTexture (GL_TEXTURE_2D, translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v = 0; v < 64; v++, dest += 64) {
		src = &menuplyr_pixels[((v * pic->height) >> 6) * pic->width];
		for (u = 0; u < 64; u++) {
			p = src[(u * pic->width) >> 6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] = d_8to24table[translation[p]];
		}
	}

	qfglTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);

	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qfglColor3ubv (color_0_8);
	qfglBegin (GL_QUADS);
	qfglTexCoord2f (0, 0);
	qfglVertex2f (x, y);
	qfglTexCoord2f (1, 0);
	qfglVertex2f (x + pic->width, y);
	qfglTexCoord2f (1, 1);
	qfglVertex2f (x + pic->width, y + pic->height);
	qfglTexCoord2f (0, 1);
	qfglVertex2f (x, y + pic->height);
	qfglEnd ();
	qfglColor3ubv (color_white);
}

/*
	Draw_ConsoleBackground

	Draws console background (obviously!)  Completely rewritten to use
	several simple yet very cool GL effects.  --KB
*/
void
Draw_ConsoleBackground (int lines)
{
	byte        alpha;
	float       ofs;
	int         y;
	glpic_t    *gl;
	qpic_t     *conback;

	// This can be a CachePic now, just like in software
	conback = Draw_CachePic ("gfx/conback.lmp", false);
	gl = (glpic_t *) conback->data;

	// spin the console? - effect described in a QER tutorial
	if (gl_conspin->value) {
		static float xangle = 0;
		static float xfactor = .3f;
		static float xstep = .005f;

		qfglPushMatrix ();
		qfglMatrixMode (GL_TEXTURE);
		qfglPushMatrix ();
		qfglLoadIdentity ();
		xangle += gl_conspin->value;
		xfactor += xstep;
		if (xfactor > 8 || xfactor < .3f)
			xstep = -xstep;
		qfglRotatef (xangle, 0, 0, 1);
		qfglScalef (xfactor, xfactor, xfactor);
	}
	// slide console up/down or stretch it?
	if (gl_constretch->int_val)
		ofs = 0;
	else
		ofs = (vid.conheight - lines) / (float) vid.conheight;

	y = vid.height * scr_consize->value;
	if (!r_active || lines > y) {
		alpha = 255;
	} else {
		// set up to draw alpha console
		alpha = 255 * (gl_conalpha->value * lines) / y;
	}

	color_0_8[3] = alpha;
	qfglColor4ubv (color_0_8);

	// draw the console texture
	qfglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qfglBegin (GL_QUADS);
	qfglTexCoord2f (0, 0 + ofs);
	qfglVertex2f (0, 0);
	qfglTexCoord2f (1, 0 + ofs);
	qfglVertex2f (vid.conwidth, 0);
	qfglTexCoord2f (1, 1);
	qfglVertex2f (vid.conwidth, lines);
	qfglTexCoord2f (0, 1);
	qfglVertex2f (0, lines);
	qfglEnd ();

	// turn off alpha blending
	if (alpha < 255) {
		qfglColor3ubv (color_0_8);
	}

	if (gl_conspin->value) {
		qfglPopMatrix ();
		qfglMatrixMode (GL_MODELVIEW);
		qfglPopMatrix ();
	}

	Draw_AltString (vid.conwidth - strlen (cl_verstring->string) * 8 - 11,
					lines - 14, cl_verstring->string);
	qfglColor3ubv (color_white);
}

/*
	Draw_TileClear

	This repeats a 64*64 tile graphic to fill the screen around a sized down
	refresh window.
*/
void
Draw_TileClear (int x, int y, int w, int h)
{
	qfglColor3ubv (color_0_8);
	qfglBindTexture (GL_TEXTURE_2D, *(int *) draw_backtile->data);
	qfglBegin (GL_QUADS);
	qfglTexCoord2f (x / 64.0, y / 64.0);
	qfglVertex2f (x, y);
	qfglTexCoord2f ((x + w) / 64.0, y / 64.0);
	qfglVertex2f (x + w, y);
	qfglTexCoord2f ((x + w) / 64.0, (y + h) / 64.0);
	qfglVertex2f (x + w, y + h);
	qfglTexCoord2f (x / 64.0, (y + h) / 64.0);
	qfglVertex2f (x, y + h);
	qfglEnd ();
	qfglColor3ubv (color_white);
}

/*
	Draw_Fill

	Fills a box of pixels with a single color
*/
void
Draw_Fill (int x, int y, int w, int h, int c)
{
	qfglDisable (GL_TEXTURE_2D);
	qfglColor3ubv (vid_basepal + c * 3);

	qfglBegin (GL_QUADS);

	qfglVertex2f (x, y);
	qfglVertex2f (x + w, y);
	qfglVertex2f (x + w, y + h);
	qfglVertex2f (x, y + h);

	qfglEnd ();
	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);
}

void
Draw_FadeScreen (void)
{
	qfglDisable (GL_TEXTURE_2D);
	qfglColor4ub (0, 0, 0, 179);
	qfglBegin (GL_QUADS);

	qfglVertex2f (0, 0);
	qfglVertex2f (vid.width, 0);
	qfglVertex2f (vid.width, vid.height);
	qfglVertex2f (0, vid.height);

	qfglEnd ();
	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);

	Sbar_Changed ();
}

/*
	Draw_BeginDisc

	Draws the little blue disc in the corner of the screen.
	Call before beginning any disc IO.
*/
void
Draw_BeginDisc (void)
{
}

/*
	Draw_EndDisc

	Erases the disc icon.
	Call after completing any disc IO
*/
void
Draw_EndDisc (void)
{
}

/*
	GL_Set2D

	Setup as if the screen was 320*200
*/
void
GL_Set2D (void)
{
	qfglViewport (glx, gly, glwidth, glheight);

	qfglMatrixMode (GL_PROJECTION);
	qfglLoadIdentity ();
	qfglOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	qfglMatrixMode (GL_MODELVIEW);
	qfglLoadIdentity ();

	qfglDisable (GL_DEPTH_TEST);
	qfglDisable (GL_CULL_FACE);

	qfglColor3ubv (color_white);
}
