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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "clview.h"//FIXME
#include "gl_draw.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_screen.h"
#include "r_shared.h"

/* Unknown renamed to GLErr_Unknown to solve conflict with winioctl.h */
unsigned int	GLErr_InvalidEnum;
unsigned int	GLErr_InvalidValue;
unsigned int	GLErr_InvalidOperation;
unsigned int	GLErr_OutOfMemory;
unsigned int	GLErr_Unknown;

extern void (*R_DrawSpriteModel) (struct entity_s *ent);


static unsigned int
R_TestErrors (unsigned int numerous)
{
	switch (qfglGetError ()) {
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
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - r_lineadj);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
						vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
						vid.height - r_lineadj);
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
						vid.height - r_lineadj -
						(r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

VISIBLE void
SCR_UpdateScreen (double realtime, SCR_Func *scr_funcs)
{
	static int  begun = 0;

	if (R_TestErrors (0))
		R_DisplayErrors ();
	R_ClearErrors ();

	if (begun) {
		begun = 0;
		GL_EndRendering ();
	}

	r_realtime = realtime;
	vid.numpages = 2 + gl_triplebuffer->int_val;

	if (!scr_initialized)
		return;

	qfglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	begun = 1;

	if (oldfov != scr_fov->value) {
		oldfov = scr_fov->value;
		vid.recalc_refdef = true;
	}
	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	V_RenderView ();	// FIXME (scr_3dfuncs?)

	SCR_SetUpToDrawConsole ();
	if (!r_worldentity.model)
		GL_Set2D ();
	GL_DrawReset ();
	SCR_TileClear ();
	GL_Set2DScaled ();
	while (*scr_funcs) {
		(*scr_funcs)();
		scr_funcs++;
	}
	GL_FlushText ();
	qfglFlush ();
}

VISIBLE tex_t *
SCR_ScreenShot (int width, int height)
{
	return 0;
}

VISIBLE void
SCR_ScreenShot_f (void)
{
}
