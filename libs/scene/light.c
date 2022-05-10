#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "QF/model.h"
#include "QF/set.h"
#include "QF/scene/light.h"
#include "QF/scene/scene.h"
#include "QF/simd/vec4f.h"

static void
expand_pvs (set_t *pvs, model_t *model)
{
	set_t       base_pvs = SET_STATIC_INIT (model->brush.visleafs, alloca);
	set_assign (&base_pvs, pvs);
	for (unsigned i = 0; i < model->brush.visleafs; i++) {
		if (set_is_member (&base_pvs, i)) {
			Mod_LeafPVS_mix (model->brush.leafs + i + 1, model, 0, pvs);
		}
	}
}

lightingdata_t *
Light_CreateLightingData (scene_t *scene)
{
	lightingdata_t *ldata = calloc (1, sizeof (lightingdata_t));

	DARRAY_INIT (&ldata->lights, 16);
	DARRAY_INIT (&ldata->lightstyles, 16);
	DARRAY_INIT (&ldata->lightleafs, 16);
	DARRAY_INIT (&ldata->lightvis, 16);

	ldata->scene = scene;

	return ldata;
}

void
Light_DestroyLightingData (lightingdata_t *ldata)
{
	DARRAY_CLEAR (&ldata->lights);
	DARRAY_CLEAR (&ldata->lightstyles);
	DARRAY_CLEAR (&ldata->lightleafs);
	DARRAY_CLEAR (&ldata->lightvis);

	free (ldata);
}

void
Light_ClearLights (lightingdata_t *ldata)
{
	ldata->lights.size = 0;
	ldata->lightstyles.size = 0;
	ldata->lightleafs.size = 0;
	ldata->lightvis.size = 0;
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

void
Light_AddLight (lightingdata_t *ldata, const light_t *light, int style)
{
	scene_t    *scene = ldata->scene;
	model_t    *model = scene->worldmodel;

	DARRAY_APPEND (&ldata->lights, *light);
	DARRAY_APPEND (&ldata->lightstyles, style);

	int         visleaf = -1;	// directional light
	if (light->position[3]) {
		// positional light
		mleaf_t    *leaf = Mod_PointInLeaf (&light->position[0], model);
		visleaf = leaf - model->brush.leafs - 1;
	} else if (!DotProduct (light->direction, light->direction)) {
		// ambient light
		visleaf = -2;
	}
	DARRAY_APPEND (&ldata->lightleafs, visleaf);
	DARRAY_APPEND (&ldata->lightvis, 0);
}

void
Light_EnableSun (lightingdata_t *ldata)
{
	scene_t    *scene = ldata->scene;
	model_t    *model = scene->worldmodel;

	if (!ldata->sun_pvs) {
		ldata->sun_pvs = set_new_size (model->brush.visleafs);
	}
	set_expand (ldata->sun_pvs, model->brush.visleafs);
	set_empty (ldata->sun_pvs);
	// Any leaf with sky surfaces can potentially see the sun, thus put
	// the sun "in" every leaf with a sky surface
	// however, skip leaf 0 as it is the exterior solid leaf
	for (unsigned l = 1; l < model->brush.modleafs; l++) {
		if (model->brush.leaf_flags[l] & SURF_DRAWSKY) {
			set_add (ldata->sun_pvs, l - 1); //pvs is 1-based
		}
	}
	// any leaf visible from a leaf with a sky surface (and thus the sun)
	// can receive shadows from the sun
	expand_pvs (ldata->sun_pvs, model);
}

void
Light_FindVisibleLights (lightingdata_t *ldata)
{
	scene_t    *scene = ldata->scene;
	mleaf_t    *leaf = scene->viewleaf;
	model_t    *model = scene->worldmodel;

	if (!leaf) {
		return;
	}
	if (!ldata->pvs) {
		ldata->pvs = set_new_size (model->brush.visleafs);
	}

	if (leaf != ldata->leaf) {
		//double start = Sys_DoubleTime ();
		int         flags = 0;

		if (leaf == model->brush.leafs) {
			set_everything (ldata->pvs);
			flags = SURF_DRAWSKY;
		} else {
			Mod_LeafPVS_set (leaf, model, 0, ldata->pvs);
			if (set_is_intersecting (ldata->pvs, ldata->sun_pvs)) {
				flags |= SURF_DRAWSKY;
			}
			expand_pvs (ldata->pvs, model);
		}
		ldata->leaf = leaf;

		//double end = Sys_DoubleTime ();
		//Sys_Printf ("find_visible_lights: %.5gus\n", (end - start) * 1e6);

		int visible = 0;
		memset (ldata->lightvis.a, 0, ldata->lightvis.size * sizeof (byte));
		for (size_t i = 0; i < ldata->lightleafs.size; i++) {
			int         l = ldata->lightleafs.a[i];
			if ((l == -2) || (l == -1 && (flags & SURF_DRAWSKY))
				|| set_is_member (ldata->pvs, l)) {
				ldata->lightvis.a[i] = 1;
				visible++;
			}
		}
		Sys_MaskPrintf (SYS_lighting,
						"find_visible_lights: %d / %zd visible\n", visible,
						ldata->lightvis.size);
	}
}
