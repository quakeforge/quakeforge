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
#include "r_cvar.h"
#include "sbar.h"

#define WARP_WIDTH              320
#define WARP_HEIGHT             200

unsigned char	d_15to8table[65536];

QF_glActiveTexture			qglActiveTexture = NULL;
QF_glMultiTexCoord2f		qglMultiTexCoord2f = NULL;
QF_glMultiTexCoord2fv		qglMultiTexCoord2fv = NULL;

const char		   *gl_extensions;
const char		   *gl_renderer;
const char		   *gl_vendor;
const char		   *gl_version;

int					gl_major;
int					gl_minor;
int					gl_release_number;

static int			gl_bgra_capable;
int					use_bgra;
int					gl_va_capable;
int					vaelements;
int         		texture_extension_number = 1;
int					gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int					gl_filter_max = GL_LINEAR;
float       		gldepthmin, gldepthmax;

// Multitexture
qboolean			gl_mtex_capable = false;
static int			gl_mtex_tmus = 0;
GLenum				gl_mtex_enum;
int					gl_mtex_active_tmus = 0;
qboolean			gl_mtex_fullbright = false;

// Combine
qboolean			gl_combine_capable = false;
int					lm_src_blend, lm_dest_blend;
float				rgb_scale = 1.0;

QF_glColorTableEXT	qglColorTableEXT = NULL;
qboolean			is8bit = false;

qboolean			gl_feature_mach64 = false;

// ATI PN_triangles
static qboolean		TruForm;
static int			tess_max;
int					tess;

// GL_LIGHT
int					gl_max_lights;

cvar_t		*gl_doublebright;
cvar_t      *gl_fb_bmodels;
cvar_t      *gl_max_size;
cvar_t      *gl_multitexture;
cvar_t		*gl_tessellate;
cvar_t		*gl_textures_bgra;
cvar_t      *gl_vaelements_max;
cvar_t      *gl_screenshot_byte_swap;
cvar_t      *vid_mode;
cvar_t      *vid_use8bit;

void gl_multitexture_f (cvar_t *var);


static void
gl_max_size_f (cvar_t *var)
{
	GLint		texSize;

	if (!var)
		return;

	// Check driver's max texture size
	qfglGetIntegerv (GL_MAX_TEXTURE_SIZE, &texSize);
	if (var->int_val < 1) {
		Cvar_SetValue (var, texSize);
	} else {
		Cvar_SetValue (var, bound (1, var->int_val, texSize));
	}
}

static void
gl_textures_bgra_f (cvar_t *var)
{
	if (!var)
		return;

	if (var->int_val && gl_bgra_capable) {
		use_bgra = 1;
	} else {
		use_bgra = 0;
	}
}

static void
gl_fb_bmodels_f (cvar_t *var)
{
	if (!var)
		return;

	if (var->int_val && gl_mtex_tmus >= 3) {
		gl_mtex_fullbright = true;
	} else {
		gl_mtex_fullbright = false;
	}
}

void
gl_multitexture_f (cvar_t *var)
{
	if (!var)
		return;

	if (var->int_val && gl_mtex_capable) {
		gl_mtex_active_tmus = gl_mtex_tmus;

		if (gl_fb_bmodels) {
			if (gl_fb_bmodels->int_val) {
				if (gl_mtex_tmus >= 3) {
					gl_mtex_fullbright = true;

					qglActiveTexture (gl_mtex_enum + 2);
					qfglEnable (GL_TEXTURE_2D);
					qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
								 GL_DECAL);
					qfglDisable (GL_TEXTURE_2D);
				} else {
					gl_mtex_fullbright = false;
					Con_Printf ("Not enough TMUs for BSP fullbrights.\n");	
				}
			}
		} else {
			gl_mtex_fullbright = false;
		}

		// Lightmaps
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		if (gl_overbright) {
			if (gl_combine_capable && gl_overbright->int_val) {
				qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
				qfglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
				qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, rgb_scale);
			} else {
				qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
		} else {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qfglDisable (GL_TEXTURE_2D);

		// Base Texture
		qglActiveTexture (gl_mtex_enum + 0);
	} else {
		gl_mtex_active_tmus = 0;
		gl_mtex_fullbright = false;
	}
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
		else
			tess = 0;
		qfglPNTrianglesiATI (GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, tess);
	} else {
		tess = 0;
		if (var)
			Con_Printf ("TruForm (GL_ATI_pn_triangles) is not supported by "
						"your hardware and/or drivers.\n");
	}
}

static void
GL_Common_Init_Cvars (void)
{
	vid_use8bit = Cvar_Get ("vid_use8bit", "0", CVAR_ROM, NULL,	"Use 8-bit "
							"shared palettes.");
	gl_textures_bgra = Cvar_Get ("gl_textures_bgra", "0", CVAR_ROM,
								 gl_textures_bgra_f, "If set to 1, try to use "
								 "BGR & BGRA textures instead of RGB & RGBA.");
	gl_fb_bmodels = Cvar_Get ("gl_fb_bmodels", "1", CVAR_ARCHIVE,
							  gl_fb_bmodels_f, "Toggles fullbright color "
							  "support for bmodels");
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

static void
CheckGLVersionString (void)
{
	gl_version = qfglGetString (GL_VERSION);
	if (sscanf (gl_version, "%d.%d", &gl_major, &gl_minor) == 2) {
		gl_release_number = 0;
		if (gl_major >= 1) {
			if (gl_minor >= 1) {
				gl_va_capable = true;
			} else {
				gl_va_capable = false;
			}
		}
	} else if (sscanf (gl_version, "%d.%d.%d", &gl_major, &gl_minor,
					   &gl_release_number) == 3) {
		if (gl_major >= 1) {
			if (gl_minor >= 1) {
				gl_va_capable = true;
			} else {
				gl_va_capable = false;
			}
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
}

static void
CheckBGRAExtensions (void)
{
	if (gl_major >= 1 && gl_minor >= 3) {
		gl_bgra_capable = true;
	} else if (QFGL_ExtensionPresent ("GL_EXT_bgra")) {
		gl_bgra_capable = true;
	} else {
		gl_bgra_capable = false;
	}
}

static void
CheckCombineExtensions (void)
{
	if (gl_major >= 1 && gl_minor >= 3) {
		gl_combine_capable = true;
		Con_Printf ("COMBINE active, multitextured doublebright enabled.\n");
	} else if (QFGL_ExtensionPresent ("GL_ARB_texture_env_combine")) {
		gl_combine_capable = true;
		Con_Printf ("COMBINE_ARB active, multitextured doublebright "
					"enabled.\n");
	} else {
		gl_combine_capable = false;
		Con_Printf ("GL_ARB_texture_env_combine not found. gl_doublebright "
					"will have no effect with gl_multitexture on.\n");
	}
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
	if (gl_major >= 1 && gl_minor >= 3) {
		qfglGetIntegerv (GL_MAX_TEXTURE_UNITS, &gl_mtex_tmus);
		if (gl_mtex_tmus >= 2) {
			Con_Printf ("enabled, %d TMUs.\n", gl_mtex_tmus);
			qglMultiTexCoord2f =
				QFGL_ExtensionAddress ("glMultiTexCoord2f");
			qglMultiTexCoord2fv =
				QFGL_ExtensionAddress ("glMultiTexCoord2fv");
			qglActiveTexture = QFGL_ExtensionAddress ("glActiveTexture");
			gl_mtex_enum = GL_TEXTURE0;
			if (qglMultiTexCoord2f && gl_mtex_enum)
				gl_mtex_capable = true;
			else
				Con_Printf ("Multitexture disabled, could not find required "
							"functions\n");
		} else {
			Con_Printf ("Multitexture disabled, not enough TMUs.\n");
		}
	} else if (QFGL_ExtensionPresent ("GL_ARB_multitexture")) {
		qfglGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &gl_mtex_tmus);
		if (gl_mtex_tmus >= 2) {
			Con_Printf ("enabled, %d TMUs.\n", gl_mtex_tmus);
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

static void
CheckLights (void)
{
	int		i;
	float	dark[4] = {0.0, 0.0, 0.0, 1.0},
//			light[4] = {1.0, 1.0, 1.0, 1.0},
			specular[4] = {0.6, 0.6, 0.6, 1.0};

	qfglGetIntegerv (GL_MAX_LIGHTS, &gl_max_lights);
	Con_Printf ("Max GL Lights %lu.\n", (long unsigned) gl_max_lights);

	qfglEnable (GL_LIGHTING);
	qfglLightModelfv (GL_LIGHT_MODEL_AMBIENT, dark);
	qfglLightModelf (GL_LIGHT_MODEL_TWO_SIDE, 0.0);
	// Set up material defaults
	qfglMaterialf (GL_FRONT, GL_SHININESS, 1.0);
	qfglMaterialfv (GL_FRONT, GL_SPECULAR, specular);
//	qfglMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, light);

	for (i = 0; i < gl_max_lights; i++) {
		qfglEnable (GL_LIGHT0 + i);
		qfglLightf (GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.0);
		qfglDisable (GL_LIGHT0 + i);
	}

	qfglDisable (GL_LIGHTING);
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
	CheckGLVersionString ();

	qfglClearColor (0, 0, 0, 0);

	qfglEnable (GL_TEXTURE_2D);
	qfglCullFace (GL_FRONT);
	qfglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qfglShadeModel (GL_FLAT);

	qfglEnable (GL_BLEND);
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	CheckMultiTextureExtensions ();
	CheckCombineExtensions ();
	CheckBGRAExtensions ();
	CheckTruFormExtensions ();
	GL_Common_Init_Cvars ();
	CheckVertexArraySize ();
	CheckLights ();
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
