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

	$Id$
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

#include <stdio.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/draw.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/va.h"

#include "client.h"
#include "glquake.h"
#include "r_cvar.h"
#include "sbar.h"

extern byte *vid_basepal;
extern cvar_t *crosshair, *cl_crossx, *cl_crossy, *crosshaircolor,
	*gl_lightmap_components;

byte *draw_chars;						// 8*8 graphic characters
qpic_t     *draw_disc;
qpic_t     *draw_backtile;

static int  translate_texture;
static int  char_texture;
static int  cs_texture;					// crosshair texturea

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
	int         bytesperpixel;
	float       sl, tl, sh, th;
} glpic_t;

extern int gl_filter_min, gl_filter_max;

typedef struct cachepic_s {
	char        name[MAX_QPATH];
	qboolean    dirty;
	qpic_t      pic;
	byte        padding[32];			// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
static cachepic_t cachepics[MAX_CACHED_PICS];
static int  numcachepics;

static byte menuplyr_pixels[4096];


qpic_t *
Draw_PicFromWad (char *name)
{
	qpic_t     *p;
	glpic_t    *gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *) p->data;

	gl->texnum = GL_LoadTexture ("", p->width, p->height, p->data, false, true, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

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
Draw_CachePic (char *path, qboolean alpha)
{
	cachepic_t *pic;
	int         i;
	qpic_t     *dat;
	glpic_t    *gl;

	// First, check and see if its cached..
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
	gl->texnum = GL_LoadTexture ("", dat->width, dat->height, dat->data, false, alpha, 1);

	// Alignment stuff..
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	// Now lets mark this cache entry as used..
	pic->dirty = false;
	numcachepics++;

	// FIXME:
	// A really ugly kluge, keep a specific image in memory
	//  for the menu system.
	// Some days I really dislike legacy support..

	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);

	// And now we are done, return what was asked for..
	return &pic->pic;
}


void
Draw_TextBox (int x, int y, int width, int lines)
{
	qpic_t     *p;
	int         cx, cy;
	int         n;

	// draw left side
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
}


extern void glrmain_init (void);
extern void glrsurf_init (void);
extern void GL_TextureMode_f (void);


void
Draw_Init (void)
{
	int         i;
	GLint		texSize;

	// Some cards have a texture size limit.
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texSize);
	Cvar_Set (gl_max_size, va("%d", texSize));

	// LordHavoc: 3DFX's dithering has terrible artifacts with lightmode 1
	if (strstr (gl_renderer, "3dfx") || strstr (gl_renderer, "Mesa Glide"))
	{
		Cvar_Set (gl_lightmode, "0");
	}

	Cmd_AddCommand ("gl_texturemode", &GL_TextureMode_f, "Texture mipmap quality.");

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;		// proper transparent color

	// now turn them into textures
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);	// 1999-12-27 Conwidth/height charset fix by TcT
	cs_texture = GL_LoadTexture ("crosshair", 8, 8, cs_data, false, true, 1);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// get the other pics we need
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	// LordHavoc: call init code for other GL renderer modules;
	glrmain_init ();
	glrsurf_init ();
}


/*
	Draw_Character8

	Draws one 8*8 graphics character with 0 being transparent.
	It can be clipped to the top of the screen to allow the console to be
	smoothly scrolled off.
*/
void
Draw_Character8 (int x, int y, int num)
{
	int         row, col;
	float       frow, fcol, size;

	if (num == 32)
		return;							// space

	num &= 255;

	if (y <= -8)
		return;							// totally off screen

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	glBindTexture (GL_TEXTURE_2D, char_texture);

	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + size, frow);
	glVertex2f (x + 8, y);
	glTexCoord2f (fcol + size, frow + size);
	glVertex2f (x + 8, y + 8);
	glTexCoord2f (fcol, frow + size);
	glVertex2f (x, y + 8);
	glEnd ();
}


void
Draw_String8 (int x, int y, char *str)
{
	while (*str) {
		Draw_Character8 (x, y, *str);
		str++;
		x += 8;
	}
}


void
Draw_AltString8 (int x, int y, char *str)
{
	while (*str) {
		Draw_Character8 (x, y, (*str) | 0x80);
		str++;
		x += 8;
	}
}


void
Draw_Crosshair (void)
{
	int         x, y;
	extern vrect_t scr_vrect;
	unsigned char *pColor;

	switch (crosshair->int_val) {
		case 0:
			break;
		case 1:
		default:
			Draw_Character8 (scr_vrect.x + scr_vrect.width / 2 - 4 +
							 cl_crossx->int_val,
							 scr_vrect.y + scr_vrect.height / 2 - 4 +
							 cl_crossy->int_val, '+');
			break;
		case 2:
			x = scr_vrect.x + scr_vrect.width / 2 - 3 + cl_crossx->int_val;
			y = scr_vrect.y + scr_vrect.height / 2 - 3 + cl_crossy->int_val;

			pColor = (unsigned char *) &d_8to24table[crosshaircolor->int_val];
			if (lighthalf)
				glColor4ub ((byte) ((int) pColor[0] >> 1),
							(byte) ((int) pColor[1] >> 1),
							(byte) ((int) pColor[2] >> 1), pColor[3]);
			else
				glColor4ubv (pColor);
			glBindTexture (GL_TEXTURE_2D, cs_texture);

			glBegin (GL_QUADS);
			glTexCoord2f (0, 0);
			glVertex2f (x - 4, y - 4);
			glTexCoord2f (1, 0);
			glVertex2f (x + 12, y - 4);
			glTexCoord2f (1, 1);
			glVertex2f (x + 12, y + 12);
			glTexCoord2f (0, 1);
			glVertex2f (x - 4, y + 12);
			glEnd ();
			glColor3ubv (lighthalf_v);
			break;
	}
}


void
Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t    *gl;

	gl = (glpic_t *) pic->data;

	glBindTexture (GL_TEXTURE_2D, gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x + pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x + pic->width, y + pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y + pic->height);
	glEnd ();
}


void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
			 int height)
{
	glpic_t    *gl;
	float       newsl, newtl, newsh, newth;
	float       oldglwidth, oldglheight;

	gl = (glpic_t *) pic->data;

	oldglwidth = gl->sh - gl->sl;
	oldglheight = gl->th - gl->tl;

	newsl = gl->sl + (srcx * oldglwidth) / pic->width;
	newsh = newsl + (width * oldglwidth) / pic->width;

	newtl = gl->tl + (srcy * oldglheight) / pic->height;
	newth = newtl + (height * oldglheight) / pic->height;

	if (lighthalf)
		glColor3f (0.4, 0.4, 0.4);
	else
		glColor3f (0.8, 0.8, 0.8);
	glBindTexture (GL_TEXTURE_2D, gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (newsl, newtl);
	glVertex2f (x, y);
	glTexCoord2f (newsh, newtl);
	glVertex2f (x + width, y);
	glTexCoord2f (newsh, newth);
	glVertex2f (x + width, y + height);
	glTexCoord2f (newsl, newth);
	glVertex2f (x, y + height);
	glEnd ();
	glColor3ubv (lighthalf_v);
}


/*
	Draw_TransPicTranslate

	Only used for the player color selection menu
*/
void
Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte * translation)
{
	int         v, u, c;
	unsigned int trans[64 * 64], *dest;
	byte       *src;
	int         p;

	glBindTexture (GL_TEXTURE_2D, translate_texture);

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

	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, trans);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	if (lighthalf)
		glColor3f (0.4, 0.4, 0.4);
	else
		glColor3f (0.8, 0.8, 0.8);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x + pic->width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x + pic->width, y + pic->height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y + pic->height);
	glEnd ();
	glColor3ubv (lighthalf_v);
}


/*
	Draw_ConsoleBackground

	Draws console background (obviously!)  Completely rewritten to use
	several simple yet very cool GL effects.  --KB
*/
void
Draw_ConsoleBackground (int lines)
{
	int         y;
	qpic_t     *conback;
	glpic_t    *gl;
	float       ofs;
	float       alpha;

	// This can be a CachePic now, just like in software
	conback = Draw_CachePic ("gfx/conback.lmp", false);
	gl = (glpic_t *) conback->data;

	// spin the console? - effect described in a QER tutorial
	if (gl_conspin->value) {
		static float xangle = 0;
		static float xfactor = .3f;
		static float xstep = .005f;

		glPushMatrix ();
		glMatrixMode (GL_TEXTURE);
		glPushMatrix ();
		glLoadIdentity ();
		xangle += gl_conspin->value;
		xfactor += xstep;
		if (xfactor > 8 || xfactor < .3f)
			xstep = -xstep;
		glRotatef (xangle, 0, 0, 1);
		glScalef (xfactor, xfactor, xfactor);
	}
	// slide console up/down or stretch it?
	if (gl_constretch->int_val)
		ofs = 0;
	else
		ofs = (vid.conheight - lines) / (float) vid.conheight;

	y = vid.height * scr_consize->value;
	if (cls.state != ca_active || lines > y) {
		alpha = 1.0;
	} else {
		// set up to draw alpha console
		alpha = (float) (gl_conalpha->value * lines) / y;
	}

	if (lighthalf)
		glColor4f (0.4, 0.4, 0.4, alpha);
	else
		glColor4f (0.8, 0.8, 0.8, alpha);

	// draw the console texture
	glBindTexture (GL_TEXTURE_2D, gl->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl + ofs);
	glVertex2f (0, 0);
	glTexCoord2f (gl->sh, gl->tl + ofs);
	glVertex2f (vid.conwidth, 0);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (vid.conwidth, lines);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (0, lines);
	glEnd ();

	// turn off alpha blending
	if (alpha < 1.0) {
		if (lighthalf)
			glColor3f (0.4, 0.4, 0.4);
		else
			glColor3f (0.8, 0.8, 0.8);
	}

	if (gl_conspin->value) {
		glPopMatrix ();
		glMatrixMode (GL_MODELVIEW);
		glPopMatrix ();
	}

	Draw_AltString8 (vid.conwidth - strlen (cl_verstring->string) * 8 - 11,
			 lines - 14, cl_verstring->string);
	glColor3ubv (lighthalf_v);
}


/*
	Draw_TileClear

	This repeats a 64*64 tile graphic to fill the screen around a sized down
	refresh window.
*/
void
Draw_TileClear (int x, int y, int w, int h)
{
	if (lighthalf)
		glColor3f (0.4, 0.4, 0.4);
	else
		glColor3f (0.8, 0.8, 0.8);
	glBindTexture (GL_TEXTURE_2D, *(int *) draw_backtile->data);
	glBegin (GL_QUADS);
	glTexCoord2f (x / 64.0, y / 64.0);
	glVertex2f (x, y);
	glTexCoord2f ((x + w) / 64.0, y / 64.0);
	glVertex2f (x + w, y);
	glTexCoord2f ((x + w) / 64.0, (y + h) / 64.0);
	glVertex2f (x + w, y + h);
	glTexCoord2f (x / 64.0, (y + h) / 64.0);
	glVertex2f (x, y + h);
	glEnd ();
	glColor3ubv (lighthalf_v);
}


/*
	Draw_Fill

	Fills a box of pixels with a single color
*/
void
Draw_Fill (int x, int y, int w, int h, int c)
{
	glDisable (GL_TEXTURE_2D);
	if (lighthalf)
		glColor3f (vid_basepal[c * 3] / 510.0, vid_basepal[c * 3 + 1] / 510.0,
				   vid_basepal[c * 3 + 2] / 510.0);
	else
		glColor3f (vid_basepal[c * 3] / 255.0, vid_basepal[c * 3 + 1] / 255.0,
				   vid_basepal[c * 3 + 2] / 255.0);

	glBegin (GL_QUADS);

	glVertex2f (x, y);
	glVertex2f (x + w, y);
	glVertex2f (x + w, y + h);
	glVertex2f (x, y + h);

	glEnd ();
	glColor3ubv (lighthalf_v);
	glEnable (GL_TEXTURE_2D);
}


//=============================================================================


void
Draw_FadeScreen (void)
{
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.7);
	glBegin (GL_QUADS);

	glVertex2f (0, 0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);

	glEnd ();
	glColor3ubv (lighthalf_v);
	glEnable (GL_TEXTURE_2D);

	Sbar_Changed ();
}


//=============================================================================


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
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);

	glColor3ubv (lighthalf_v);
}
