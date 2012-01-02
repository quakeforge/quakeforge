/*
	vid_common_glsl.c

	Common OpenGLSL video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2011      Bill Currie <bill@taniwha.org>

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

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
#include "QF/dstring.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_vid.h"

#include "compat.h"
#include "d_iface.h"
#include "r_cvar.h"
#include "sbar.h"

VISIBLE int					glsl_palette;
VISIBLE int					gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
VISIBLE int					gl_filter_max = GL_LINEAR;
VISIBLE float       		gldepthmin, gldepthmax;
VISIBLE qboolean			is8bit = false;

static void
GL_Common_Init_Cvars (void)
{
}

void
VID_SetPalette (unsigned char *palette)
{
	byte       *pal, *ip, *op;
	unsigned int r, g, b, v;
	unsigned short i;
	unsigned int *table;

	// 8 8 8 encoding
	Sys_MaskPrintf (SYS_VID, "Converting 8to24\n");

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

	Sys_MaskPrintf (SYS_VID, "Converting palette to RGBA texture\n");
	pal = malloc (256 * 4);
	for (i = 0, ip = palette, op = pal; i < 255; i++) {
		*op++ = *ip++;
		*op++ = *ip++;
		*op++ = *ip++;
		*op++ = 255;	// alpha = 1
	}
	QuatZero (op);		// color 255 = transparent (alpha = 0)

	if (!glsl_palette) {
		GLuint      tex;
		qfglGenTextures (1, &tex);
		glsl_palette = tex;
	}
	qfglBindTexture (GL_TEXTURE_2D, glsl_palette);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pal);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qfglGenerateMipmap (GL_TEXTURE_2D);
	free (pal);
}

void
GL_Init_Common (void)
{
	GLF_FindFunctions ();

	GL_Common_Init_Cvars ();

	qfglClearColor (0, 0, 0, 0);

	qfglEnable (GL_TEXTURE_2D);
	qfglCullFace (GL_FRONT);

	qfglEnable (GL_BLEND);
	qfglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void
VID_Init8bitPalette (void)
{
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

int
GL_CompileShader (const char *name, const char *shader_src, int type)
{
	const char *src[1];
	int         shader;
	int         compiled;

	src[0] = shader_src;
	shader = qfglCreateShader (type);
	qfglShaderSource (shader, 1, src, 0);
	qfglCompileShader (shader);
	qfglGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled || (developer->int_val & SYS_GLSL)) {
		dstring_t  *log = dstring_new ();
		int         size;
		qfglGetShaderiv (shader, GL_INFO_LOG_LENGTH, &size);
		log->size = size + 1;	// for terminating null
		dstring_adjust (log);
		qfglGetShaderInfoLog (shader, log->size, 0, log->str);
		if (!compiled)
			qfglDeleteShader (shader);
		Sys_Printf ("Shader (%s) compile log:\n----8<----\n%s----8<----\n",
					name, log->str);
		dstring_delete (log);
		if (!compiled)
			return 0;
	}
	return shader;
}

static const char *
type_name (GLenum type)
{
	switch (type) {
		case GL_FLOAT_VEC2:
			return "vec2";
		case GL_FLOAT_VEC3:
			return "vec3";
		case GL_FLOAT_VEC4:
			return "vec4";
		case GL_INT_VEC2:
			return "ivec2";
		case GL_INT_VEC3:
			return "ivec3";
		case GL_INT_VEC4:
			return "ivec4";
		case GL_BOOL:
			return "bool";
		case GL_BOOL_VEC2:
			return "bvec2";
		case GL_BOOL_VEC3:
			return "bvec3";
		case GL_BOOL_VEC4:
			return "bvec4";
		case GL_FLOAT_MAT2:
			return "mat2";
		case GL_FLOAT_MAT3:
			return "mat3";
		case GL_FLOAT_MAT4:
			return "mat4";
		case GL_SAMPLER_2D:
			return "sampler_2d";
		case GL_SAMPLER_CUBE:
			return "sampler_cube";
		case GL_BYTE:
			return "byte";
		case GL_UNSIGNED_BYTE:
			return "unsigned byte";
		case GL_SHORT:
			return "short";
		case GL_UNSIGNED_SHORT:
			return "unsigned short";
		case GL_INT:
			return "int";
		case GL_UNSIGNED_INT:
			return "unsigned int";
		case GL_FLOAT:
			return "float";
		case GL_FIXED:
			return "fixed";
	}
	return va("%x", type);
}

static void
dump_program (const char *name, int program)
{
	GLint       ind = 0;
	GLint       count = 0;
	GLint       size;
	dstring_t  *pname;
	GLint       psize = 0;
	GLenum      ptype = 0;

	pname = dstring_new ();
	qfglGetProgramiv (program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &size);
	pname->size = size;
	dstring_adjust (pname);

	qfglGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
	Sys_Printf("Program %s (%d) has %i uniforms\n", name, program, count);
	for (ind = 0; ind < count; ind++) {
		qfglGetActiveUniform(program, ind, pname->size, 0, &psize, &ptype,
							 pname->str);
		Sys_Printf ("Uniform %i name \"%s\" size %i type %s\n", (int)ind,
					pname->str, (int)psize, type_name (ptype));
	}

	qfglGetProgramiv (program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &size);
	pname->size = size;
	dstring_adjust (pname);

	qfglGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
	Sys_Printf("Program %s (%d) has %i attributes\n", name, program, count);
	for (ind = 0; ind < count; ind++) {
		qfglGetActiveAttrib(program, ind, pname->size, 0, &psize, &ptype,
							pname->str);
		Sys_Printf ("Attribute %i name \"%s\" size %i type %s\n", (int)ind,
					pname->str, (int)psize, type_name (ptype));
	}
}

int
GL_LinkProgram (const char *name, int vert, int frag)
{
	int         program;
	int         linked;

	program = qfglCreateProgram ();
	qfglAttachShader (program, vert);
	qfglAttachShader (program, frag);
	qfglLinkProgram (program);

	qfglGetProgramiv (program, GL_LINK_STATUS, &linked);
	if (!linked || (developer->int_val & SYS_GLSL)) {
		dstring_t  *log = dstring_new ();
		int         size;
		qfglGetProgramiv (program, GL_INFO_LOG_LENGTH, &size);
		log->size = size + 1;	// for terminating null
		dstring_adjust (log);
		qfglGetProgramInfoLog (program, log->size, 0, log->str);
		if (!linked)
			qfglDeleteProgram (program);
		Sys_Printf ("Program (%s) link log:\n----8<----\n%s----8<----\n",
					name, log->str);
		dstring_delete (log);
		if (!linked)
			return 0;
	}
	if (developer->int_val & SYS_GLSL)
		dump_program (name, program);
	return program;
}

int
GL_ResolveShaderParam (int program, shaderparam_t *param)
{
	if (param->uniform) {
		param->location = qfglGetUniformLocation (program, param->name);
	} else {
		param->location = qfglGetAttribLocation (program, param->name);
	}
	if (param->location < 0) {
		Sys_Printf ("could not resolve %s %s\n",
					param->uniform ? "uniform" : "attribute", param->name);
	} else {
		Sys_MaskPrintf (SYS_GLSL, "Resolved %s %s @ %d\n",
						param->uniform ? "uniform" : "attribute",
						param->name, param->location);
	}
	return param->location;
}

void
GL_DumpAttribArrays (void)
{
	GLint       max = 0;
	GLint       ind;
	GLint       enabled;
	GLint       size = -1;
	GLint       stride = -1;
	GLint       type = -1;
	GLint       norm = -1;
	GLint       binding = -1;
	GLint       current[4] = {-1, -1, -1, -1};
	void       *pointer = 0;

	qfglGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &max);

	for (ind = 0; ind < max; ind++) {
		qfglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
		Sys_Printf ("attrib %d: %sabled\n", ind, enabled ? "en" : "dis");
		qfglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
		qfglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
		qfglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
		qfglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &norm);
		qfglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
							   &binding);
		qfglGetVertexAttribiv (ind, GL_CURRENT_VERTEX_ATTRIB, current);
		qfglGetVertexAttribPointerv (ind, GL_VERTEX_ATTRIB_ARRAY_POINTER,
									 &pointer);
		Sys_Printf ("    %d %d '%s' %d %d (%d %d %d %d) %p\n", size, stride,
					type_name (type), norm, binding, QuatExpand (current),
					pointer);
	}
}
