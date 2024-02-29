/*
	glsl_model_iqm.c

	iqm model processing for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/04/27

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_iqm.h"
#include "QF/GLSL/qf_textures.h"

#include "mod_internal.h"
#include "r_shared.h"

static byte null_texture[] = {
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
};

static byte null_normmap[] = {
	127, 127, 255, 255,
	127, 127, 255, 255,
	127, 127, 255, 255,
	127, 127, 255, 255,
};

static void
glsl_iqm_clear (model_t *mod, void *data)
{
	iqm_t      *iqm = (iqm_t *) mod->mesh;
	glsliqm_t  *glsl = (glsliqm_t *) iqm->extra_data;
	GLuint      bufs[2];
	int         i;

	mod->needload = true;

	bufs[0] = glsl->vertex_array;
	bufs[1] = glsl->element_array;
	qfeglDeleteBuffers (2, bufs);

	for (i = 0; i < iqm->num_meshes; i++) {
		GLSL_ReleaseTexture (glsl->textures[i]);
		GLSL_ReleaseTexture (glsl->normmaps[i]);
	}
	free (glsl->textures);
	free (glsl);
	Mod_FreeIQM (iqm);
}

static void
glsl_iqm_load_textures (iqm_t *iqm)
{
	glsliqm_t  *glsl = (glsliqm_t *) iqm->extra_data;
	int         i;
	dstring_t  *str = dstring_new ();
	tex_t      *tex;

	glsl->textures = malloc (2 * iqm->num_meshes * sizeof (GLuint));
	glsl->normmaps = &glsl->textures[iqm->num_meshes];
	for (i = 0; i < iqm->num_meshes; i++) {
		dstring_copystr (str, iqm->text + iqm->meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		if ((tex = LoadImage (va (0, "textures/%s", str->str), 1)))
			glsl->textures[i] = GLSL_LoadRGBATexture (str->str, tex->width,
													  tex->height, tex->data);
		else
			glsl->textures[i] = GLSL_LoadRGBATexture ("", 2, 2, null_texture);
		if ((tex = LoadImage (va (0, "textures/%s_norm", str->str), 1)))
			glsl->normmaps[i] = GLSL_LoadRGBATexture (str->str, tex->width,
													  tex->height, tex->data);
		else
			glsl->normmaps[i] = GLSL_LoadRGBATexture ("", 2, 2, null_normmap);
	}
	dstring_delete (str);
}

static void
glsl_iqm_load_arrays (iqm_t *iqm)
{
	glsliqm_t  *glsl = (glsliqm_t *) iqm->extra_data;
	GLuint      bufs[2];

	qfeglGenBuffers (2, bufs);
	glsl->vertex_array = bufs[0];
	glsl->element_array = bufs[1];
	qfeglBindBuffer (GL_ARRAY_BUFFER, glsl->vertex_array);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, glsl->element_array);
	qfeglBufferData (GL_ARRAY_BUFFER, iqm->num_verts * iqm->stride,
					 iqm->vertices, GL_STATIC_DRAW);
	int esize = iqm->num_verts > 0xfff0 ? sizeof (uint32_t) : sizeof (uint16_t);
	qfeglBufferData (GL_ELEMENT_ARRAY_BUFFER,
					 iqm->num_elements * esize, iqm->elements16,
					 GL_STATIC_DRAW);
	qfeglBindBuffer (GL_ARRAY_BUFFER, 0);
	qfeglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
	free (iqm->vertices);
	iqm->vertices = 0;
	free (iqm->elements16);
	iqm->elements16 = 0;
}

void
glsl_Mod_IQMFinish (model_t *mod)
{
	iqm_t      *iqm = (iqm_t *) mod->mesh;
	mod->clear = glsl_iqm_clear;
	iqm->extra_data = calloc (1, sizeof (glsliqm_t));
	glsl_iqm_load_textures (iqm);
	glsl_iqm_load_arrays (iqm);
}
