/*
	sw_ralias.c

	routines for setting up to draw alias models

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include <stdlib.h>

#include "QF/image.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "d_ifacea.h"
#include "r_internal.h"

#define LIGHT_MIN	5					// lowest light value we'll allow, to
										// avoid the need for inner-loop light
										// clamping

affinetridesc_t r_affinetridesc;

void       *acolormap;					// FIXME: should go away

trivertx_t *r_apverts;

// TODO: these probably will go away with optimized rasterization
static mdl_t      *pmdl;
vec3_t      r_plightvec;
int         r_ambientlight;
float       r_shadelight;
static aliashdr_t *paliashdr;
finalvert_t *pfinalverts;
auxvert_t  *pauxverts;
float ziscale;
static model_t *pmodel;

static vec3_t alias_forward, alias_right, alias_up;

static maliasskindesc_t *pskindesc;

int         r_amodels_drawn;
int         a_skinwidth;
int         r_anumverts;

float       aliastransform[3][4];

typedef struct {
	int         index0;
	int         index1;
} aedge_t;

static aedge_t aedges[12] = {
	{0, 1}, {1, 2}, {2, 3}, {3, 0},
	{4, 5}, {5, 6}, {6, 7}, {7, 4},
	{0, 5}, {1, 4}, {2, 7}, {3, 6}
};

static void R_AliasSetUpTransform (entity_t *ent, int trivial_accept);

qboolean
R_AliasCheckBBox (entity_t *ent)
{
	int         i, flags, frame, numv;
	aliashdr_t *pahdr;
	float       zi, basepts[8][3], v0, v1, frac;
	finalvert_t *pv0, *pv1, viewpts[16];
	auxvert_t  *pa0, *pa1, viewaux[16];
	maliasframedesc_t *pframedesc;
	qboolean    zclipped, zfullyclipped;
	unsigned int anyclip, allclip;
	int         minz;

	// expand, rotate, and translate points into worldspace
	ent->visibility.trivial_accept = 0;
	pmodel = ent->renderer.model;
	if (!(pahdr = pmodel->aliashdr))
		pahdr = Cache_Get (&pmodel->cache);
	pmdl = (mdl_t *) ((byte *) pahdr + pahdr->model);

	R_AliasSetUpTransform (ent, 0);

	// construct the base bounding box for this frame
	frame = ent->animation.frame;
// TODO: don't repeat this check when drawing?
	if ((frame >= pmdl->numframes) || (frame < 0)) {
		Sys_MaskPrintf (SYS_dev, "No such frame %d %s\n", frame, pmodel->path);
		frame = 0;
	}

	pframedesc = &pahdr->frames[frame];

	// x worldspace coordinates
	basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] =
		(float) pframedesc->bboxmin.v[0];
	basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] =
		(float) pframedesc->bboxmax.v[0];

	// y worldspace coordinates
	basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] =
		(float) pframedesc->bboxmin.v[1];
	basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] =
		(float) pframedesc->bboxmax.v[1];

	// z worldspace coordinates
	basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] =
		(float) pframedesc->bboxmin.v[2];
	basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] =
		(float) pframedesc->bboxmax.v[2];

	zclipped = false;
	zfullyclipped = true;

	minz = 9999;
	for (i = 0; i < 8; i++) {
		R_AliasTransformVector (&basepts[i][0], &viewaux[i].fv[0]);

		if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE) {
			// we must clip points that are closer than the near clip plane
			viewpts[i].flags = ALIAS_Z_CLIP;
			zclipped = true;
		} else {
			if (viewaux[i].fv[2] < minz)
				minz = viewaux[i].fv[2];
			viewpts[i].flags = 0;
			zfullyclipped = false;
		}
	}

	if (zfullyclipped) {
		if (!pmodel->aliashdr)
			Cache_Release (&pmodel->cache);
		return false;					// everything was near-z-clipped
	}

	numv = 8;

	if (zclipped) {
		// organize points by edges, use edges to get new points (possible
		// trivial reject)
		for (i = 0; i < 12; i++) {
			// edge endpoints
			pv0 = &viewpts[aedges[i].index0];
			pv1 = &viewpts[aedges[i].index1];
			pa0 = &viewaux[aedges[i].index0];
			pa1 = &viewaux[aedges[i].index1];

			// if one end is clipped and the other isn't, make a new point
			if (pv0->flags ^ pv1->flags) {
				frac = (ALIAS_Z_CLIP_PLANE - pa0->fv[2]) /
					(pa1->fv[2] - pa0->fv[2]);
				viewaux[numv].fv[0] = pa0->fv[0] +
					(pa1->fv[0] - pa0->fv[0]) * frac;
				viewaux[numv].fv[1] = pa0->fv[1] +
					(pa1->fv[1] - pa0->fv[1]) * frac;
				viewaux[numv].fv[2] = ALIAS_Z_CLIP_PLANE;
				viewpts[numv].flags = 0;
				numv++;
			}
		}
	}
	// project the vertices that remain after clipping
	anyclip = 0;
	allclip = ALIAS_XY_CLIP_MASK;

// TODO: probably should do this loop in ASM, especially if we use floats
	for (i = 0; i < numv; i++) {
		// we don't need to bother with vertices that were z-clipped
		if (viewpts[i].flags & ALIAS_Z_CLIP)
			continue;

		zi = 1.0 / viewaux[i].fv[2];

		// FIXME: do with chop mode in ASM, or convert to float
		v0 = (viewaux[i].fv[0] * xscale * zi) + xcenter;
		v1 = (viewaux[i].fv[1] * yscale * zi) + ycenter;

		flags = 0;

		if (v0 < r_refdef.fvrectx)
			flags |= ALIAS_LEFT_CLIP;
		if (v1 < r_refdef.fvrecty)
			flags |= ALIAS_TOP_CLIP;
		if (v0 > r_refdef.fvrectright)
			flags |= ALIAS_RIGHT_CLIP;
		if (v1 > r_refdef.fvrectbottom)
			flags |= ALIAS_BOTTOM_CLIP;

		anyclip |= flags;
		allclip &= flags;
	}

	if (allclip) {
		if (!pmodel->aliashdr)
			Cache_Release (&pmodel->cache);
		return false;					// trivial reject off one side
	}

	ent->visibility.trivial_accept = !anyclip & !zclipped;

	if (ent->visibility.trivial_accept) {
		if (minz > (r_aliastransition + (pmdl->size * r_resfudge))) {
			ent->visibility.trivial_accept |= 2;
		}
	}

	if (!pmodel->aliashdr)
		Cache_Release (&pmodel->cache);
	return true;
}

void
R_AliasTransformVector (vec3_t in, vec3_t out)
{
	out[0] = DotProduct (in, aliastransform[0]) + aliastransform[0][3];
	out[1] = DotProduct (in, aliastransform[1]) + aliastransform[1][3];
	out[2] = DotProduct (in, aliastransform[2]) + aliastransform[2][3];
}

void
R_AliasClipAndProjectFinalVert (finalvert_t *fv, auxvert_t *av)
{
	if (av->fv[2] < ALIAS_Z_CLIP_PLANE) {
		fv->flags |= ALIAS_Z_CLIP;
		return;
	}

	R_AliasProjectFinalVert (fv, av);

	if (fv->v[0] < r_refdef.aliasvrect.x)
		fv->flags |= ALIAS_LEFT_CLIP;
	if (fv->v[1] < r_refdef.aliasvrect.y)
		fv->flags |= ALIAS_TOP_CLIP;
	if (fv->v[0] > r_refdef.aliasvrectright)
		fv->flags |= ALIAS_RIGHT_CLIP;
	if (fv->v[1] > r_refdef.aliasvrectbottom)
		fv->flags |= ALIAS_BOTTOM_CLIP;
}

static void
R_AliasTransformFinalVert16 (auxvert_t *av, trivertx_t *pverts)
{
	trivertx_t  * pextra;
	float       vextra[3];

	pextra = pverts + pmdl->numverts;
	vextra[0] = pverts->v[0] + pextra->v[0] / (float)256;
	vextra[1] = pverts->v[1] + pextra->v[1] / (float)256;
	vextra[2] = pverts->v[2] + pextra->v[2] / (float)256;
	av->fv[0] = DotProduct (vextra, aliastransform[0]) +
		aliastransform[0][3];
	av->fv[1] = DotProduct (vextra, aliastransform[1]) +
		aliastransform[1][3];
	av->fv[2] = DotProduct (vextra, aliastransform[2]) +
		aliastransform[2][3];
}

static void
R_AliasTransformFinalVert8 (auxvert_t *av, trivertx_t *pverts)
{
	av->fv[0] = DotProduct (pverts->v, aliastransform[0]) +
		aliastransform[0][3];
	av->fv[1] = DotProduct (pverts->v, aliastransform[1]) +
		aliastransform[1][3];
	av->fv[2] = DotProduct (pverts->v, aliastransform[2]) +
		aliastransform[2][3];
}

/*
	R_AliasPreparePoints

	General clipped case
*/
static void
R_AliasPreparePoints (void)
{
	int         i;
	stvert_t   *pstverts;
	finalvert_t *fv;
	auxvert_t  *av;
	mtriangle_t *ptri;
	finalvert_t *pfv[3];

	pstverts = (stvert_t *) ((byte *) paliashdr + paliashdr->stverts);
	r_anumverts = pmdl->numverts;
	fv = pfinalverts;
	av = pauxverts;

	if (pmdl->ident == HEADER_MDL16) {
		for (i = 0; i < r_anumverts; i++, fv++, av++, r_apverts++,
				 pstverts++) {
			R_AliasTransformFinalVert16 (av, r_apverts);
			R_AliasTransformFinalVert (fv, r_apverts, pstverts);
			R_AliasClipAndProjectFinalVert (fv, av);
		}
	} else {
		for (i = 0; i < r_anumverts; i++, fv++, av++, r_apverts++,
				 pstverts++) {
			R_AliasTransformFinalVert8 (av, r_apverts);
			R_AliasTransformFinalVert (fv, r_apverts, pstverts);
			R_AliasClipAndProjectFinalVert (fv, av);
		}
	}

	// clip and draw all triangles
	r_affinetridesc.numtriangles = 1;

	ptri = (mtriangle_t *) ((byte *) paliashdr + paliashdr->triangles);
	for (i = 0; i < pmdl->numtris; i++, ptri++) {
		pfv[0] = &pfinalverts[ptri->vertindex[0]];
		pfv[1] = &pfinalverts[ptri->vertindex[1]];
		pfv[2] = &pfinalverts[ptri->vertindex[2]];

		if (pfv[0]->flags & pfv[1]->flags & pfv[2]->flags
			& (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))
			continue;					// completely clipped

		if (!((pfv[0]->flags | pfv[1]->flags | pfv[2]->flags)
			  & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP))) {	// totally unclipped
			r_affinetridesc.pfinalverts = pfinalverts;
			r_affinetridesc.ptriangles = ptri;
			D_PolysetDraw ();
		} else {						// partially clipped
			R_AliasClipTriangle (ptri);
		}
	}
}

static void
R_AliasSetUpTransform (entity_t *ent, int trivial_accept)
{
	int         i;
	float       rotationmatrix[3][4], t2matrix[3][4];
	static float tmatrix[3][4];
	static float viewmatrix[3][4];
	mat4f_t     mat;

	Transform_GetWorldMatrix (ent->transform, mat);
	VectorCopy (mat[0], alias_forward);
	VectorNegate (mat[1], alias_right);
	VectorCopy (mat[2], alias_up);

	tmatrix[0][0] = pmdl->scale[0];
	tmatrix[1][1] = pmdl->scale[1];
	tmatrix[2][2] = pmdl->scale[2];

	tmatrix[0][3] = pmdl->scale_origin[0];
	tmatrix[1][3] = pmdl->scale_origin[1];
	tmatrix[2][3] = pmdl->scale_origin[2];

// TODO: can do this with simple matrix rearrangement

	for (i = 0; i < 3; i++) {
		t2matrix[i][0] = alias_forward[i];
		t2matrix[i][1] = -alias_right[i];
		t2matrix[i][2] = alias_up[i];
	}

	t2matrix[0][3] = r_entorigin[0] - r_refdef.frame.position[0];
	t2matrix[1][3] = r_entorigin[1] - r_refdef.frame.position[1];
	t2matrix[2][3] = r_entorigin[2] - r_refdef.frame.position[2];

// FIXME: can do more efficiently than full concatenation
	R_ConcatTransforms (t2matrix, tmatrix, rotationmatrix);

// TODO: should be global, set when vright, etc., set
	VectorCopy (vright, viewmatrix[0]);
	VectorCopy (vup, viewmatrix[1]);
	VectorNegate (viewmatrix[1], viewmatrix[1]);
	VectorCopy (vfwd, viewmatrix[2]);

//	viewmatrix[0][3] = 0;
//	viewmatrix[1][3] = 0;
//	viewmatrix[2][3] = 0;

	R_ConcatTransforms (viewmatrix, rotationmatrix, aliastransform);

// do the scaling up of x and y to screen coordinates as part of the transform
// for the unclipped case (it would mess up clipping in the clipped case).
// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
// correspondingly so the projected x and y come out right
// FIXME: make this work for clipped case too?

	if (trivial_accept && pmdl->ident != HEADER_MDL16) {
		for (i = 0; i < 4; i++) {
			aliastransform[0][i] *= aliasxscale *
				(1.0 / ((float) 0x8000 * 0x10000));
			aliastransform[1][i] *= aliasyscale *
				(1.0 / ((float) 0x8000 * 0x10000));
			aliastransform[2][i] *= 1.0 / ((float) 0x8000 * 0x10000);
		}
	}
}

/*
	R_AliasTransformFinalVert

	now this function just copies the texture coordinates and calculates
	lighting actual 3D transform is done by R_AliasTransformFinalVert8/16
	functions above */
void
R_AliasTransformFinalVert (finalvert_t *fv, trivertx_t *pverts,
						   stvert_t *pstverts)
{
	int         temp;
	float       lightcos, *plightnormal;

	fv->v[2] = pstverts->s;
	fv->v[3] = pstverts->t;

	fv->flags = pstverts->onseam;

	// lighting
	plightnormal = r_avertexnormals[pverts->lightnormalindex];
	lightcos = DotProduct (plightnormal, r_plightvec);
	temp = r_ambientlight;

	if (lightcos < 0) {
		temp += (int) (r_shadelight * lightcos);

		// clamp; because we limited the minimum ambient and shading light,
		// we don't have to clamp low light, just bright
		if (temp < 0)
			temp = 0;
	}

	fv->v[4] = temp;
}

#ifdef PIC
#undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifndef USE_INTEL_ASM
void
R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts)
{
	int         i, temp;
	float       lightcos, *plightnormal, zi;
	trivertx_t *pverts;

	pverts = r_apverts;

	for (i = 0; i < r_anumverts; i++, fv++, pverts++, pstverts++) {
		// transform and project
		zi = 1.0 / (DotProduct (pverts->v, aliastransform[2]) +
					aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in
		// the projection
		fv->v[5] = zi;

		fv->v[0] = ((DotProduct (pverts->v, aliastransform[0]) +
					 aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v[1] = ((DotProduct (pverts->v, aliastransform[1]) +
					 aliastransform[1][3]) * zi) + aliasycenter;

		fv->v[2] = pstverts->s;
		fv->v[3] = pstverts->t;
		fv->flags = pstverts->onseam;

		// lighting
		plightnormal = r_avertexnormals[pverts->lightnormalindex];
		lightcos = DotProduct (plightnormal, r_plightvec);
		temp = r_ambientlight;

		if (lightcos < 0) {
			temp += (int) (r_shadelight * lightcos);

			// clamp; because we limited the minimum ambient and shading
			// light, we don't have to clamp low light, just bright
			if (temp < 0)
				temp = 0;
		}

		fv->v[4] = temp;
	}
}
#endif

void
R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av)
{
	float       zi;

	// project points
	zi = 1.0 / av->fv[2];

	fv->v[5] = zi * ziscale;

	fv->v[0] = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
	fv->v[1] = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}

static void
R_AliasPrepareUnclippedPoints (void)
{
	stvert_t   *pstverts;
	finalvert_t *fv;

	pstverts = (stvert_t *) ((byte *) paliashdr + paliashdr->stverts);
	r_anumverts = pmdl->numverts;
// FIXME: just use pfinalverts directly?
	fv = pfinalverts;

	R_AliasTransformAndProjectFinalVerts (fv, pstverts);

	if (r_affinetridesc.drawtype)
		D_PolysetDrawFinalVerts (fv, r_anumverts);

	r_affinetridesc.pfinalverts = pfinalverts;
	r_affinetridesc.ptriangles = (mtriangle_t *)
		((byte *) paliashdr + paliashdr->triangles);
	r_affinetridesc.numtriangles = pmdl->numtris;

	D_PolysetDraw ();
}

static void
R_AliasSetupSkin (entity_t *ent)
{
	int         skinnum;

	skinnum = ent->renderer.skinnum;
	if ((skinnum >= pmdl->numskins) || (skinnum < 0)) {
		Sys_MaskPrintf (SYS_dev, "R_AliasSetupSkin: no such skin # %d\n",
						skinnum);
		skinnum = 0;
	}

	pskindesc = R_AliasGetSkindesc (&ent->animation, skinnum, paliashdr);

	a_skinwidth = pmdl->skinwidth;

	r_affinetridesc.pskin = (void *) ((byte *) paliashdr + pskindesc->skin);
	r_affinetridesc.skinwidth = a_skinwidth;
	r_affinetridesc.seamfixupX16 = (a_skinwidth >> 1) << 16;
	r_affinetridesc.skinheight = pmdl->skinheight;

	acolormap = vid.colormap8;
	if (ent->renderer.skin) {
		tex_t      *base;

		base = ent->renderer.skin->texels;
		if (base) {
			r_affinetridesc.pskin = base->data;
			r_affinetridesc.skinwidth = base->width;
			r_affinetridesc.skinheight = base->height;
		}
		acolormap = ent->renderer.skin->colormap;
	}
}


static void
R_AliasSetupLighting (alight_t *plighting)
{
	// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't
	// have to clamp off the bottom
	r_ambientlight = plighting->ambientlight;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_ambientlight = (255 - r_ambientlight) << VID_CBITS;

	if (r_ambientlight < LIGHT_MIN)
		r_ambientlight = LIGHT_MIN;

	r_shadelight = plighting->shadelight;

	if (r_shadelight < 0)
		r_shadelight = 0;

	r_shadelight *= VID_GRADES;

	// rotate the lighting vector into the model's frame of reference
	r_plightvec[0] = DotProduct (plighting->plightvec, alias_forward);
	r_plightvec[1] = -DotProduct (plighting->plightvec, alias_right);
	r_plightvec[2] = DotProduct (plighting->plightvec, alias_up);
}

/*
	R_AliasSetupFrame

	set r_apverts
*/
static void
R_AliasSetupFrame (entity_t *ent)
{
	maliasframedesc_t *frame;

	frame = R_AliasGetFramedesc (&ent->animation, paliashdr);
	r_apverts = (trivertx_t *) ((byte *) paliashdr + frame->frame);
}


void
R_AliasDrawModel (entity_t *ent, alight_t *plighting)
{
	int          size;
	finalvert_t *finalverts;

	r_amodels_drawn++;

	if (!(paliashdr = ent->renderer.model->aliashdr))
		paliashdr = Cache_Get (&ent->renderer.model->cache);
	pmdl = (mdl_t *) ((byte *) paliashdr + paliashdr->model);

	size = (CACHE_SIZE - 1)
		+ sizeof (finalvert_t) * (pmdl->numverts + 1)
		+ sizeof (auxvert_t) * pmdl->numverts;
	finalverts = (finalvert_t *) Hunk_TempAlloc (0, size);
	if (!finalverts)
		Sys_Error ("R_AliasDrawModel: out of memory");

	// cache align
	pfinalverts = (finalvert_t *)
		(((intptr_t) &finalverts[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	pauxverts = (auxvert_t *) &pfinalverts[pmdl->numverts + 1];

	R_AliasSetupSkin (ent);
	R_AliasSetUpTransform (ent, ent->visibility.trivial_accept);
	R_AliasSetupLighting (plighting);
	R_AliasSetupFrame (ent);

	r_affinetridesc.drawtype = ((ent->visibility.trivial_accept == 3)
								&& r_recursiveaffinetriangles);

	if (!acolormap)
		acolormap = vid.colormap8;

	if (r_affinetridesc.drawtype) {
		D_PolysetUpdateTables ();		// FIXME: precalc...
	} else {
#ifdef USE_INTEL_ASM
		D_Aff8Patch (acolormap);
#endif
	}

	if (ent != vr_data.view_model)
		ziscale = (float) 0x8000 *(float) 0x10000;
	else
		ziscale = (float) 0x8000 *(float) 0x10000 *3.0;

	if (ent->visibility.trivial_accept
		&& pmdl->ident != HEADER_MDL16) {
		R_AliasPrepareUnclippedPoints ();
	} else {
		R_AliasPreparePoints ();
	}

	if (!ent->renderer.model->aliashdr) {
		Cache_Release (&ent->renderer.model->cache);
	}
}
