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

#ifdef HAVE_MATH_H
# include <math.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/extensions.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"
#include "sbar.h"

#define WARP_WIDTH              320
#define WARP_HEIGHT             200

unsigned char	gl_15to8table[65536];

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
int					gl_use_bgra;
int					gl_va_capable;
static  int					driver_vaelements;
int					vaelements;
int					gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
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
float				gl_rgb_scale = 1.0;

QF_glColorTableEXT	qglColorTableEXT = NULL;

qboolean			gl_feature_mach64 = false;

// GL_EXT_texture_filter_anisotropic
qboolean 			gl_Anisotropy;
static float		aniso_max;
float				gl_aniso;

// GL_ATI_pn_triangles
static qboolean		TruForm;
static int			tess_max;
int			gl_tess;

// GL_LIGHT
int					gl_max_lights;

cvar_t		*gl_anisotropy;
cvar_t      *gl_fb_bmodels;
cvar_t      *gl_finish;
cvar_t      *gl_max_size;
cvar_t      *gl_multitexture;
cvar_t		*gl_tessellate;
cvar_t		*gl_textures_bgra;
cvar_t      *gl_vaelements_max;
cvar_t      *gl_vector_light;
cvar_t      *gl_screenshot_byte_swap;

cvar_t     *gl_affinemodels;
cvar_t     *gl_clear;
cvar_t     *gl_conspin;
cvar_t     *gl_constretch;
cvar_t     *gl_dlight_polyblend;
cvar_t     *gl_dlight_smooth;
cvar_t     *gl_driver;
cvar_t     *gl_fb_models;
cvar_t     *gl_keeptjunctions;
cvar_t     *gl_lerp_anim;
cvar_t     *gl_lightmap_align;
cvar_t     *gl_lightmap_subimage;
cvar_t     *gl_nocolors;
cvar_t	   *gl_overbright;
cvar_t     *gl_particle_mip;
cvar_t     *gl_particle_size;
cvar_t     *gl_picmip;
cvar_t     *gl_playermip;
cvar_t     *gl_reporttjunctions;
cvar_t     *gl_sky_clip;
cvar_t     *gl_sky_debug;
cvar_t     *gl_sky_multipass;
cvar_t     *gl_texsort;
cvar_t     *gl_triplebuffer;
static cvar_t *vid_use8bit;

static void
gl_triplebuffer_f (cvar_t *var)
{
	vid.numpages = var->int_val ? 3 : 2;
}

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
		gl_use_bgra = 1;
	} else {
		gl_use_bgra = 0;
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
					Sys_MaskPrintf (SYS_vid,
									"Not enough TMUs for BSP fullbrights.\n");
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
				qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, gl_rgb_scale);
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
gl_anisotropy_f (cvar_t * var)
{
	if (gl_Anisotropy) {
		if (var)
			gl_aniso = (bound (1.0, var->value, aniso_max));
		else
			gl_aniso = 1.0;
	} else {
		gl_aniso = 1.0;
		if (var)
			Sys_MaskPrintf (SYS_vid,
							"Anisotropy (GL_EXT_texture_filter_anisotropic) "
							"is not supported by your hardware and/or "
							"drivers.\n");
	}
}

static void
gl_tessellate_f (cvar_t * var)
{
	if (TruForm) {
		if (var)
			gl_tess = (bound (0, var->int_val, tess_max));
		else
			gl_tess = 0;
		qfglPNTrianglesiATI (GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI, gl_tess);
	} else {
		gl_tess = 0;
		if (var)
			Sys_MaskPrintf (SYS_vid,
							"TruForm (GL_ATI_pn_triangles) is not supported "
							"by your hardware and/or drivers.\n");
	}
}

static void
gl_vaelements_max_f (cvar_t *var)
{
	if (var->int_val)
		vaelements = min (var->int_val, driver_vaelements);
	else
		vaelements = driver_vaelements;
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
	gl_finish = Cvar_Get ("gl_finish", "1", CVAR_ARCHIVE, NULL,
							"wait for rendering to finish");
	gl_max_size = Cvar_Get ("gl_max_size", "0", CVAR_NONE, gl_max_size_f,
							"Texture dimension");
	gl_multitexture = Cvar_Get ("gl_multitexture", "1", CVAR_ARCHIVE,
								gl_multitexture_f, "Use multitexture when "
								"available.");
	gl_screenshot_byte_swap =
		Cvar_Get ("gl_screenshot_byte_swap", "0", CVAR_NONE,
				  gl_screenshot_byte_swap_f, "Swap the bytes for gl "
				  "screenshots. Needed if you get screenshots with red and "
				  "blue swapped.");
	gl_anisotropy =
		Cvar_Get ("gl_anisotropy", "1.0", CVAR_NONE, gl_anisotropy_f,
				  nva ("Specifies degree of anisotropy, from 1.0 to %f. "
					   "Higher anisotropy means less distortion of textures "
					   "at shallow angles to the viewer.", aniso_max));
	gl_tessellate =
		Cvar_Get ("gl_tessellate", "0", CVAR_NONE, gl_tessellate_f,
				  nva ("Specifies tessellation level from 0 to %i. Higher "
					   "tessellation level means more triangles.", tess_max));
	gl_vaelements_max = Cvar_Get ("gl_vaelements_max", "0", CVAR_ROM,
								  gl_vaelements_max_f,
								  "Limit the vertex array size for buggy "
								  "drivers. 0 (default) uses driver provided "
								  "limit, -1 disables use of vertex arrays.");
	gl_vector_light = Cvar_Get ("gl_vector_light", "1", CVAR_NONE, NULL,
						"Enable use of GL vector lighting. 0 = flat lighting.");
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_ARCHIVE, NULL,
								"Makes texture rendering quality better if "
								"set to 1");
	gl_clear = Cvar_Get ("gl_clear", "0", CVAR_NONE, NULL, "Set to 1 to make "
						 "background black. Useful for removing HOM effect");
	gl_conspin = Cvar_Get ("gl_conspin", "0", CVAR_ARCHIVE, NULL,
						   "speed at which the console spins");
	gl_constretch = Cvar_Get ("gl_constretch", "0", CVAR_ARCHIVE, NULL,
							  "toggle console between slide and stretch");
	gl_dlight_polyblend = Cvar_Get ("gl_dlight_polyblend", "0", CVAR_ARCHIVE,
									NULL, "Set to 1 to use a dynamic light "
									"effect faster on GL");
	gl_dlight_smooth = Cvar_Get ("gl_dlight_smooth", "1", CVAR_ARCHIVE, NULL,
								 "Smooth dynamic vertex lighting");
	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_ARCHIVE, NULL,
							 "Toggles fullbright color support for models");
	gl_keeptjunctions = Cvar_Get ("gl_keeptjunctions", "1", CVAR_ARCHIVE, NULL,
								  "Set to 0 to turn off colinear vertexes "
								  "upon level load.");
	gl_lerp_anim = Cvar_Get ("gl_lerp_anim", "1", CVAR_ARCHIVE, NULL,
							 "Toggles model animation interpolation");

	gl_lightmap_align = Cvar_Get ("gl_lightmap_align", "1", CVAR_NONE, NULL,
								  "Workaround for nvidia slow path. Set to 4 "
								  "or 16 if you have an nvidia 3d "
								  "accelerator, set to 1 otherwise.");
	gl_lightmap_subimage = Cvar_Get ("gl_lightmap_subimage", "1", CVAR_NONE,
									 NULL, "Lightmap Update method. Default 2 "
									 "updates a minimum 'dirty rectangle' "
									 "around the area changed. 1 updates "
									 "every line that changed. 0 updates the "
									 "entire lightmap.");
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL,
							"Set to 1, turns off all player colors");
	gl_overbright = Cvar_Get ("gl_overbright", "0", CVAR_NONE,
							  gl_overbright_f, "Darken lightmaps so that "
							  "dynamic lights can be overbright. 1 = 0.75 "
							  "brightness, 2 = 0.5 brightness.");
	gl_particle_mip = Cvar_Get ("gl_particle_mip", "0", CVAR_NONE, NULL,
								"Toggles particle texture mipmapping.");
	gl_particle_size = Cvar_Get ("gl_particle_size", "5", CVAR_NONE, NULL,
								 "Vertical and horizontal size of particle "
								 "textures as a power of 2. Default is 5 "
								 "(32 texel square).");
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, NULL, "Dimensions of "
						  "textures. 0 is normal, 1 is half, 2 is 1/4");
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL,
							 "Detail of player skins. 0 best, 4 worst.");
	gl_reporttjunctions = Cvar_Get ("gl_reporttjunctions", "0", CVAR_NONE,
									NULL, "None");
	gl_sky_clip = Cvar_Get ("gl_sky_clip", "2", CVAR_ARCHIVE, NULL,
							"controls amount of sky overdraw");
	gl_sky_debug = Cvar_Get ("gl_sky_debug", "0", CVAR_NONE, NULL,
							 "debugging `info' for sky clipping");
	gl_sky_divide = Cvar_Get ("gl_sky_divide", "1", CVAR_ARCHIVE, NULL,
							  "subdivide sky polys");
	gl_sky_multipass = Cvar_Get ("gl_sky_multipass", "1", CVAR_ARCHIVE, NULL,
								"controls whether the skydome is single or "
								"double pass");
	gl_texsort = Cvar_Get ("gl_texsort", "1", CVAR_NONE, NULL, "None");
	gl_triplebuffer = Cvar_Get ("gl_triplebuffer", "1", CVAR_ARCHIVE,
								gl_triplebuffer_f,
								"Set to 1 by default. Fixes status bar "
								"flicker on some hardware");
}

static void
CheckGLVersionString (void)
{
	gl_version = (char *) qfglGetString (GL_VERSION);
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
	Sys_MaskPrintf (SYS_vid, "GL_VERSION: %s\n", gl_version);

	gl_vendor = (char *) qfglGetString (GL_VENDOR);
	Sys_MaskPrintf (SYS_vid, "GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (char *) qfglGetString (GL_RENDERER);
	Sys_MaskPrintf (SYS_vid, "GL_RENDERER: %s\n", gl_renderer);
	gl_extensions = (char *) qfglGetString (GL_EXTENSIONS);
	Sys_MaskPrintf (SYS_vid, "GL_EXTENSIONS: %s\n", gl_extensions);

	if (strstr (gl_renderer, "Mesa DRI Mach64"))
		gl_feature_mach64 = true;
}

static void
CheckAnisotropyExtensions (void)
{
	if (QFGL_ExtensionPresent ("GL_EXT_texture_filter_anisotropic")) {
		gl_Anisotropy = true;
		qfglGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso_max);
	} else {
		gl_Anisotropy = false;
		aniso_max = 1.0;
	}
}

static void
CheckBGRAExtensions (void)
{
	if (gl_major > 1 || (gl_major == 1 && gl_minor >= 3)) {
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
		Sys_MaskPrintf (SYS_vid, "COMBINE active, multitextured doublebright "
						"enabled.\n");
	} else if (QFGL_ExtensionPresent ("GL_ARB_texture_env_combine")) {
		gl_combine_capable = true;
		Sys_MaskPrintf (SYS_vid, "COMBINE_ARB active, multitextured "
						"doublebright enabled.\n");
	} else {
		gl_combine_capable = false;
	}
}

/*
	CheckMultiTextureExtensions

	Check for ARB multitexture support
*/
static void
CheckMultiTextureExtensions (void)
{
	Sys_MaskPrintf (SYS_vid, "Checking for multitexture: ");
	if (COM_CheckParm ("-nomtex")) {
		Sys_MaskPrintf (SYS_vid, "disabled.\n");
		return;
	}
	if (gl_major >= 1 && gl_minor >= 3) {
		qfglGetIntegerv (GL_MAX_TEXTURE_UNITS, &gl_mtex_tmus);
		if (gl_mtex_tmus >= 2) {
			Sys_MaskPrintf (SYS_vid, "enabled, %d TMUs.\n", gl_mtex_tmus);
			qglMultiTexCoord2f =
				QFGL_ExtensionAddress ("glMultiTexCoord2f");
			qglMultiTexCoord2fv =
				QFGL_ExtensionAddress ("glMultiTexCoord2fv");
			qglActiveTexture = QFGL_ExtensionAddress ("glActiveTexture");
			gl_mtex_enum = GL_TEXTURE0;
			if (qglMultiTexCoord2f && gl_mtex_enum)
				gl_mtex_capable = true;
			else
				Sys_MaskPrintf (SYS_vid, "Multitexture disabled, could not "
								"find required functions\n");
		} else {
			Sys_MaskPrintf (SYS_vid,
							"Multitexture disabled, not enough TMUs.\n");
		}
	} else if (QFGL_ExtensionPresent ("GL_ARB_multitexture")) {
		qfglGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &gl_mtex_tmus);
		if (gl_mtex_tmus >= 2) {
			Sys_MaskPrintf (SYS_vid, "enabled, %d TMUs.\n", gl_mtex_tmus);
			qglMultiTexCoord2f =
				QFGL_ExtensionAddress ("glMultiTexCoord2fARB");
			qglMultiTexCoord2fv =
				QFGL_ExtensionAddress ("glMultiTexCoord2fvARB");
			qglActiveTexture = QFGL_ExtensionAddress ("glActiveTextureARB");
			gl_mtex_enum = GL_TEXTURE0_ARB;
			if (qglMultiTexCoord2f && gl_mtex_enum)
				gl_mtex_capable = true;
			else
				Sys_MaskPrintf (SYS_vid, "Multitexture disabled, could not "
								"find required functions\n");
		} else {
			Sys_MaskPrintf (SYS_vid,
							"Multitexture disabled, not enough TMUs.\n");
		}
	} else {
		Sys_MaskPrintf (SYS_vid, "not found.\n");
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
		gl_tess = 0;
		tess_max = 0;
	}
}

static void
CheckVertexArraySize (void)
{
	qfglGetIntegerv (GL_MAX_ELEMENTS_VERTICES, &driver_vaelements);
	if (driver_vaelements > 65536)
		driver_vaelements = 65536;
	vaelements = driver_vaelements;
//	qfglGetIntegerv (MAX_ELEMENTS_INDICES, *vaindices);
}

static void
CheckLights (void)
{
	int		i;
	float	dark[4] = {0.0, 0.0, 0.0, 1.0},
			ambient[4] = {0.2, 0.2, 0.2, 1.0},
			diffuse[4] = {0.7, 0.7, 0.7, 1.0},
			specular[4] = {0.1, 0.1, 0.1, 1.0};

	qfglGetIntegerv (GL_MAX_LIGHTS, &gl_max_lights);
	Sys_MaskPrintf (SYS_vid, "Max GL Lights %d.\n", gl_max_lights);

	qfglEnable (GL_LIGHTING);
	qfglLightModelfv (GL_LIGHT_MODEL_AMBIENT, dark);
	qfglLightModelf (GL_LIGHT_MODEL_TWO_SIDE, 0.0);

	for (i = 0; i < gl_max_lights; i++) {
		qfglEnable (GL_LIGHT0 + i);
		qfglLightf (GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, 0.5);
		qfglDisable (GL_LIGHT0 + i);
	}

	// Set up material defaults
	qfglMaterialfv (GL_FRONT, GL_AMBIENT, ambient);
	qfglMaterialfv (GL_FRONT, GL_DIFFUSE, diffuse);
	qfglMaterialfv (GL_FRONT, GL_SPECULAR, specular);
	qfglMaterialf (GL_FRONT, GL_SHININESS, 1.0);

	qfglDisable (GL_LIGHTING);
}

static void
Tdfx_Init8bitPalette (void)
{
	// Check for 8bit Extensions and initialize them.
	int         i;

	if (vr_data.vid->is8bit)
		return;

	if (QFGL_ExtensionPresent ("3DFX_set_global_palette")) {
		char       *oldpal;
		GLubyte     table[256][4];
		QF_gl3DfxSetPaletteEXT qgl3DfxSetPaletteEXT = NULL;

		if (!(qgl3DfxSetPaletteEXT =
			  QFGL_ExtensionAddress ("gl3DfxSetPaletteEXT"))) {
			Sys_MaskPrintf (SYS_vid, "3DFX_set_global_palette not found.\n");
			return;
		}

		Sys_MaskPrintf (SYS_vid, "3DFX_set_global_palette.\n");

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
		vr_data.vid->is8bit = true;
	} else {
		Sys_MaskPrintf (SYS_vid, "\n    3DFX_set_global_palette not found.");
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

	if (vr_data.vid->is8bit)
		return;

	if (QFGL_ExtensionPresent ("GL_EXT_shared_texture_palette")) {
		if (!(qglColorTableEXT = QFGL_ExtensionAddress ("glColorTableEXT"))) {
			Sys_MaskPrintf (SYS_vid, "glColorTableEXT not found.\n");
			return;
		}

		Sys_MaskPrintf (SYS_vid, "GL_EXT_shared_texture_palette\n");

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
		vr_data.vid->is8bit = true;
	} else {
		Sys_MaskPrintf (SYS_vid,
						"\n    GL_EXT_shared_texture_palette not found.");
	}
}

static void
VID_Init8bitPalette (void)
{
	Sys_MaskPrintf (SYS_vid, "Checking for 8-bit extension: ");
	if (vid_use8bit->int_val) {
		Tdfx_Init8bitPalette ();
		Shared_Init8bitPalette ();
		if (!vr_data.vid->is8bit)
			Sys_MaskPrintf (SYS_vid, "\n  8-bit extension not found.\n");
	} else {
		Sys_MaskPrintf (SYS_vid, "disabled.\n");
	}
}

void
GL_SetPalette (void *data, const byte *palette)
{
	const byte *pal;
	char        s[255];
	float       dist, bestdist;
	int			r1, g1, b1, k;
	unsigned int r, g, b, v;
	unsigned short i;
	unsigned int *table;
	static qboolean palflag = false;
	QFile      *f;
	static int  inited_8 = 0;

	if (!inited_8) {
		inited_8 = 1;
		// Check for 8-bit extension and initialize if present
		VID_Init8bitPalette ();
	}
	// 8 8 8 encoding
	Sys_MaskPrintf (SYS_vid, "Converting 8to24\n");

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

	f = QFS_FOpenFile ("glquake/15to8.pal");
	if (f) {
		Qread (f, gl_15to8table, 1 << 15);
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
			gl_15to8table[i] = k;
		}
		snprintf (s, sizeof (s), "%s/glquake/15to8.pal", qfs_gamedir->dir.def);
		if ((f = QFS_WOpen (s, 0)) != NULL) {
			Qwrite (f, gl_15to8table, 1 << 15);
			Qclose (f);
		}
	}
}

void
GL_Init_Common (void)
{
	GLF_FindFunctions ();
	CheckGLVersionString ();

	CheckAnisotropyExtensions ();
	CheckMultiTextureExtensions ();
	CheckCombineExtensions ();
	CheckBGRAExtensions ();
	CheckTruFormExtensions ();
	CheckVertexArraySize ();
	CheckLights ();

	GL_Common_Init_Cvars ();

	qfglClearColor (0, 0, 0, 0);

	qfglEnable (GL_TEXTURE_2D);
	qfglFrontFace (GL_CW);
	qfglCullFace (GL_BACK);
	qfglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qfglShadeModel (GL_FLAT);

	qfglEnable (GL_BLEND);
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	if (gl_Anisotropy)
		qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						   gl_aniso);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qfglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}
