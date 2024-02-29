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

#include "QF/scene/entity.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

#define s_dynlight (r_refdef.scene->base + scene_dynlight)
#define s_sw_matrix (r_refdef.scene->base + scene_sw_matrix)

#ifdef PIC
# undef USE_INTEL_ASM //XXX asm pic hack
#endif

const byte *r_colormap;
int         r_numallocatededges;
bool        r_drawpolys;
bool        r_drawculledpolys;
bool        r_worldpolysbacktofront;
bool        r_recursiveaffinetriangles = true;
int         r_pixbytes = 1;
int         r_outofsurfaces;
int         r_outofedges;

bool        r_viewchanged;

int         c_surf;
int         r_maxsurfsseen, r_maxedgesseen;
static int  r_cnumsurfs;
static bool        r_surfsonstack;
int         r_clipflags;

static byte       *r_stack_start;

// screen size info
float       xcenter, ycenter;
float       xscale, yscale;
float       xscaleinv, yscaleinv;
float       xscaleshrink, yscaleshrink;
float       aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int         screenwidth;

float       pixelAspect;

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
sw_R_Init (struct plitem_s *config)
{
	if (config) {
		Sys_Printf (ONG"WARNING"DFL": sw_R_Init: render config ignored\n");
	}
	int         dummy;

	// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte *) & dummy;

	R_Init_Cvars ();

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

	r_maxedges = NUMSTACKEDGES;
	r_maxsurfs = NUMSTACKSURFACES;

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
		view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
		view_clipplanes[3].rightedge = false;

// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	Sys_MakeCodeWriteable ((long) R_EdgeCodeStart,
						   (long) R_EdgeCodeEnd - (long) R_EdgeCodeStart);
#endif // USE_INTEL_ASM
	D_Init ();

	Skin_Init ();
}

uint32_t
SW_AddEntity (entity_t ent)
{
	// This takes advantage of the implicit (FIXME, make explicit) grouping of
	// the sw components: as all entities that get added here will always have
	// all three components, the three component pools are always in sync, thus
	// the pool count can be used as a render id which can in turn be used to
	// index the components within the pools.
	ecs_registry_t *reg = ent.reg;
	ecs_pool_t *pool = &reg->comp_pools[s_sw_matrix];
	uint32_t    render_id = pool->count;

	transform_t transform = Entity_Transform (ent);
	Ent_SetComponent (ent.id, ent.base + scene_sw_matrix, reg,
					  Transform_GetWorldMatrixPtr (transform));
	auto animation = Entity_GetAnimation (ent);
	Ent_SetComponent (ent.id, ent.base + scene_sw_frame, reg, &animation->frame);
	auto renderer = Entity_GetRenderer (ent);
	mod_brush_t *brush = &renderer->model->brush;
	Ent_SetComponent (ent.id, ent.base + scene_sw_brush, reg, &brush);

	return render_id;
}

static void
reset_sw_components (ecs_registry_t *reg)
{
	static uint32_t sw_comps[] = {
		scene_sw_matrix,
		scene_sw_frame,
		scene_sw_brush,
	};

	for (int i = 0; i < 3; i++) {
		uint32_t    comp = r_refdef.scene->base + sw_comps[i];
		ecs_pool_t *pool = &reg->comp_pools[comp];
		pool->count = 0;	// remove component from every entity
		// reserve first component object (render id 0) for the world
		// pseudo-entity.
		//FIXME takes advantage of the lack of checks for the validity of the
		//entity id.
		Ent_SetComponent (0, comp, reg, 0);
		// make sure entity 0 gets allocated a new component object as the
		// world pseudo-entity currently has no actual entity (FIXME)
		pool->dense[0] = nullent;
	}
}

void
R_NewScene (scene_t *scene)
{
	model_t    *worldmodel = scene->worldmodel;
	mod_brush_t *brush = &worldmodel->brush;

	r_refdef.worldmodel = worldmodel;

	if (brush->skytexture)
		R_InitSky (brush->skytexture);

	R_ClearParticles ();

	r_cnumsurfs = r_maxsurfs;

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

	r_numallocatededges = r_maxedges;

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

void
R_SetColormap (const byte *cmap)
{
	r_colormap = cmap;
// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	Sys_MakeCodeWriteable ((long) R_Surf8Start,
						   (long) R_Surf8End - (long) R_Surf8Start);
	R_SurfPatch ();
#endif // USE_INTEL_ASM
}

static inline void
draw_sprite_entity (entity_t ent)
{
	R_DrawSprite (ent);
}

static inline void
draw_mesh_entity (entity_t ent)
{
	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	visibility_t *visibility = Ent_GetComponent (ent.id, ent.base + scene_visibility,
												 ent.reg);
	visibility->trivial_accept = 0;	//FIXME
	if (R_AliasCheckBBox (ent)) {
		alight_t    lighting;
		R_Setup_Lighting (ent, &lighting);
		R_AliasDrawModel (ent, &lighting);
	}
}

static inline void
draw_iqm_entity (entity_t ent)
{
	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	visibility_t *visibility = Ent_GetComponent (ent.id, ent.base + scene_visibility,
												 ent.reg);
	visibility->trivial_accept = 0;	//FIXME

	alight_t    lighting;
	R_Setup_Lighting (ent, &lighting);
	R_IQMDrawModel (ent, &lighting);
}

void
R_DrawEntitiesOnList (entqueue_t *queue)
{
	if (!r_drawentities)
		return;

	R_LowFPPrecision ();
#define RE_LOOP(type_name) \
	do { \
		for (size_t i = 0; i < queue->ent_queues[mod_##type_name].size; \
			 i++) { \
			entity_t    ent = queue->ent_queues[mod_##type_name].a[i]; \
			transform_t transform = Entity_Transform (ent); \
			r_entorigin = Transform_GetWorldPosition (transform); \
			draw_##type_name##_entity (ent); \
		} \
	} while (0)

	RE_LOOP (mesh);
	RE_LOOP (iqm);
	RE_LOOP (sprite);

	R_HighFPPrecision ();
}

static void
R_DrawViewModel (void)
{
	// FIXME: remove and do real lighting
	int         j;
	vec3_t      dist;
	float       add;
	float       minlight;
	entity_t    viewent;
	alight_t    lighting;

	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel
		|| !r_drawentities)
		return;

	viewent = vr_data.view_model;
	if (!Entity_Valid (viewent)) {
		return;
	}

	auto renderer = Entity_GetRenderer (viewent);
	if (!renderer->model)
		return;

	if (!Ent_HasComponent (viewent.id, viewent.base + scene_visibility, viewent.reg)) {
		// ensure the view model has a visibility component because one won't
		// be added automatically, and the model rendering code expects there
		// to be one
		Ent_SetComponent (viewent.id, viewent.base + scene_visibility, viewent.reg, 0);
	}

	transform_t transform = Entity_Transform (viewent);
	VectorCopy (Transform_GetWorldPosition (transform), r_entorigin);

	VectorNegate (vup, lighting.lightvec);

	minlight = max (renderer->min_light, renderer->model->min_light);

	j = max (R_LightPoint (&r_refdef.worldmodel->brush,
						   r_entorigin), minlight * 128);

	lighting.ambientlight = j;
	lighting.shadelight = j;

	// add dynamic lights
	auto dlight_pool = &r_refdef.registry->comp_pools[s_dynlight];
	auto dlight_data = (dlight_t *) dlight_pool->data;
	for (uint32_t i = 0; i < dlight_pool->count; i++) {
		auto dl = &dlight_data[i];
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
R_BmodelCheckBBox (const vec4f_t *transform, float radius, float *minmaxs)
{
	int         i, *pindex, clipflags;
	vec3_t      acceptpt, rejectpt;
	double      d;

	clipflags = 0;

	if (transform[0][0] != 1 || transform[1][1] != 1 || transform[2][2] != 1) {
		for (i = 0; i < 4; i++) {
			d = DotProduct (transform[3], view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= radius)
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
	float       minmaxs[6];

	if (!r_drawentities)
		return;

	insubmodel = true;

	auto dlight_pool = &r_refdef.registry->comp_pools[s_dynlight];
	auto dlight_data = (dlight_t *) dlight_pool->data;

	for (size_t i = 0; i < queue->ent_queues[mod_brush].size; i++) {
		entity_t    ent = queue->ent_queues[mod_brush].a[i];
		uint32_t    render_id = SW_AddEntity (ent);

		vec4f_t    *transform = Ent_GetComponent (ent.id, ent.base + scene_sw_matrix,
												  ent.reg);
		VectorCopy (transform[3], origin);
		auto renderer = Entity_GetRenderer (ent);
		model_t    *model = renderer->model;

		// see if the bounding box lets us trivially reject, also
		// sets trivial accept status
		for (j = 0; j < 3; j++) {
			minmaxs[j] = origin[j] + model->mins[j];
			minmaxs[3 + j] = origin[j] + model->maxs[j];
		}

		clipflags = R_BmodelCheckBBox (transform, model->radius, minmaxs);

		if (clipflags != BMODEL_FULLY_CLIPPED) {
			mod_brush_t *brush = &model->brush;
			VectorCopy (origin, r_entorigin);
			VectorSubtract (r_refdef.frame.position, r_entorigin, modelorg);

			r_pcurrentvertbase = brush->vertexes;

			// FIXME: stop transforming twice
			R_RotateBmodel (transform);

			// calculate dynamic lighting for bmodel if it's not an
			// instanced model
			if (brush->firstmodelsurface != 0) {
				for (k = 0; k < dlight_pool->count; k++) {
					auto dlight = &dlight_data[k];
					vec4f_t     lightorigin;
					VectorSubtract (dlight->origin, origin, lightorigin);
					lightorigin[3] = 1;
					R_RecursiveMarkLights (brush, lightorigin,
										   dlight, k,
										   brush->hulls[0].firstclipnode);
				}
			}
			// if the driver wants polygons, deliver those.
			// Z-buffering is on at this point, so no clipping to the
			// world tree is needed, just frustum clipping
			if (r_drawpolys | r_drawculledpolys) {
				R_ZDrawSubmodelPolys (render_id, brush);
			} else {
				visibility_t *visibility = Ent_GetComponent (ent.id, ent.base +
															 scene_visibility,
															 ent.reg);
				int         topnode_id = visibility->topnode_id;
				mod_brush_t *world_brush = &r_refdef.worldmodel->brush;

				if (topnode_id >= 0) {
					// not a leaf; has to be clipped to the world
					// BSP
					mnode_t    *node = world_brush->nodes + topnode_id;
					r_clipflags = clipflags;
					R_DrawSolidClippedSubmodelPolygons (render_id, brush, node);
				} else {
					// falls entirely in one leaf, so we just put
					// all the edges in the edge list and let 1/z
					// sorting handle drawing order
					mleaf_t    *leaf = world_brush->leafs + ~topnode_id;
					R_DrawSubmodelPolygons (render_id, brush, clipflags, leaf);
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
	if (r_norefresh)
		return;
	if (!r_refdef.worldmodel) {
		return;
	}

	reset_sw_components (r_refdef.registry);
	*(mod_brush_t **) SW_COMP (scene_sw_brush, 0) = &r_refdef.worldmodel->brush;
	R_SetupFrame ();

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	R_LowFPPrecision ();

	R_EdgeDrawing (r_ent_queue);

	if (Entity_Valid (vr_data.view_model)) {
		R_DrawViewModel ();
	}

	if (r_aliasstats)
		R_PrintAliasStats ();

	// back to high floating-point precision
	R_HighFPPrecision ();
}

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

	if ((intptr_t) (&r_colormap) & 3)
		Sys_Error ("Globals are missaligned");

	d_framecount++;
	R_RenderView_ ();
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

void
R_ClearState (void)
{
	r_refdef.worldmodel = 0;
	R_ClearParticles ();
}
