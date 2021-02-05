/*
	sw_model_iqm.c

	iqm model processing for SW

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

#include "qfalloca.h"

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "d_iface.h"
#include "mod_internal.h"
#include "r_internal.h"

static tex_t null_texture = {
	2, 2, tex_palette, 0, {15, 15, 15, 15}
};

static void
sw_iqm_clear (model_t *mod)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	swiqm_t    *sw = (swiqm_t *) iqm->extra_data;
	int         i, j;

	mod->needload = true;

	for (i = 0; i < iqm->num_meshes; i++) {
		if (!sw->skins[i])
			continue;
		for (j = i + 1; j < iqm->num_meshes; j++)
			if (sw->skins[j] == sw->skins[i])
				sw->skins[j] = 0;
		if (sw->skins[i] != &null_texture)
			free (sw->skins[i]);
	}
	free (sw->skins);
	free (sw->blend_palette);
	free (sw);
	Mod_FreeIQM (iqm);
}

static inline void
convert_coord (byte *tc, int size)
{
	*(int32_t *) tc = (int32_t) (*(float *) tc * (size - 1)) << 16;
}

static void
sw_iqm_load_textures (iqm_t *iqm)
{
	int         i, j;
	dstring_t  *str = dstring_new ();
	swiqm_t    *sw = (swiqm_t *) iqm->extra_data;
	tex_t      *tex;
	byte       *done_verts;
	int         bytes;

	bytes = (iqm->num_verts + 7) / 8;
	done_verts = alloca (bytes);
	memset (done_verts, 0, bytes);
	sw->skins = malloc (iqm->num_meshes * sizeof (tex_t *));
	for (i = 0; i < iqm->num_meshes; i++) {
		for (j = 0; j < i; j++) {
			if (iqm->meshes[j].material == iqm->meshes[i].material) {
				sw->skins[i] = sw->skins[j];
				break;
			}
		}
		if (j < i)
			continue;
		dstring_copystr (str, iqm->text + iqm->meshes[i].material);
		QFS_StripExtension (str->str, str->str);
		if ((tex = LoadImage (va ("textures/%s", str->str))))
			tex = sw->skins[i] = ConvertImage (tex, vid.basepal);
		else
			tex = sw->skins[i] = &null_texture;
		for (j = 0; j < (int) iqm->meshes[i].num_triangles * 3; j++) {
			int         ind = iqm->meshes[i].first_triangle * 3 + j;
			int         vind = iqm->elements[ind];
			byte       *vert = iqm->vertices + iqm->stride * vind;
			byte       *tc = vert + sw->texcoord->offset;

			if (done_verts[vind / 8] & (1 << (vind % 8)))
				continue;
			done_verts[vind / 8] |= (1 << (vind % 8));

			convert_coord (tc + 0, tex->width);
			convert_coord (tc + 4, tex->height);
		}
	}
	dstring_delete (str);
}

static void
sw_iqm_convert_tris (iqm_t *iqm)
{
	mtriangle_t *tris;
	uint32_t    i;
	uint32_t    num_tris;

	num_tris = iqm->num_elements / 3;
	tris = malloc (num_tris * sizeof (mtriangle_t));
	for (i = 0; i < num_tris; i++) {
		tris[i].facesfront = 1;
		VectorCopy (iqm->elements + i * 3, tris[i].vertindex);
	}
	free (iqm->elements);
	iqm->elements = (uint16_t *) tris;
}

void
sw_Mod_IQMFinish (model_t *mod)
{
	iqm_t      *iqm = (iqm_t *) mod->aliashdr;
	swiqm_t    *sw;
	int         i;

	mod->clear = sw_iqm_clear;
	iqm->extra_data = sw = calloc (1, sizeof (swiqm_t));
	sw->blend_palette = Mod_IQMBuildBlendPalette (iqm, &sw->palette_size);
	for (i = 0; i < iqm->num_arrays; i++) {
		if (iqm->vertexarrays[i].type == IQM_POSITION)
			sw->position = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_TEXCOORD)
			sw->texcoord = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_NORMAL)
			sw->normal = &iqm->vertexarrays[i];
		if (iqm->vertexarrays[i].type == IQM_BLENDINDEXES)
			sw->bindices = &iqm->vertexarrays[i];
	}
	sw_iqm_load_textures (iqm);
	sw_iqm_convert_tris (iqm);
}
