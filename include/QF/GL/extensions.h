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

	$Id$
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

#ifndef GL_EXT_texture_object
# define GL_EXT_texture_object
# define GL_TEXTURE_PRIORITY_EXT 			0x8066
# define GL_TEXTURE_RESIDENT_EXT 			0x8067
# define GL_TEXTURE_1D_BINDING_EXT			0x8068
# define GL_TEXTURE_2D_BINDING_EXT			0x8069
# define GL_TEXTURE_3D_BINDING_EXT			0x806A
#endif

#ifndef GL_EXT_point_parameters
# define GL_EXT_point_parameters
# define GL_POINT_SIZE_MIN_EXT				0x8126
# define GL_POINT_SIZE_MAX_EXT				0x8127
# define GL_POINT_FADE_THRESHOLD_SIZE_EXT	0x8128
# define GL_DISTANCE_ATTENUATION_EXT 		0x8129
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
typedef void (GLAPIENTRY *QF_glActiveTextureARB) (GLenum texture);
typedef void (GLAPIENTRY *QF_glClientActiveTextureARB) (GLenum texture);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1dARB) (GLenum target, GLdouble s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2dARB) (GLenum target, GLdouble s, GLdouble t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3dARB) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4dARB) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1dvARB) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2dvARB) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3dvARB) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4dvARB) (GLenum target, const GLdouble *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1fARB) (GLenum target, GLfloat s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3fARB) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4fARB) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1fvARB) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2fvARB) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3fvARB) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4fvARB) (GLenum target, const GLfloat *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1iARB) (GLenum target, GLint s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2iARB) (GLenum target, GLint s, GLint t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3iARB) (GLenum target, GLint s, GLint t, GLint r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4iARB) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1ivARB) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2ivARB) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3ivARB) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4ivARB) (GLenum target, const GLint *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1sARB) (GLenum target, GLshort s);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2sARB) (GLenum target, GLshort s, GLshort t);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3sARB) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4sARB) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (GLAPIENTRY *QF_glMultiTexCoord1svARB) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord2svARB) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord3svARB) (GLenum target, const GLshort *v);
typedef void (GLAPIENTRY *QF_glMultiTexCoord4svARB) (GLenum target, const GLshort *v);

// GL_EXT_paletted_texture
typedef void (GLAPIENTRY *QF_glColorTableEXT) (GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
typedef void (GLAPIENTRY *QF_glGetColorTableEXT) (GLenum target, GLenum format, GLenum type, GLvoid *data);
typedef void (GLAPIENTRY *QF_glGetColorTableParameterivEXT) (GLenum target, GLenum pname, GLint *params);
typedef void (GLAPIENTRY *QF_glGetColorTableParameterfvEXT) (GLenum target, GLenum pname, GLfloat *params);

// GL_EXT_point_parameters
typedef void (GLAPIENTRY *QF_glPointParameterfEXT) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY *QF_glPointParameterfvEXT) (GLenum pname, const GLfloat *params);

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
typedef long    FxI32;
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
