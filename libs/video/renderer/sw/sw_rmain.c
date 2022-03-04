/*
	r_main.c

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
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

#ifdef PIC
# undef USE_INTEL_ASM //XXX asm pic hack
#endif

void       *colormap;
static vec3_t      viewlightvec;
static alight_t    r_viewlighting = { 128, 192, viewlightvec };
int         r_numallocatededges;
qboolean    r_drawpolys;
qboolean    r_drawculledpolys;
qboolean    r_worldpolysbacktofront;
qboolean    r_recursiveaffinetriangles = true;
int         r_pixbytes = 1;
float       r_aliasuvscale = 1.0;
int         r_outofsurfaces;
int         r_outofedges;

qboolean    r_dowarp, r_dowarpold, r_viewchanged;

int         c_surf;
int         r_maxsurfsseen, r_maxedgesseen;
static int  r_cnumsurfs;
static qboolean    r_surfsonstack;
int         r_clipflags;

byte       *r_warpbuffer;

static byte       *r_stack_start;

// screen size info
float       xcenter, ycenter;
float       xscale, yscale;
float       xscaleinv, yscaleinv;
float       xscaleshrink, yscaleshrink;
float       aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int         screenwidth;

float       pixelAspect;
static float       screenAspect;
static float       verticalFieldOfView;
static float       xOrigin, yOrigin;

plane_t     screenedge[4];

// refresh flags
int         r_polycount;
int         r_drawnpolycount;

int        *pfrustum_indexes[4];
int         r_frustum_indexes[4 * 6];

float       r_aliastransition, r_resfudge;

static float dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
static float se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

void
sw_R_Init (void)
{
	int         dummy;

	r_ent_queue = EntQueue_New (mod_num_types);

	// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte *) & dummy;

	R_Init_Cvars ();
	R_Particles_Init_Cvars ();

	Draw_Init ();
	SCR_Init ();
	R_SetFPCW ();
#ifdef USE_INTEL_ASM
	R_InitVars ();
#endif

	R_InitTurb ();

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f, "Tests the current "
					"refresh rate for the current location");
	Cmd_AddCommand ("loadsky", R_LoadSky_f, "Load a skybox");

	Cvar_SetValue (r_maxedges, (float) NUMSTACKEDGES);
	Cvar_SetValue (r_maxsurfs, (float) NUMSTACKSURFACES);

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
		view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
		view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	Sys_MakeCodeWriteable ((long) R_EdgeCodeStart,
						   (long) R_EdgeCodeEnd - (long) R_EdgeCodeStart);
#endif // USE_INTEL_ASM
	D_Init ();

	Skin_Init ();
}

void
R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
	mod_brush_t *brush = &worldmodel->brush;

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.renderer.model = worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	for (unsigned i = 0; i < brush->modleafs; i++)
		brush->leafs[i].efrags = NULL;

	if (brush->skytexture)
		R_InitSky (brush->skytexture);

	// Force a vis update
	r_viewleaf = NULL;
	R_MarkLeaves ();

	R_ClearParticles ();

	r_cnumsurfs = r_maxsurfs->int_val;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES) {
		surfaces = Hunk_AllocName (0, r_cnumsurfs * sizeof (surf_t),
								   "surfaces");

		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	} else {
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = r_maxedges->int_val;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES) {
		auxedges = NULL;
	} else {
		auxedges = Hunk_AllocName (0, r_numallocatededges * sizeof (edge_t),
								   "edges");
	}

	r_dowarpold = false;
	r_viewchanged = false;
}

/*
	R_ViewChanged

	Called every time the vid structure or r_refdef changes.
	Guaranteed to be called before the first refresh
*/
void
R_ViewChanged (void)
{
	int         i;
	float       res_scale;

	r_viewchanged = true;

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

	r_refdef.aliasvrect.x = (int) (r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int) (r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int) (r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int) (r_refdef.vrect.height *
										r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
		r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
		r_refdef.aliasvrect.height;

	pixelAspect = 1;//FIXME vid.aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;

	screenAspect = r_refdef.vrect.width * pixelAspect / r_refdef.vrect.height;
	// 320*200 1.0 pixelAspect = 1.6 screenAspect
	// 320*240 1.0 pixelAspect = 1.3333 screenAspect
	// proper 320*200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

	// values for perspective projection
	// if math were exact, the values would range from 0.5 to to range+0.5
	// hopefully they wll be in the 0.000001 to range+.999999 and truncate
	// the polygon rasterization will never render in the first row or column
	// but will definately render in the [range] row and column, so adjust the
	// buffer origin to get an exact edge to edge fill
	xcenter = ((float) r_refdef.vrect.width * XCENTERING) +
		r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float) r_refdef.vrect.height * YCENTERING) +
		r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width - 6) / r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink * pixelAspect;

	// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin *
									  r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;

	// right side clip
	screenedge[1].normal[0] = 1.0 / ((1.0 - xOrigin) *
									 r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;

	// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin * verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;

	// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0 - yOrigin) * verticalFieldOfView);
	screenedge[3].normal[2] = 1;
	screenedge[3].type = PLANE_ANYZ;

	for (i = 0; i < 4; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrt ((double) (r_refdef.vrect.width * r_refdef.vrect.height) /
					  (320.0 * 152.0)) * (2.0 /
										  r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase->value * res_scale;
	r_resfudge = r_aliastransadj->value * res_scale;

// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	Sys_MakeCodeWriteable ((long) R_Surf8Start,
						   (long) R_Surf8End - (long) R_Surf8Start);
	colormap = vid.colormap8;
	R_SurfPatch ();
#endif // USE_INTEL_ASM

	D_ViewChanged ();
}

static inline void
draw_sprite_entity (entity_t *ent)
{
	VectorSubtract (r_origin, r_entorigin, modelorg);
	R_DrawSprite ();
}

static inline void
setup_lighting (alight_t *lighting)
{
	float       minlight = 0;
	int         j;
	// FIXME: remove and do real lighting
	vec3_t      dist;
	float       add;
	float       lightvec[3] = { -1, 0, 0 };

	minlight = max (currententity->renderer.model->min_light,
					currententity->renderer.min_light);

	// 128 instead of 255 due to clamping below
	j = max (R_LightPoint (&r_worldentity.renderer.model->brush, r_entorigin),
			 minlight * 128);

	lighting->ambientlight = j;
	lighting->shadelight = j;

	lighting->plightvec = lightvec;

	for (unsigned lnum = 0; lnum < r_maxdlights; lnum++) {
		if (r_dlights[lnum].die >= vr_data.realtime) {
			VectorSubtract (r_entorigin, r_dlights[lnum].origin, dist);
			add = r_dlights[lnum].radius - VectorLength (dist);

			if (add > 0)
				lighting->ambientlight += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (lighting->ambientlight > 128)
		lighting->ambientlight = 128;
	if (lighting->ambientlight + lighting->shadelight > 192)
		lighting->shadelight = 192 - lighting->ambientlight;
}

static inline void
draw_alias_entity (entity_t *ent)
{
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	currententity->visibility.trivial_accept = 0;	//FIXME
	if (R_AliasCheckBBox ()) {
		alight_t    lighting;
		setup_lighting (&lighting);
		R_AliasDrawModel (&lighting);
	}
}

static inline void
draw_iqm_entity (entity_t *ent)
{
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	currententity->visibility.trivial_accept = 0;	//FIXME

	alight_t    lighting;
	setup_lighting (&lighting);
	R_IQMDrawModel (&lighting);
}

static void
R_DrawEntitiesOnList (void)
{
	if (!r_drawentities->int_val)
		return;

#define RE_LOOP(type_name) \
	do { \
		for (size_t i = 0; i < r_ent_queue->ent_queues[mod_##type_name].size; \
			 i++) { \
			entity_t   *ent = r_ent_queue->ent_queues[mod_##type_name].a[i]; \
			VectorCopy (Transform_GetWorldPosition (ent->transform), \
						r_entorigin); \
			currententity = ent; \
			draw_##type_name##_entity (ent); \
		} \
	} while (0)

	RE_LOOP (alias);
	RE_LOOP (iqm);
	RE_LOOP (sprite);
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

	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel->int_val
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

	R_AliasDrawModel (&r_viewlighting);
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
			d = DotProduct (mat[3], view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

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

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct (rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3 + 0]];
			acceptpt[1] = minmaxs[pindex[3 + 1]];
			acceptpt[2] = minmaxs[pindex[3 + 2]];

			d = DotProduct (acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

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

	if (!r_drawentities->int_val)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;

	for (size_t i = 0; i < r_ent_queue->ent_queues[mod_brush].size; i++) {
		entity_t   *ent = r_ent_queue->ent_queues[mod_brush].a[i];
		currententity = ent;

		VectorCopy (Transform_GetWorldPosition (ent->transform), origin);
		clmodel = ent->renderer.model;

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
			VectorCopy (modelorg, r_worldmodelorg);
			r_pcurrentvertbase = brush->vertexes;

			// FIXME: stop transforming twice
			R_RotateBmodel ();

			// calculate dynamic lighting for bmodel if it's not an
			// instanced model
			if (brush->firstmodelsurface != 0) {
				vec3_t      lightorigin;

				for (k = 0; k < r_maxdlights; k++) {
					if ((r_dlights[k].die < vr_data.realtime) ||
						(!r_dlights[k].radius)) {
						continue;
					}

					VectorSubtract (r_dlights[k].origin, origin, lightorigin);
					R_RecursiveMarkLights (brush, lightorigin,
										   &r_dlights[k], k,
										   brush->nodes
								+ brush->hulls[0].firstclipnode);
				}
			}
			// if the driver wants polygons, deliver those.
			// Z-buffering is on at this point, so no clipping to the
			// world tree is needed, just frustum clipping
			if (r_drawpolys | r_drawculledpolys) {
				R_ZDrawSubmodelPolys (clmodel);
			} else {
				if (ent->visibility.topnode) {
					mnode_t    *topnode = ent->visibility.topnode;

					if (topnode->contents >= 0) {
						// not a leaf; has to be clipped to the world
						// BSP
						r_clipflags = clipflags;
						R_DrawSolidClippedSubmodelPolygons (clmodel);
					} else {
						// falls entirely in one leaf, so we just put
						// all the edges in the edge list and let 1/z
						// sorting handle drawing order
						R_DrawSubmodelPolygons (clmodel, clipflags);
					}
				}
			}

			// put back world rotation and frustum clipping
			// FIXME: R_RotateBmodel should just work off base_vxx
			VectorCopy (base_vpn, vpn);
			VectorCopy (base_vup, vup);
			VectorCopy (base_vright, vright);
			VectorCopy (base_modelorg, modelorg);
			VectorCopy (oldorigin, modelorg);
			R_TransformFrustum ();
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

	if (auxedges) {
		r_edges = auxedges;
	} else {
		r_edges = (edge_t *)
			(((intptr_t) &ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack) {
		surfaces = (surf_t *)
			(((intptr_t) &lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	}

	R_BeginEdgeFrame ();

	if (r_dspeeds->int_val) {
		rw_time1 = Sys_DoubleTime ();
	}

	R_RenderWorld ();

	if (r_drawculledpolys)
		R_ScanEdges ();

	// only the world can be drawn back to front with no z reads or compares,
	// just z writes, so have the driver turn z compares on now
	D_TurnZOn ();

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

	if (!(r_drawpolys | r_drawculledpolys))
		R_ScanEdges ();
}

/*
	R_RenderView

	r_refdef must be set before the first call
*/
static void
R_RenderView_ (void)
{
	byte        warpbuffer[WARP_WIDTH * WARP_HEIGHT];

	if (r_norefresh->int_val)
		return;
	if (!r_worldentity.renderer.model) {
		return;
	}

	r_warpbuffer = warpbuffer;

	if (r_timegraph->int_val || r_speeds->int_val || r_dspeeds->int_val)
		r_time1 = Sys_DoubleTime ();

	R_SetupFrame ();

	R_MarkLeaves ();				// done here so we know if we're in water

	R_PushDlights (vec3_origin);

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	R_LowFPPrecision ();

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

	R_DrawParticles ();

	if (r_dspeeds->int_val)
		dp_time2 = Sys_DoubleTime ();

	if (r_dowarp)
		D_WarpScreen ();

	if (r_timegraph->int_val)
		R_TimeGraph ();

	if (r_zgraph->int_val)
		R_ZGraph ();

	if (r_aliasstats->int_val)
		R_PrintAliasStats ();

	if (r_speeds->int_val)
		R_PrintTimes ();

	if (r_dspeeds->int_val)
		R_PrintDSpeeds ();

	if (r_reportsurfout->int_val && r_outofsurfaces)
		Sys_Printf ("Short %d surfaces\n", r_outofsurfaces);

	if (r_reportedgeout->int_val && r_outofedges)
		Sys_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);

	// back to high floating-point precision
	R_HighFPPrecision ();
}

//XXX FIXME static void R_RenderViewFishEye (void);

void
R_RenderView (void)
{
	int         dummy;
	int         delta;

	delta = (byte *) & dummy - r_stack_start;
	if (delta < -10000 || delta > 10000)
		Sys_Error ("R_RenderView: called without enough stack");

	if (Hunk_LowMark (0) & 3)
		Sys_Error ("Hunk is missaligned");

	if ((intptr_t) (&dummy) & 3)
		Sys_Error ("Stack is missaligned");

	if ((intptr_t) (&r_warpbuffer) & 3)
		Sys_Error ("Globals are missaligned");
	R_RenderView_ ();
/*XXX FIXME
	if (!scr_fisheye->int_val)
		R_RenderView_ ();
	else
		R_RenderViewFishEye ();*/
}

void
R_InitTurb (void)
{
	int         i;

	for (i = 0; i < (SIN_BUFFER_SIZE); i++) {
		sintable[i] = AMP + sin (i * 3.14159 * 2 / CYCLE) * AMP;
		intsintable[i] = AMP2 + sin (i * 3.14159 * 2 / CYCLE) * AMP2;
		// AMP2 not 20
	}
}
/*XXX FIXME
#define BOX_FRONT  0
#define BOX_BEHIND 2
#define BOX_LEFT   3
#define BOX_RIGHT  1
#define BOX_TOP    4
#define BOX_BOTTOM 5

#define DEG(x) (x / M_PI * 180.0)
#define RAD(x) (x * M_PI / 180.0)

struct my_coords
{
	double x, y, z;
};

struct my_angles
{
	double yaw, pitch, roll;
};

static void
x_rot (struct my_coords *c, double pitch)
{
	double nx, ny, nz;

	nx = c->x;
	ny = (c->y * cos(pitch)) - (c->z * sin(pitch));
	nz = (c->y * sin(pitch)) + (c->z * cos(pitch));

	c->x = nx; c->y = ny; c->z = nz;
}

static void
y_rot (struct my_coords *c, double yaw)
{
	double nx, ny, nz;

	nx = (c->x * cos(yaw)) - (c->z * sin(yaw));
	ny = c->y;
	nz = (c->x * sin(yaw)) + (c->z * cos(yaw));

	c->x = nx; c->y = ny; c->z = nz;
}

static void
z_rot (struct my_coords *c, double roll)
{
	double nx, ny, nz;

	nx = (c->x * cos(roll)) - (c->y * sin(roll));
	ny = (c->x * sin(roll)) + (c->y * cos(roll));
	nz = c->z;

	c->x = nx; c->y = ny; c->z = nz;
}

static void
my_get_angles (struct my_coords *in_o, struct my_coords *in_u, struct my_angles *a)
{
	double rad_yaw, rad_pitch;
	struct my_coords o, u;

	a->pitch = 0.0;
	a->yaw = 0.0;
	a->roll = 0.0;

	// make a copy of the coords
	o.x = in_o->x; o.y = in_o->y; o.z = in_o->z;
	u.x = in_u->x; u.y = in_u->y; u.z = in_u->z;

	// special case when looking straight up or down
	if ((o.x == 0.0) && (o.z == 0.0)) {
		a->yaw   = 0.0;
		if (o.y > 0.0) { a->pitch = -90.0; a->roll = 180.0 - DEG(atan2(u.x, u.z)); } // down
		else           { a->pitch =  90.0; a->roll = DEG(atan2(u.x, u.z)); } // up
		return;
	}

	// get yaw angle and then rotate o and u so that yaw = 0
	rad_yaw = atan2 (-o.x, o.z);
	a->yaw  = DEG (rad_yaw);

	y_rot (&o, -rad_yaw);
	y_rot (&u, -rad_yaw);

	// get pitch and then rotate o and u so that pitch = 0
	rad_pitch = atan2 (-o.y, o.z);
	a->pitch  = DEG (rad_pitch);

	x_rot (&o, -rad_pitch);
	x_rot (&u, -rad_pitch);

	// get roll
	a->roll = DEG (-atan2(u.x, u.y));
}

static void
get_ypr (double yaw, double pitch, double roll, int side, struct my_angles *a)
{
	struct my_coords o, u;

	// get 'o' (observer) and 'u' ('this_way_up') depending on box side
	switch(side) {
		case BOX_FRONT:
			o.x =  0.0; o.y =  0.0; o.z =  1.0;
			u.x =  0.0; u.y =  1.0; u.z =  0.0;
			break;
		case BOX_BEHIND:
			o.x =  0.0; o.y =  0.0; o.z = -1.0;
			u.x =  0.0; u.y =  1.0; u.z =  0.0;
			break;
		case BOX_LEFT:
			o.x = -1.0; o.y =  0.0; o.z =  0.0;
			u.x = -1.0; u.y =  1.0; u.z =  0.0;
			break;
		case BOX_RIGHT:
			o.x =  1.0; o.y =  0.0; o.z =  0.0;
			u.x =  0.0; u.y =  1.0; u.z =  0.0;
			break;
		case BOX_TOP:
			o.x =  0.0; o.y = -1.0; o.z =  0.0;
			u.x =  0.0; u.y =  0.0; u.z = -1.0;
			break;
		case BOX_BOTTOM:
			o.x =  0.0; o.y =  1.0; o.z =  0.0;
			u.x =  0.0; u.y =  0.0; u.z = -1.0;
			break;
	}
	z_rot (&o, roll); z_rot (&u, roll);
	x_rot (&o, pitch); x_rot (&u, pitch);
	y_rot (&o, yaw); y_rot (&u, yaw);

	my_get_angles (&o, &u, a);

	// normalise angles
	while (a->yaw   <   0.0) a->yaw   += 360.0;
	while (a->yaw   > 360.0) a->yaw   -= 360.0;
	while (a->pitch <   0.0) a->pitch += 360.0;
	while (a->pitch > 360.0) a->pitch -= 360.0;
	while (a->roll  <   0.0) a->roll  += 360.0;
	while (a->roll  > 360.0) a->roll  -= 360.0;
}

static void
fisheyelookuptable (byte **buf, int width, int height, byte *scrp, double fov)
{
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			double dx = x-width/2;
			double dy = -(y-height/2);
			double yaw = sqrt(dx*dx+dy*dy)*fov/((double)width);
			double roll = -atan2(dy, dx);
			double sx = sin(yaw) * cos(roll);
			double sy = sin(yaw) * sin(roll);
			double sz = cos(yaw);

			// determine which side of the box we need
			double abs_x = fabs(sx);
			double abs_y = fabs(sy);
			double abs_z = fabs(sz);
			int side;
			double xs = 0, ys = 0;
			if (abs_x > abs_y) {
				if (abs_x > abs_z) { side = ((sx > 0.0) ? BOX_RIGHT : BOX_LEFT);   }
				else               { side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND); }
			} else {
				if (abs_y > abs_z) { side = ((sy > 0.0) ? BOX_TOP   : BOX_BOTTOM); }
				else               { side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND); }
			}

			#define RC(x) ((x / 2.06) + 0.5)
			#define R2(x) ((x / 2.03) + 0.5)

			// scale up our vector [x,y,z] to the box
			switch(side) {
				case BOX_FRONT:  xs = RC( sx /  sz); ys = R2( sy /  sz); break;
				case BOX_BEHIND: xs = RC(-sx / -sz); ys = R2( sy / -sz); break;
				case BOX_LEFT:   xs = RC( sz / -sx); ys = R2( sy / -sx); break;
				case BOX_RIGHT:  xs = RC(-sz /  sx); ys = R2( sy /  sx); break;
				case BOX_TOP:    xs = RC( sx /  sy); ys = R2( sz / -sy); break; //bot
				case BOX_BOTTOM: xs = RC(-sx /  sy); ys = R2( sz / -sy); break; //top??
			}

			if (xs <  0.0) xs = 0.0;
			if (xs >= 1.0) xs = 0.999;
			if (ys <  0.0) ys = 0.0;
			if (ys >= 1.0) ys = 0.999;
			*buf++ = scrp+(((int)(xs*(double)width))+
						   ((int)(ys*(double)height))*width)+
						   side*width*height;
		}
	}
}

static void
rendercopy (void *dest)
{
	void *p = vid.buffer;
	// XXX
	vid.buffer = dest;
	R_RenderView_ ();
	vid.buffer = p;
}

static void
renderside (byte* bufs, double yaw, double pitch, double roll, int side)
{
	struct my_angles a;

	get_ypr (RAD(yaw), RAD(pitch), RAD(roll), side, &a);
	if (side == BOX_RIGHT) { a.roll = -a.roll; a.pitch = -a.pitch; }
	if (side == BOX_LEFT)  { a.roll = -a.roll; a.pitch = -a.pitch; }
	if (side == BOX_TOP)   { a.yaw += 180.0; a.pitch = 180.0 - a.pitch; }
	r_refdef.viewangles[YAW] = a.yaw;
	r_refdef.viewangles[PITCH] = a.pitch;
	r_refdef.viewangles[ROLL] = a.roll;
	rendercopy (bufs);
}

static void
renderlookup (byte **offs, byte* bufs)
{
	byte       *p = (byte*)vid.buffer;
	unsigned   x, y;
	for (y = 0; y < vid.height; y++) {
		for (x = 0; x < vid.width; x++, offs++)
		    p[x] = **offs;
		p += vid.rowbytes;
	}
}

static void
R_RenderViewFishEye (void)
{
	int width = vid.width; //r_refdef.vrect.width;
	int height = vid.height; //r_refdef.vrect.height;
	int scrsize = width*height;
	int fov = scr_ffov->int_val;
	int views = scr_fviews->int_val;
	double yaw = r_refdef.viewangles[YAW];
	double pitch = r_refdef.viewangles[PITCH];
	double roll = 0; //r_refdef.viewangles[ROLL];
	static int pwidth = -1;
	static int pheight = -1;
	static int pfov = -1;
	static int pviews = -1;
	static byte *scrbufs = NULL;
	static byte **offs = NULL;

	if (fov < 1) fov = 1;

	if (pwidth != width || pheight != height || pfov != fov) {
		if (scrbufs) free (scrbufs);
		if (offs) free (offs);
		scrbufs = malloc (scrsize*6); // front|right|back|left|top|bottom
		SYS_CHECKMEM (scrbufs);
		offs = malloc (scrsize*sizeof(byte*));
		SYS_CHECKMEM (offs);
		pwidth = width;
		pheight = height;
		pfov = fov;
		fisheyelookuptable (offs, width, height, scrbufs, ((double)fov)*M_PI/180.0);
	}

	if (views != pviews) {
		pviews = views;
		memset (scrbufs, 0, scrsize*6);
	}

	switch (views) {
		case 6:  renderside (scrbufs+scrsize*2, yaw, pitch, roll, BOX_BEHIND);
		case 5:  renderside (scrbufs+scrsize*5, yaw, pitch, roll, BOX_BOTTOM);
		case 4:  renderside (scrbufs+scrsize*4, yaw, pitch, roll, BOX_TOP);
		case 3:  renderside (scrbufs+scrsize*3, yaw, pitch, roll, BOX_LEFT);
		case 2:  renderside (scrbufs+scrsize,   yaw, pitch, roll, BOX_RIGHT);
		default: renderside (scrbufs,           yaw, pitch, roll, BOX_FRONT);
	}

	r_refdef.viewangles[YAW] = yaw;
	r_refdef.viewangles[PITCH] = pitch;
	r_refdef.viewangles[ROLL] = roll;
	renderlookup (offs, scrbufs);
}*/

void
R_ClearState (void)
{
	r_worldentity.renderer.model = 0;
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
