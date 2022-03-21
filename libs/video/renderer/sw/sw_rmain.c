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

vec3_t      vup, base_vup;
vec3_t      vfwd, base_vfwd;
vec3_t      vright, base_vright;
float       r_viewmatrix[3][4];

float       r_aliastransition, r_resfudge;

void
sw_R_Init (void)
{
	int         dummy;

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

	r_refdef.worldmodel = worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	for (unsigned i = 0; i < brush->modleafs; i++)
		brush->leafs[i].efrags = NULL;

	if (brush->skytexture)
		R_InitSky (brush->skytexture);

	// Force a vis update
	r_refdef.viewleaf = NULL;
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
	R_DrawSprite (ent);
}

static inline void
setup_lighting (entity_t *ent, alight_t *lighting)
{
	float       minlight = 0;
	int         j;
	// FIXME: remove and do real lighting
	vec3_t      dist;
	float       add;
	float       lightvec[3] = { -1, 0, 0 };

	minlight = max (ent->renderer.model->min_light, ent->renderer.min_light);

	// 128 instead of 255 due to clamping below
	j = max (R_LightPoint (&r_refdef.worldmodel->brush, r_entorigin),
			 minlight * 128);

	lighting->ambientlight = j;
	lighting->shadelight = j;

	VectorCopy (lightvec, lighting->lightvec);

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
	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	ent->visibility.trivial_accept = 0;	//FIXME
	if (R_AliasCheckBBox (ent)) {
		alight_t    lighting;
		setup_lighting (ent, &lighting);
		R_AliasDrawModel (ent, &lighting);
	}
}

static inline void
draw_iqm_entity (entity_t *ent)
{
	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	ent->visibility.trivial_accept = 0;	//FIXME

	alight_t    lighting;
	setup_lighting (ent, &lighting);
	R_IQMDrawModel (ent, &lighting);
}

void
R_DrawEntitiesOnList (entqueue_t *queue)
{
	if (!r_drawentities->int_val)
		return;

	R_LowFPPrecision ();
#define RE_LOOP(type_name) \
	do { \
		for (size_t i = 0; i < queue->ent_queues[mod_##type_name].size; \
			 i++) { \
			entity_t   *ent = queue->ent_queues[mod_##type_name].a[i]; \
			VectorCopy (Transform_GetWorldPosition (ent->transform), \
						r_entorigin); \
			draw_##type_name##_entity (ent); \
		} \
	} while (0)

	RE_LOOP (alias);
	RE_LOOP (iqm);
	RE_LOOP (sprite);

	R_HighFPPrecision ();
}

static void
R_DrawViewModel (void)
{
	// FIXME: remove and do real lighting
	int         j;
	unsigned int lnum;
	vec3_t      dist;
	float       add;
	float       minlight;
	dlight_t   *dl;
	entity_t   *viewent;
	alight_t    lighting;

	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel->int_val
		|| !r_drawentities->int_val)
		return;

	viewent = vr_data.view_model;
	if (!viewent->renderer.model)
		return;

	VectorCopy (Transform_GetWorldPosition (viewent->transform), r_entorigin);

	VectorNegate (vup, lighting.lightvec);

	minlight = max (viewent->renderer.min_light,
					viewent->renderer.model->min_light);

	j = max (R_LightPoint (&r_refdef.worldmodel->brush,
						   r_entorigin), minlight * 128);

	lighting.ambientlight = j;
	lighting.shadelight = j;

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
			lighting.ambientlight += add;
	}

	// clamp lighting so it doesn't overbright as much
	if (lighting.ambientlight > 128)
		lighting.ambientlight = 128;
	if (lighting.ambientlight + lighting.shadelight > 192)
		lighting.shadelight = 192 - lighting.ambientlight;

	R_AliasDrawModel (viewent, &lighting);
}

static int
R_BmodelCheckBBox (entity_t *ent, model_t *clmodel, float *minmaxs)
{
	int         i, *pindex, clipflags;
	vec3_t      acceptpt, rejectpt;
	double      d;
	mat4f_t     mat;

	clipflags = 0;

	Transform_GetWorldMatrix (ent->transform, mat);
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
R_DrawBrushEntitiesOnList (entqueue_t *queue)
{
	int         j, clipflags;
	unsigned int k;
	vec3_t      origin;
	model_t    *clmodel;
	float       minmaxs[6];

	if (!r_drawentities->int_val)
		return;

	insubmodel = true;

	for (size_t i = 0; i < queue->ent_queues[mod_brush].size; i++) {
		entity_t   *ent = queue->ent_queues[mod_brush].a[i];

		VectorCopy (Transform_GetWorldPosition (ent->transform), origin);
		clmodel = ent->renderer.model;

		// see if the bounding box lets us trivially reject, also
		// sets trivial accept status
		for (j = 0; j < 3; j++) {
			minmaxs[j] = origin[j] + clmodel->mins[j];
			minmaxs[3 + j] = origin[j] + clmodel->maxs[j];
		}

		clipflags = R_BmodelCheckBBox (ent, clmodel, minmaxs);

		if (clipflags != BMODEL_FULLY_CLIPPED) {
			mod_brush_t *brush = &clmodel->brush;
			VectorCopy (origin, r_entorigin);
			VectorSubtract (r_refdef.frame.position, r_entorigin, modelorg);

			r_pcurrentvertbase = brush->vertexes;

			// FIXME: stop transforming twice
			R_RotateBmodel (ent->transform);

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
				R_ZDrawSubmodelPolys (ent, clmodel);
			} else {
				if (ent->visibility.topnode) {
					mnode_t    *topnode = ent->visibility.topnode;

					if (topnode->contents >= 0) {
						// not a leaf; has to be clipped to the world
						// BSP
						r_clipflags = clipflags;
						R_DrawSolidClippedSubmodelPolygons (ent, clmodel, topnode);
					} else {
						// falls entirely in one leaf, so we just put
						// all the edges in the edge list and let 1/z
						// sorting handle drawing order
						R_DrawSubmodelPolygons (ent, clmodel, clipflags, topnode);
					}
				}
			}

			// put back world rotation and frustum clipping
			// FIXME: R_RotateBmodel should just work off base_vxx
			VectorCopy (base_vfwd, vfwd);
			VectorCopy (base_vup, vup);
			VectorCopy (base_vright, vright);
			VectorCopy (base_modelorg, modelorg);
			R_TransformFrustum ();
		}
	}

	insubmodel = false;
}

static void
R_EdgeDrawing (entqueue_t *queue)
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

	R_RenderWorld ();

	if (r_drawculledpolys)
		R_ScanEdges ();

	// only the world can be drawn back to front with no z reads or compares,
	// just z writes, so have the driver turn z compares on now
	D_TurnZOn ();

	R_DrawBrushEntitiesOnList (queue);

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
	if (!r_refdef.worldmodel) {
		return;
	}

	r_warpbuffer = warpbuffer;

	R_SetupFrame ();

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	R_LowFPPrecision ();

	R_EdgeDrawing (r_ent_queue);

	R_DrawViewModel ();

	if (r_dowarp)
		D_WarpScreen ();

	if (r_aliasstats->int_val)
		R_PrintAliasStats ();

	// back to high floating-point precision
	R_HighFPPrecision ();
}

static void R_RenderViewFishEye (void);

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
	if (!scr_fisheye->int_val)
		R_RenderView_ ();
	else
		R_RenderViewFishEye ();
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

#define BOX_FRONT  0
#define BOX_RIGHT  1
#define BOX_BEHIND 2
#define BOX_LEFT   3
#define BOX_TOP    4
#define BOX_BOTTOM 5

static mat4f_t box_rotations[] = {
	{ { 1, 0, 0, 0},		// front
	  { 0, 1, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ { 0,-1, 0, 0},		// right
	  { 1, 0, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ {-1, 0, 0, 0},		// back
	  { 0,-1, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ { 0, 1, 0, 0},		// left
	  {-1, 0, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ { 0, 0, 1, 0},		// top
	  { 0, 1, 0, 0},
	  {-1, 0, 0, 0},
	  { 0, 0, 0, 1} },
	{ { 0, 0,-1, 0},		// bottom
	  { 0, 1, 0, 0},
	  { 1, 0, 0, 0},
	  { 0, 0, 0, 1} },
};

static int sw_cube_map_size;

static void
fisheyelookuptable (byte **buf, int width, int height, byte *scrp, double fov)
{
	int x, y;
	int scrsize = sw_cube_map_size * sw_cube_map_size;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			double dx = x-width/2;
			double dy = -(y-height/2);
			double yaw = sqrt(dx*dx+dy*dy)*fov/((double)width);
			double roll = atan2(dy, dx);
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

			#define RC(x) ((x / 2) + 0.5)
			#define R2(x) ((x / 2) + 0.5)

			// scale up our vector [x,y,z] to the box
			switch(side) {
				case BOX_FRONT:  xs = RC( sx /  sz); ys = R2(-sy /  sz); break;
				case BOX_BEHIND: xs = RC(-sx / -sz); ys = R2(-sy / -sz); break;
				case BOX_LEFT:   xs = RC( sz / -sx); ys = R2(-sy / -sx); break;
				case BOX_RIGHT:  xs = RC(-sz /  sx); ys = R2(-sy /  sx); break;
				case BOX_TOP:    xs = RC( sx /  sy); ys = R2( sz /  sy); break;
				case BOX_BOTTOM: xs = RC( sx / -sy); ys = R2( sz /  sy); break;
			}

			if (xs <  0.0) xs = 0.0;
			if (xs >= 1.0) xs = 0.999;
			if (ys <  0.0) ys = 0.0;
			if (ys >= 1.0) ys = 0.999;
			int sxs = xs * sw_cube_map_size;
			int sys = ys * sw_cube_map_size;
			*buf++ = scrp+side * scrsize + sys * sw_cube_map_size + sxs;
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
renderside (byte *bufs, int side)
{
	mat4f_t     camera;
	mat4f_t     camera_inverse;
	mat4f_t     rotinv;

	memcpy (camera, r_refdef.camera, sizeof (camera));
	memcpy (camera_inverse, r_refdef.camera_inverse, sizeof (camera_inverse));
	mmulf (r_refdef.camera, camera, box_rotations[side]);
	mat4ftranspose (rotinv, box_rotations[side]);
	mmulf (r_refdef.camera_inverse, rotinv, camera_inverse);

	//FIXME see fixme in r_screen.c
	r_refdef.frame.mat[0] = -r_refdef.camera[1];
	r_refdef.frame.mat[1] =  r_refdef.camera[0];
	r_refdef.frame.mat[2] =  r_refdef.camera[2];
	r_refdef.frame.mat[3] =  r_refdef.camera[3];

	float fov_x = r_refdef.fov_x;
	float fov_y = r_refdef.fov_y;
	unsigned width = vid.width;
	unsigned height = vid.height;
	vrect_t rect = r_refdef.vrect;
	r_refdef.fov_x = r_refdef.fov_y = 90;
	r_refdef.vrect.x = r_refdef.vrect.y = 0;
	r_refdef.vrect.height = r_refdef.vrect.width = sw_cube_map_size;
	vid.rowbytes = vid.height = vid.width = sw_cube_map_size;
	R_ViewChanged ();

	rendercopy (bufs);

	vid.rowbytes = width;
	r_refdef.vrect = rect;
	vid.height = height;
	vid.width = width;
	r_refdef.fov_x = fov_x;
	r_refdef.fov_y = fov_y;

	memcpy (r_refdef.camera, camera, sizeof (camera));
	memcpy (r_refdef.camera_inverse, camera_inverse, sizeof (camera_inverse));
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
	sw_cube_map_size = vid.height;
	int width = vid.width; //r_refdef.vrect.width;
	int height = vid.height; //r_refdef.vrect.height;
	int scrsize = sw_cube_map_size * sw_cube_map_size;
	int fov = scr_ffov->int_val;
	int views = scr_fviews->int_val;
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
		// front|right|back|left|top|bottom
		scrbufs = malloc (scrsize*6);
		SYS_CHECKMEM (scrbufs);
		offs = malloc (width*height*sizeof(byte*));
		SYS_CHECKMEM (offs);
		pwidth = width;
		pheight = height;
		pfov = fov;
		fisheyelookuptable (offs, width, height, scrbufs, fov*M_PI/180.0);
	}

	if (views != pviews) {
		pviews = views;
		memset (scrbufs, 0, scrsize*6);
	}

	switch (views) {
		case 6:  renderside (scrbufs + scrsize * BOX_BEHIND, BOX_BEHIND);
		case 5:  renderside (scrbufs + scrsize * BOX_BOTTOM, BOX_BOTTOM);
		case 4:  renderside (scrbufs + scrsize * BOX_TOP,    BOX_TOP);
		case 3:  renderside (scrbufs + scrsize * BOX_LEFT,   BOX_LEFT);
		case 2:  renderside (scrbufs + scrsize * BOX_RIGHT,  BOX_RIGHT);
		default: renderside (scrbufs + scrsize * BOX_FRONT,  BOX_FRONT);
	}
#if 0
	memset (scrbufs + scrsize*0, 31, sw_cube_map_size);
	memset (scrbufs + scrsize*1, 31, sw_cube_map_size);
	memset (scrbufs + scrsize*2, 31, sw_cube_map_size);
	memset (scrbufs + scrsize*3, 31, sw_cube_map_size);
	memset (scrbufs + scrsize*4, 31, sw_cube_map_size);
	memset (scrbufs + scrsize*5, 31, sw_cube_map_size);
	for (int y = 0; y < sw_cube_map_size * 6; y++) {
		scrbufs[y * sw_cube_map_size] = 31;
		scrbufs[y * sw_cube_map_size + sw_cube_map_size - 1] = 31;
	}
#endif
	renderlookup (offs, scrbufs);
}

void
R_ClearState (void)
{
	r_refdef.worldmodel = 0;
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
