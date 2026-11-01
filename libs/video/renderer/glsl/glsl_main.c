/*
	glsl_main.c

	GLSL rendering

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/image.h"
#include "QF/plist.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_bsp.h"
#include "QF/GLSL/qf_draw.h"
#include "QF/GLSL/qf_fisheye.h"
#include "QF/GLSL/qf_lightmap.h"
#include "QF/GLSL/qf_main.h"
#include "QF/GLSL/qf_mesh.h"
#include "QF/GLSL/qf_particles.h"
#include "QF/GLSL/qf_sprite.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_warp.h"

#include "mod_internal.h"
#include "r_internal.h"
#include "vid_gl.h"

mat4f_t glsl_projection;
mat4f_t glsl_inv_projection;
mat4f_t glsl_view;
mat4f_t glsl_inv_view;

static plitem_t *glsl_entqueue_item;
static int glsl_entqueue_count;
static int *glsl_entqueue_values;
static exprtab_t glsl_entqueue_symtab;
static exprtype_t glsl_entqueue_type;
static exprenum_t glsl_entqueue_enum = {
	.type = &glsl_entqueue_type,
	.symtab = &glsl_entqueue_symtab,
};
static exprtype_t glsl_entqueue_type = {
	.name = "entqueue",
	.size = sizeof (int),
	.binops = cexpr_enum_binops,
	.get_string = cexpr_enum_get_string,
	.data = &glsl_entqueue_enum,
};

static void
glsl_LoadEntqueueInfo (plitem_t *item, plitem_t *messages)
{
	static const char *modtype_names[] = {
		"mod_brush",
		"mod_sprite",
		"mod_mesh",
		"mod_light",
	};
	static_assert(countof (modtype_names) == mod_num_types);

	auto queues = PL_ObjectForKey (item, "queues");
	if (!queues) {
		PL_Message (messages, item, "no queues member");
		return;
	}
	if (PL_Type (queues) != QFArray) {
		PL_Message (messages, item, "queues not an array");
		return;
	}
	if (PL_A_NumObjects (queues) < 1) {
		PL_Message (messages, queues, "queues empty");
		return;
	}
	int num_syms = mod_num_types + PL_A_NumObjects (queues);
	glsl_entqueue_symtab = (exprtab_t) {
		.symbols = malloc (sizeof (exprsym_t[num_syms])),
		.tab = Hash_NewTable (61, cexpr_getkey, 0, 0, nullptr),
	};
	glsl_entqueue_values = malloc (sizeof (int[num_syms]));
	auto symbols = glsl_entqueue_symtab.symbols;
	for (int i = 0; i < mod_num_types; i++) {
		symbols[i] = (exprsym_t) {
			.name = modtype_names[i],
			.type = &glsl_entqueue_type,
			.value = &glsl_entqueue_values[i],
		};
		glsl_entqueue_values[i] = i;
		Hash_Add (glsl_entqueue_symtab.tab, &symbols[i]);
	}
	for (int i = mod_num_types; i < num_syms; i++) {
		auto eq = PL_ObjectAtIndex (queues, i - mod_num_types);
		const char *name = PL_String (eq);
		if (!name) {
			PL_Message (messages, eq, "queues[%d] not a string",
						i - mod_num_types);
		}
		if (Hash_Find (glsl_entqueue_symtab.tab, name)) {
			PL_Message (messages, eq, "duplicate entqueue name: %s", name);
		}
		symbols[i] = (exprsym_t) {
			.name = name,
			.type = &glsl_entqueue_type,
			.value = &glsl_entqueue_values[i],
		};
		glsl_entqueue_values[i] = i;
		Hash_Add (glsl_entqueue_symtab.tab, &symbols[i]);
	}
	glsl_entqueue_count = num_syms;
	glsl_entqueue_item = PL_Retain (item);
}

static void
R_SetupView (void)
{
	static mat4f_t z_up = {
		{ 0, 0,  1, 0},
		{-1, 0,  0, 0},
		{ 0, 1,  0, 0},
		{ 0, 0,  0, 1},
	};
	static mat4f_t inv_z_up = {
		{ 0, -1, 0, 0},
		{ 0,  0, 1, 0},
		{ 1,  0, 0, 0},
		{ 0,  0, 0, 1},
	};

	mmulf (glsl_view, z_up, r_refdef.camera_inverse);
	mmulf (glsl_inv_view, r_refdef.camera, inv_z_up);

	qfeglEnable (GL_CULL_FACE);
	qfeglEnable (GL_DEPTH_TEST);
}

void
glsl_R_RenderEntities (entqueue_t *queue)
{
	int         begun;

	if (!r_drawentities)
		return;
#define RE_LOOP(type_name, Type) \
	do { \
		begun = 0; \
		for (size_t i = 0; i < queue->ent_queues[mod_##type_name].size; \
			 i++) { \
			entity_t    ent = queue->ent_queues[mod_##type_name].a[i]; \
			if (!begun) { \
				glsl_R_##Type##Begin (); \
				begun = 1; \
			} \
			glsl_R_Draw##Type (ent); \
		} \
		if (begun) \
			glsl_R_##Type##End (); \
	} while (0)

	RE_LOOP (mesh, Mesh);
	RE_LOOP (sprite, Sprite);
}

static void
R_DrawViewModel (void)
{
	entity_t    ent = vr_data.view_model;
	if (!Entity_Valid (ent)) {
		return;
	}
	auto renderer = Entity_GetRenderer (ent);
	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel
		|| !r_drawentities
		|| !renderer->model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qfeglDepthRangef (0, 0.3);
	glsl_R_MeshBegin ();
	glsl_R_DrawMesh (ent);
	glsl_R_MeshEnd ();
	qfeglDepthRangef (0, 1);
}

void
glsl_R_RenderView (void)
{
	if (!r_refdef.worldmodel) {
		return;
	}

	memcpy (glsl_projection, glsl_ctx->projection, sizeof (mat4f_t));
	memcpy (glsl_inv_projection, glsl_ctx->inv_projection, sizeof (mat4f_t));

	R_SetupView ();
	glsl_R_DrawWorld ();
	glsl_R_DrawSky ();
	if (Entity_Valid (vr_data.view_model)) {
		R_DrawViewModel ();
	}
}

static void
glsl_R_TimeRefresh_f (void)
{
/* FIXME update for simd
	double      start, stop, time;
	int         i;

	glsl_ctx->end_rendering ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		glsl_R_RenderView ();
		glsl_ctx->end_rendering ();
	}

	stop = Sys_DoubleTime ();
	time = stop - start;
	Sys_Printf ("%g seconds (%g fps)\n", time, 128 / time);
*/
}

void
glsl_R_Init (memhunk_t *hunk, plitem_t *config)
{
	if (config) {
		auto entqueues = PL_ObjectForKey (config, "entqueues");
		if (entqueues) {
			auto messages = PL_NewArray ();
			glsl_LoadEntqueueInfo (entqueues, messages);
			if (PL_A_NumObjects (messages)) {
				for (int i = 0; i < PL_A_NumObjects (messages); i++) {
					Sys_Printf ("%s\n",
								PL_String (PL_ObjectAtIndex (messages, i)));
				}
				Sys_Error ("invalid entqueues");
			}
			PL_Release (messages);
		}

		if (!entqueues) {
			Sys_Printf (ONG"WARNING"DFL":glsl_R_Init:render config ignored\n");
		}
	}
	Cmd_AddCommand ("timerefresh", glsl_R_TimeRefresh_f,
					"Test the current refresh rate for the current location.");
	R_Init_Cvars ();
	SCR_Init (hunk);
	glsl_Draw_Init ();

	qfeglDepthFunc (GL_LEQUAL);

	glsl_R_InitBsp ();
	glsl_R_InitMesh ();
	glsl_R_InitSprites ();
	glsl_R_InitParticles ();
	glsl_R_InitTrails ();
	glsl_InitFisheye ();
	glsl_InitWarp ();
	Skin_Init ();
}

void
glsl_R_Shutdown (void)
{
	glsl_R_ShutdownParticles ();
	glsl_Lightmap_Shutdown ();
	glsl_R_ShutdownBsp ();
	SCR_Shutdown ();
	glsl_Draw_Shutdown ();
}

void
glsl_R_NewScene (scene_t *scene)
{
	qfZoneScoped (true);
	int         i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	if (glsl_entqueue_count > scene->ent_queue->num_queues) {
		EntQueue_Delete (scene->ent_queue);
		scene->ent_queue = EntQueue_New (glsl_entqueue_count);
		scene->entqueue_enum = &glsl_entqueue_enum;
	}

	r_refdef.worldmodel = scene->worldmodel;

	R_ClearParticles ();
	glsl_R_RegisterTextures (scene->models, scene->num_models);
	glsl_R_BuildLightmaps (scene->models, scene->num_models);
	glsl_R_BuildDisplayLists (scene->models, scene->num_models);
}

void
glsl_R_ClearState (void)
{
	r_refdef.worldmodel = 0;
	R_ClearParticles ();
}
