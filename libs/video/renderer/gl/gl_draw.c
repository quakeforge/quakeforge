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
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"
#include "QF/GL/types.h"
#include "QF/ui/view.h"

#include "compat.h"
#include "r_internal.h"
#include "sbar.h"
#include "varrays.h"

#define CELL_SIZE (1.0 / 16.0)	// conchars is 16x16
#define CELL_INSET (1.0 / 4.0)	// of a pixel
typedef struct {
	float       tlx, tly;
	float       trx, try;
	float       brx, bry;
	float       blx, bly;
} cc_cell_t;

static int         textUseVA;
static int         tVAsize;
static int        *tVAindices;
static int         tVAcount;
static float      *textVertices, *tV;
static float      *textCoords, *tC;

static qpic_t	   *draw_backtile;

static cc_cell_t char_cells[256];
static GLuint translate_texture;
static int	char_texture;
static int	cs_texture;					// crosshair texturea

static byte color_0_8[4] = { 204, 204, 204, 255 };

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

	if (vaelements < 0) {
		textUseVA = 0;
		tVAsize = 2048;
		Sys_MaskPrintf (SYS_dev, "Text: Vertex Array use disabled.\n");
	} else {
		textUseVA = 1;
		if (vaelements > 3)
			tVAsize = vaelements - (vaelements % 4);
		else
			tVAsize = 2048;
		Sys_MaskPrintf (SYS_dev, "Text: %i maximum vertex elements.\n",
						tVAsize);
	}

	if (textVertices)
		free (textVertices);
	textVertices = calloc (tVAsize, 2 * sizeof (float));

	if (textCoords)
		free (textCoords);
	textCoords = calloc (tVAsize, 2 * sizeof (float));

	if (textUseVA) {
		qfglTexCoordPointer (2, GL_FLOAT, 0, textCoords);
		qfglVertexPointer (2, GL_FLOAT, 0, textVertices);
	}
	if (tVAindices)
		free (tVAindices);
	tVAindices = (int *) calloc (tVAsize, sizeof (int));
	for (i = 0; i < tVAsize; i++)
		tVAindices[i] = i;
}

qpic_t *
gl_Draw_MakePic (int width, int height, const byte *data)
{
	glpic_t	   *gl;
	qpic_t	   *pic;

	pic = malloc (field_offset (qpic_t, data[sizeof (glpic_t)]));
	pic->width = width;
	pic->height = height;
	gl = (glpic_t *) pic->data;
	gl->texnum = GL_LoadTexture ("", width, height, data, false, true, 1);
	return pic;
}

void
gl_Draw_DestroyPic (qpic_t *pic)
{
	//FIXME gl texture management sucks
	free (pic);
}

qpic_t *
gl_Draw_PicFromWad (const char *name)
{
	glpic_t	   *gl;
	qpic_t	   *p, *pic;
	tex_t	   *targa;

	pic = W_GetLumpName (name);
	targa = LoadImage (name, 1);
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

qpic_t *
gl_Draw_CachePic (const char *path, qboolean alpha)
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
		qpic_t     *dat = (qpic_t *) QFS_LoadFile (QFS_FOpenFile (path), 0);
		if (!dat)
			Sys_Error ("Draw_CachePic: failed to load %s", path);

		// Adjust for endian..
		SwapPic (dat);
		// Check for a .tga first
		targa = LoadImage (path, 1);
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

	memset (pic->name, 0, sizeof (pic->name));
	strncpy (pic->name, path, sizeof (pic->name) - 1);

	// Now lets mark this cache entry as used..
	pic->dirty = false;
	numcachepics++;

	// And now we are done, return what was asked for..
	return &pic->pic;
}

void
gl_Draw_UncachePic (const char *path)
{
	cachepic_t *pic;
	int         i;

	//FIXME chachpic and uncachepic suck in GL
	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++) {
		if ((!strcmp (path, pic->name)) && !pic->dirty) {
			pic->dirty = true;
			return;
		}
	}
}

void
gl_Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
	int         cx, cy, n;
	qpic_t     *p;

	// draw left side
	color_white[3] = alpha;
	qfglColor4ubv (color_white);
	cx = x;
	cy = y;
	p = gl_Draw_CachePic ("gfx/box_tl.lmp", true);
	gl_Draw_Pic (cx, cy, p);
	p = gl_Draw_CachePic ("gfx/box_ml.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		gl_Draw_Pic (cx, cy, p);
	}
	p = gl_Draw_CachePic ("gfx/box_bl.lmp", true);
	gl_Draw_Pic (cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = gl_Draw_CachePic ("gfx/box_tm.lmp", true);
		gl_Draw_Pic (cx, cy, p);
		p = gl_Draw_CachePic ("gfx/box_mm.lmp", true);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = gl_Draw_CachePic ("gfx/box_mm2.lmp", true);
			gl_Draw_Pic (cx, cy, p);
		}
		p = gl_Draw_CachePic ("gfx/box_bm.lmp", true);
		gl_Draw_Pic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = gl_Draw_CachePic ("gfx/box_tr.lmp", true);
	gl_Draw_Pic (cx, cy, p);
	p = gl_Draw_CachePic ("gfx/box_mr.lmp", true);
	for (n = 0; n < lines; n++) {
		cy += 8;
		gl_Draw_Pic (cx, cy, p);
	}
	p = gl_Draw_CachePic ("gfx/box_br.lmp", true);
	gl_Draw_Pic (cx, cy + 8, p);
	qfglColor3ubv (color_white);
}

void
gl_Draw_Init (void)
{
	int	     i;
	tex_t	*image;
	float    width, height;
	qpic_t  *ch_pic;

	Cmd_AddCommand ("gl_texturemode", &GL_TextureMode_f,
					"Texture mipmap quality.");

	QFS_GamedirCallback (Draw_ClearCache);

	// load the console background and the charset by hand, because we need to
	// write the version string into the background before turning it into a
	// texture

	image = LoadImage ("gfx/conchars", 1);
	if (image) {
		if (image->format < 4) {
			char_texture = GL_LoadTexture ("charset", image->width,
										   image->height, image->data, false,
										   false, 3);
		} else
			char_texture = GL_LoadTexture ("charset", image->width,
										   image->height, image->data, false,
										   true, 4);
		width = image->width;
		height = image->height;
	} else {
		draw_chars = W_GetLumpName ("conchars");
		for (i = 0; i < 256 * 64; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;		// proper transparent color

		char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false,
									   true, 1);
		width = 128;
		height = 128;
	}

	// initialize the character cell texture coordinates.
	for (i = 0; i < 256; i++) {
		float       fcol, frow;

		fcol = (i & 15) * CELL_SIZE;
		frow = (i >> 4) * CELL_SIZE;

		char_cells[i].tlx = fcol + CELL_INSET / width;
		char_cells[i].tly = frow + CELL_INSET / height;
		char_cells[i].trx = fcol - CELL_INSET / width  + CELL_SIZE;
		char_cells[i].try = frow + CELL_INSET / height;
		char_cells[i].brx = fcol - CELL_INSET / width  + CELL_SIZE;
		char_cells[i].bry = frow - CELL_INSET / height + CELL_SIZE;
		char_cells[i].blx = fcol + CELL_INSET / width;
		char_cells[i].bly = frow - CELL_INSET / height + CELL_SIZE;
	}

	ch_pic = Draw_CrosshairPic ();
	cs_texture = GL_LoadTexture ("crosshair",
								 CROSSHAIR_WIDTH * CROSSHAIR_TILEX,
								 CROSSHAIR_HEIGHT * CROSSHAIR_TILEY,
								 ch_pic->data, false, true, 1);
	free (ch_pic);

	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// save a texture slot for translated picture
	qfglGenTextures (1, &translate_texture);

	// get the other pics we need
	draw_backtile = gl_Draw_PicFromWad ("backtile");

	Draw_InitText ();
}

static inline void
flush_text (void)
{
	qfglBindTexture (GL_TEXTURE_2D, char_texture);
	if (textUseVA) {
		qfglDrawElements (GL_QUADS, tVAcount, GL_UNSIGNED_INT, tVAindices);
	} else {
		float      *v = textVertices;
		float      *c = textCoords;
		int         i;

		qfglBegin (GL_QUADS);
		for (i = 0; i < tVAcount; i++) {
			qfglTexCoord2fv (c);
			qfglVertex2fv (v);
			c += 2;
			v += 2;
		}
		qfglEnd ();
	}
	tVAcount = 0;
	tV = textVertices;
	tC = textCoords;
}

static inline void
queue_character (float x, float y, int chr)
{
	float      *coord = &char_cells[chr & 255].tlx;
	*tV++ = x;
	*tV++ = y;
	*tV++ = x + 8.0;
	*tV++ = y;
	*tV++ = x + 8.0;
	*tV++ = y + 8.0;
	*tV++ = x;
	*tV++ = y + 8.0;
	*tC++ = *coord++;
	*tC++ = *coord++;
	*tC++ = *coord++;
	*tC++ = *coord++;
	*tC++ = *coord++;
	*tC++ = *coord++;
	*tC++ = *coord++;
	*tC++ = *coord++;
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
void
gl_Draw_Character (int x, int y, unsigned int chr)
{
	chr &= 255;

	if (chr == 32)
		return;							// space
	if (y <= -8)
		return;							// totally off screen

	queue_character ((float) x, (float) y, chr);
	tVA_increment ();
}

void
gl_Draw_String (int x, int y, const char *str)
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

void
gl_Draw_nString (int x, int y, const char *str, int count)
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
gl_Draw_AltString (int x, int y, const char *str)
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
	gl_Draw_Character (x - 4, y - 4, '+');
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

static void
crosshair_4 (int x, int y)
{
	unsigned char *pColor;

	pColor = (unsigned char *) &d_8to24table[crosshaircolor->int_val];
	qfglColor4ubv (pColor);
	qfglBindTexture (GL_TEXTURE_2D, cs_texture);

	qfglBegin (GL_QUADS);

	qfglTexCoord2f (0, 0.5);
	qfglVertex2f (x - 7, y - 7);
	qfglTexCoord2f (0.5, 0.5);
	qfglVertex2f (x + 9, y - 7);
	qfglTexCoord2f (0.5, 1);
	qfglVertex2f (x + 9, y + 9);
	qfglTexCoord2f (0, 1);
	qfglVertex2f (x - 7, y + 9);

	qfglEnd ();
	qfglColor3ubv (color_white);
}

static void
crosshair_5 (int x, int y)	//FIXME don't use until the data is filled in
{
	unsigned char *pColor;

	pColor = (unsigned char *) &d_8to24table[crosshaircolor->int_val];
	qfglColor4ubv (pColor);
	qfglBindTexture (GL_TEXTURE_2D, cs_texture);

	qfglBegin (GL_QUADS);

	qfglTexCoord2f (0.5, 0.5);
	qfglVertex2f (x - 7, y - 7);
	qfglTexCoord2f (1, 0.5);
	qfglVertex2f (x + 9, y - 7);
	qfglTexCoord2f (1, 1);
	qfglVertex2f (x + 9, y + 9);
	qfglTexCoord2f (0.5, 1);
	qfglVertex2f (x - 7, y + 9);

	qfglEnd ();
	qfglColor3ubv (color_white);
}

static void (*crosshair_func[]) (int x, int y) = {
	crosshair_1,
	crosshair_2,
	crosshair_3,
	crosshair_4,
	crosshair_5,
};

void
gl_Draw_Crosshair (void)
{
	int            x, y;
	int            ch;

	ch = crosshair->int_val - 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	x = vid.conview->xlen / 2 + cl_crossx->int_val;
	y = vid.conview->ylen / 2 + cl_crossy->int_val;

	crosshair_func[ch] (x, y);
}

void
gl_Draw_CrosshairAt (int ch, int x, int y)
{
	ch -= 1;
	if ((unsigned) ch >= sizeof (crosshair_func) / sizeof (crosshair_func[0]))
		return;

	crosshair_func[ch] (x, y);
}

void
gl_Draw_Pic (int x, int y, qpic_t *pic)
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
gl_Draw_Picf (float x, float y, qpic_t *pic)
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
gl_Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
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
void
gl_Draw_ConsoleBackground (int lines, byte alpha)
{
	float       ofs;
	glpic_t    *gl;
	qpic_t     *conback;

	GL_FlushText (); // Flush text that should be rendered before the console

	// This can be a CachePic now, just like in software
	conback = gl_Draw_CachePic ("gfx/conback.lmp", false);
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
		ofs = (vid.conview->ylen - lines) / (float) vid.conview->ylen;

	color_0_8[3] = alpha;
	qfglColor4ubv (color_0_8);

	// draw the console texture
	qfglBindTexture (GL_TEXTURE_2D, gl->texnum);
	qfglBegin (GL_QUADS);
	qfglTexCoord2f (0, 0 + ofs);
	qfglVertex2f (0, 0);
	qfglTexCoord2f (1, 0 + ofs);
	qfglVertex2f (vid.conview->xlen, 0);
	qfglTexCoord2f (1, 1);
	qfglVertex2f (vid.conview->xlen, lines);
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

	int         len = strlen (cl_verstring->string);
	gl_Draw_AltString (vid.conview->xlen - len * 8 - 11, lines - 14,
					   cl_verstring->string);
	qfglColor3ubv (color_white);
}

/*
	Draw_TileClear

	This repeats a 64*64 tile graphic to fill the screen around a sized down
	refresh window.
*/
void
gl_Draw_TileClear (int x, int y, int w, int h)
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
void
gl_Draw_Fill (int x, int y, int w, int h, int c)
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

void
gl_Draw_FadeScreen (void)
{
	GL_FlushText (); // Flush text that should be rendered before the menu

	qfglDisable (GL_TEXTURE_2D);
	qfglColor4ub (0, 0, 0, 179);
	qfglBegin (GL_QUADS);

	qfglVertex2f (0, 0);
	qfglVertex2f (vid.conview->xlen, 0);
	qfglVertex2f (vid.conview->xlen, vid.conview->ylen);
	qfglVertex2f (0, vid.conview->ylen);

	qfglEnd ();
	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);
}

static void
set_2d (int width, int height)
{
	qfglViewport (0, 0, vid.width, vid.height);

	qfglMatrixMode (GL_PROJECTION);
	qfglLoadIdentity ();
	qfglOrtho (0, width, height, 0, -99999, 99999);

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
GL_Set2D (void)
{
	set_2d (vid.width, vid.height);
}

void
GL_Set2DScaled (void)
{
	set_2d (vid.conview->xlen, vid.conview->ylen);
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

void
gl_Draw_BlendScreen (quat_t color)
{
	if (!color[3])
		return;

	qfglDisable (GL_TEXTURE_2D);

	qfglBegin (GL_QUADS);

	qfglColor4fv (color);
	qfglVertex2f (0, 0);
	qfglVertex2f (vid.conview->xlen, 0);
	qfglVertex2f (vid.conview->xlen, vid.conview->ylen);
	qfglVertex2f (0, vid.conview->ylen);

	qfglEnd ();

	qfglColor3ubv (color_white);
	qfglEnable (GL_TEXTURE_2D);
}
