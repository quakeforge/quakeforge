#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/plist.h"
#include "QF/progs.h"	//for ED_ConvertToPlist
#include "QF/set.h"
#include "QF/scene/light.h"
#include "QF/scene/scene.h"
#include "QF/simd/vec4f.h"

#include "client/world.h"

static void
dump_light (light_t *light, int leaf)
{
	Sys_MaskPrintf (SYS_lighting,
					"[%g, %g, %g] %g, "
					"[%g, %g, %g, %g], [%g %g %g] %g, [%g, %g, %g, %g] %d\n",
					VEC4_EXP (light->color),
					VEC4_EXP (light->position),
					VEC4_EXP (light->direction),
					VEC4_EXP (light->attenuation),
					leaf);
}

static float
parse_float (const char *str, float defval)
{
	float       val = defval;
	if (str) {
		char       *end;
		val = strtof (str, &end);
		if (end == str) {
			val = defval;
		}
	}
	return val;
}

static void
parse_vector (const char *str, vec_t *val)
{
	if (str) {
		int         num = sscanf (str, "%f %f %f", VectorExpandAddr (val));
		while (num < 3) {
			val[num++] = 0;
		}
	}
}

static float
ecos (float ang)
{
	if (ang == 90 || ang == -90) {
		return 0;
	}
	if (ang == 180 || ang == -180) {
		return -1;
	}
	if (ang == 0 || ang == 360) {
		return 1;
	}
	return cos (ang * M_PI / 180);
}

static float
esin (float ang)
{
	if (ang == 90) {
		return 1;
	}
	if (ang == -90) {
		return -1;
	}
	if (ang == 180 || ang == -180) {
		return 0;
	}
	if (ang == 0 || ang == 360) {
		return 0;
	}
	return sin (ang * M_PI / 180);
}

static vec4f_t
sun_vector (const vec_t *ang)
{
	// ang is yaw, pitch (maybe roll, but ignored
	// negative as the vector points *to* the sun, but ang specifies the
	// direction from the sun
	vec4f_t     vec = {
		-ecos (ang[1]) * ecos (ang[0]),
		-ecos (ang[1]) * esin (ang[0]),
		-esin (ang[1]),
		0,
	};
	return vec;
}

static void
parse_sun (lightingdata_t *ldata, plitem_t *entity)
{
	light_t     light = {};
	float       sunlight;
	//float       sunlight2;
	vec3_t      sunangle = { 0, -90, 0 };

	Light_EnableSun (ldata);
	sunlight = parse_float (PL_String (PL_ObjectForKey (entity,
													    "_sunlight")), 0);
	//sunlight2 = parse_float (PL_String (PL_ObjectForKey (entity,
	//													 "_sunlight2")), 0);
	parse_vector (PL_String (PL_ObjectForKey (entity, "_sun_mangle")),
				  sunangle);
	if (sunlight <= 0) {
		return;
	}
	VectorSet (1, 1, 1, light.color);
	light.color[3] = sunlight;
	light.position = sun_vector (sunangle);
	light.direction = light.position;
	light.direction[3] = 1;
	light.attenuation = (vec4f_t) { 0, 0, 1, 0 };
	Light_AddLight (ldata, &light, 0);
}

static vec4f_t
parse_position (const char *str)
{
	vec3_t      vec = {};
	sscanf (str, "%f %f %f", VectorExpandAddr (vec));
	return (vec4f_t) {vec[0], vec[1], vec[2], 1};
}

static void
parse_light (light_t *light, int *style, const plitem_t *entity,
			 const plitem_t *targets)
{
	const char *str;
	int         model = 0;
	float       atten = 1;

	/*Sys_Printf ("{\n");
	for (int i = PL_D_NumKeys (entity); i-- > 0; ) {
		const char *field = PL_KeyAtIndex (entity, i);
		const char *value = PL_String (PL_ObjectForKey (entity, field));
		Sys_Printf ("\t%s = %s\n", field, value);
	}
	Sys_Printf ("}\n");*/

	// omnidirectional light (unit length xyz so not treated as ambient)
	light->direction = (vec4f_t) { 0, 0, 1, 1 };
	// bright white
	light->color = (vec4f_t) { 1, 1, 1, 300 };

	if ((str = PL_String (PL_ObjectForKey (entity, "origin")))) {
		light->position = parse_position (str);
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "target")))) {
		plitem_t   *target = PL_ObjectForKey (targets, str);
		vec4f_t     dir = { 1, 0, 0, 0 };
		if (target) {
			if ((str = PL_String (PL_ObjectForKey (target, "origin")))) {
				dir = parse_position (str);
				dir = normalf (dir - light->position);
			}
		}

		float angle = 40;
		if ((str = PL_String (PL_ObjectForKey (entity, "angle")))) {
			angle = atof (str);
		}
		dir[3] = -cos (angle * M_PI / 360); // half angle
		light->direction = dir;
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "light_lev")))
		|| (str = PL_String (PL_ObjectForKey (entity, "_light")))) {
		light->color[3] = atof (str);
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "style")))) {
		*style = atoi (str) & 0x3f;
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "delay")))) {
		model = atoi (str) & 0x7;
		if (model == LM_INVERSE2) {
			model = LM_INVERSE3;	//FIXME for marcher (need a map)
		}
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "color")))
		|| (str = PL_String (PL_ObjectForKey (entity, "_color")))) {
		union {
			float       a[4];
			vec4f_t     v;
		} color = { .v = light->color };
		sscanf (str, "%f %f %f", VectorExpandAddr (color.a));
		light->color = color.v;
		if (light->color[0] > 1 || light->color[1] > 1 || light->color[2] > 1) {
			VectorScale (light->color, 1/255.0, light->color);
		}
	}

	if ((str = PL_String (PL_ObjectForKey (entity, "wait")))) {
		atten = atof (str);
		if (atten <= 0) {
			atten = 1;
		}
	}

	vec4f_t     attenuation = { 	// inverse square
		1, 0, 0,
		0,
	};
	switch (model) {
		case LM_LINEAR:
			attenuation = (vec4f_t) {
				0, 0, 1,
				atten / fabsf (light->color[3]),
			};
			break;
		case LM_INVERSE:
			attenuation = (vec4f_t) {
				0, atten / 128, 0,
				0,
			};
			break;
		case LM_INVERSE2:
			attenuation = (vec4f_t) {
				atten * atten / 16384, 0, 0,
				0,
			};
			break;
		case LM_INFINITE:
			attenuation = (vec4f_t) {
				0, 0, 1,
				0,
			};
			break;
		case LM_AMBIENT:
			attenuation = (vec4f_t) {
				0, 0, 1,
				0,
			};
			light->direction = (vec4f_t) { 0, 0, 0, 1 };
			break;
		case LM_INVERSE3:
			attenuation = (vec4f_t) {
				atten * atten / 16384, 2 * atten / 128, 1,
				0,
			};
			break;
	}
	light->attenuation = attenuation;
}

void
CL_LoadLights (plitem_t *entities, scene_t *scene)
{
	lightingdata_t *ldata = scene->lights;
	model_t    *model = scene->worldmodel;

	Light_ClearLights (ldata);
	ldata->sun_pvs = set_new_size (model->brush.visleafs);
	if (!entities) {
		return;
	}

	plitem_t   *targets = PL_NewDictionary (0);

	// find all the targets so spotlights can be aimed
	for (int i = 1; i < PL_A_NumObjects (entities); i++) {
		plitem_t   *entity = PL_ObjectAtIndex (entities, i);
		const char *targetname = PL_String (PL_ObjectForKey (entity,
															 "targetname"));
		if (targetname && !PL_ObjectForKey (targets, targetname)) {
			PL_D_AddObject (targets, targetname, entity);
		}
	}

	for (int i = 0; i < PL_A_NumObjects (entities); i++) {
		plitem_t   *entity = PL_ObjectAtIndex (entities, i);
		const char *classname = PL_String (PL_ObjectForKey (entity,
															"classname"));
		if (!classname) {
			continue;
		}
		if (!strcmp (classname, "worldspawn")) {
			// parse_sun can add many lights
			parse_sun (ldata, entity);
			const char *str;
			if ((str = PL_String (PL_ObjectForKey (entity, "light_lev")))) {
				light_t     light = {};
				light.color = (vec4f_t) { 1, 1, 1, atof (str) };
				light.attenuation = (vec4f_t) { 0, 0, 1, 0 };
				light.direction = (vec4f_t) { 0, 0, 0, 1 };
				Light_AddLight (ldata, &light, 0);
			}
		} else if (!strncmp (classname, "light", 5)) {
			light_t     light = {};
			int         style = 0;

			parse_light (&light, &style, entity, targets);
			// some lights have 0 output, so drop them
			if (light.color[3]) {
				Light_AddLight (ldata, &light, style);
			}
		}
	}
	// targets does not own the objects, so need to remove them before
	// freeing targets
	for (int i = PL_D_NumKeys (targets); i-- > 0; ) {
		PL_RemoveObjectForKey (targets, PL_KeyAtIndex (targets, i));
	}
	PL_Free (targets);

	for (size_t i = 0; i < ldata->lights.size; i++) {
		dump_light (&ldata->lights.a[i], ldata->lightleafs.a[i]);
	}
	Sys_MaskPrintf (SYS_lighting, "loaded %zd lights\n", ldata->lights.size);
}
