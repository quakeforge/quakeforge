/*
	glsl_draw.c

	2D drawing support for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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

#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "gl_draw.h"
#include "r_shared.h"

static const char quaketext_vert[] =
#include "quaketxt.vc"
;

static const char quaketext_frag[] =
#include "quaketxt.fc"
;

typedef struct {
	const char *name;
	qboolean    uniform;
	int         location;
} shaderparam_t;

VISIBLE byte *draw_chars;
static dstring_t *char_queue;
static int  char_texture;
static int  qtxt_vert;
static int  qtxt_frag;
static int  qtxt_prog;
static float proj_matrix[16];

static shaderparam_t charmap = {"charmap", 1};
static shaderparam_t palette = {"palette", 1};
static shaderparam_t matrix = {"mvp_mat", 1};
static shaderparam_t vertex = {"vertex", 0};
static shaderparam_t dchar = {"char", 0};

VISIBLE qpic_t *
Draw_PicFromWad (const char *name)
{
	return 0;
}

VISIBLE qpic_t *
Draw_CachePic (const char *path, qboolean alpha)
{
	return 0;
}

VISIBLE void
Draw_TextBox (int x, int y, int width, int lines, byte alpha)
{
}

static int
compile_shader (const char *name, const char *shader_src, int type)
{
	const char *src[1];
	int         shader;
	int         compiled;

	src[0] = shader_src;
	shader = qfglCreateShader (type);
	qfglShaderSource (shader, 1, src, 0);
	qfglCompileShader (shader);
	qfglGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		dstring_t  *log = dstring_new ();
		int         size;
		qfglGetShaderiv (shader, GL_INFO_LOG_LENGTH, &size);
		log->size = size + 1;	// for terminating null
		dstring_adjust (log);
		qfglGetShaderInfoLog (shader, log->size, 0, log->str);
		qfglDeleteShader (shader);
		Sys_Printf ("Shader (%s) compile error:\n----8<----\n%s----8<----\n",
					name, log->str);
		dstring_delete (log);
		return 0;
	}
	return shader;
}

static int
link_program (const char *name, int vert, int frag)
{
	int         program;
	int         linked;

	program = qfglCreateProgram ();
	qfglAttachShader (program, vert);
	qfglAttachShader (program, frag);
	qfglLinkProgram (program);

	qfglGetProgramiv (program, GL_LINK_STATUS, &linked);
	if (!linked) {
		dstring_t  *log = dstring_new ();
		int         size;
		qfglGetProgramiv (program, GL_INFO_LOG_LENGTH, &size);
		log->size = size + 1;	// for terminating null
		dstring_adjust (log);
		qfglGetProgramInfoLog (program, log->size, 0, log->str);
		qfglDeleteProgram (program);
		Sys_Printf ("Program (%s) link error:\n----8<----\n%s----8<----\n",
					name, log->str);
		dstring_delete (log);
		return 0;
	}
	return program;
}

static int
resolve_shader_param (int program, shaderparam_t *param)
{
	if (param->uniform) {
		param->location = qfglGetUniformLocation (program, param->name);
	} else {
		param->location = qfglGetAttribLocation (program, param->name);
	}
	if (param->location < 0) {
		Sys_Printf ("could not resolve %s %s\n",
					param->uniform ? "uniform" : "attribute", param->name);
	}
	return param->location;
}

VISIBLE void
Draw_Init (void)
{
	GLuint      tnum;
	int         i;

	char_queue = dstring_new ();
	qtxt_vert = compile_shader ("quaketxt.vert", quaketext_vert,
								GL_VERTEX_SHADER);
	qtxt_frag = compile_shader ("quaketxt.frag", quaketext_frag,
								GL_FRAGMENT_SHADER);
	qtxt_prog = link_program ("quaketxt", qtxt_vert, qtxt_frag);
	resolve_shader_param (qtxt_prog, &charmap);
	resolve_shader_param (qtxt_prog, &palette);
	resolve_shader_param (qtxt_prog, &matrix);
	resolve_shader_param (qtxt_prog, &vertex);
	resolve_shader_param (qtxt_prog, &dchar);

	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;		// proper transparent color

	qfglGenTextures (1, &tnum);
	char_texture = tnum;
	qfglBindTexture (GL_TEXTURE_2D, char_texture);
	qfglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE,
					128, 128, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, draw_chars);
}

static inline void
queue_character (int x, int y, byte chr)
{
	unsigned short *v;
	int         i;
	const int   size = 5 * 2 * 4;

	char_queue->size += size;
	dstring_adjust (char_queue);
	v = (unsigned short *) (char_queue->str + char_queue->size - size);
	for (i = 0; i < 4; i++) {
		*v++ = x;
		*v++ = y;
		*v++ = i & 1;
		*v++ = (i >> 1) & 1;
		*v++ = chr;
	}
}

static void
flush_text (void)
{
	qfglUseProgram (qtxt_prog);
	qfglEnableVertexAttribArray (vertex.location);
	qfglEnableVertexAttribArray (dchar.location);

	qfglUniformMatrix4fv (matrix.location, 1, false, proj_matrix);
	qfglUniform1i (charmap.location, 0);
	qfglActiveTexture(GL_TEXTURE0 + 0);
	qfglBindTexture(GL_TEXTURE_2D, char_texture);

	qfglUniform1i (palette.location, 1);
	qfglActiveTexture(GL_TEXTURE0 + 1);
	qfglBindTexture(GL_TEXTURE_2D, glsl_palette);

	qfglVertexAttribPointer (vertex.location, 4, GL_UNSIGNED_SHORT, 0, 10,
							 char_queue->str);
	qfglVertexAttribPointer (dchar.location, 1, GL_UNSIGNED_SHORT, 0, 10,
							 char_queue->str + 8);

	qfglDrawArrays(GL_QUADS, 0, char_queue->size / 10);

	qfglDisableVertexAttribArray (dchar.location);
	qfglDisableVertexAttribArray (vertex.location);
	char_queue->size = 0;
}

VISIBLE void
Draw_Character (int x, int y, unsigned int chr)
{
	chr &= 255;

	if (chr == 32)
		return;							// space
	if (y <= -8)
		return;							// totally off screen

	queue_character (x, y, chr);
}

VISIBLE void
Draw_String (int x, int y, const char *str)
{
	byte        chr;

	if (!str || !str[0])
		return;							// totally off screen
	if (y <= -8)
		return;							// totally off screen

	while (*str) {
		if ((chr = *str++) != 32)
			queue_character (x, y, chr);
		x += 8;
	}
}

VISIBLE void
Draw_nString (int x, int y, const char *str, int count)
{
	byte        chr;

	if (!str || !str[0])
		return;							// totally off screen
	if (y <= -8)
		return;							// totally off screen

	while (count-- && *str) {
		if ((chr = *str++) != 32)
			queue_character (x, y, chr);
		x += 8;
	}
}

void
Draw_AltString (int x, int y, const char *str)
{
}

VISIBLE void
Draw_Crosshair (void)
{
}

void
Draw_CrosshairAt (int ch, int x, int y)
{
}

VISIBLE void
Draw_Pic (int x, int y, qpic_t *pic)
{
}

VISIBLE void
Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width,
			 int height)
{
}

VISIBLE void
Draw_ConsoleBackground (int lines, byte alpha)
{
}

VISIBLE void
Draw_TileClear (int x, int y, int w, int h)
{
}

VISIBLE void
Draw_Fill (int x, int y, int w, int h, int c)
{
}

VISIBLE void
Draw_FadeScreen (void)
{
}

static void
ortho_mat (float *proj, float xmin, float xmax, float ymin, float ymax,
		   float znear, float zfar)
{
	proj[0] = 2/(xmax-xmin);
	proj[4] = 0;
	proj[8] = 0;
	proj[12] = -(xmax+xmin)/(xmax-xmin);

	proj[1] = 0;
	proj[5] = 2/(ymax-ymin);
	proj[9] = 0;
	proj[13] = -(ymax+ymin)/(ymax-ymin);

	proj[2] = 0;
	proj[6] = 0;
	proj[10] = -2/(zfar-znear);
	proj[14] = -(zfar+znear)/(zfar-znear);

	proj[3] = 0;
	proj[7] = 0;
	proj[11] = 0;
	proj[15] = 1;
}

static void
set_2d (int width, int height)
{
	qfglViewport (0, 0, vid.width, vid.height);

	qfglDisable (GL_DEPTH_TEST);
	qfglDisable (GL_CULL_FACE);

	qfglColor3ubv (color_white);

	ortho_mat (proj_matrix, 0, width, height, 0, -99999, 99999);
}

void
GL_Set2D (void)
{
    set_2d (vid.width, vid.height);
}

void
GL_Set2DScaled (void)
{
    set_2d (vid.conwidth, vid.conheight);
}

void
GL_DrawReset (void)
{
	char_queue->size = 0;
}

void
GL_FlushText (void)
{
	if (char_queue->size)
		flush_text ();
}
