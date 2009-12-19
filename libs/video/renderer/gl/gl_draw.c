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

#include <stdio.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_screen.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"
#include "QF/GL/types.h"

#include "compat.h"
#include "gl_draw.h"
#include "r_cvar.h"
#include "r_shared.h"
#include "sbar.h"
#include "varrays.h"

byte	   *draw_chars;						// 8*8 graphic characters

int			tVAsize;
int		   *tVAindices;
int			tVAcount;
float	   *textVertices, *tV;
float	   *textCoords, *tC;

qpic_t	   *draw_backtile;

static int	translate_texture;
static int	char_texture;
static int	cs_texture;					// crosshair texturea

static byte color_0_8[4] = { 204, 204, 204, 255 };

// NOTE: this array is INCORRECT for direct uploading see Draw_Init
static byte cs_data[8 * 8 * 4] = {
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

typedef struct {
	int			texnum;
} glpic_t;

typedef struct cachepic_s {
	char		name[MAX_QPATH];
	qboolean	dirty;
	qpic_t		pic;
	byte		padding[32];			// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
static cachepic_t cachepics[MAX_CACHED_PICS];
static int		numcachepics;

static byte		menuplyr_pixels[4096];


static void
Draw_InitText (void)
{
	int		i;

	if (vaelements > 3) {
		tVAsize = vaelements - (vaelements % 4);
	} else if (vaelements >= 0) {
		tVAsize = 2048;
	} else
		tVAsize = 0;

	if (tVAsize) {
		Sys_DPrintf ("Text: %i maximum vertex elements.\n", tVAsize);

		if (textVertices)
			free (textVertices);
		textVertices = calloc (tVAsize, 2 * sizeof (float));

		if (textCoords)
			free (textCoords);
		textCoords = calloc (tVAsize, 2 * sizeof (float));

		qfglTexCoordPointer (2, GL_FLOAT, 0, textCoords);
		qfglVertexPointer (2, GL_FLOAT, 0, textVertices);
		if (tVAindices)
			free (tVAindices);
		tVAindices = (int *) calloc (tVAsize, sizeof (int));
		for (i = 0; i < tVAsize; i++)
			tVAindices[i] = i;
	} else {
		Sys_DPrintf ("Text: Vertex Array use disabled.\n");
	}
}

VISIBLE qpic_t *
Draw_PicFromWad (const char *name)
{
	glpic_t	   *gl;
	qpic_t	   *p, *pic;
	tex_t	   *targa;

	pic = W_GetLumpName (name);
	targa = LoadImage (name);
	if (targa) {
		p = malloc (sizeof (qpic_t) + sizeof (glpic_t));
		p->width = pic->width;
		p->height = pic->height;
		gl = (glpic_t *) p->data;
		if (targa->format < 4) {
			gl->texnum = GL_LoadTexture (name, targa->width, targa->height,
										 targa->data, false, false, 3);
		} else
			gl->texnum = GL_LoadTexture (name, targa->width, targa->height,
										 targa->data, false, true, 4);
	} else {
		p = pic;
		gl = (glpic_t *) p->data;
		gl->texnum = GL_LoadTexture (name, p->width, p->height, p->data,
									 false, true, 1);
	}
	return p;
}

static void
Draw_ClearCache (int phase)
{
	cachepic_t *pic;
	int         i;

	if (phase)
		return;
	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		pic->dirty = true;
}

VISIBLE qpic_t *
Draw_CachePic (const char *path, qboolean alpha)
{
	cachepic_t *pic;
	int         i;
	glpic_t    *gl;
	tex_t      *targa;

	// First, check if its cached..
	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		if ((!strcmp (path, pic->name)) && !pic->dirty)
			return &pic->pic;

	// Its not cached, lets make sure we have space in the cache..
	if (numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");

	gl = (glpic_t *) pic->pic.data;

	if (!strcmp (path + strlen (path) - 4, ".lmp")) {
		// Load the picture..
		qpic_t     *dat = (qpic_t *) QFS_LoadFile (path, 0);
		if (!dat)
			Sys_Error ("Draw_CachePic: failed to load %s", path);

		// Adjust for endian..
		SwapPic (dat);
		// Check for a .tga first
		targa = LoadImage (path);
		if (targa) {
			if (targa->format < 4) {
				gl->texnum = GL_LoadTexture ("", targa->width, targa->height,
											 targa->data, false, alpha, 3);
			} else
				gl->texnum = GL_LoadTexture ("", targa->width, targa->height,
											 targa->data, false, alpha, 4);
		} else {
			gl->texnum = GL_LoadTexture ("", dat->width, dat->height,
										 dat->data, false, alpha, 1);
		}
		pic->pic.width = dat->width;
		pic->pic.height = dat->height;
		if (!strcmp (path, "gfx/menuplyr.lmp"))
			memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);
		free (dat);
	} else
		Sys_Error ("Draw_CachePic: failed to load %s", path);

	strncpy (pic->name, path, sizeof (pic->name));

	// Now lets mark this cache entry as used..
	pic->dirty = false;
	numcachepics++;

	// And now we are done, return what was asked for..
	return &pic->pic;
}

VISIBLE void
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

VISIBLE void
Draw_Init (void)
{
	int	     i;
	tex_t	*image;
	byte    *cs_tmp_data;

	Cmd_AddCommand ("gl_texturemode", &GL_TextureMode_f,
					"Texture mipmap quality.");

	QFS_GamedirCallback (Draw_ClearCache);

	// load the console background and the charset by hand, because we need to
	// write the version string into the background before turning it into a
	// texture
	
	image = LoadImage ("gfx/conchars");
	if (image) {	
		if (image->format < 4) {
			char_texture = GL_LoadTexture ("charset", image->width,
										   image->height, image->data, false,
										   false, 3);
		} else
			char_texture = GL_LoadTexture ("charset", image->width,
										   image->height, image->data, false,
										   true, 4);
	} else {
		draw_chars = W_GetLumpName ("conchars");
		for (i = 0; i < 256 * 64; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;		// proper transparent color
		
		char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false,
									   true, 1);
	}

	// re-arrange the cs_data bytes so they're layed out properly for
	// subimaging
	cs_tmp_data = malloc (sizeof (cs_data));
	for (i = 0; i < 8 * 8; i++) {
		int         x, y;
		x = i % 8;
		y = i / 8;
		cs_tmp_data[y * 16 + x + 0 * 8 * 8 + 0] = cs_data[i + 0 * 8 * 8];
		cs_tmp_data[y * 16 + x + 0 * 8 * 8 + 8] = cs_data[i + 1 * 8 * 8];
		cs_tmp_data[y * 16 + x + 2 * 8 * 8 + 0] = cs_data[i + 2 * 8 * 8];
		cs_tmp_data[y * 16 + x + 2 * 8 * 8 + 8] = cs_data[i + 3 * 8 * 8];
	}
	cs_texture = GL_LoadTexture ("crosshair", 16, 16, cs_tmp_data,
								 false, true, 1);
	free (cs_tmp_data);

	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// get the other pics we need
	draw_backtile = Draw_PicFromWad ("backtile");

	// LordHavoc: call init code for other GL renderer modules
	glrmain_init ();
	gl_lightmap_init ();

	Draw_InitText ();
}

#define CELL_SIZE 0.0625

static inline void
flush_text (void)
{
	qfglBindTexture (GL_TEXTURE_2D, char_texture);
	qfglDrawElements (GL_QUADS, tVAcount, GL_UNSIGNED_INT, tVAindices);
	tVAcount = 0;
	tV = textVertices;
	tC = textCoords;
}

static inline void
queue_character (float x, float y, int chr)
{
	float		frow, fcol;

	frow = (chr >> 4) * CELL_SIZE;
	fcol = (chr & 15) * CELL_SIZE;

	*tV++ = x;
	*tV++ = y;
	*tV++ = x + 8.0;
	*tV++ = y;
	*tV++ = x + 8.0;
	*tV++ = y + 8.0;
	*tV++ = x;
	*tV++ = y + 8.0;
	*tC++ = fcol;
	*tC++ = frow;
	*tC++ = fcol + CELL_SIZE;
	*tC++ = frow;
	*tC++ = fcol + CELL_SIZE;
	*tC++ = frow + CELL_SIZE;
	*tC++ = fcol;
	*tC++ = frow + CELL_SIZE;
}

static inline void
tVA_increment (void)
{
	tVAcount += 4;
	if (tVAcount + 4 > tVAsize)
		flush_text ();
}

/*
	Draw_Character

	Draws one 8*8 graphics character with 0 being transparent.
	It can be clipped to the top of the screen to allow the console to be
	smoothly scrolled off.
*/
VISIBLE void
Draw_Character (int x, int y, unsigned int chr)
{
	chr &= 255;

	if (chr == 32)
		return;							// space
	if (y <= -8)
		return;							// totally off screen

	queue_character ((float) x, (float) y, chr);
	tVA_increment ();
}

VISIBLE void
Draw_String (int x, int y, const char *str)
{
	unsigned char	chr;
	float			x1, y1;

	if (!str || !str[0])
		return;
	if (y <= -8)
		return;							// totally off screen

	x1 = (float) x;
	y1 = (float) y;

	while (*str) {
		if ((chr = *str++) != 32) {		// Don't render spaces
			queue_character (x1, y1, chr);
			tVA_increment ();
		}
		x1 += 8.0;
	}
}

VISIBLE void
Draw_nString (int x, int y, const char *str, int count)
{
	unsigned char	chr;
	float			x1, y1;

	if (!str || !str[0])
		return;
	if (y <= -8)
		return;                         // totally off screen

	x1 = (float) x;
	y1 = (float) y;

	while (count-- && *str) {
		if ((chr = *str++) != 32) {		// Don't render spaces
			queue_character (x1, y1, chr);
			tVA_increment ();
		}
		x1 += 8.0;
	}
}

void
Draw_AltString (int x, int y, const char *str)
{
	unsigned char	chr;
	float			x1, y1;

	if (!str || !str[0])
		return;
	if (y <= -8)
		return;							// totally off screen

	x1 = (float) x;
	y1 = (float) y;

	while (*str) {
		if ((chr = *str++ | 0x80) != (0x80 | 32)) {		// Don't render spaces
			queue_character (x1, y1, chr);
			tVA_increment ();
		}
		x1 += 8.0;
	}
}

static void
crosshair_1 (int x, int y)
{
	Draw_Character (x - 4, y - 4, '+');
}

static void
crosshair_2 (int x, int y)
{
	unsigned char *pColor;

	pColor = (unsigned char *) &d_8to24table[crosshaircolor->int_val];
	qfglColor4ubv (pColor);
	qfglBindTexture (GL_TEXTURE_2D, cs_texture);

	qfglBegin (GL_QUADS);

	qfglTexCoord2f (0, 0);
	qfglVertex2f (x - 7, y - 7);
	qfglTexCoord2f (0.5, 0);
	qfglVertex2f (x + 9, y - 7);
	qfglTexCoord2f (0.5, 0.5);
	qfglVertex2f (x + 9, y + 9);
	qfglTexCoord2f (0, 0.5);
	qfglVertex2f (x - 7, y + 9);

	qfglEnd ();
	qfglColor3ubv (color_white);
}

static void
crosshair_3 (int x, int y)
{
	unsigned char *pColor;

	pColor = (unsigned char *) &d_8to24table[crosshaircolor->int_val];
	qfglColor4ubv (pColor);
	qfglBindTexture (GL_TEXTURE_2D, cs_texture);

	qfglBegin (GL_QUADS);

	qfglTexCoord2f (0.5, 0);
	qfglVertex2f (x - 7, y - 7);
	qfglTexCoord2f (1, 0);
	qfglVertex2f (x + 9, y - 7);
	qfglTexCoord2f (1, 0.5);
	qfglVertex2f (x + 9, y + 9);
	qfglTexCoord2f (0.5, 0.5);
	qfglVertex2f (x - 7, y + 9);

	qfglEnd ();
	qfglColor3ubv (color_white);
}

static void (*crosshair_func[]) (int x, int y) = {
	crosshair_1,
	crosshair_2,
	crosshair_3,
};

VISIBLE void
Draw_Crosshair (void)
{
	int            x, y;
	int            ch;

	ch = crosshair->int_val - 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx->int_val;
	y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy->int_val;

	crosshair_func[ch] (x, y);
}

void
Draw_CrosshairAt (int ch, int x, int y)
{
	ch -= 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	crosshair_func[ch] (x, y);
}

VISIBLE void
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

VISIBLE void
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
	Draw_ConsoleBackground

	Draws console background (obviously!)  Completely rewritten to use
	several simple yet very cool GL effects.  --KB
*/
VISIBLE void
Draw_ConsoleBackground (int lines, byte alpha)
{
	float       ofs;
	glpic_t    *gl;
	qpic_t     *conback;

	GL_FlushText (); // Flush text that should be rendered before the console

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
	if (gl_constretch->int_val) {
		ofs = 0;
	} else
		ofs = (vid.conheight - lines) / (float) vid.conheight;

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
VISIBLE void
Draw_TileClear (int x, int y, int w, int h)
{
	glpic_t    *gl;
	qfglColor3ubv (color_0_8);
	gl = (glpic_t *) draw_backtile->data;
	qfglBindTexture (GL_TEXTURE_2D, gl->texnum);
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
VISIBLE void
Draw_Fill (int x, int y, int w, int h, int c)
{
	qfglDisable (GL_TEXTURE_2D);
	qfglColor3ubv (vid.palette + c * 3);

	qfglBegin (GL_QUADS);

	qfglVertex2f (x, y);
	qfglVertex2f (x + w, y);
	qfglVertex2f (x + w, y + h);
	qfglVertex2f (x, y + h);

	qfglEnd ();
	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);
}

VISIBLE void
Draw_FadeScreen (void)
{
	GL_FlushText (); // Flush text that should be rendered before the menu

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

	qfglEnableClientState (GL_VERTEX_ARRAY);
	qfglVertexPointer (2, GL_FLOAT, 0, textVertices);
	qfglEnableClientState (GL_TEXTURE_COORD_ARRAY);
	qfglTexCoordPointer (2, GL_FLOAT, 0, textCoords);
	qfglDisableClientState (GL_COLOR_ARRAY);
}

void
GL_DrawReset (void)
{
	tVAcount = 0;
	tV = textVertices;
	tC = textCoords;
}

void
GL_FlushText (void)
{
	if (tVAcount) {
		flush_text ();
	}
}
