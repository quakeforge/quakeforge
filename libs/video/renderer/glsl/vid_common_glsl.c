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
#include "QF/GLSL/qf_textures.h"

#include "compat.h"
#include "d_iface.h"
#include "r_internal.h"

static const char quakeforge_effect[] =
#include "libs/video/renderer/glsl/quakeforge.slc"
;

int					glsl_palette;
int					glsl_colormap;

static void
GLSL_Common_Init_Cvars (void)
{
}

void
GLSL_SetPalette (const byte *palette)
{
	const byte *col, *ip;
	byte       *pal, *op;
	unsigned    r, g, b, v;
	unsigned short i;
	unsigned   *table;

	// 8 8 8 encoding
	Sys_MaskPrintf (SYS_VID, "Converting 8to24\n");

	table = d_8to24table;
	for (i = 0; i < 255; i++) { // used to be i<256, see d_8to24table below
		r = palette[i * 3 + 0];
		g = palette[i * 3 + 1];
		b = palette[i * 3 + 2];
#ifdef WORDS_BIGENDIAN
		v = (255 << 0) + (r << 24) + (g << 16) + (b << 8);
#else
		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
#endif
		*table++ = v;
	}
	d_8to24table[255] = 0;	// 255 is transparent

	Sys_MaskPrintf (SYS_VID, "Converting palette/colormap to RGBA textures\n");
	pal = malloc (256 * VID_GRADES * 4);
	for (i = 0, col = vr_data.vid->colormap8, op = pal; i < 256 * VID_GRADES;
		 i++) {
		ip = palette + *col++ * 3;
		*op++ = *ip++;
		*op++ = *ip++;
		*op++ = *ip++;
		*op++ = 255;	// alpha = 1
	}
	for (i = 0; i < VID_GRADES; i++)
		pal[i * 256 * 4 + 255 + 3] = 0;

	if (!glsl_colormap) {
		GLuint      tex;
		qfeglGenTextures (1, &tex);
		glsl_colormap = tex;
	}
	qfeglBindTexture (GL_TEXTURE_2D, glsl_colormap);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, VID_GRADES, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pal);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	for (i = 0, ip = palette, op = pal; i < 255; i++) {
		*op++ = *ip++;
		*op++ = *ip++;
		*op++ = *ip++;
		*op++ = 255;	// alpha = 1
	}
	QuatZero (op);

	if (!glsl_palette) {
		GLuint      tex;
		qfeglGenTextures (1, &tex);
		glsl_palette = tex;
	}
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, pal);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qfeglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	free (pal);
}

void
GLSL_Init_Common (void)
{
	EGLF_FindFunctions ();

	GLSL_Common_Init_Cvars ();

	GLSL_TextureInit ();

	if (developer->int_val & SYS_GLSL) {
		GLint       max;

		qfeglGetIntegerv (GL_MAX_VERTEX_UNIFORM_VECTORS, &max);
		Sys_Printf ("max vertex uniform vectors: %d\n", max);
		qfeglGetIntegerv (GL_MAX_FRAGMENT_UNIFORM_VECTORS, &max);
		Sys_Printf ("max fragment uniforms: %d\n", max);
		qfeglGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &max);
		Sys_Printf ("max texture image units: %d\n", max);
		qfeglGetIntegerv (GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max);
		Sys_Printf ("max vertex texture image units: %d\n", max);
		qfeglGetIntegerv (GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max);
		Sys_Printf ("max combined texture image units: %d\n", max);
		qfeglGetIntegerv (GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max);
		Sys_Printf ("max cube map texture size: %d\n", max);
		qfeglGetIntegerv (GL_MAX_RENDERBUFFER_SIZE, &max);
		Sys_Printf ("max renderbuffer size: %d\n", max);
		qfeglGetIntegerv (GL_MAX_VARYING_VECTORS, &max);
		Sys_Printf ("max varying vectors: %d\n", max);
		qfeglGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &max);
		Sys_Printf ("max vertex attribs: %d\n", max);
	}

	qfeglClearColor (0, 0, 0, 0);

	qfeglPixelStorei (GL_UNPACK_ALIGNMENT, 1);

	qfeglEnable (GL_TEXTURE_2D);
	qfeglFrontFace (GL_CW);
	qfeglCullFace (GL_BACK);

	qfeglEnable (GL_BLEND);
	qfeglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLSL_RegisterEffect ("QuakeForge", quakeforge_effect);
}

int
GLSL_CompileShader (const char *name, const shader_t *shader, int type)
{
	int         sid;
	int         compiled;

	sid = qfeglCreateShader (type);
	qfeglShaderSource (sid, shader->num_strings, shader->strings, 0);
	qfeglCompileShader (sid);
	qfeglGetShaderiv (sid, GL_COMPILE_STATUS, &compiled);
	if (!compiled || (developer->int_val & SYS_GLSL)) {
		dstring_t  *log = dstring_new ();
		int         size;
		qfeglGetShaderiv (sid, GL_INFO_LOG_LENGTH, &size);
		log->size = size + 1;	// for terminating null
		dstring_adjust (log);
		qfeglGetShaderInfoLog (sid, log->size, 0, log->str);
		if (!compiled)
			qfeglDeleteShader (sid);
		Sys_Printf ("Shader (%s) compile log:\n----8<----\n%s----8<----\n",
					name, log->str);
		dstring_delete (log);
		if (!compiled)
			return 0;
	}
	return sid;
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
	qfeglGetProgramiv (program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &size);
	pname->size = size;
	dstring_adjust (pname);

	qfeglGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
	Sys_Printf("Program %s (%d) has %i uniforms\n", name, program, count);
	for (ind = 0; ind < count; ind++) {
		qfeglGetActiveUniform(program, ind, pname->size, 0, &psize, &ptype,
							 pname->str);
		Sys_Printf ("Uniform %i name \"%s\" size %i type %s\n", (int)ind,
					pname->str, (int)psize, type_name (ptype));
	}

	qfeglGetProgramiv (program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &size);
	pname->size = size;
	dstring_adjust (pname);

	qfeglGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
	Sys_Printf("Program %s (%d) has %i attributes\n", name, program, count);
	for (ind = 0; ind < count; ind++) {
		qfeglGetActiveAttrib(program, ind, pname->size, 0, &psize, &ptype,
							pname->str);
		Sys_Printf ("Attribute %i name \"%s\" size %i type %s\n", (int)ind,
					pname->str, (int)psize, type_name (ptype));
	}
	dstring_delete (pname);
}

int
GLSL_LinkProgram (const char *name, int vert, int frag)
{
	int         program;
	int         linked;

	program = qfeglCreateProgram ();
	qfeglAttachShader (program, vert);
	qfeglAttachShader (program, frag);
	qfeglLinkProgram (program);

	qfeglGetProgramiv (program, GL_LINK_STATUS, &linked);
	if (!linked || (developer->int_val & SYS_GLSL)) {
		dstring_t  *log = dstring_new ();
		int         size;
		qfeglGetProgramiv (program, GL_INFO_LOG_LENGTH, &size);
		log->size = size + 1;	// for terminating null
		dstring_adjust (log);
		qfeglGetProgramInfoLog (program, log->size, 0, log->str);
		if (!linked)
			qfeglDeleteProgram (program);
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
GLSL_ResolveShaderParam (int program, shaderparam_t *param)
{
	if (param->uniform) {
		param->location = qfeglGetUniformLocation (program, param->name);
	} else {
		param->location = qfeglGetAttribLocation (program, param->name);
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
GLSL_DumpAttribArrays (void)
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

	qfeglGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &max);

	for (ind = 0; ind < max; ind++) {
		qfeglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
		Sys_Printf ("attrib %d: %sabled\n", ind, enabled ? "en" : "dis");
		qfeglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
		qfeglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
		qfeglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
		qfeglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &norm);
		qfeglGetVertexAttribiv (ind, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
							   &binding);
		qfeglGetVertexAttribiv (ind, GL_CURRENT_VERTEX_ATTRIB, current);
		qfeglGetVertexAttribPointerv (ind, GL_VERTEX_ATTRIB_ARRAY_POINTER,
									 &pointer);
		Sys_Printf ("    %d %d '%s' %d %d (%d %d %d %d) %p\n", size, stride,
					type_name (type), norm, binding, QuatExpand (current),
					pointer);
	}
}
