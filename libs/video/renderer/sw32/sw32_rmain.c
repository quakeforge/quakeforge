/*
	sw32_rmain.c

	(description)

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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sound.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"

//define    PASSAGES

static vec3_t      viewlightvec;
static alight_t    r_viewlighting = { 128, 192, viewlightvec };
int         sw32_r_numallocatededges;
qboolean    sw32_r_drawpolys;
qboolean    sw32_r_drawculledpolys;
qboolean    sw32_r_worldpolysbacktofront;
float       sw32_r_aliasuvscale = 1.0;
int         sw32_r_outofsurfaces;
int         sw32_r_outofedges;

qboolean    sw32_r_dowarp, sw32_r_dowarpold, sw32_r_viewchanged;

int         sw32_c_surf;
int         sw32_r_maxsurfsseen, sw32_r_maxedgesseen;
static int  r_cnumsurfs;
static qboolean    r_surfsonstack;
int         sw32_r_clipflags;

byte       *sw32_r_warpbuffer;

static byte       *r_stack_start;

// screen size info
float       sw32_xcenter, sw32_ycenter;
float       sw32_xscale, sw32_yscale;
float       sw32_xscaleinv, sw32_yscaleinv;
float       sw32_xscaleshrink, sw32_yscaleshrink;
float       sw32_aliasxscale, sw32_aliasyscale, sw32_aliasxcenter, sw32_aliasycenter;

int         sw32_screenwidth;

float       sw32_pixelAspect;
static float       screenAspect;
static float       verticalFieldOfView;
static float       xOrigin, yOrigin;

plane_t     sw32_screenedge[4];

// refresh flags
int         sw32_r_polycount;
int         sw32_r_drawnpolycount;

int        *sw32_pfrustum_indexes[4];
int         sw32_r_frustum_indexes[4 * 6];

float       sw32_r_aliastransition, sw32_r_resfudge;

static float dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
static float se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

void
sw32_R_Textures_Init (void)
{
	int         x, y, m;
	byte       *dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip =
		Hunk_AllocName (sizeof (texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2,
						"notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof (texture_t);

	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m = 0; m < 4; m++) {
		dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++)
			for (x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

void
sw32_R_Init (void)
{
	int         dummy;

	// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte *) & dummy;

	R_Init_Cvars ();
	sw32_R_Particles_Init_Cvars ();

	sw32_Draw_Init ();
	SCR_Init ();
	sw32_R_InitTurb ();

	Cmd_AddCommand ("timerefresh", sw32_R_TimeRefresh_f, "Tests the current "
					"refresh rate for the current location");
	Cmd_AddCommand ("pointfile", sw32_R_ReadPointFile_f, "Load a pointfile to "
					"determine map leaks");
	Cmd_AddCommand ("loadsky", sw32_R_LoadSky_f, "Load a skybox");

	Cvar_SetValue (r_maxedges, (float) NUMSTACKEDGES);
	Cvar_SetValue (r_maxsurfs, (float) NUMSTACKSURFACES);

	sw32_view_clipplanes[0].leftedge = true;
	sw32_view_clipplanes[1].rightedge = true;
	sw32_view_clipplanes[1].leftedge = sw32_view_clipplanes[2].leftedge =
		sw32_view_clipplanes[3].leftedge = false;
	sw32_view_clipplanes[0].rightedge = sw32_view_clipplanes[2].rightedge =
		sw32_view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	sw32_D_Init ();

	Skin_Init ();
}

void
sw32_R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
	int         i;
	mod_brush_t *brush = &worldmodel->brush;

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.renderer.model = worldmodel;

	R_FreeAllEntities ();

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < brush->numleafs; i++)
		brush->leafs[i].efrags = NULL;

	if (brush->skytexture)
		sw32_R_InitSky (brush->skytexture);

	// Force a vis update
	r_viewleaf = NULL;
	R_MarkLeaves ();

	sw32_R_ClearParticles ();

	r_cnumsurfs = r_maxsurfs->int_val;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES) {
		sw32_surfaces = Hunk_AllocName (r_cnumsurfs * sizeof (surf_t), "surfaces");

		sw32_surface_p = sw32_surfaces;
		sw32_surf_max = &sw32_surfaces[r_cnumsurfs];
		r_surfsonstack = false;
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		sw32_surfaces--;
	} else {
		r_surfsonstack = true;
	}

	sw32_r_maxedgesseen = 0;
	sw32_r_maxsurfsseen = 0;

	sw32_r_numallocatededges = r_maxedges->int_val;

	if (sw32_r_numallocatededges < MINEDGES)
		sw32_r_numallocatededges = MINEDGES;

	if (sw32_r_numallocatededges <= NUMSTACKEDGES) {
		sw32_auxedges = NULL;
	} else {
		sw32_auxedges = Hunk_AllocName (sw32_r_numallocatededges * sizeof (edge_t),
								   "edges");
	}

	sw32_r_dowarpold = false;
	sw32_r_viewchanged = false;
}

/*
	R_ViewChanged

	Called every time the vid structure or r_refdef changes.
	Guaranteed to be called before the first refresh
*/
void
sw32_R_ViewChanged (void)
{
	int         i;
	float       res_scale;

	sw32_r_viewchanged = true;

	r_refdef.horizontalFieldOfView = 2.0 * tan (r_refdef.fov_x / 360 * M_PI);
	r_refdef.fvrectx = (float) r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float) r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x << 20) + (1 << 19) - 1;
	r_refdef.fvrecty = (float) r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float) r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 =
		(r_refdef.vrectright << 20) + (1 << 19) - 1;
	r_refdef.fvrectright = (float) r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float) r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float) r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float) r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float) r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int) (r_refdef.vrect.x * sw32_r_aliasuvscale);
	r_refdef.aliasvrect.y = (int) (r_refdef.vrect.y * sw32_r_aliasuvscale);
	r_refdef.aliasvrect.width = (int) (r_refdef.vrect.width * sw32_r_aliasuvscale);
	r_refdef.aliasvrect.height = (int) (r_refdef.vrect.height *
										sw32_r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
		r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
		r_refdef.aliasvrect.height;

	sw32_pixelAspect = 1;//FIXME vid.aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;

	screenAspect = r_refdef.vrect.width * sw32_pixelAspect / r_refdef.vrect.height;
	// 320*200 1.0 sw32_pixelAspect = 1.6 screenAspect
	// 320*240 1.0 sw32_pixelAspect = 1.3333 screenAspect
	// proper 320*200 sw32_pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

	// values for perspective projection
	// if math were exact, the values would range from 0.5 to to range+0.5
	// hopefully they wll be in the 0.000001 to range+.999999 and truncate
	// the polygon rasterization will never render in the first row or column
	// but will definately render in the [range] row and column, so adjust the
	// buffer origin to get an exact edge to edge fill
	sw32_xcenter = ((float) r_refdef.vrect.width * XCENTERING) +
		r_refdef.vrect.x - 0.5;
	sw32_aliasxcenter = sw32_xcenter * sw32_r_aliasuvscale;
	sw32_ycenter = ((float) r_refdef.vrect.height * YCENTERING) +
		r_refdef.vrect.y - 0.5;
	sw32_aliasycenter = sw32_ycenter * sw32_r_aliasuvscale;

	sw32_xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	sw32_aliasxscale = sw32_xscale * sw32_r_aliasuvscale;
	sw32_xscaleinv = 1.0 / sw32_xscale;
	sw32_yscale = sw32_xscale * sw32_pixelAspect;
	sw32_aliasyscale = sw32_yscale * sw32_r_aliasuvscale;
	sw32_yscaleinv = 1.0 / sw32_yscale;
	sw32_xscaleshrink = (r_refdef.vrect.width - 6) / r_refdef.horizontalFieldOfView;
	sw32_yscaleshrink = sw32_xscaleshrink * sw32_pixelAspect;

	// left side clip
	sw32_screenedge[0].normal[0] = -1.0 / (xOrigin *
									  r_refdef.horizontalFieldOfView);
	sw32_screenedge[0].normal[1] = 0;
	sw32_screenedge[0].normal[2] = 1;
	sw32_screenedge[0].type = PLANE_ANYZ;

	// right side clip
	sw32_screenedge[1].normal[0] = 1.0 / ((1.0 - xOrigin) *
									 r_refdef.horizontalFieldOfView);
	sw32_screenedge[1].normal[1] = 0;
	sw32_screenedge[1].normal[2] = 1;
	sw32_screenedge[1].type = PLANE_ANYZ;

	// top side clip
	sw32_screenedge[2].normal[0] = 0;
	sw32_screenedge[2].normal[1] = -1.0 / (yOrigin * verticalFieldOfView);
	sw32_screenedge[2].normal[2] = 1;
	sw32_screenedge[2].type = PLANE_ANYZ;

	// bottom side clip
	sw32_screenedge[3].normal[0] = 0;
	sw32_screenedge[3].normal[1] = 1.0 / ((1.0 - yOrigin) * verticalFieldOfView);
	sw32_screenedge[3].normal[2] = 1;
	sw32_screenedge[3].type = PLANE_ANYZ;

	for (i = 0; i < 4; i++)
		VectorNormalize (sw32_screenedge[i].normal);

	res_scale = sqrt ((double) (r_refdef.vrect.width * r_refdef.vrect.height) /
					  (320.0 * 152.0)) * (2.0 /
										  r_refdef.horizontalFieldOfView);
	sw32_r_aliastransition = r_aliastransbase->value * res_scale;
	sw32_r_resfudge = r_aliastransadj->value * res_scale;

	sw32_D_ViewChanged ();
}

static void
R_DrawEntitiesOnList (void)
{
	int         j;
	unsigned int lnum;
	alight_t    lighting;
	entity_t   *ent;

	// FIXME: remove and do real lighting
	float       lightvec[3] = { -1, 0, 0 };
	vec3_t      dist;
	float       add;
	float       minlight;

	if (!r_drawentities->int_val)
		return;

	for (ent = r_ent_queue; ent; ent = ent->next) {
		currententity = ent;

		VectorCopy (Transform_GetWorldPosition (currententity->transform),
					r_entorigin);
		switch (currententity->renderer.model->type) {
			case mod_sprite:
				VectorSubtract (r_origin, r_entorigin, modelorg);
				sw32_R_DrawSprite ();
				break;

			case mod_alias:
			case mod_iqm:
				VectorSubtract (r_origin, r_entorigin, modelorg);

				minlight = max (currententity->renderer.min_light,
								currententity->renderer.model->min_light);

				// see if the bounding box lets us trivially reject, also
				// sets trivial accept status
				currententity->visibility.trivial_accept = 0;	//FIXME
				if (currententity->renderer.model->type == mod_iqm//FIXME
					|| sw32_R_AliasCheckBBox ()) {
					// 128 instead of 255 due to clamping below
					j = max (R_LightPoint (&r_worldentity.renderer.model->brush,
										   r_entorigin),
							 minlight * 128);

					lighting.ambientlight = j;
					lighting.shadelight = j;

					lighting.plightvec = lightvec;

					for (lnum = 0; lnum < r_maxdlights; lnum++) {
						if (r_dlights[lnum].die >= vr_data.realtime) {
							VectorSubtract (r_entorigin,
											r_dlights[lnum].origin, dist);
							add = r_dlights[lnum].radius - VectorLength (dist);

							if (add > 0)
								lighting.ambientlight += add;
						}
					}

					// clamp lighting so it doesn't overbright as much
					if (lighting.ambientlight > 128)
						lighting.ambientlight = 128;
					if (lighting.ambientlight + lighting.shadelight > 192)
						lighting.shadelight = 192 - lighting.ambientlight;

					if (currententity->renderer.model->type == mod_iqm)
						sw32_R_IQMDrawModel (&lighting);
					else
						sw32_R_AliasDrawModel (&lighting);
				}

				break;

			default:
				break;
		}
	}
}

static void
R_DrawViewModel (void)
{
	// FIXME: remove and do real lighting
	float       lightvec[3] = { -1, 0, 0 };
	int         j;
	unsigned int lnum;
	vec3_t      dist;
	float       add;
	float       minlight;
	dlight_t   *dl;

	if (vr_data.inhibit_viewmodel || !r_drawviewmodel->int_val
		|| !r_drawentities->int_val)
		return;

	currententity = vr_data.view_model;
	if (!currententity->renderer.model)
		return;

	VectorCopy (Transform_GetWorldPosition (currententity->transform),
				r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	VectorCopy (vup, viewlightvec);
	VectorNegate (viewlightvec, viewlightvec);

	minlight = max (currententity->renderer.min_light,
					currententity->renderer.model->min_light);

	j = max (R_LightPoint (&r_worldentity.renderer.model->brush,
						   r_entorigin), minlight * 128);

	r_viewlighting.ambientlight = j;
	r_viewlighting.shadelight = j;

	// add dynamic lights
	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		dl = &r_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < vr_data.realtime)
			continue;

		VectorSubtract (r_entorigin, dl->origin, dist);
		add = dl->radius - VectorLength (dist);
		if (add > 0)
			r_viewlighting.ambientlight += add;
	}

	// clamp lighting so it doesn't overbright as much
	if (r_viewlighting.ambientlight > 128)
		r_viewlighting.ambientlight = 128;
	if (r_viewlighting.ambientlight + r_viewlighting.shadelight > 192)
		r_viewlighting.shadelight = 192 - r_viewlighting.ambientlight;

	r_viewlighting.plightvec = lightvec;

	sw32_R_AliasDrawModel (&r_viewlighting);
}

static int
R_BmodelCheckBBox (model_t *clmodel, float *minmaxs)
{
	int         i, *pindex, clipflags;
	vec3_t      acceptpt, rejectpt;
	double      d;
	mat4f_t     mat;

	clipflags = 0;

	Transform_GetWorldMatrix (currententity->transform, mat);
	if (mat[0][0] != 1 || mat[1][1] != 1 || mat[2][2] != 1) {
		for (i = 0; i < 4; i++) {
			d = DotProduct (mat[3], sw32_view_clipplanes[i].normal);
			d -= sw32_view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1 << i);
		}
	} else {
		for (i = 0; i < 4; i++) {
			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the
			// sign bit of the floating point values

			pindex = sw32_pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct (rejectpt, sw32_view_clipplanes[i].normal);
			d -= sw32_view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3 + 0]];
			acceptpt[1] = minmaxs[pindex[3 + 1]];
			acceptpt[2] = minmaxs[pindex[3 + 2]];

			d = DotProduct (acceptpt, sw32_view_clipplanes[i].normal);
			d -= sw32_view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1 << i);
		}
	}

	return clipflags;
}

static void
R_DrawBEntitiesOnList (void)
{
	int         j, clipflags;
	unsigned int k;
	vec3_t      oldorigin;
	vec3_t      origin;
	model_t    *clmodel;
	float       minmaxs[6];
	entity_t   *ent;

	if (!r_drawentities->int_val)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;

	for (ent = r_ent_queue; ent; ent = ent->next) {
		currententity = ent;

		VectorCopy (Transform_GetWorldPosition (currententity->transform),
					origin);
		switch (currententity->renderer.model->type) {
			case mod_brush:
				clmodel = currententity->renderer.model;

				// see if the bounding box lets us trivially reject, also
				// sets trivial accept status
				for (j = 0; j < 3; j++) {
					minmaxs[j] = origin[j] + clmodel->mins[j];
					minmaxs[3 + j] = origin[j] + clmodel->maxs[j];
				}

				clipflags = R_BmodelCheckBBox (clmodel, minmaxs);

				if (clipflags != BMODEL_FULLY_CLIPPED) {
					mod_brush_t *brush = &clmodel->brush;
					VectorCopy (origin, r_entorigin);
					VectorSubtract (r_origin, r_entorigin, modelorg);

					// FIXME: is this needed?
					VectorCopy (modelorg, sw32_r_worldmodelorg);
					r_pcurrentvertbase = brush->vertexes;

					// FIXME: stop transforming twice
					sw32_R_RotateBmodel ();

					// calculate dynamic lighting for bmodel if it's not an
					// instanced model
					if (brush->firstmodelsurface != 0) {
						vec3_t      lightorigin;

						for (k = 0; k < r_maxdlights; k++) {
							if ((r_dlights[k].die < vr_data.realtime) ||
								(!r_dlights[k].radius)) continue;

							VectorSubtract (r_dlights[k].origin, origin,
											lightorigin);
							R_RecursiveMarkLights (brush, lightorigin,
												   &r_dlights[k], k,
												   brush->nodes
										+ brush->hulls[0].firstclipnode);
						}
					}
					// if the driver wants polygons, deliver those.
					// Z-buffering is on at this point, so no clipping to the
					// world tree is needed, just frustum clipping
					if (sw32_r_drawpolys | sw32_r_drawculledpolys) {
						sw32_R_ZDrawSubmodelPolys (clmodel);
					} else {
						if (currententity->visibility.topnode) {
							mnode_t    *topnode
								= currententity->visibility.topnode;

							if (topnode->contents >= 0) {
								// not a leaf; has to be clipped to the world
								// BSP
								sw32_r_clipflags = clipflags;
								sw32_R_DrawSolidClippedSubmodelPolygons (clmodel);
							} else {
								// falls entirely in one leaf, so we just put
								// all the edges in the edge list and let 1/z
								// sorting handle drawing order
								sw32_R_DrawSubmodelPolygons (clmodel, clipflags);
							}
						}
					}

					// put back world rotation and frustum clipping
					// FIXME: sw32_R_RotateBmodel should just work off base_vxx
					VectorCopy (base_vpn, vpn);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					VectorCopy (oldorigin, modelorg);
					sw32_R_TransformFrustum ();
				}
				break;
			default:
				break;
		}
	}

	insubmodel = false;
}

static void
R_PrintDSpeeds (void)
{
	float       ms, dp_time, r_time2, rw_time, db_time, se_time, de_time,

		dv_time;

	r_time2 = Sys_DoubleTime ();

	dp_time = (dp_time2 - dp_time1) * 1000;
	rw_time = (rw_time2 - rw_time1) * 1000;
	db_time = (db_time2 - db_time1) * 1000;
	se_time = (se_time2 - se_time1) * 1000;
	de_time = (de_time2 - de_time1) * 1000;
	dv_time = (dv_time2 - dv_time1) * 1000;
	ms = (r_time2 - r_time1) * 1000;

	Sys_Printf ("%3i %4.1fp %3iw %4.1fb %3is %4.1fe %4.1fv\n",
				(int) ms, dp_time, (int) rw_time, db_time, (int) se_time,
				de_time, dv_time);
}

static void
R_EdgeDrawing (void)
{
	edge_t      ledges[NUMSTACKEDGES +
					   ((CACHE_SIZE - 1) / sizeof (edge_t)) + 1];
	surf_t      lsurfs[NUMSTACKSURFACES +
					   ((CACHE_SIZE - 1) / sizeof (surf_t)) + 1];

	if (sw32_auxedges) {
		sw32_r_edges = sw32_auxedges;
	} else {
		sw32_r_edges = (edge_t *)
			(((intptr_t) &ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack) {
		sw32_surfaces = (surf_t *)
			(((intptr_t) &lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		sw32_surf_max = &sw32_surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		sw32_surfaces--;
	}

	sw32_R_BeginEdgeFrame ();

	if (r_dspeeds->int_val) {
		rw_time1 = Sys_DoubleTime ();
	}

	sw32_R_RenderWorld ();

	if (sw32_r_drawculledpolys)
		sw32_R_ScanEdges ();

	// only the world can be drawn back to front with no z reads or compares,
	// just z writes, so have the driver turn z compares on now
	sw32_D_TurnZOn ();

	if (r_dspeeds->int_val) {
		rw_time2 = Sys_DoubleTime ();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds->int_val) {
		db_time2 = Sys_DoubleTime ();
		se_time1 = db_time2;
	}

	if (!r_dspeeds->int_val) {
		VID_UnlockBuffer ();
		S_ExtraUpdate ();		// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}

	if (!(sw32_r_drawpolys | sw32_r_drawculledpolys))
		sw32_R_ScanEdges ();
}

// LordHavoc: took out of stack and made 4x size for 32bit capacity
static byte warpbuffer[WARP_WIDTH * WARP_HEIGHT * 4];

/*
	R_RenderView

	r_refdef must be set before the first call
*/
static void
R_RenderView_ (void)
{
	if (r_norefresh->int_val)
		return;

	sw32_r_warpbuffer = warpbuffer;

	if (r_timegraph->int_val || r_speeds->int_val || r_dspeeds->int_val)
		r_time1 = Sys_DoubleTime ();

	sw32_R_SetupFrame ();

#ifdef PASSAGES
	SetVisibilityByPassages ();
#else
	R_MarkLeaves ();				// done here so we know if we're in water
#endif
	R_PushDlights (vec3_origin);

	if (!r_worldentity.renderer.model)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (!r_dspeeds->int_val) {
		VID_UnlockBuffer ();
		S_ExtraUpdate ();		// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}

	R_EdgeDrawing ();

	if (!r_dspeeds->int_val) {
		VID_UnlockBuffer ();
		S_ExtraUpdate ();		// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}

	if (r_dspeeds->int_val) {
		se_time2 = Sys_DoubleTime ();
		de_time1 = se_time2;
	}

	R_DrawEntitiesOnList ();

	if (r_dspeeds->int_val) {
		de_time2 = Sys_DoubleTime ();
		dv_time1 = de_time2;
	}

	R_DrawViewModel ();

	if (r_dspeeds->int_val) {
		dv_time2 = Sys_DoubleTime ();
		dp_time1 = Sys_DoubleTime ();
	}

	sw32_R_DrawParticles ();

	if (r_dspeeds->int_val)
		dp_time2 = Sys_DoubleTime ();

	if (sw32_r_dowarp)
		sw32_D_WarpScreen ();

	if (r_timegraph->int_val)
		R_TimeGraph ();

	if (r_zgraph->int_val)
		R_ZGraph ();

	if (r_aliasstats->int_val)
		sw32_R_PrintAliasStats ();

	if (r_speeds->int_val)
		sw32_R_PrintTimes ();

	if (r_dspeeds->int_val)
		R_PrintDSpeeds ();

	if (r_reportsurfout->int_val && sw32_r_outofsurfaces)
		Sys_Printf ("Short %d surfaces\n", sw32_r_outofsurfaces);

	if (r_reportedgeout->int_val && sw32_r_outofedges)
		Sys_Printf ("Short roughly %d edges\n", sw32_r_outofedges * 2 / 3);
}

void
sw32_R_RenderView (void)
{
	int         dummy;
	int         delta;

	delta = (byte *) & dummy - r_stack_start;
	if (delta < -10000 || delta > 10000)
		Sys_Error ("R_RenderView: called without enough stack");

	if (Hunk_LowMark () & 3)
		Sys_Error ("Hunk is missaligned");

	if ((intptr_t) (&dummy) & 3)
		Sys_Error ("Stack is missaligned");

	if ((intptr_t) (&sw32_r_warpbuffer) & 3)
		Sys_Error ("Globals are missaligned");

	R_RenderView_ ();
}

void
sw32_R_InitTurb (void)
{
	int         i;

	for (i = 0; i < MAXWIDTH; i++) {
		sw32_sintable[i] = AMP + sin (i * 3.14159 * 2 / CYCLE) * AMP;
		sw32_intsintable[i] = AMP2 + sin (i * 3.14159 * 2 / CYCLE) * AMP2;
		// AMP2 not 20
	}
}

void
sw32_R_ClearState (void)
{
	R_ClearEfrags ();
	R_ClearDlights ();
	sw32_R_ClearParticles ();
}
