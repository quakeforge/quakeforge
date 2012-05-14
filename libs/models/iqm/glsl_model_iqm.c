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
glsl_iqm_clear (model_t *mod)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	glsliqm_t  *glsl = (glsliqm_t *) iqm->extra_data;
	int         i;

	mod->needload = true;

	for (i = 0; i < iqm->num_meshes; i++) {
		GLSL_ReleaseTexture (glsl->textures[i]);
		GLSL_ReleaseTexture (glsl->normmaps[i]);
	}
	free (glsl);
	free (iqm->text);
	free (iqm->vertices);
	free (iqm->vertexarrays);
	free (iqm->elements);
	free (iqm->meshes);
	free (iqm->joints);
	free (iqm->baseframe);
	free (iqm->inverse_baseframe);
	free (iqm->anims);
	free (iqm->frames[0]);
	free (iqm->frames);
	free (iqm);
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
		if ((tex = LoadImage (va ("textures/%s", str->str))))
			glsl->textures[i] = GLSL_LoadRGBATexture (str->str, tex->width,
													  tex->height, tex->data);
		else
			glsl->textures[i] = GLSL_LoadRGBATexture ("", 2, 2, null_texture);
		if ((tex = LoadImage (va ("textures/%s_norm", str->str))))
			glsl->normmaps[i] = GLSL_LoadRGBATexture (str->str, tex->width,
													  tex->height, tex->data);
		else
			glsl->normmaps[i] = GLSL_LoadRGBATexture ("", 2, 2, null_normmap);
	}
	dstring_delete (str);
}

void
glsl_Mod_IQMFinish (model_t *mod)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	mod->clear = glsl_iqm_clear;
	iqm->extra_data = calloc (1, sizeof (glsliqm_t));
	glsl_iqm_load_textures (iqm);
}
