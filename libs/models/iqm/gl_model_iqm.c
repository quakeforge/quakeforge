/*
	gl_model_iqm.c

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

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/GL/qf_iqm.h"
#include "QF/GL/qf_textures.h"

#include "mod_internal.h"

static byte null_texture[] = {
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
	204, 204, 204, 255,
};

static void
gl_iqm_clear (model_t *mod, void *data)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	gliqm_t    *gl = (gliqm_t *) iqm->extra_data;

	mod->needload = true;

	free (gl->blend_palette);
	free (gl);
	Mod_FreeIQM (iqm);
}

static void
gl_iqm_load_textures (iqm_t *iqm)
{
	gliqm_t  *gl = (gliqm_t *) iqm->extra_data;
	int         i;
	dstring_t  *str = dstring_new ();
	tex_t      *tex;

	gl->textures = malloc (iqm->num_meshes * sizeof (int));
	for (i = 0; i < iqm->num_meshes; i++) {
		dstring_copystr (str, iqm->text + iqm->meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		if ((tex = LoadImage (va ("textures/%s", str->str), 1)))
			gl->textures[i] = GL_LoadTexture (str->str, tex->width,
											  tex->height, tex->data, true,
											  false,
											  tex->format > 2 ? tex->format
															  : 1);
		else
			gl->textures[i] = GL_LoadTexture ("", 2, 2, null_texture, true,
											  false, 4);
	}
	dstring_delete (str);
}

void
gl_Mod_IQMFinish (model_t *mod)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	gliqm_t    *gl;
	int         i;

	mod->clear = gl_iqm_clear;
	iqm->extra_data = gl = calloc (1, sizeof (gliqm_t));
	gl_iqm_load_textures (iqm);
	gl->blend_palette = Mod_IQMBuildBlendPalette (iqm, &gl->palette_size);
	for (i = 0; i < iqm->num_arrays; i++) {
		if (iqm->vertexarrays[i].type == IQM_POSITION)
			gl->position = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_TEXCOORD)
			gl->texcoord = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_NORMAL)
			gl->normal = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_BLENDINDEXES)
			gl->bindices = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_COLOR)
			gl->color = &iqm->vertexarrays[i];
	}
}
