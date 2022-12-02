/*
	qfgl_ext.h

	QuakeForge OpenGL extension interface definitions

	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>

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

#ifndef __qfgl_ext_h_
#define __qfgl_ext_h_

#include "QF/qtypes.h"
#include "QF/GL/defines.h"
#include "QF/GL/types.h"

// Define GLAPIENTRY to a useful value
#ifndef GLAPIENTRY
# ifdef _WIN32
#  include <windows.h>
#  define GLAPIENTRY WINAPI
#  undef LoadImage
#  undef E_POINTER
# else
#  ifdef APIENTRY
#   define GLAPIENTRY APIENTRY
#  else
#   define GLAPIENTRY
#  endif
# endif
#endif

#include "QF/GL/ati.h"		// Uses defines, types, APIENTRY

// OpenGL numbers for extensions we use or want to use
#ifndef GL_EXT_abgr
# define GL_EXT_abgr
# define GL_ABGR_EXT 						0x8000
#endif

#ifndef GL_EXT_bgra
# define GL_EXT_bgra
# define GL_BGR_EXT 						0x80E0
# define GL_BGRA_EXT						0x80E1
#endif

#ifndef GL_EXT_paletted_texture
# define GL_EXT_paletted_texture
# define GL_COLOR_INDEX1_EXT 				0x80E2
# define GL_COLOR_INDEX2_EXT 				0x80E3
# define GL_COLOR_INDEX4_EXT 				0x80E4
# define GL_COLOR_INDEX8_EXT 				0x80E5
# define GL_COLOR_INDEX12_EXT				0x80E6
# define GL_COLOR_INDEX16_EXT				0x80E7
# define GL_TEXTURE_INDEX_SIZE_EXT			0x80ED
#endif

#ifndef GL_EXT_texture_filter_anisotropic
# define GL_EXT_texture_filter_anisotropic
# define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84fe
# define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84ff
#endif

#ifndef GL_EXT_texture_object
# define GL_EXT_texture_object
# define GL_TEXTURE_PRIORITY_EXT 			0x8066
# define GL_TEXTURE_RESIDENT_EXT 			0x8067
# define GL_TEXTURE_1D_BINDING_EXT			0x8068
# define GL_TEXTURE_2D_BINDING_EXT			0x8069
# define GL_TEXTURE_3D_BINDING_EXT			0x806A
#endif

#ifndef GL_ARB_point_parameters
# define GL_ARB_point_parameters
# define GL_POINT_SIZE_MIN_ARB				0x8126
# define GL_POINT_SIZE_MAX_ARB				0x8127
# define GL_POINT_FADE_THRESHOLD_SIZE_ARB	0x8128
# define GL_DISTANCE_ATTENUATION_ARB 		0x8129
#endif

#ifndef GL_EXT_shared_texture_palette
# define GL_EXT_shared_texture_palette
# define GL_SHARED_TEXTURE_PALETTE_EXT		0x81FB
#endif

#ifndef GL_ARB_multitexture
# define GL_ARB_multitexture
# define GL_TEXTURE0_ARB 					0x84C0
# define GL_TEXTURE1_ARB 					0x84C1
# define GL_TEXTURE2_ARB 					0x84C2
# define GL_TEXTURE3_ARB 					0x84C3
# define GL_TEXTURE4_ARB 					0x84C4
# define GL_TEXTURE5_ARB 					0x84C5
# define GL_TEXTURE6_ARB 					0x84C6
# define GL_TEXTURE7_ARB 					0x84C7
# define GL_TEXTURE8_ARB 					0x84C8
# define GL_TEXTURE9_ARB 					0x84C9
# define GL_TEXTURE10_ARB 					0x84CA
# define GL_TEXTURE11_ARB 					0x84CB
# define GL_TEXTURE12_ARB 					0x84CC
# define GL_TEXTURE13_ARB 					0x84CD
# define GL_TEXTURE14_ARB 					0x84CE
# define GL_TEXTURE15_ARB					0x84CF
# define GL_TEXTURE16_ARB 					0x84D0
# define GL_TEXTURE17_ARB 					0x84D1
# define GL_TEXTURE18_ARB 					0x84D2
# define GL_TEXTURE19_ARB 					0x84D3
# define GL_TEXTURE20_ARB 					0x84D4
# define GL_TEXTURE21_ARB 					0x84D5
# define GL_TEXTURE22_ARB 					0x84D6
# define GL_TEXTURE23_ARB 					0x84D7
# define GL_TEXTURE24_ARB 					0x84D8
# define GL_TEXTURE25_ARB 					0x84D9
# define GL_TEXTURE26_ARB 					0x84DA
# define GL_TEXTURE27_ARB 					0x84DB
# define GL_TEXTURE28_ARB 					0x84DC
# define GL_TEXTURE29_ARB 					0x84DD
# define GL_TEXTURE30_ARB 					0x84DE
# define GL_TEXTURE31_ARB 					0x84DF
# define GL_ACTIVE_TEXTURE_ARB 				0x84E0
# define GL_CLIENT_ACTIVE_TEXTURE_ARB		0x84E1
# define GL_MAX_TEXTURE_UNITS_ARB			0x84E2
#endif

#ifndef GL_ARB_point_sprite
# define GL_ARB_point_sprite
# define GL_POINT_SPRITE_ARB				0x8861
# define GL_COORD_REPLACE_ARB				0x8862
#endif

#ifndef GL_ARB_texture_cube_map
# define GL_ARB_texture_cube_map
# define GL_NORMAL_MAP_ARB					0x8511
# define GL_REFLECTION_MAP_ARB				0x8512
# define GL_TEXTURE_CUBE_MAP_ARB			0x8513
# define GL_TEXTURE_BINDING_CUBE_MAP_ARB	0x8514
# define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB	0x8515
# define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB	0x8516
# define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB	0x8517
# define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB	0x8518
# define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB	0x8519
# define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB	0x851A
# define GL_PROXY_TEXTURE_CUBE_MAP_ARB		0x851B
# define GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB	0x851C
#endif

#ifndef GL_ARB_texture_env_combine
# define GL_ARB_texture_env_combine
# define GL_COMBINE_ARB						0x8570
# define GL_COMBINE_RGB_ARB					0x8571
# define GL_COMBINE_ALPHA_ARB				0x8572
# define GL_RGB_SCALE_ARB					0x8573
# define GL_ADD_SIGNED_ARB					0x8574
# define GL_INTERPOLATE_ARB					0x8575
# define GL_SUBTRACT_ARB					0x84E7
# define GL_CONSTANT_ARB					0x8576
# define GL_PRIMARY_COLOR_ARB				0x8577
# define GL_PREVIOUS_ARB					0x8578
# define GL_SOURCE0_RGB_ARB					0x8580
# define GL_SOURCE1_RGB_ARB					0x8581
# define GL_SOURCE2_RGB_ARB					0x8582
# define GL_SOURCE0_ALPHA_ARB				0x8588
# define GL_SOURCE1_ALPHA_ARB				0x8589
# define GL_SOURCE2_ALPHA_ARB				0x858A
# define GL_OPERAND0_RGB_ARB				0x8590
# define GL_OPERAND1_RGB_ARB				0x8591
# define GL_OPERAND2_RGB_ARB				0x8592
# define GL_OPERAND0_ALPHA_ARB				0x8598
# define GL_OPERAND1_ALPHA_ARB				0x8599
# define GL_OPERAND2_ALPHA_ARB				0x859A
#endif

#ifndef GL_ARB_texture_env_dot3
# define GL_DOT3_RGB_ARB					0x86AE
# define GL_DOT3_RGBA_ARB					0x86AF
#endif

#ifndef GL_ARB_depth_texture
# define GL_DEPTH_COMPONENT16_ARB			0x81A5
# define GL_DEPTH_COMPONENT24_ARB			0x81A6
# define GL_DEPTH_COMPONENT32_ARB			0x81A7
# define GL_TEXTURE_DEPTH_SIZE_ARB			0x884A
# define GL_DEPTH_TEXTURE_MODE_ARB			0x884B
#endif

/* Standard OpenGL external function defs */
typedef void (GLAPIENTRY *QF_glBlendColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (GLAPIENTRY *QF_glBlendEquation) (GLenum mode);
typedef void (GLAPIENTRY *QF_glDrawRangeElements) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (GLAPIENTRY *QF_glColorTable) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
typedef void (GLAPIENTRY *QF_glColorTableParameterfv) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (GLAPIENTRY *QF_glColorTableParameteriv) (GLenum target, GLenum pname, const GLint *params);
typedef void (GLAPIENTRY *QF_glCopyColorTable) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY *QF_glGetColorTable) (GLenum target, GLenum format, GLenum type, GLvoid *table);
typedef void (GLAPIENTRY *QF_glGetColorTableParameterfv) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY *QF_glGetColorTableParameteriv) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *QF_glColorSubTable) (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid *data);
typedef void (GLAPIENTRY *QF_glCopyColorSubTable) (GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY *QF_glConvolutionFilter1D) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid *image);
typedef void (GLAPIENTRY *QF_glConvolutionFilter2D) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *image);
typedef void (GLAPIENTRY *QF_glConvolutionParameterf) (GLenum target, GLenum pname, GLfloat params);
typedef void (GLAPIENTRY *QF_glConvolutionParameterfv) (GLenum target, GLenum pname, const GLfloat *params);
typedef void (GLAPIENTRY *QF_glConvolutionParameteri) (GLenum target, GLenum pname, GLint params);
typedef void (GLAPIENTRY *QF_glConvolutionParameteriv) (GLenum target, GLenum pname, const GLint *params);
typedef void (GLAPIENTRY *QF_glCopyConvolutionFilter1D) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (GLAPIENTRY *QF_glCopyConvolutionFilter2D) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY *QF_glGetConvolutionFilter) (GLenum target, GLenum format, GLenum type, GLvoid *image);
typedef void (GLAPIENTRY *QF_glGetConvolutionParameterfv) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY *QF_glGetConvolutionParameteriv) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *QF_glGetSeparableFilter) (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span);
typedef void (GLAPIENTRY *QF_glSeparableFilter2D) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *row, const GLvoid *column);
typedef void (GLAPIENTRY *QF_glGetHistogram) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (GLAPIENTRY *QF_glGetHistogramParameterfv) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY *QF_glGetHistogramParameteriv) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *QF_glGetMinmax) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (GLAPIENTRY *QF_glGetMinmaxParameterfv) (GLenum target, GLenum pname, GLfloat *params);
typedef void (GLAPIENTRY *QF_glGetMinmaxParameteriv) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *QF_glHistogram) (GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
typedef void (GLAPIENTRY *QF_glMinmax) (GLenum target, GLenum internalformat, GLboolean sink);
typedef void (GLAPIENTRY *QF_glResetHistogram) (GLenum target);
typedef void (GLAPIENTRY *QF_glResetMinmax) (GLenum target);
typedef void (GLAPIENTRY *QF_glTexImage3D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (GLAPIENTRY *QF_glTexSubImage3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (GLAPIENTRY *QF_glCopyTexSubImage3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

// GL_ARB_multitexture
typedef void (GLAPIENTRY *QF_glActiveTexture) (GLenum texture);
typedef void (GLAPIENTRY *QF_glClientActiveTexture) (GLenum texture);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1d) (GLenum target, GLdouble s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2d) (GLenum target, GLdouble s, GLdouble t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3d) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4d) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1dv) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2dv) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3dv) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4dv) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1f) (GLenum target, GLfloat s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2f) (GLenum target, GLfloat s, GLfloat t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3f) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4f) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1fv) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2fv) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3fv) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4fv) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1i) (GLenum target, GLint s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2i) (GLenum target, GLint s, GLint t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3i) (GLenum target, GLint s, GLint t, GLint r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4i) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1iv) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2iv) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3iv) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4iv) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1s) (GLenum target, GLshort s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2s) (GLenum target, GLshort s, GLshort t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3s) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4s) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1sv) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2sv) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3sv) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4sv) (GLenum target, const GLshort *v);

// GL_ARB_point_parameters
typedef void (GLAPIENTRY *QF_glPointParameterfARB) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY *QF_glPointParameterfvARB) (GLenum pname, const GLfloat *params);

// GL_EXT_paletted_texture
typedef void (GLAPIENTRY *QF_glColorTableEXT) (GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
typedef void (GLAPIENTRY *QF_glGetColorTableEXT) (GLenum target, GLenum format, GLenum type, GLvoid *data);
typedef void (GLAPIENTRY *QF_glGetColorTableParameterivEXT) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *QF_glGetColorTableParameterfvEXT) (GLenum target, GLenum pname, GLfloat *params);

// GL_EXT_subtexture
typedef void (GLAPIENTRY *QF_glTexSubImage1DEXT) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (GLAPIENTRY *QF_glTexSubImage2DEXT) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);

// GL_EXT_texture_object
typedef GLboolean (GLAPIENTRY *QF_glAreTexturesResidentEXT) (GLsizei n, const GLuint *textures, GLboolean *residences);
typedef void (GLAPIENTRY *QF_glBindTextureEXT) (GLenum target, GLuint texture);
typedef void (GLAPIENTRY *QF_glDeleteTexturesEXT) (GLsizei n, const GLuint *textures);
typedef void (GLAPIENTRY *QF_glGenTexturesEXT) (GLsizei n, GLuint *textures);
typedef GLboolean (GLAPIENTRY *QF_glIsTextureEXT) (GLuint texture);
typedef void (GLAPIENTRY *QF_glPrioritizeTexturesEXT) (GLsizei n, const GLuint *textures, const GLclampf *priorities);

// GL_EXT_vertex_array
typedef void (GLAPIENTRY *QF_glArrayElementEXT) (GLint i);
typedef void (GLAPIENTRY *QF_glColorPointerEXT) (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (GLAPIENTRY *QF_glDrawArraysEXT) (GLenum mode, GLint first, GLsizei count);
typedef void (GLAPIENTRY *QF_glEdgeFlagPointerEXT) (GLsizei stride, GLsizei count, const GLboolean *pointer);
typedef void (GLAPIENTRY *QF_glGetPointervEXT) (GLenum pname, GLvoid* *params);
typedef void (GLAPIENTRY *QF_glIndexPointerEXT) (GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (GLAPIENTRY *QF_glNormalPointerEXT) (GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (GLAPIENTRY *QF_glTexCoordPointerEXT) (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);
typedef void (GLAPIENTRY *QF_glVertexPointerEXT) (GLint size, GLenum type, GLsizei stride, GLsizei count, const GLvoid *pointer);

// 3Dfx
typedef int     FxI32;
typedef FxI32   GrScreenResolution_t;
typedef FxI32   GrDitherMode_t;
typedef FxI32   GrScreenRefresh_t;

typedef void (GLAPIENTRY *QF_gl3DfxSetPaletteEXT) (GLuint *pal);
typedef void (GLAPIENTRY * QF_3DfxSetDitherModeEXT) (GrDitherMode_t mode);

// GLX 1.3
typedef void *(GLAPIENTRY *QF_glXGetProcAddressARB) (const GLubyte *procName);

// WGL (Windows GL)
typedef const GLubyte *(GLAPIENTRY *QF_wglGetExtensionsStringEXT) (void);

/* QuakeForge extension functions */
qboolean QFGL_ExtensionPresent (const char *);
void *QFGL_ExtensionAddress (const char *);

#endif	// __qfgl_ext_h_
