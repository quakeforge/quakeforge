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

#include <string.h>
#include <stdio.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "QF/compat.h"
#include "vid.h"
#include "QF/sys.h"
#include "QF/mathlib.h"					// needed by: protocol.h, render.h,
										// client.h,
			// modelgen.h, glmodel.h
#include "wad.h"
#include "draw.h"
#include "QF/cvar.h"
#include "net.h"						// needed by: client.h
#include "protocol.h"					// needed by: client.h
#include "QF/cmd.h"
#include "sbar.h"
#include "render.h"						// needed by: client.h, model.h,
										// glquake.h
#include "client.h"						// need cls in this file
#include "QF/console.h"
#include "glquake.h"
#include "view.h"

static int  GL_LoadPicTexture (qpic_t *pic);

extern byte *host_basepal;
extern unsigned char d_15to8table[65536];
extern cvar_t *crosshair, *cl_crossx, *cl_crossy, *crosshaircolor;
extern qboolean lighthalf;

cvar_t     *gl_nobind;
cvar_t     *gl_max_size;
cvar_t     *gl_picmip;

cvar_t     *gl_constretch;
cvar_t     *gl_conalpha;
cvar_t     *gl_conspin;
cvar_t     *cl_verstring;
cvar_t     *gl_lightmode;				// LordHavoc: lighting mode

byte       *draw_chars;					// 8*8 graphic characters
qpic_t     *draw_disc;
qpic_t     *draw_backtile;

static int  ltexcrctable[256];			// cache mismatch checking  --KB


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

int         gl_lightmap_format = 4;
int         gl_solid_format = 3;
int         gl_alpha_format = 4;

static int  gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int  gl_filter_max = GL_LINEAR;


typedef struct {
	int         texnum;
	char        identifier[64];
	int         width, height;
	int         bytesperpixel;
	qboolean    mipmap;
	int         crc;					// not really a standard CRC, but it
	// works
} gltexture_t;

static gltexture_t gltextures[MAX_GLTEXTURES];
static int  numgltextures = 0;

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

  Note, this is a kluge, which may slow down sane cards..
  As such its all contained in ifdefs..

=============================================================================
*/

#undef gl_draw_scraps

#ifdef gl_draw_scraps

#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

static int  scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
static byte scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT * 4];
static qboolean scrap_dirty;
static int  scrap_texnum;

// returns a texture number and the position inside it
static int
Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int         i, j;
	int         best, best2;
	int         texnum;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++) {
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++) {
			best2 = 0;

			for (j = 0; j < w; j++) {
				if (scrap_allocated[texnum][i + j] >= best)
					break;
				if (scrap_allocated[texnum][i + j] > best2)
					best2 = scrap_allocated[texnum][i + j];
			}
			if (j == w) {				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		scrap_dirty = true;
		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
	return 0;
}

static void
Scrap_Upload (void)
{
	int         texnum;

	scrap_uploads++;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++) {
		glBindTexture (GL_TEXTURE_2D, scrap_texnum);
		GL_Upload8 (scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);
	}
	scrap_dirty = false;
}

#endif

//=============================================================================
/* Support Routines */

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

qpic_t     *
Draw_PicFromWad (char *name)
{
	qpic_t     *p;
	glpic_t    *gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *) p->data;

#ifdef gl_draw_scraps
	// load little ones into the scrap
	if (p->width < 64 && p->height < 64) {
		int         x, y;
		int         i, j, k;
		int         texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		k = 0;
		for (i = 0; i < p->height; i++)
			for (j = 0; j < p->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] =
					p->data[k];
		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x + 0.01) / (float) BLOCK_WIDTH;
		gl->sh = (x + p->width - 0.01) / (float) BLOCK_WIDTH;
		gl->tl = (y + 0.01) / (float) BLOCK_WIDTH;
		gl->th = (y + p->height - 0.01) / (float) BLOCK_WIDTH;
	} else
#endif
	{
		gl->texnum = GL_LoadPicTexture (p);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}

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

/*
================
Draw_CachePic
================
*/
qpic_t     *
Draw_CachePic (char *path)
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
	gl->texnum = GL_LoadPicTexture (dat);

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
	// for the menu system.
	// 
	// Some days I really dislike legacy support..

	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width * dat->height);

	// And now we are done, return what was asked for..
	return &pic->pic;
}


typedef struct {
	char       *name;
	int         minimize, maximize;
} glmode_t;

static glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
static void
Draw_TextureMode_f (void)
{
	int         i;
	gltexture_t *glt;

	if (Cmd_Argc () == 1) {
		for (i = 0; i < 6; i++)
			if (gl_filter_min == modes[i].minimize) {
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i = 0; i < 6; i++) {
		if (!strcasecmp (modes[i].name, Cmd_Argv (1)))
			break;
	}
	if (i == 6) {
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->mipmap) {
			glBindTexture (GL_TEXTURE_2D, glt->texnum);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
							 gl_filter_min);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
							 gl_filter_max);
		}
	}
}

extern void glrmain_init ();
extern void glrsurf_init ();

/*
===============
Draw_Init
===============
*/
void
Draw_Init (void)
{
	int         i;

	// LordHavoc: lighting mode
	gl_lightmode = Cvar_Get ("gl_lightmode", "0", CVAR_ARCHIVE, NULL,
							 "Lighting mode (0 = GLQuake style, 1 = new style)");
	gl_nobind = Cvar_Get ("gl_nobind", "0", CVAR_NONE, NULL,
						  "whether or not to inhibit texture binding");
	gl_max_size = Cvar_Get ("gl_max_size", "1024", CVAR_NONE, NULL, "None");
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, NULL, "None");

	// Console effects  --KB
	gl_constretch = Cvar_Get ("gl_constretch", "0", CVAR_ARCHIVE, NULL,
							  "whether slide the console or stretch it");
	gl_conalpha = Cvar_Get ("gl_conalpha", "0.6", CVAR_ARCHIVE, NULL,
							"alpha value for the console background");
	gl_conspin = Cvar_Get ("gl_conspin", "0", CVAR_ARCHIVE, NULL,
						   "speed at which the console spins");

	cl_verstring = Cvar_Get ("cl_verstring", PROGRAM " " VERSION, CVAR_NONE, 
			NULL, "client version string");

	// 3dfx can only handle 256 wide textures
	if (!strncasecmp ((char *) gl_renderer, "3dfx", 4) ||
		!strncasecmp ((char *) gl_renderer, "Mesa", 4))
		Cvar_Set (gl_max_size, "256");

	// LordHavoc: 3DFX's dithering has terrible artifacting when using
	// lightmode 1
	if (!strncasecmp ((char *) gl_renderer, "3dfx", 4))
		Cvar_Set (gl_lightmode, "0");
	lighthalf = gl_lightmode->int_val != 0;	// to avoid re-rendering all
	// lightmaps on first frame

	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f, "No Description");

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;		// proper transparent color

	// now turn them into textures
//  char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);    // 1999-12-27 Conwidth/height charset fix by TcT
//  Draw_CrosshairAdjust();
	cs_texture = GL_LoadTexture ("crosshair", 8, 8, cs_data, false, true, 1);
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);	// 1999-12-27 
																						// 
	// 
	// Conwidth/height 
	// charset 
	// fix 
	// by 
	// TcT

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

#ifdef gl_draw_scraps
	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;
#endif

	// 
	// get the other pics we need
	// 
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	// LordHavoc: call init code for other GL renderer modules;
	glrmain_init ();
	glrsurf_init ();
}



/*
================
Draw_Character8

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
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

	if (lighthalf)
		glColor3f (0.5, 0.5, 0.5);
	else
		glColor3f (1, 1, 1);
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

/*
================
Draw_String8
================
*/
void
Draw_String8 (int x, int y, char *str)
{
	while (*str) {
		Draw_Character8 (x, y, *str);
		str++;
		x += 8;
	}
}

/*
================
Draw_AltString8
================
*/
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

	if (crosshair->int_val == 2) {
		x = scr_vrect.x + scr_vrect.width / 2 - 3 + cl_crossx->int_val;
		y = scr_vrect.y + scr_vrect.height / 2 - 3 + cl_crossy->int_val;

		pColor =
			(unsigned char *) &d_8to24table[(byte) crosshaircolor->int_val];
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
	} else if (crosshair->int_val)
		Draw_Character8 (scr_vrect.x + scr_vrect.width / 2 - 4 +
						 cl_crossx->int_val,
						 scr_vrect.y + scr_vrect.height / 2 - 4 +
						 cl_crossy->int_val, '+');
}

/*
=============
Draw_Pic
=============
*/
void
Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t    *gl;

#ifdef gl_draw_scraps
	if (scrap_dirty)
		Scrap_Upload ();
#endif
	gl = (glpic_t *) pic->data;
	if (lighthalf)
		glColor3f (0.4, 0.4, 0.4);
	else
		glColor3f (0.8, 0.8, 0.8);
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

#ifdef gl_draw_scraps
	if (scrap_dirty)
		Scrap_Upload ();
#endif
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
}

/*
=============
Draw_TransPic
=============
*/
void
Draw_TransPic (int x, int y, qpic_t *pic)
{

	if (x < 0 || (unsigned) (x + pic->width) > vid.width || y < 0 ||
		(unsigned) (y + pic->height) > vid.height) {
		Sys_Error ("Draw_TransPic: bad coordinates");
	}

	Draw_Pic (x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void
Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte * translation)
{
	int         v, u, c;
	unsigned    trans[64 * 64], *dest;
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

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
	conback = Draw_CachePic ("gfx/conback.lmp");
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
	if (gl_constretch->value)
		ofs = 0;
	else
		ofs = (vid.conheight - lines) / (float) vid.conheight;

	y = vid.height >> 1;
	if (lines > y) {
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
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
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
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void
Draw_Fill (int x, int y, int w, int h, int c)
{
	glDisable (GL_TEXTURE_2D);
	if (lighthalf)
		glColor3f (host_basepal[c * 3] / 510.0, host_basepal[c * 3 + 1] / 510.0,
				   host_basepal[c * 3 + 2] / 510.0);
	else
		glColor3f (host_basepal[c * 3] / 255.0, host_basepal[c * 3 + 1] / 255.0,
				   host_basepal[c * 3 + 2] / 255.0);

	glBegin (GL_QUADS);

	glVertex2f (x, y);
	glVertex2f (x + w, y);
	glVertex2f (x + w, y + h);
	glVertex2f (x, y + h);

	glEnd ();
	if (lighthalf)
		glColor3f (0.5, 0.5, 0.5);
	else
		glColor3f (1, 1, 1);
	glEnable (GL_TEXTURE_2D);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
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
	if (lighthalf)
		glColor3f (0.5, 0.5, 0.5);
	else
		glColor3f (1, 1, 1);
	glEnable (GL_TEXTURE_2D);

	Sbar_Changed ();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void
Draw_BeginDisc (void)
{
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void
Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
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

	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (lighthalf)
		glColor3f (0.5, 0.5, 0.5);
	else
		glColor3f (1, 1, 1);
}

//====================================================================

/*
================
GL_ResampleTexture
================
*/
static void
GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,
					int outwidth, int outheight)
{
	int         i, j;
	unsigned   *inrow;
	unsigned    frac, fracstep;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j += 4) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 1] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 2] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 3] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

/*
================
GL_Resample8BitTexture -- JACK
================
*/
static void
GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight,
						unsigned char *out, int outwidth, int outheight)
{
	int         i, j;
	unsigned char *inrow;
	unsigned    frac, fracstep;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j += 4) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 1] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 2] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 3] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
static void
GL_MipMap (byte * in, int width, int height)
{
	int         i, j;
	byte       *out;

	width <<= 2;
	height >>= 1;
	out = in;
	for (i = 0; i < height; i++, in += width) {
		for (j = 0; j < width; j += 8, out += 4, in += 8) {
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
static void
GL_MipMap8Bit (byte * in, int width, int height)
{
	int         i, j;
	byte       *out;
	unsigned short r, g, b;
	byte       *at1, *at2, *at3, *at4;

	height >>= 1;
	out = in;
	for (i = 0; i < height; i++, in += width)
		for (j = 0; j < width; j += 2, out += 1, in += 2) {
			at1 = (byte *) & d_8to24table[in[0]];
			at2 = (byte *) & d_8to24table[in[1]];
			at3 = (byte *) & d_8to24table[in[width + 0]];
			at4 = (byte *) & d_8to24table[in[width + 1]];

			r = (at1[0] + at2[0] + at3[0] + at4[0]);
			r >>= 5;
			g = (at1[1] + at2[1] + at3[1] + at4[1]);
			g >>= 5;
			b = (at1[2] + at2[2] + at3[2] + at4[2]);
			b >>= 5;

			out[0] = d_15to8table[(r << 0) + (g << 5) + (b << 10)];
		}
}

/*
===============
GL_Upload32
===============
*/
static void
GL_Upload32 (unsigned *data, int width, int height, qboolean mipmap,
			 qboolean alpha)
{
	int         samples;
	static unsigned scaled[1024 * 512];	// [512*256];
	int         scaled_width, scaled_height;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= gl_picmip->int_val;
	scaled_height >>= gl_picmip->int_val;

	scaled_width = min (scaled_width, gl_max_size->int_val);
	scaled_height = min (scaled_height, gl_max_size->int_val);

	if (scaled_width * scaled_height > sizeof (scaled) / 4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;

#if 0
	if (mipmap)
		gluBuild2DMipmaps (GL_TEXTURE_2D, samples, width, height, GL_RGBA,
						   GL_UNSIGNED_BYTE, trans);
	else if (scaled_width == width && scaled_height == height)
		glTexImage2D (GL_TEXTURE_2D, 0, samples, width, height, 0, GL_RGBA,
					  GL_UNSIGNED_BYTE, trans);
	else {
		gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, trans,
					   scaled_width, scaled_height, GL_UNSIGNED_BYTE, scaled);
		glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
					  GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
#else

	if (scaled_width == width && scaled_height == height) {
		if (!mipmap) {
			glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width,
						  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width * height * 4);
	} else
		GL_ResampleTexture (data, width, height, scaled, scaled_width,
							scaled_height);

	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
				  GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap) {
		int         miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap ((byte *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width,
						  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
  done:;
#endif


	if (mipmap) {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
}

void
GL_Upload8_EXT (byte * data, int width, int height, qboolean mipmap,
				qboolean alpha)
{
	int         i, s;
	qboolean    noalpha;
	int         samples;
	static unsigned char scaled[1024 * 512];	// [512*256];
	int         scaled_width, scaled_height;

	s = width * height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha) {
		noalpha = true;
		for (i = 0; i < s; i++) {
			if (data[i] == 255)
				noalpha = false;
		}

		if (alpha && noalpha)
			alpha = false;
	}
	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= gl_picmip->int_val;
	scaled_height >>= gl_picmip->int_val;

	scaled_width = min (scaled_width, gl_max_size->int_val);
	scaled_height = min (scaled_height, gl_max_size->int_val);

	if (scaled_width * scaled_height > sizeof (scaled))
		Sys_Error ("GL_LoadTexture: too big");

	samples = 1;						// alpha ? gl_alpha_format :
	// gl_solid_format;

	if (scaled_width == width && scaled_height == height) {
		if (!mipmap) {
/* FIXME - what if this extension isn't available? */
#ifdef HAVE_GL_COLOR_INDEX8_EXT
			glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width,
						  scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
						  data);
#else
			/* FIXME - should warn that this isn't available */
#endif
			goto done;
		}
		memcpy (scaled, data, width * height);
	} else
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width,
								scaled_height);

// FIXME - what if this extension isn't available?
#ifdef HAVE_GL_COLOR_INDEX8_EXT
	glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width,
				  scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);
#else
	/* FIXME - should warn that this isn't available */
#endif
	if (mipmap) {
		int         miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap8Bit ((byte *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
/* FIXME - what if this extension isn't available? */
#ifdef HAVE_GL_COLOR_INDEX8_EXT
			glTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT,
						  scaled_width, scaled_height, 0, GL_COLOR_INDEX,
						  GL_UNSIGNED_BYTE, scaled);
#else
			/* FIXME - should warn that this isn't available */
#endif
		}
	}
  done:;

	if (mipmap) {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
}

extern qboolean VID_Is8bit ();

/*
===============
GL_Upload8
===============
*/
void
GL_Upload8 (byte * data, int width, int height, qboolean mipmap, qboolean alpha)
{
	static unsigned trans[640 * 480];	// FIXME, temporary
	int         i, s;
	qboolean    noalpha;
	int         p;

	s = width * height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha) {
		noalpha = true;
		for (i = 0; i < s; i++) {
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	} else {
		if (s & 3)
			Sys_Error ("GL_Upload8: s&3");
		for (i = 0; i < s; i += 4) {
			trans[i] = d_8to24table[data[i]];
			trans[i + 1] = d_8to24table[data[i + 1]];
			trans[i + 2] = d_8to24table[data[i + 2]];
			trans[i + 3] = d_8to24table[data[i + 3]];
		}
	}

#ifdef gl_draw_scraps
	if (VID_Is8bit () && !alpha && (data != scrap_texels[0]))
#else
	if (VID_Is8bit () && !alpha)
#endif
	{
		GL_Upload8_EXT (data, width, height, mipmap, alpha);
		return;
	}

	GL_Upload32 (trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
int
GL_LoadTexture (char *identifier, int width, int height, byte * data,
				qboolean mipmap, qboolean alpha, int bytesperpixel)
{
	int         i, s, lcrc;
	gltexture_t *glt;

	// LordHavoc's cache check, not a standard crc but it works  --KB
	lcrc = 0;
	s = width * height * bytesperpixel;	// size
	for (i = 0; i < 256; i++)
		ltexcrctable[i] = i + 1;
	for (i = 0; i < s; i++)
		lcrc += (ltexcrctable[data[i] & 255]++);

	// see if the texture is already present
	if (identifier[0]) {
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
			if (!strcmp (identifier, glt->identifier)) {
				if (lcrc != glt->crc
					|| width != glt->width
					|| height != glt->height
					|| bytesperpixel != glt->bytesperpixel) goto SetupTexture;
				else
					return gltextures[i].texnum;
			}
		}
	}

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("numgltextures == MAX_GLTEXTURES");

	glt = &gltextures[numgltextures];
	numgltextures++;

	strncpy (glt->identifier, identifier, sizeof (glt->identifier) - 1);
	glt->identifier[sizeof (glt->identifier) - 1] = '\0';

	glt->texnum = texture_extension_number;
	texture_extension_number++;

  SetupTexture:
	glt->crc = lcrc;
	glt->width = width;
	glt->height = height;
	glt->bytesperpixel = bytesperpixel;
	glt->mipmap = mipmap;

	glBindTexture (GL_TEXTURE_2D, glt->texnum);

	switch (glt->bytesperpixel) {
		case 1:
		GL_Upload8 (data, width, height, mipmap, alpha);
		break;
		case 4:
		GL_Upload32 ((unsigned *) data, width, height, mipmap, alpha);
		break;
		default:
		Sys_Error ("SetupTexture: unknown bytesperpixel %i",
				   glt->bytesperpixel);
	}

	return glt->texnum;
}


/*
================
GL_LoadPicTexture
================
*/
static int
GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true,
						   1);
}
