#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "QF/model.h"
#include "QF/render.h"
#include "QF/set.h"
#include "QF/scene/entity.h"
#include "QF/scene/light.h"
#include "QF/scene/scene.h"
#include "QF/simd/vec4f.h"

#include "QF/ui/imui.h"
#define IMUI_context imui_ctx

static void
expand_pvs (set_t *pvs, mod_brush_t *brush)
{
	set_t       base_pvs = SET_STATIC_INIT (brush->visleafs, alloca);
	set_assign (&base_pvs, pvs);
	for (unsigned i = 0; i < brush->visleafs; i++) {
		if (set_is_member (&base_pvs, i)) {
			Mod_LeafPVS_mix (brush->leafs + i + 1, brush, 0, pvs);
		}
	}
}

lightingdata_t *
Light_CreateLightingData (scene_t *scene)
{
	lightingdata_t *ldata = calloc (1, sizeof (lightingdata_t));

	ldata->scene = scene;

	return ldata;
}

void
Light_DestroyLightingData (lightingdata_t *ldata)
{
	free (ldata);
}

void
Light_ClearLights (lightingdata_t *ldata)
{
	if (ldata->sun_pvs) {
		set_delete (ldata->sun_pvs);
	}
	ldata->sun_pvs = 0;
	if (ldata->pvs) {
		set_delete (ldata->pvs);
	}
	ldata->pvs = 0;
	ldata->leaf = 0;
}

static bool
test_light_leaf (const light_t *light, const mleaf_t *leaf)
{
	// FIXME directional lights should check the direction against the
	// leaf's portals (need to find the portals first, though)
	if (!light->position[3] || !light->attenuation[3]) {
		// non-positional lights or lights with infinite radius always light
		// the leafs they can see
		return true;
	}
	// use Minkowski difference to see if the light hits the leaf's bounding
	// box (thanks to Nick Alger:
	// https://stackoverflow.com/questions/5122228/box-to-sphere-collision)
	float r = 1/light->attenuation[3];
	vec4f_t c = light->position / light->position[3];
	vec4f_t mins = loadvec3f (leaf->mins);
	vec4f_t maxs = loadvec3f (leaf->maxs);
	vec4i_t tmin = c < mins;
	vec4i_t tmax = maxs < c;
	vec4i_t tcen = ~tmin & ~tmax;
	vec4f_t p = (vec4f_t)((tmin & (vec4i_t)mins)
						+ (tcen & (vec4i_t)c)
						+ (tmax & (vec4i_t)maxs));
	p[3] = 1;
	vec4f_t d = p - c;
	if (dotf (d, d)[0] > r * r) {
		return false;
	}
	return true;
}

static void
link_light (lightingdata_t *ldata, const light_t *light, entity_t ent)
{
	scene_t    *scene = ldata->scene;
	model_t    *model = scene->worldmodel;

	set_t       _pvs = SET_STATIC_INIT (model->brush.visleafs, alloca);
	set_t      *pvs = &_pvs;
	uint32_t    leafnum = ~0u;
	if (light->position[3]) {
		// positional light
		mleaf_t    *leaf = Mod_PointInLeaf (light->position, &model->brush);
		Mod_LeafPVS_set (leaf, &model->brush, 0, pvs);
		leafnum = leaf - model->brush.leafs;
	} else if (DotProduct (light->direction, light->direction)) {
		// directional light (sun)
		pvs = ldata->sun_pvs;
		leafnum = 0;
	} else {
		// ambient light
		Mod_LeafPVS_set (model->brush.leafs, &model->brush, 0xff, pvs);
	}
	Ent_SetComponent (ent.id, scene_lightleaf, ent.reg, &leafnum);

	efrag_t *efrags = 0;
	efrag_t **lastlink = &efrags;
	for (auto li = set_first (pvs); li; li = set_next (li)) {
		mleaf_t    *leaf = model->brush.leafs + li->element + 1;
		if (test_light_leaf (light, leaf)) {
			lastlink = R_LinkEfrag (leaf, ent, mod_light, lastlink);
		}
	}
	if (Ent_HasComponent (ent.id, scene_efrags, ent.reg)) {
		Ent_RemoveComponent (ent.id, scene_efrags, ent.reg);
	}
	Ent_SetComponent (ent.id, scene_efrags, ent.reg, &efrags);
}

void
Light_AddLight (lightingdata_t *ldata, const light_t *light, uint32_t style)
{
	scene_t    *scene = ldata->scene;

	entity_t    ent = {
		.reg = scene->reg,
		.id = ECS_NewEntity (scene->reg),
	};

	Ent_SetComponent (ent.id, scene_light, ent.reg, light);
	Ent_SetComponent (ent.id, scene_lightstyle, ent.reg, &style);

	link_light (ldata, light, ent);
}

void
Light_LinkLight (lightingdata_t *ldata, uint32_t entid)
{
	scene_t    *scene = ldata->scene;

	entity_t    ent = {
		.reg = scene->reg,
		.id = entid,
	};
	dlight_t *dlight = Ent_GetComponent (ent.id, scene_dynlight, ent.reg);
	if (!dlight) {
		Sys_Error ("no dlight on entity to link");
	}
	light_t light = {
		.color = dlight->color,
		.position = dlight->origin,
		.direction = {0, 0, 1, 1},
		.attenuation = {0, 0, 1, 1/dlight->radius},
	};

	link_light (ldata, &light, ent);
}

void
Light_EnableSun (lightingdata_t *ldata)
{
	scene_t    *scene = ldata->scene;
	auto brush = &scene->worldmodel->brush;

	if (!ldata->sun_pvs) {
		ldata->sun_pvs = set_new_size (brush->visleafs);
	}
	set_expand (ldata->sun_pvs, brush->visleafs);
	set_empty (ldata->sun_pvs);
	// Any leaf with sky surfaces can potentially see the sun, thus put
	// the sun "in" every leaf with a sky surface
	// however, skip leaf 0 as it is the exterior solid leaf
	for (unsigned l = 1; l < brush->modleafs; l++) {
		if (brush->leaf_flags[l] & SURF_DRAWSKY) {
			set_add (ldata->sun_pvs, l - 1); //pvs is 1-based
		}
	}
	// any leaf visible from a leaf with a sky surface (and thus the sun)
	// can receive shadows from the sun
	expand_pvs (ldata->sun_pvs, brush);
}

void
Light_DecayLights (lightingdata_t *ldata, float frametime, double realtime)
{
	auto reg = ldata->scene->reg;
	auto dlight_pool = &reg->comp_pools[scene_dynlight];
	auto dlight_data = (dlight_t *) dlight_pool->data;

	for (uint32_t i = 0; i < dlight_pool->count; i++) {
		auto dlight = &dlight_data[i];
		dlight->radius -= frametime * dlight->decay;
		if (dlight->radius <= 0 || dlight->die < realtime) {
			uint32_t ent = dlight_pool->dense[i];
			Ent_RemoveComponent (ent, scene_dynlight, reg);
			if (!Ent_HasComponent (ent, scene_efrags, reg)) {
				Sys_Error ("dlight with no efrags");
			}
			Ent_RemoveComponent (ent, scene_efrags, reg);
			i--;
		}
	}
}

void
Light_dyn_light_ui (void *comp, imui_ctx_t *imui_ctx,
					ecs_registry_t *reg, uint32_t ent, void *data)
{
	dlight_t *dlight = comp;
	UI_Horizontal {
		UI_Labelf ("Origin: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.1f %6.1f %6.1f %6g", VEC4_EXP (dlight->origin));
	}
	UI_Horizontal {
		UI_Label ("Color: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%5.3f %5.3f %5.3f %5.3f", VEC4_EXP (dlight->color));
	}
	UI_Horizontal {
		UI_Labelf ("Radius: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6g", dlight->radius);
	}
	UI_Horizontal {
		UI_Labelf ("Die: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.2f", dlight->die);
	}
	UI_Horizontal {
		UI_Labelf ("Decay: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.2f", dlight->decay);
	}
	UI_Horizontal {
		UI_Labelf ("Min Light: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6g", dlight->minlight);
	}
}

void
Light_light_ui (void *comp, imui_ctx_t *imui_ctx,
				ecs_registry_t *reg, uint32_t ent, void *data)
{
	light_t *light = comp;
	UI_Horizontal {
		UI_Label ("Color: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%5.3f %5.3f %5.3f %3g", VEC4_EXP (light->color));
	}
	UI_Horizontal {
		UI_Labelf ("Position: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.1f %6.1f %6.1f %6g", VEC4_EXP (light->position));
	}
	UI_Horizontal {
		UI_Labelf ("Direction: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%6.3f %6.3f %6.3f %6.3g", VEC4_EXP (light->direction));
	}
	UI_Horizontal {
		UI_Labelf ("Attenuation: ");
		UI_FlexibleSpace ();
		UI_Labelf ("%g %g %g %g", VEC4_EXP (light->attenuation));
	}
}
