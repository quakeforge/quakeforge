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
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/extensions.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "d_iface.h"
#include "sbar.h"

#define WARP_WIDTH              320
#define WARP_HEIGHT             200

unsigned char	d_15to8table[65536];

QF_glActiveTextureARB		qglActiveTexture = NULL;
QF_glMultiTexCoord2fARB		qglMultiTexCoord2f = NULL;
QF_glMultiTexCoord2fvARB	qglMultiTexCoord2fv = NULL;

const char		   *gl_extensions;
const char		   *gl_renderer;
const char		   *gl_vendor;
const char		   *gl_version;

int					gl_major;
int					gl_minor;
int					gl_release_number;

int					gl_va_capable;
int					vaelements;
int         		texture_extension_number = 1;
int					gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int					gl_filter_max = GL_LINEAR;
float       		gldepthmin, gldepthmax;

// ARB Multitexture
qboolean			gl_mtex_active = false;
qboolean    		gl_mtex_capable = false;
GLenum				gl_mtex_enum = GL_TEXTURE0_ARB;

QF_glColorTableEXT	qglColorTableEXT = NULL;
qboolean			is8bit = false;

qboolean			gl_feature_mach64 = false;

// ATI PN_triangles
qboolean			TruForm;
GLint				tess, tess_max;

cvar_t      *gl_max_size;
cvar_t      *gl_multitexture;
cvar_t		*gl_tessellate;
cvar_t      *gl_vaelements_max;
cvar_t      *gl_screenshot_byte_swap;
cvar_t      *vid_mode;
cvar_t      *vid_use8bit;


static void
gl_max_size_f (cvar_t *var)
{
	GLint		texSize;

	// Check driver's max texture size
	qfglGetIntegerv (GL_MAX_TEXTURE_SIZE, &texSize);
	if (var->int_val < 1)
		Cvar_SetValue (var, texSize);
	else
		Cvar_SetValue (var, bound (1, var->int_val, texSize));	
}

static void
gl_multitexture_f (cvar_t *var)
{
	if (var)
		gl_mtex_active = gl_mtex_capable && var->int_val;
}

static void
gl_screenshot_byte_swap_f (cvar_t *var)
{
	if (var)
		qfglPixelStorei (GL_PACK_SWAP_BYTES,
						 var->int_val ? GL_TRUE : GL_FALSE);
}

static void
gl_tessellate_f (cvar_t * var)
{
	if (TruForm) {
		if (var)
			tess = (bound (0, var->int_val, tess_max));
		qfglPNTrianglesiATI (GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, tess);
	} else {
		tess = 0;
		if (var)
			Con_Printf ("TruForm (GL_ATI_pn_triangles) is not supported by "
						"your hardware and/or drivers.");
	}
}

static void
GL_Common_Init_Cvars (void)
{
	vid_use8bit = Cvar_Get ("vid_use8bit", "0", CVAR_ROM, NULL,	"Use 8-bit "
							"shared palettes.");
	gl_max_size = Cvar_Get ("gl_max_size", "0", CVAR_NONE, gl_max_size_f,
							"Texture dimension");
	gl_multitexture = Cvar_Get ("gl_multitexture", "0", CVAR_ARCHIVE,
								gl_multitexture_f, "Use multitexture when "
								"available.");
	gl_screenshot_byte_swap =
		Cvar_Get ("gl_screenshot_byte_swap", "0", CVAR_NONE,
				  gl_screenshot_byte_swap_f, "Swap the bytes for gl "
				  "screenshots. Needed if you get screenshots with red and "
				  "blue swapped.");
	gl_tessellate =
		Cvar_Get ("gl_tessellate", "0", CVAR_NONE, gl_tessellate_f,
				  nva ("Specifies tessellation level from 0 to %i. Higher "
					   "tessellation level means more triangles.", tess_max));
	gl_vaelements_max = Cvar_Get ("gl_vaelements_max", "0", CVAR_ROM, NULL,
								  "Limit the vertex array size for buggy "
								  "drivers. 0 (default) uses driver provided "
								  "limit, -1 disables use of vertex arrays.");
}

/*
	CheckMultiTextureExtensions

	Check for ARB multitexture support
*/
static void
CheckMultiTextureExtensions (void)
{
	Con_Printf ("Checking for multitexture: ");
	if (COM_CheckParm ("-nomtex")) {
		Con_Printf ("disabled.\n");
		return;
	}

	if (QFGL_ExtensionPresent ("GL_ARB_multitexture")) {
		int max_texture_units = 0;

		qfglGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &max_texture_units);
		if (max_texture_units >= 2) {
			Con_Printf ("enabled, %d TMUs.\n", max_texture_units);
			qglMultiTexCoord2f =
				QFGL_ExtensionAddress ("glMultiTexCoord2fARB");
			qglMultiTexCoord2fv =
				QFGL_ExtensionAddress ("glMultiTexCoord2fvARB");
			qglActiveTexture = QFGL_ExtensionAddress ("glActiveTextureARB");
			gl_mtex_enum = GL_TEXTURE0_ARB;
			if (qglMultiTexCoord2f && gl_mtex_enum)
				gl_mtex_capable = true;
			else
				Con_Printf ("Multitexture disabled, could not find required "
							"functions\n");
		} else {
			Con_Printf ("Multitexture disabled, not enough TMUs.\n");
		}
	} else {
		Con_Printf ("not found.\n");
	}
}

static void
CheckTruFormExtensions (void)
{
	if (QFGL_ExtensionPresent ("GL_ATI_pn_triangles")) {
		TruForm = true;
		qfglGetIntegerv (GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI,
						 &tess_max);
		qfglPNTrianglesiATI (GL_PN_TRIANGLES_NORMAL_MODE_ATI,
							 GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI);
		qfglPNTrianglesiATI (GL_PN_TRIANGLES_POINT_MODE_ATI,
							 GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI);
	} else {
		TruForm = false;
		tess = 0;
		tess_max = 0;
	}
}

static void
CheckVertexArraySize (void)
{
	qfglGetIntegerv (GL_MAX_ELEMENTS_VERTICES, &vaelements);
	if (vaelements > 65536)
		vaelements = 65536;
	if (gl_vaelements_max->int_val)
		vaelements = min (gl_vaelements_max->int_val, vaelements);
//	qfglGetIntegerv (MAX_ELEMENTS_INDICES, *vaindices);
}

void
VID_SetPalette (unsigned char *palette)
{
	byte       *pal;
	char        s[255];
	float       dist, bestdist;
	int			r1, g1, b1, k;
	unsigned int r, g, b, v;
	unsigned short i;
	unsigned int *table;
	static qboolean palflag = false;
	QFile      *f;

	// 8 8 8 encoding
//	Con_Printf ("Converting 8to24\n");

	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 255; i++) { // used to be i<256, see d_8to24table below
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

#ifdef WORDS_BIGENDIAN
		v = (255 << 0) + (r << 24) + (g << 16) + (b << 8);
#else
		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
#endif
		*table++ = v;
	}
	d_8to24table[255] = 0;	// 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	if (palflag)
		return;
	palflag = true;

	QFS_FOpenFile ("glquake/15to8.pal", &f);
	if (f) {
		Qread (f, d_15to8table, 1 << 15);
		Qclose (f);
	} else {
		for (i = 0; i < (1 << 15); i++) {
			/* Maps
			   000000000000000
			   000000000011111 = Red  = 0x001F
			   000001111100000 = Blue = 0x03E0 
			   111110000000000 = Grn  = 0x7C00
			*/
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
		snprintf (s, sizeof (s), "%s/glquake/15to8.pal", qfs_gamedir->dir.def);
		if ((f = QFS_WOpen (s, 0)) != NULL) {
			Qwrite (f, d_15to8table, 1 << 15);
			Qclose (f);
		}
	}
}

void
GL_Init_Common (void)
{
	GLF_FindFunctions ();
	gl_version = qfglGetString (GL_VERSION);
	if (sscanf (gl_version, "%d.%d", &gl_major, &gl_minor) == 2) {
		gl_release_number = 0;
		if (gl_major >= 1) {
			if (gl_minor >= 1) {
				gl_va_capable = true;
			} else
				gl_va_capable = false;
		}
	} else if (sscanf (gl_version, "%d.%d.%d", &gl_major, &gl_minor,
					   &gl_release_number) == 3) {
		if (gl_major >= 1) {
			if (gl_minor >= 1) {
				gl_va_capable = true;
			} else
				gl_va_capable = false;
		}
	} else {
		Sys_Error ("Malformed OpenGL version string!");
	}
	Con_Printf ("GL_VERSION: %s\n", gl_version);

	gl_vendor = qfglGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = qfglGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);
	gl_extensions = qfglGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	if (strstr (gl_renderer, "Mesa DRI Mach64"))
		gl_feature_mach64 = true;

	qfglClearColor (0, 0, 0, 0);
	qfglCullFace (GL_FRONT);
	qfglEnable (GL_TEXTURE_2D);

	qfglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	qfglShadeModel (GL_FLAT);

	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qfglEnable (GL_BLEND);
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	CheckMultiTextureExtensions ();
	CheckTruFormExtensions ();
	GL_Common_Init_Cvars ();
	CheckVertexArraySize ();
}

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

static void
Tdfx_Init8bitPalette (void)
{
	// Check for 8bit Extensions and initialize them.
	int         i;

	if (is8bit)
		return;

	if (QFGL_ExtensionPresent ("3DFX_set_global_palette")) {
		char       *oldpal;
		GLubyte     table[256][4];
		QF_gl3DfxSetPaletteEXT qgl3DfxSetPaletteEXT = NULL;

		if (!(qgl3DfxSetPaletteEXT =
			  QFGL_ExtensionAddress ("gl3DfxSetPaletteEXT"))) {
			Con_Printf ("3DFX_set_global_palette not found.\n");
			return;
		}

		Con_Printf ("3DFX_set_global_palette.\n");

		oldpal = (char *) d_8to24table;	// d_8to24table3dfx;
		for (i = 0; i < 256; i++) {
			table[i][2] = *oldpal++;
			table[i][1] = *oldpal++;
			table[i][0] = *oldpal++;
			table[i][3] = 255;
			oldpal++;
		}
		qfglEnable (GL_SHARED_TEXTURE_PALETTE_EXT);
		qgl3DfxSetPaletteEXT ((GLuint *) table);
		is8bit = true;
	} else {
		Con_Printf ("\n    3DFX_set_global_palette not found.");
	}
}

/*
  The GL_EXT_shared_texture_palette seems like an idea which is 
  /almost/ a good idea, but seems to be severely broken with many
  drivers, as such it is disabled.
  
  It should be noted, that a palette object extension as suggested by
  the GL_EXT_shared_texture_palette spec might be a very good idea in
  general.
*/
static void
Shared_Init8bitPalette (void)
{
	int 		i;
	GLubyte 	thePalette[256 * 3];
	GLubyte 	*oldPalette, *newPalette;

	if (is8bit)
		return;

	if (QFGL_ExtensionPresent ("GL_EXT_shared_texture_palette")) {
		if (!(qglColorTableEXT = QFGL_ExtensionAddress ("glColorTableEXT"))) {
			Con_Printf ("glColorTableEXT not found.\n");
			return;
		}

		Con_Printf ("GL_EXT_shared_texture_palette\n");

		qfglEnable (GL_SHARED_TEXTURE_PALETTE_EXT);
		oldPalette = (GLubyte *) d_8to24table;	// d_8to24table3dfx;
		newPalette = thePalette;
		for (i = 0; i < 256; i++) {
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			oldPalette++;
		}
		qglColorTableEXT (GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB,
							GL_UNSIGNED_BYTE, (GLvoid *) thePalette);
		is8bit = true;
	} else {
		Con_Printf ("\n    GL_EXT_shared_texture_palette not found.");
	}
}

void
VID_Init8bitPalette (void)
{
	Con_Printf ("Checking for 8-bit extension: ");
	if (vid_use8bit->int_val) {
		Tdfx_Init8bitPalette ();
		Shared_Init8bitPalette ();
		if (!is8bit)
			Con_Printf ("\n  8-bit extension not found.\n");
	} else {
		Con_Printf ("disabled.\n");
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
D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
}
