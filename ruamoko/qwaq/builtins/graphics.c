/*
	graphics.c

	Basic game engine builtins

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/7/24

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "QF/backtrace.h"
#include "QF/cbuf.h"
#include "QF/draw.h"
#include "QF/image.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/plist.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/ruamoko.h"
#include "QF/screen.h"
#include "QF/sound.h"

#include "QF/input/event.h"
#include "QF/math/bitop.h"
#include "QF/scene/light.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"
#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/text.h"

#include "rua_internal.h"

#include "ruamoko/qwaq/qwaq.h"

CLIENT_PLUGIN_PROTOS
static plugin_list_t client_plugin_list[] = {
	CLIENT_PLUGIN_LIST
};

typedef struct qwaq_comp_s {
	pr_uint_t   size;
	pr_func_t   create;
	pr_func_t   destroy;
	pr_func_t   rangeid;//not supported yet
	pr_string_t name;
	pr_ptr_t    data;
	pr_func_t   ui;
} qwaq_comp_t;

typedef struct qwaq_ecs_s {
	progs_t    *pr;
	struct graphics_resources_s *res;

	component_t *components;
	pr_func_t  *create;
	pr_func_t  *destroy;
	// rangeid not supported yet (too hard basket)
	// name is redundant (in components)
	pr_ptr_t   *data;
	pr_func_t  *ui;
	uint32_t    base;
	uint32_t    update;
	ecs_registry_t *reg;
} qwaq_ecs_t;

typedef struct graphics_resources_s {
	progs_t    *pr;
	double      con_frametime;
	double      con_realtime, basetime;
	double      old_conrealtime;

	pr_func_t   qcevent;
	pr_ptr_t    qcevent_data;
	int         event_handler_id;
	view_t      screen_view;
	uint32_t    canvas;

	qwaq_ecs_t  ecs;
} graphics_resources_t;

static void
quit_f (void)
{
	if (!con_module)
		Sys_Printf ("I hope you wanted to quit\n");
	Sys_Quit ();
}

static byte default_palette[256][3];
static byte default_colormap[64 * 256 + 1] = { [64 * 256] = 32 };

static progs_t *bi_rprogs;
static pr_func_t qc2d;
static canvas_system_t canvas_sys;

static void
bi_2d (void)
{
	if (qc2d)
		PR_ExecuteProgram (bi_rprogs, qc2d);
	Con_DrawConsole ();
	Canvas_Draw (canvas_sys);
}

static SCR_Func bi_2dfuncs[] = {
	bi_2d,
	0,
};

static void
vidsize_listener (void *data, const viddef_t *vdef)
{
	graphics_resources_t *res = data;
	Canvas_SetLen (canvas_sys, res->canvas,
				   (view_pos_t) { vdef->width, vdef->height });
}

static void
bi_get_component (progs_t *pr, void *_res)
{
	graphics_resources_t *res = _res;
	uint32_t ent = P_UINT (pr, 0);
	uint32_t comp = P_UINT (pr, 1);
	void *dst = (byte *) P_GPOINTER (pr, 2);
	void *src = Ent_GetComponent (ent, comp + res->ecs.base, res->ecs.reg);
	memcpy (dst, src, res->ecs.components[comp].size);
}

static void
bi_set_component (progs_t *pr, void *_res)
{
	graphics_resources_t *res = _res;
	uint32_t ent = P_UINT (pr, 0);
	uint32_t comp = P_UINT (pr, 1);
	void *src = (byte *) P_GPOINTER (pr, 2);
	Ent_SetComponent (ent, comp + res->ecs.base, res->ecs.reg, src);
}

static void
bi_set_update (progs_t *pr, void *_res)
{
	graphics_resources_t *res = _res;
	uint32_t ent = P_UINT (pr, 0);
	pr_func_t func = P_FUNCTION (pr, 1);
	Ent_SetComponent (ent, res->ecs.update, res->ecs.reg, &func);
}

static void
bi_new_entity (progs_t *pr, void *_res)
{
	graphics_resources_t *res = _res;
	R_UINT (pr) = ECS_NewEntity (res->ecs.reg);
}

static void
bi_del_entity (progs_t *pr, void *_res)
{
	graphics_resources_t *res = _res;
	ECS_DelEntity (res->ecs.reg, P_UINT (pr, 0));
}

static void
bi_comp_create (void *comp, ecs_registry_t *reg, uint32_t ent,
				const component_t *component)
{
	qwaq_ecs_t *ecs = component->data;
	auto pr = ecs->pr;
	uint32_t comp_id = component - ecs->components;

	PR_PushFrame (pr);
	auto params = PR_SaveParams (pr);

	const size_t param_size = sizeof (pr_quaternion_t);
	int num = (component->size + param_size - 1) / param_size;
	// 8 so dvec4 can be used
	PR_SetupParams (pr, 2 + num, 8);
	pr->pr_argc = 2;
	P_POINTER (pr, 0) = PR_SetPointer (pr, &P_INT (pr, 2));
	P_UINT (pr, 1) = ent;
	PR_ExecuteProgram (pr, ecs->create[comp_id]);
	memcpy (comp, &P_INT (pr, 2), component->size);
	PR_RestoreParams (pr, params);
	PR_PopFrame (pr);
}

static void
bi_comp_destroy (void *comp, ecs_registry_t *reg, uint32_t ent,
				 const component_t *component)
{
	qwaq_ecs_t *ecs = component->data;
	auto pr = ecs->pr;
	uint32_t comp_id = component - ecs->components;

	PR_PushFrame (pr);
	auto params = PR_SaveParams (pr);

	const size_t param_size = sizeof (pr_quaternion_t);
	int num = (component->size + param_size - 1) / param_size;
	// 8 so dvec4 can be used
	PR_SetupParams (pr, 2 + num, 8);
	pr->pr_argc = 2;
	P_POINTER (pr, 0) = PR_SetPointer (pr, &P_INT (pr, 2));
	P_UINT (pr, 1) = ent;
	memcpy (&P_INT (pr, 2), comp, component->size);
	PR_ExecuteProgram (pr, ecs->destroy[comp_id]);
	PR_RestoreParams (pr, params);
	PR_PopFrame (pr);
}

static void
bi_comp_ui (void *comp, ecs_registry_t *reg, uint32_t ent,
			const component_t *component)
{
}

static void
bi_create_registry (graphics_resources_t *res, int num_components,
					qwaq_comp_t *components)
{
	size_t size = sizeof (component_t[num_components + 1])
				+ sizeof (pr_func_t[num_components])//create
				+ sizeof (pr_func_t[num_components])//destroy
				+ sizeof (pr_ptr_t[num_components])//data
				+ sizeof (pr_func_t[num_components]);//ui
	res->ecs.components = malloc (size);
	res->ecs.create = (pr_func_t *) &res->ecs.components[num_components + 1];
	res->ecs.destroy = (pr_func_t *) &res->ecs.create[num_components];
	res->ecs.data = (pr_ptr_t *) &res->ecs.destroy[num_components];
	res->ecs.ui = (pr_func_t *) &res->ecs.data[num_components];
	for (int i = 0; i < num_components; i++) {
		res->ecs.components[i] = (component_t) {
			.size = components[i].size,
			.create = components[i].create ? bi_comp_create : nullptr,
			.destroy = components[i].destroy ? bi_comp_destroy : nullptr,
			.name = PR_GetString (res->pr, components[i].name),
			.data = res,
			.ui = components[i].destroy ? bi_comp_ui : nullptr,
		};
		res->ecs.create[i] = components[i].create;
		res->ecs.destroy[i] = components[i].destroy;
		res->ecs.data[i] = components[i].data;
		res->ecs.ui[i] = components[i].ui;
	}
	res->ecs.components[num_components] = (component_t) {
		.size = sizeof (pr_func_t),
		.name = "update",
		.data = res,
	};
	res->ecs.reg = ECS_NewRegistry ("qwaq ecs");
	res->ecs.base = ECS_RegisterComponents (res->ecs.reg, res->ecs.components,
											num_components + 1);
	res->ecs.update = res->ecs.base + num_components;
	ECS_CreateComponentPools (res->ecs.reg);
}

static void
bi_init_graphics (progs_t *pr, void *_res)
{
	graphics_resources_t *res = _res;
	VID_Init (default_palette[0], default_colormap);
	IN_Init ();
	Mod_Init ();
	int plitem_id = P_INT (pr, 0);
	plitem_t *plitem = nullptr;
	if (plitem_id) {
		plitem = Plist_GetItem (pr, plitem_id);
	}
	R_Init (plitem);
	Font_Init ();

	Con_Load ("client");
	auto reg = ECS_NewRegistry ("qwaq gr");
	Canvas_InitSys (&canvas_sys, reg);
	if (con_module) {
		auto cd = con_module->data->console;
		cd->realtime = &res->con_realtime;
		cd->frametime = &res->con_frametime;
		cd->quit = quit_f;
		cd->cbuf = qwaq_cbuf;
		cd->component_base = ECS_RegisterComponents (reg, cd->components,
													 cd->num_components);
		cd->canvas_sys = &canvas_sys;
	}

	int num_components = P_INT (pr, 1);
	auto components = &P_STRUCT (pr, qwaq_comp_t, 2);
	if (num_components > 0) {
		bi_create_registry (res, num_components, components);
	}

	ECS_CreateComponentPools (reg);

	res->canvas = Canvas_New (canvas_sys);
	res->screen_view = Canvas_GetRootView (canvas_sys, res->canvas);
	View_SetPos (res->screen_view, 0, 0);
	View_SetLen (res->screen_view, viddef.width, viddef.height);
	View_SetGravity (res->screen_view, grav_northwest);
	View_SetVisible (res->screen_view, 1);

	VID_OnVidResize_AddListener (vidsize_listener, res);

	//Key_SetKeyDest (key_game);
	Con_Init ();
	VID_SendSize ();

	S_Init (0, &res->con_frametime);
	//CDAudio_Init ();
	Con_NewMap ();
	res->basetime = Sys_DoubleTime ();
}

static void
bi_newscene (progs_t *pr, void *_res)
{
	pr_ulong_t  scene_id = P_ULONG (pr, 0);
	SCR_NewScene (Scene_GetScene (pr, scene_id));
	r_funcs->R_LoadSkys ("eso0932a");
}

static vec4f_t
vec_select (vec4i_t mask, vec4f_t a, vec4f_t b)
{
	return (vec4f_t) ((mask & (vec4i_t) a) | (~mask & (vec4i_t) b));
}

static vec4f_t
box_corner (ent_aabb_t box, int corner)
{
	vec4f_t b_min = loadvec3f (box.mins);
	vec4f_t b_max = loadvec3f (box.maxs);
	b_max[3] = 1;
	vec4i_t mask = {
		((corner >> 0) & 1) - 1,
		((corner >> 1) & 1) - 1,
		((corner >> 2) & 1) - 1,
	};
	return vec_select (mask, b_min, b_max);
}


static ent_aabb_t
transform_bounds (const mat4f_t mat, ent_aabb_t bounds)
{
	vec4f_t nb[] = {
		{ INFINITY, INFINITY, INFINITY },
		{-INFINITY,-INFINITY,-INFINITY },
	};
	for (int i = 0; i < 8; i++) {
		vec4f_t pos = box_corner (bounds, i);
		pos = mvmulf (mat, pos);
		pos /= pos[3];
		nb[0] = minv4f (nb[0], pos);
		nb[1] = maxv4f (nb[1], pos);
	}
	return (ent_aabb_t) {
		.mins = { VectorExpand (nb[0]) },
		.maxs = { VectorExpand (nb[1]) },
	};
}


static void
bi_refresh (progs_t *pr, void *_res)
{
	qfFrameMark;
	graphics_resources_t *res = _res;
	res->con_realtime = Sys_DoubleTime () - res->basetime;
	res->con_frametime = res->con_realtime - res->old_conrealtime;
	res->old_conrealtime = res->con_realtime;
	bi_rprogs = pr;
	IN_ProcessEvents ();
	//GIB_Thread_Execute ();
	Cbuf_Execute_Stack (qwaq_cbuf);
	auto scene = Scene_GetScene (pr, P_ULONG (pr, 0));
	transform_t camera = nulltransform;
	if (scene && scene->camera != nullent) {
		entity_t ent = { .reg = scene->reg, .id = scene->camera,
						 .base = scene->base };
		if (Ent_HasComponent (ent.id, ent.base + scene_href, ent.reg)) {
			camera = Entity_Transform (ent);
		}
	}
	if (res->ecs.reg) {
		auto reg = res->ecs.reg;
		auto pool = &reg->comp_pools[res->ecs.update];
		uint32_t    count = pool->count;
		uint32_t   *entities = pool->dense;
		auto funcs = (pr_func_t *) pool->data;

		PR_PushFrame (pr);
		auto params = PR_SaveParams (pr);
		while (count-- > 0) {
			PR_SetupParams (pr, 1, 1);
			P_UINT (pr, 0) = *entities++;
			PR_ExecuteProgram (pr, *funcs++);
		}
		PR_RestoreParams (pr, params);
		PR_PopFrame (pr);
	}
	if (scene) {
		uint32_t c_animation = scene->base + scene_animation;
		uint32_t c_renderer = scene->base + scene_renderer;
		auto reg = scene->reg;
		auto animpool = reg->comp_pools + c_animation;
		auto rendpool = reg->comp_pools + c_renderer;
		Anim_Update (res->con_realtime, animpool, rendpool);
		if (scene->lights) {
			auto reg = scene->reg;
			auto pool = &reg->comp_pools[scene->base + scene_renderer];
			const uint32_t c_receiver = scene->base + scene_shadow_receiver;
			const uint32_t c_caster = scene->base + scene_shadow_caster;
			for (uint32_t i = 0; i < pool->count; i++) {
				auto rend = &((renderer_t *) pool->data)[i];
				if (!rend->model) {
					continue;
				}
				uint32_t ent = pool->dense[i];
				auto xform = Entity_Transform ((entity_t) {
						.reg = scene->reg,
						.id = ent,
						.base = scene->base,
				});
				ent_aabb_t aabb = {
					.mins = { VectorExpand (rend->model->mins) },
					.maxs = { VectorExpand (rend->model->maxs) },
				};
				auto mat = Transform_GetWorldMatrixPtr (xform);
				aabb = transform_bounds (mat, aabb);
				if (!rend->noshadowreceive) {
					Ent_SetComponent (ent, c_receiver, scene->reg, &aabb);
				} else {
					Ent_RemoveComponent (ent, c_receiver, scene->reg);
				}
				if (!rend->noshadowcast) {
					Ent_SetComponent (ent, c_caster, scene->reg, &aabb);
				} else {
					Ent_RemoveComponent (ent, c_caster, scene->reg);
				}
			}
			Light_CalculateBounds (scene->lights);
		}
	}
	SCR_UpdateScreen (camera, res->con_realtime, bi_2dfuncs);
	R_FLOAT (pr) = res->con_frametime;
}

static void
bi_refresh_2d (progs_t *pr, void *_res)
{
	qc2d = P_FUNCTION (pr, 0);
}

static void
bi_setpalette (progs_t *pr, void *_res)
{
	byte       *palette = (byte *) P_GPOINTER (pr, 0);
	byte       *colormap = (byte *) P_GPOINTER (pr, 1);
	VID_SetPalette (palette, colormap);
}

static void
bi_setevents (progs_t *pr, void *_res)
{
	graphics_resources_t *res = _res;
	res->qcevent = P_FUNCTION (pr, 0);
	res->qcevent_data = P_POINTER (pr, 1);
}

static void
bi_setctxcbuf (progs_t *pr, void *_res)
{
	IMT_SetContextCbuf (P_INT (pr, 0), qwaq_cbuf);
}

static void
bi_addcbuftxt (progs_t *pr, void *_res)
{
	Cbuf_AddText (qwaq_cbuf, P_GSTRING (pr, 0));
}

#define bi(x,n,np,params...) {#x, bi_##x, n, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(get_component, -1, 3, p(uint), p(uint), p(ptr)),
	bi(set_component, -1, 3, p(uint), p(uint), p(ptr)),
	bi(set_update,    -1, 2, p(uint), p(func)),
	bi(new_entity,    -1, 0),
	bi(del_entity,    -1, 1, p(uint)),
	bi(init_graphics, -1, 1, p(ptr)),
	bi(newscene,      -1, 1, p(long)),
	bi(refresh,       -1, 1, p(long)),
	bi(refresh_2d,    -1, 1, p(func)),
	bi(setpalette,    -1, 2, p(ptr), p(ptr)),
	bi(setevents,     -1, 2, p(func), p(ptr)),
	bi(setctxcbuf,    -1, 1, p(int)),
	bi(addcbuftxt,    -1, 1, p(string)),
	{0}
};

static int
event_handler (const IE_event_t *ie_event, void *_res)
{
	graphics_resources_t *res = _res;
	if (res->qcevent) {
		auto pr = res->pr;
		int num_params = sizeof (IE_event_t) / sizeof (pr_type_t);
		num_params = (num_params + 3) / 4 + 2;

		PR_PushFrame (pr);
		auto params = PR_SaveParams (pr);
		PR_SetupParams (pr, num_params, 2);
		auto event = &P_PACKED (pr, IE_event_t, 2);
		P_POINTER (pr, 0) = PR_SetPointer (pr, event);
		P_POINTER (pr, 1) = res->qcevent_data;
		*event = *ie_event;
		PR_ExecuteProgram (pr, res->qcevent);
		int ret = R_INT (pr);
		PR_RestoreParams (pr, params);
		PR_PopFrame (pr);
		return ret;
	}
	return IN_Binding_HandleEvent (ie_event);
}

static void
BI_shutdown (void *data)
{
	printf ("BI_shutdown\n");
	ECS_DelRegistry (canvas_sys.reg);
	ColorCache_Shutdown ();
}

static byte *
write_raw_rgb (byte *dst, byte r, byte g, byte b)
{
	*dst++ = r;
	*dst++ = g;
	*dst++ = b;
	return dst;
}

static byte *
write_rgb (byte *dst, byte r, byte g, byte b)
{
#define shift(x) (((x) << 2) | (((x) & 0x3f) >> 4))
	*dst++ = shift(r);
	*dst++ = shift(g);
	*dst++ = shift(b);
	return dst;
#undef shift
}

static byte *
write_grey (byte *dst, byte grey)
{
	return write_rgb (dst, grey, grey, grey);
}

static byte *
genererate_irgb (byte *dst, byte lo, byte melo, byte mehi, byte hi)
{
	for (int i = 0; i < 16; i++) {
		byte        l = i & 8 ? melo : lo;
		byte        h = i & 8 ? hi : mehi;
		byte        r = i & 4 ? h : l;
		byte        g = i & 2 ? h : l;
		byte        b = i & 1 ? h : l;
		if (i == 6) {	// make dim yellow brown
			g = melo;
		}
		dst = write_rgb (dst, r, g, b);
	}
	return dst;
}

static byte *
write_run (byte *dst, byte *hue, byte ch, const byte *levels)
{
	byte        rgb[3] = {
		*hue & 4 ? levels[4] : levels[0],
		*hue & 2 ? levels[4] : levels[0],
		*hue & 1 ? levels[4] : levels[0],
	};
	byte        chan = 2 - BITOP_LOG2 (ch);
	int         ind = *hue & ch ? 4 : 0;
	int         dir = *hue & ch ? -1 : 1;

	for (int i = 0; i < 4; i++, ind += dir) {
		rgb[chan] = levels[ind];
		dst = write_rgb (dst, rgb[0], rgb[1], rgb[2]);
	}

	*hue ^= ch;
	return dst;
}

static byte *
write_cycle (byte *dst, byte lo, byte melo, byte me, byte mehi, byte hi)
{
	const byte  levels[] = { lo, melo, me, mehi, hi };
	byte        hue = 1;
	dst = write_run (dst, &hue, 4, levels);
	dst = write_run (dst, &hue, 1, levels);
	dst = write_run (dst, &hue, 2, levels);

	dst = write_run (dst, &hue, 4, levels);
	dst = write_run (dst, &hue, 1, levels);
	dst = write_run (dst, &hue, 2, levels);
	return dst;
}

static void
generate_palette (void)
{
	const byte  grey[] = {
		0,  5,  8,  11, 14, 17, 20, 24,
		28, 32, 36, 40, 45, 50, 56, 63,
	};
	byte       *dst = default_palette[0];

	dst = genererate_irgb (dst, 0, 21, 42, 63);

	for (int i = 0; i < 16; i++) {
		dst = write_grey (dst, grey[i]);
	}

	dst = write_cycle (dst, 11, 12, 13, 15, 16);
	dst = write_cycle (dst,  8, 10, 12, 14, 16);
	dst = write_cycle (dst,  0,  4,  8, 12, 16);

	dst = write_cycle (dst, 20, 22, 24, 26, 28);
	dst = write_cycle (dst, 14, 17, 21, 24, 28);
	dst = write_cycle (dst,  0,  7, 14, 21, 28);

	dst = write_cycle (dst, 45, 49, 54, 58, 63);
	dst = write_cycle (dst, 31, 39, 47, 55, 63);
	dst = write_cycle (dst,  0, 16, 31, 47, 63);

	// std is black, but want vis trans and some phosphors
	dst = write_raw_rgb (dst,  40,  40,  40);
	dst = write_raw_rgb (dst,  40, 127,  40);
	dst = write_raw_rgb (dst,  51, 255,  51);
	dst = write_raw_rgb (dst, 102, 255, 102);
	dst = write_raw_rgb (dst, 127, 102,   0);
	dst = write_raw_rgb (dst, 255, 176,   0);
	dst = write_raw_rgb (dst, 255, 204,   0);

	dst = write_raw_rgb (dst, 176, 160, 176);
}

static void
generate_colormap (void)
{
	byte        colors[64][256][3];
	tex_t       tex = {
		.width = 256,
		.height = 64,
		.format = tex_rgb,
		.data = colors[0][0],
	};

	// baseline colors
	for (int i = 0; i < 256; i++) {
		VectorCopy (default_palette[i], colors[31][i]);
		VectorCopy (default_palette[i], colors[32][i]);
	}
	for (int i = 0; i < 64; i++) {
		// main colors
		if (i < 31 || i > 32) {
			float       scale = i < 32 ? 2 - i / 31.0 : 1 - (i - 32) / 31.0;
			for (int j = 0; j < 224; j++) {
				for (int k = 0; k < 3; k++) {
					colors[i][j][k] = bound (0, scale * colors[31][j][k], 255);
				}
			}
		}
		// fullbrights, but avoid copying the source row to itself
		if (i != 31) {
			memcpy (colors[i][224], colors[31][224], 32 * 3);
		}
	}
	size_t mark = Hunk_LowMark (nullptr);
	auto cmap = ConvertImage (&tex, default_palette[0], "cmap");
	// the colormap has an extra byte indicating the number of fullbright
	// entries, but that byte is not in the image, so don't try to copy it,
	// thus the - 1
	memcpy (default_colormap, cmap->data, sizeof (default_colormap) - 1);
	Hunk_FreeToLowMark (nullptr, mark);
}

static void
graphics_destroy (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	graphics_resources_t *res = _res;

	free (res);
}

static void
graphics_clear (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	graphics_resources_t *res = _res;

	ECS_DelRegistry (res->ecs.reg);

	// everything is allocated in one block
	free (res->ecs.components);
	res->ecs = (qwaq_ecs_t) {};
}

static const char *bi_dirconf = R"(
{
	QF = {
		Path = "QF";
	};
	qwaq = {
		Inherit = (QF);
		Path = "qwaq";
	};
	qwaq:* = {
		Inherit = (qwaq);
		Path = "qwaq/$gamedir";
	};
}
)";

void
BI_Graphics_Init (progs_t *pr)
{
	graphics_resources_t *res = malloc (sizeof (graphics_resources_t));
	*res = (graphics_resources_t) {
		.pr = pr,
	};

	PR_Resources_Register (pr, "Graphics", res,
						   graphics_clear, graphics_destroy);
	PR_RegisterBuiltins (pr, builtins, res);
	BT_Init (this_program);

	QFS_SetConfig (PL_GetPropertyList (bi_dirconf, nullptr));

	qwaq_thread_t *thread = PR_Resources_Find (pr, "qwaq_thread");
	QFS_Init (thread->hunk, "qwaq");

	PI_Init ();
	PI_RegisterPlugins (client_plugin_list);

	Sys_RegisterShutdown (BI_shutdown, pr);

	R_Progs_Init (pr);
	RUA_Game_Init (pr, thread->rua_security);
	S_Progs_Init (pr);

	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	generate_palette ();
	generate_colormap ();

	res->event_handler_id = IE_Add_Handler (event_handler, res);
	IE_Set_Focus (res->event_handler_id);
}
