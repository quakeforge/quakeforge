/*
	glsl_main.c

	GLSL rendering

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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/png.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_draw.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"
#include "QF/ui/view.h"

#include "r_internal.h"
#include "vid_gl.h"

/* Unknown renamed to GLErr_Unknown to solve conflict with winioctl.h */
static unsigned int GLErr_InvalidEnum;
static unsigned int GLErr_InvalidValue;
static unsigned int GLErr_InvalidOperation;
static unsigned int GLErr_OutOfMemory;
static unsigned int GLErr_Unknown;

extern void (*R_DrawSpriteModel) (struct entity_s *ent);


static unsigned int
R_TestErrors (unsigned int numerous)
{
	switch (qfeglGetError ()) {
	case GL_NO_ERROR:
		return numerous;
		break;
	case GL_INVALID_ENUM:
		GLErr_InvalidEnum++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_VALUE:
		GLErr_InvalidValue++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_OPERATION:
		GLErr_InvalidOperation++;
		R_TestErrors (numerous++);
		break;
	case GL_OUT_OF_MEMORY:
		GLErr_OutOfMemory++;
		R_TestErrors (numerous++);
		break;
	default:
		GLErr_Unknown++;
		R_TestErrors (numerous++);
		break;
	}

	return numerous;
}

static void
R_DisplayErrors (void)
{
	if (GLErr_InvalidEnum)
		printf ("%d OpenGL errors: Invalid Enum!\n", GLErr_InvalidEnum);
	if (GLErr_InvalidValue)
		printf ("%d OpenGL errors: Invalid Value!\n", GLErr_InvalidValue);
	if (GLErr_InvalidOperation)
		printf ("%d OpenGL errors: Invalid Operation!\n", GLErr_InvalidOperation);
	if (GLErr_OutOfMemory)
		printf ("%d OpenGL errors: Out Of Memory!\n", GLErr_OutOfMemory);
	if (GLErr_Unknown)
		printf ("%d Unknown OpenGL errors!\n", GLErr_Unknown);
}

static void
R_ClearErrors (void)
{
	GLErr_InvalidEnum = 0;
	GLErr_InvalidValue = 0;
	GLErr_InvalidOperation = 0;
	GLErr_OutOfMemory = 0;
	GLErr_Unknown = 0;
}

static void
SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - vr_data.lineadj);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						vid.height - vr_data.lineadj);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0,
						r_refdef.vrect.x + r_refdef.vrect.width,
						r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
						r_refdef.vrect.y + r_refdef.vrect.height,
						r_refdef.vrect.width,
						vid.height - vr_data.lineadj -
						(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

void
glsl_R_RenderFrame (SCR_Func *scr_funcs)
{
	static int  begun = 0;

	if (R_TestErrors (0))
		R_DisplayErrors ();
	R_ClearErrors ();

	if (begun) {
		begun = 0;
		glsl_ctx->end_rendering ();
	}

	//FIXME forces the status bar to redraw. needed because it does not fully
	//update in sw modes but must in glsl mode
	vr_data.scr_copyeverything = 1;

	qfeglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	begun = 1;

	glsl_R_RenderView ();

	GLSL_Set2D ();
	GLSL_DrawReset ();
	SCR_TileClear ();
	GLSL_Set2DScaled ();

	view_draw (vr_data.scr_view);
	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
		GLSL_FlushText ();
	}
	GLSL_End2D ();
	qfeglFlush ();
}

tex_t *
glsl_SCR_CaptureBGR (void)
{
	byte       *r, *b;
	int         count, i;
	tex_t      *tex;

	count = vid.width * vid.height;
	tex = malloc (sizeof (tex_t) + count * 3);
	tex->data = (byte *) (tex + 1);
	SYS_CHECKMEM (tex);
	tex->width = vid.width;
	tex->height = vid.height;
	tex->format = tex_rgb;
	tex->palette = 0;
	qfeglReadPixels (0, 0, vid.width, vid.height, GL_RGB,
					GL_UNSIGNED_BYTE, tex->data);
	for (i = 0, r = tex->data, b = tex->data + 2; i < count;
		 i++, r += 3, b += 3) {
		byte        t = *b;
		*b = *r;
		*r = t;
	}
	return tex;
}
