
/*
	vid_common_gl.c

	Common OpenGL video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2000      Marcus Sundberg [mackan@stacken.kth.se]

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
#include "config.h"
#endif

#include <GL/gl.h>

#ifdef HAVE_GL_GLEXT_H
#include <GL/glext.h>
#endif

#include <string.h>

#ifdef HAVE_DLFCN_H
# include <dlfcn.h>
#endif
#ifndef RTLD_LAZY
# ifdef DL_LAZY
#  define RTLD_LAZY     DL_LAZY
# else
#  define RTLD_LAZY     0
# endif
#endif

#include "QF/console.h"
#include "glquake.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "sbar.h"

#define WARP_WIDTH              320
#define WARP_HEIGHT             200

#ifdef HAVE_DLOPEN
static void *dlhand = NULL;
#endif

//unsigned short    d_8to16table[256];
unsigned int d_8to24table[256];
unsigned char d_15to8table[65536];

cvar_t     *vid_mode;

/*-----------------------------------------------------------------------*/

int         texture_mode = GL_LINEAR;
int         texture_extension_number = 1;
float       gldepthmin, gldepthmax;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

// ARB Multitexture
int         gl_mtex_enum = TEXTURE0_SGIS;
qboolean    gl_arb_mtex = false;
qboolean    gl_mtexable = false;

qboolean    is8bit = false;
cvar_t     *vid_use8bit;

/*-----------------------------------------------------------------------*/

/*
	CheckMultiTextureExtensions

	Check for ARB, SGIS, or EXT multitexture support
*/

void
CheckMultiTextureExtensions (void)
{
	Con_Printf ("Checking for multitexture... ");
	if (COM_CheckParm ("-nomtex")) {
		Con_Printf ("disabled\n");
		return;
	}
#ifdef HAVE_DLOPEN
	dlhand = dlopen (NULL, RTLD_LAZY);
	if (dlhand == NULL) {
		Con_Printf ("unable to check\n");
		return;
	}
	if (strstr (gl_extensions, "GL_ARB_multitexture ")) {
		Con_Printf ("GL_ARB_multitexture\n");
		qglMTexCoord2f = (void *) dlsym (dlhand, "glMultiTexCoord2fARB");
		qglSelectTexture = (void *) dlsym (dlhand, "glActiveTextureARB");
		gl_mtex_enum = GL_TEXTURE0_ARB;
		gl_mtexable = true;
		gl_arb_mtex = true;
	} else if (strstr (gl_extensions, "GL_SGIS_multitexture ")) {
		Con_Printf ("GL_SGIS_multitexture\n");
		qglMTexCoord2f = (void *) dlsym (dlhand, "glMTexCoord2fSGIS");
		qglSelectTexture = (void *) dlsym (dlhand, "glSelectTextureSGIS");
		gl_mtex_enum = TEXTURE0_SGIS;
		gl_mtexable = true;
		gl_arb_mtex = false;
	} else if (strstr (gl_extensions, "GL_EXT_multitexture ")) {
		Con_Printf ("GL_EXT_multitexture\n");
		qglMTexCoord2f = (void *) dlsym (dlhand, "glMTexCoord2fEXT");
		qglSelectTexture = (void *) dlsym (dlhand, "glSelectTextureEXT");
		gl_mtex_enum = TEXTURE0_SGIS;
		gl_mtexable = true;
		gl_arb_mtex = false;
	} else {
		Con_Printf ("none found\n");
	}
	dlclose (dlhand);
	dlhand = NULL;
#else
	gl_mtexable = false;
#endif
}

void
VID_SetPalette (unsigned char *palette)
{
	byte       *pal;
	unsigned int r, g, b;
	unsigned int v;
	int         r1, g1, b1;
	int         k;
	unsigned short i;
	unsigned int *table;
	QFile      *f;
	char        s[255];
	float       dist, bestdist;
	static qboolean palflag = false;

//
// 8 8 8 encoding
//
//  Con_Printf("Converting 8to24\n");

	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 255; i++) {			// used to be i<256, see d_8to24table 
										// 
		// 
		// below
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

//      v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//      v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		*table++ = v;
	}
	d_8to24table[255] = 0;				// 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.
	if (palflag)
		return;
	palflag = true;

	COM_FOpenFile ("glquake/15to8.pal", &f);
	if (f) {
		Qread (f, d_15to8table, 1 << 15);
		Qclose (f);
	} else {
		for (i = 0; i < (1 << 15); i++) {
			/* Maps 000000000000000 000000000011111 = Red  = 0x1F
			   000001111100000 = Blue = 0x03E0 111110000000000 = Grn  =
			   0x7C00 */
			r = ((i & 0x1F) << 3) + 4;
			g = ((i & 0x03E0) >> 2) + 4;
			b = ((i & 0x7C00) >> 7) + 4;

			pal = (unsigned char *) d_8to24table;

			for (v = 0, k = 0, bestdist = 10000.0; v < 256; v++, pal += 4) {
				r1 = (int) r - (int) pal[0];
				g1 = (int) g - (int) pal[1];
				b1 = (int) b - (int) pal[2];
				dist = sqrt (((r1 * r1) + (g1 * g1) + (b1 * b1)));
				if (dist < bestdist) {
					k = v;
					bestdist = dist;
				}
			}
			d_15to8table[i] = k;
		}
		snprintf (s, sizeof (s), "%s/glquake/15to8.pal", com_gamedir);
		COM_CreatePath (s);
		if ((f = Qopen (s, "wb")) != NULL) {
			Qwrite (f, d_15to8table, 1 << 15);
			Qclose (f);
		}
	}
}

/*
===============
GL_Init_Common
===============
*/
void
GL_Init_Common (void)
{
	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	glClearColor (0, 0, 0, 0);
	glCullFace (GL_FRONT);
	glEnable (GL_TEXTURE_2D);

	glEnable (GL_ALPHA_TEST);
	glAlphaFunc (GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	glShadeModel (GL_FLAT);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	CheckMultiTextureExtensions ();

}

/*
=================
GL_BeginRendering
=================
*/
void
GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}

qboolean
VID_Is8bit (void)
{
	return is8bit;
}

#ifdef HAVE_TDFXGL
void        3
dfx_Init8bitPalette ()
{
// Check for 8bit Extensions and initialize them.
	int         i;

	dlhand = dlopen (NULL, RTLD_LAZY);

	Con_Printf ("8-bit GL extensions: ");

	if (dlhand == NULL) {
		Con_Printf ("unable to check.\n");
		return;
	}

	if (strstr (gl_extensions, "3DFX_set_global_palette")) {
		char       *oldpal;
		GLubyte     table[256][4];
		gl3DfxSetPaletteEXT_FUNC load_texture = NULL;

		Con_Printf ("3DFX_set_global_palette.\n");
		load_texture = (void *) dlsym (dlhand, "gl3DfxSetPaletteEXT");

		glEnable (GL_SHARED_TEXTURE_PALETTE_EXT);
		oldpal = (char *) d_8to24table;	// d_8to24table3dfx;
		for (i = 0; i < 256; i++) {
			table[i][2] = *oldpal++;
			table[i][1] = *oldpal++;
			table[i][0] = *oldpal++;
			table[i][3] = 255;
			oldpal++;
		}
		load_texture ((GLuint *) table);
		is8bit = true;
	} else
		Shared_Init8bitPalette ();

	dlclose (dlhand);
	dlhand = NULL;
	Con_Printf ("not found.\n");
}
#endif

#ifdef GL_SHARED_TEXTURE_PALETTE_EXT
void
Shared_Init8bitPalette ()
{
	int         i;
	char        thePalette[256 * 3];
	char       *oldPalette, *newPalette;

	if (strstr (gl_extensions, "GL_EXT_shared_texture_palette") == NULL)
		return;

#ifdef HAVE_TDFXGL
	glColorTableEXT_FUNC load_texture = NULL;

	load_texture = (void *) dlsym (dlhand, "glColorTableEXT");
#endif

	Con_Printf ("8-bit GL extensions enabled.\n");
	glEnable (GL_SHARED_TEXTURE_PALETTE_EXT);
	oldPalette = (char *) d_8to24table;	// d_8to24table3dfx;
	newPalette = thePalette;
	for (i = 0; i < 256; i++) {
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		*newPalette++ = *oldPalette++;
		oldPalette++;
	}
	is8bit = true;

	if strstr
		(gl_renderer, "Mesa Glide") {
#ifdef HAVE_TDFXGL
		load_texture (GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB,
					  GL_UNSIGNED_BYTE, (void *) thePalette);
#endif
	} else
		glColorTable (GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB,
					  GL_UNSIGNED_BYTE, (void *) thePalette);
}

#endif

void
VID_Init8bitPalette (void)
{
	if (COM_CheckParm ("-no8bit")) {
		Con_Printf ("disabled.\n");
		return;
	}
	vid_use8bit = Cvar_Get ("vid_use8bit", "0", CVAR_ROM, 0,
							"Whether to use Shared Palettes.");
	if (vid_use8bit->value) {
#ifdef HAVE_TDFXGL
		3           dfx_Init8bitPalette ();
#else
#ifdef GL_SHARED_TEXTURE_PALETTE_EXT
		Shared_Init8bitPalette ();
#endif
#endif
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
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
}
