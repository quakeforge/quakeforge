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

#include "cmd.h"
#include "console.h"
#include "crc.h"
#include "draw.h"
#include "glquake.h"
#include "sbar.h"
#include "screen.h"
#include "sys.h"

static int  GL_LoadPicTexture (qpic_t *pic, qboolean alpha);

extern byte *host_basepal;
extern unsigned char d_15to8table[65536];
extern cvar_t *crosshair, *cl_crossx, *cl_crossy, *crosshaircolor,
	*gl_lightmap_components;

cvar_t     *gl_max_size;
cvar_t     *gl_picmip;

cvar_t     *gl_constretch;
cvar_t     *gl_conalpha;
cvar_t     *gl_conspin;
cvar_t     *cl_verstring;
cvar_t     *gl_lightmode;				// LordHavoc: lighting mode

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

int         gl_lightmap_format = 4; //FIXME: use GL types? --Despair
int         gl_solid_format = 3;
int         gl_alpha_format = 4;

int  gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int  gl_filter_max = GL_LINEAR;


typedef struct {
	int         texnum;
	char        identifier[64];
	int         width, height;
	int         bytesperpixel;
	qboolean    mipmap;
	unsigned short crc;					// LordHavoc: CRC for texture
										// validation
} gltexture_t;

static gltexture_t gltextures[MAX_GLTEXTURES];
static int  numgltextures = 0;

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

qpic_t *
Draw_PicFromWad (char *name)
{
	qpic_t     *p;
	glpic_t    *gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *) p->data;

	gl->texnum = GL_LoadPicTexture (p, true);
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

/*
	Draw_CachePic
*/
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
	gl->texnum = GL_LoadPicTexture (dat, alpha);

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
	Draw_TextureMode_f
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
		Con_Printf ("current filter is unknown?\n");
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

/*
	Draw_Init
*/
void
Draw_Init (void)
{
	int         i;

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
	if (lighthalf) {
		lighthalf_v[0] = lighthalf_v[1] = lighthalf_v[2] = 128;
	} else {
		lighthalf_v[0] = lighthalf_v[1] = lighthalf_v[2] = 255;
	}

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
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);	// 1999-12-27 Conwidth/height charset fix by TcT
	cs_texture = GL_LoadTexture ("crosshair", 8, 8, cs_data, false, true, 1);
//  char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);    // 1999-12-27 Conwidth/height charset fix by TcT

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// 
	// get the other pics we need
	// 
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	// LordHavoc: call init code for other GL renderer modules;
	glrmain_init ();
	glrsurf_init ();
}

void
Draw_Init_Cvars (void)
{
	gl_lightmode = Cvar_Get ("gl_lightmode", "1", CVAR_ARCHIVE,
							 "Lighting mode (0 = GLQuake style, 1 = new style)");
	gl_max_size = Cvar_Get ("gl_max_size", "1024", CVAR_NONE, "Texture dimension"); 
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, "Dimensions of displayed textures. 0 is normal, 1 is half, 2 is 1/4"); 
	gl_constretch = Cvar_Get ("gl_constretch", "0", CVAR_ARCHIVE,
							  "whether slide the console or stretch it");
	gl_conalpha = Cvar_Get ("gl_conalpha", "0.6", CVAR_ARCHIVE,
							"alpha value for the console background");
	gl_conspin = Cvar_Get ("gl_conspin", "0", CVAR_ARCHIVE,
						   "speed at which the console spins");
	gl_lightmap_components = Cvar_Get ("gl_lightmap_components", "4", CVAR_ROM, "Lightmap texture components. 1 is greyscale, 3 is RGB, 4 is RGBA.");
	cl_verstring = Cvar_Get ("cl_verstring", PROGRAM " " VERSION, CVAR_NONE,
							 "Client version string");
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

/*
	Draw_String8
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
	Draw_AltString8
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

/*
	Draw_Pic
*/
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
	// draw version string if not downloading
	if (!cls.download)
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
	glColor3ubv (lighthalf_v);
	glEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
	Draw_FadeScreen
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

//====================================================================

/*
	GL_ResampleTexture
*/
static void
GL_ResampleTexture (unsigned int *in, int inwidth, int inheight,
					unsigned int *out, int outwidth, int outheight)
{
	int         i, j;
	unsigned int *inrow;
	unsigned int frac, fracstep;

	if (!outwidth || !outheight)
		return;
	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j ++) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

/*
	GL_Resample8BitTexture -- JACK
*/
#if defined(GL_SHARED_TEXTURE_PALETTE_EXT) && defined(HAVE_GL_COLOR_INDEX8_EXT)
static void
GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight,
						unsigned char *out, int outwidth, int outheight)
{
	int         i, j;
	unsigned char *inrow;
	unsigned int frac, fracstep;

	if (!outwidth || !outheight)
		return;
	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth) {
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j ++) {
			out[j] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}
#endif

/*
	GL_MipMap

	Operates in place, quartering the size of the texture
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
	GL_MipMap8Bit

	Mipping for 8 bit textures
*/
#if defined(GL_SHARED_TEXTURE_PALETTE_EXT) && defined(HAVE_GL_COLOR_INDEX8_EXT)
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
#endif

/*
	GL_Upload32
*/
static void
GL_Upload32 (unsigned int *data, int width, int height, qboolean mipmap,
			 qboolean alpha)
{
	unsigned int *scaled;
	int         scaled_width, scaled_height, samples;

        if (!width || !height)
                return; // Null texture

	// Snap the height and width to a power of 2.
	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= gl_picmip->int_val;
	scaled_height >>= gl_picmip->int_val;

	scaled_width = min (scaled_width, gl_max_size->int_val);
	scaled_height = min (scaled_height, gl_max_size->int_val);

	if (!(scaled = malloc (scaled_width * scaled_height * sizeof (GLuint))))
		Sys_Error ("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;

	// If the real width/height and the 'scaled' width/height then we
	// rescale it.
	if (scaled_width == width && scaled_height == height) {
		memcpy (scaled, data, width * height * sizeof (GLuint));
	} else {
		GL_ResampleTexture (data, width, height, scaled, scaled_width,
							scaled_height);
	}

	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0,
				  GL_RGBA, GL_UNSIGNED_BYTE, scaled);

	if (mipmap) {
		int         miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap ((byte *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			scaled_width = max (scaled_width, 1);
			scaled_height = max (scaled_height, 1);
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width,
						  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}

	if (mipmap) {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		if (gl_picmip->int_val)
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		else
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

		free (scaled);
}

/*
	GL_Upload8_EXT

	If we have shared or global palettes, upload an 8-bit texture. If we don't,
	this function does nothing.
*/
void
GL_Upload8_EXT (byte * data, int width, int height, qboolean mipmap,
				qboolean alpha)
{
#if defined(GL_SHARED_TEXTURE_PALETTE_EXT) && defined(HAVE_GL_COLOR_INDEX8_EXT)

	byte       *scaled;
	int         scaled_width, scaled_height;

	// Snap the height and width to a power of 2.
	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1);
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1);

	scaled_width >>= gl_picmip->int_val;
	scaled_height >>= gl_picmip->int_val;

	scaled_width = min (scaled_width, gl_max_size->int_val);
	scaled_height = min (scaled_height, gl_max_size->int_val);

	if (!(scaled = malloc (scaled_width * scaled_height)))
		Sys_Error ("GL_LoadTexture: too big");

	// If the real width/height and the 'scaled' width/height then we
	// rescale it.
	if (scaled_width == width && scaled_height == height) {
		memcpy (scaled, data, width * height);
	} else {
		GL_Resample8BitTexture (data, width, height, scaled, scaled_width,
								scaled_height);
	}

	glTexImage2D (GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width,
				  scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, scaled);

	if (mipmap) {
		int         miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1) {
			GL_MipMap8Bit ((byte *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			scaled_width = max (scaled_width, 1);
			scaled_height = max (scaled_height, 1);
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT,
						  scaled_width, scaled_height, 0, GL_COLOR_INDEX,
						  GL_UNSIGNED_BYTE, scaled);
		}
	}

	if (mipmap) {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		if (gl_picmip->int_val)
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		else
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	free (scaled);
#endif
}

extern qboolean VID_Is8bit (void);

/*
	GL_Upload8
*/
void
GL_Upload8 (byte * data, int width, int height, qboolean mipmap, qboolean alpha)
{
	unsigned int *trans = NULL;
	int         i, s, p;

	s = width * height;
	trans = malloc (s * sizeof (unsigned int));

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha) {
		alpha = false;
		for (i = 0; i < s; i++) {
			p = data[i];
			if (p == 255)
				alpha = true;
			trans[i] = d_8to24table[p];
		}
	} else {
		if (s & 3)
			Sys_Error ("GL_Upload8: width*height divisible by 3");
		for (i = 0; i < s; i += 4) {
			trans[i] = d_8to24table[data[i]];
			trans[i + 1] = d_8to24table[data[i + 1]];
			trans[i + 2] = d_8to24table[data[i + 2]];
			trans[i + 3] = d_8to24table[data[i + 3]];
		}
	}

#if defined(GL_SHARED_TEXTURE_PALETTE_EXT) && defined(HAVE_GL_COLOR_INDEX8_EXT)

	if (VID_Is8bit () && !alpha) {
		GL_Upload8_EXT (data, width, height, mipmap, alpha);
	} else {
#else
	{
#endif
		GL_Upload32 (trans, width, height, mipmap, alpha);
	}

	free (trans);
}

/*
	GL_LoadTexture
*/
int
GL_LoadTexture (char *identifier, int width, int height, byte * data,
				qboolean mipmap, qboolean alpha, int bytesperpixel)
{
	gltexture_t *glt;
	int         i, crc;

	// LordHavoc: now just using a standard CRC for texture validation
	crc = CRC_Block (data, width * height * bytesperpixel);

	// see if the texture is already present
	if (identifier[0]) {
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
			if (strequal (identifier, glt->identifier)) {
				if (crc != glt->crc
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
	glt->crc = crc;
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
			GL_Upload32 ((GLuint *) data, width, height, mipmap, alpha);
			break;
		default:
			Sys_Error ("SetupTexture: unknown bytesperpixel %i",
					   glt->bytesperpixel);
	}

	return glt->texnum;
}


/*
	GL_LoadPicTexture
*/
static int
GL_LoadPicTexture (qpic_t *pic, qboolean alpha)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, alpha,
						   1);
}
