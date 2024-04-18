/*
	cl_entities.c

	Client side entity management

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/6/28

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

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/idparse.h"
#include "QF/quakefs.h"
#include "QF/plist.h"
#include "QF/progs.h"

#include "QF/scene/light.h"

#include "QF/plugin/vid_render.h"	//FIXME

#include "client/effects.h"
#include "client/entities.h"
#include "client/temp_entities.h"
#include "client/world.h"

worldscene_t cl_world = {
	.models = DARRAY_STATIC_INIT (32),
};

static void
CL_World_Shutdown (void *data)
{
	qfZoneScoped (true);
	if (cl_world.edicts) {
		PL_Release (cl_world.edicts);
	}
	if (cl_world.scene) {
		if (cl_world.scene->lights) {
			Light_DestroyLightingData (cl_world.scene->lights);
		}
		Scene_DeleteScene (cl_world.scene);
	}
	free (cl_world.models.a);
	free (cl_static_entities.a);
}

void
CL_World_Init (void)
{
	qfZoneScoped (true);
	Sys_RegisterShutdown (CL_World_Shutdown, 0);
	scene_system_t extra_systems[] = {
		{	.system = &effect_system,
			.components = effect_components,
			.component_count = effect_comp_count },
		{}
	};
	cl_world.scene = Scene_NewScene (extra_systems);
	cl_world.scene->lights = Light_CreateLightingData (cl_world.scene);
}

void
CL_ParseBaseline (qmsg_t *msg, entity_state_t *baseline, int version)
{
	qfZoneScoped (true);
	int         bits = 0;

	if (version == 2)
		bits = MSG_ReadByte (msg);

	if (bits & B_LARGEMODEL)
		baseline->modelindex = MSG_ReadShort (msg);
	else
		baseline->modelindex = MSG_ReadByte (msg);

	if (bits & B_LARGEFRAME)
		baseline->frame = MSG_ReadShort (msg);
	else
		baseline->frame = MSG_ReadByte (msg);

	baseline->colormap = MSG_ReadByte (msg);
	baseline->skinnum = MSG_ReadByte (msg);

	MSG_ReadCoordAngleV (msg, (vec_t*)&baseline->origin, baseline->angles);//FIXME
	baseline->origin[3] = 1;//FIXME

	if (bits & B_ALPHA)
		baseline->alpha = MSG_ReadByte (msg);
	else
		baseline->alpha = 255;//FIXME alpha
	baseline->scale = 16;
	baseline->glow_size = 0;
	baseline->glow_color = 254;
	baseline->colormod = 255;
}

void
CL_ParseStatic (qmsg_t *msg, int version)
{
	qfZoneScoped (true);
	entity_t	    ent;
	entity_state_t	es;

	ent = Scene_CreateEntity (cl_world.scene);
	CL_Init_Entity (ent);

	CL_ParseBaseline (msg, &es, version);
	DARRAY_APPEND (&cl_static_entities, es);

	auto renderer = Entity_GetRenderer (ent);
	auto animation = Entity_GetAnimation (ent);

	// copy it to the current state
	renderer->model = cl_world.models.a[es.modelindex];
	renderer->noshadows = renderer->model->shadow_alpha < 0.5;
	animation->frame = es.frame;
	renderer->skinnum = es.skinnum;

	CL_TransformEntity (ent, es.scale / 16.0, es.angles, es.origin);

	R_AddEfrags (&cl_world.scene->worldmodel->brush, ent);
}

static void
map_cfg (const char *mapname, int all)
{
	qfZoneScoped (true);
	char       *name = malloc (strlen (mapname) + 4 + 1);
	cbuf_t     *cbuf = Cbuf_New (&id_interp);
	QFile      *f;

	QFS_StripExtension (mapname, name);
	strcat (name, ".cfg");
	f = QFS_FOpenFile (name);
	if (f) {
		Qclose (f);
		Cmd_Exec_File (cbuf, name, 1);
	} else {
		Cmd_Exec_File (cbuf, "maps_default.cfg", 1);
	}
	if (all) {
		Cbuf_Execute_Stack (cbuf);
	} else {
		Cbuf_Execute_Sets (cbuf);
	}
	free (name);
	Cbuf_Delete (cbuf);
}

void
CL_MapCfg (const char *mapname)
{
	map_cfg (mapname, 0);
}

static plitem_t *
map_ent (const char *mapname)
{
	static progs_t edpr;
	char       *name = malloc (strlen (mapname) + 4 + 1);
	char       *buf;
	plitem_t   *edicts = 0;
	QFile      *ent_file;

	QFS_StripExtension (mapname, name);
	strcat (name, ".ent");
	ent_file = QFS_VOpenFile (name, 0, cl_world.models.a[1]->vpath);
	if ((buf = (char *) QFS_LoadFile (ent_file, 0))) {
		edicts = ED_Parse (&edpr, buf);
		free (buf);
	} else {
		edicts = ED_Parse (&edpr, cl_world.models.a[1]->brush.entities);
	}
	free (name);
	return edicts;
}

static void
CL_LoadSky (const char *name)
{
	plitem_t   *worldspawn = cl_world.worldspawn;
	plitem_t   *item;
	static const char *sky_keys[] = {
		"sky",      // Q2/DarkPlaces
		"skyname",  // old QF
		"qlsky",    // QuakeLives
		0
	};

	if (!name) {
		if (!worldspawn) {
			r_funcs->R_LoadSkys (0);
			return;
		}
		for (const char **key = sky_keys; *key; key++) {
			if ((item = PL_ObjectForKey (cl_world.worldspawn, *key))) {
				name = PL_String (item);
				break;
			}
		}
	}
	r_funcs->R_LoadSkys (name);
}

void
CL_World_NewMap (const char *mapname, const char *skyname)
{
	qfZoneScoped (true);
	model_t    *worldmodel = cl_world.models.a[1];
	cl_world.scene->worldmodel = worldmodel;

	cl_static_entities.size = 0;

	if (cl_world.models.a[1] && cl_world.models.a[1]->brush.entities) {
		if (cl_world.edicts) {
			PL_Release (cl_world.edicts);
		}
		cl_world.edicts = map_ent (mapname);
		if (cl_world.edicts) {
			cl_world.worldspawn = PL_ObjectAtIndex (cl_world.edicts, 0);
			CL_LoadSky (skyname);
			Fog_ParseWorldspawn (cl_world.worldspawn);
		}
	}
	CL_LoadLights (cl_world.edicts, cl_world.scene);

	cl_world.scene->models = cl_world.models.a;
	cl_world.scene->num_models = cl_world.models.size;
	SCR_NewScene (cl_world.scene);
	map_cfg (mapname, 1);
}

void
CL_World_Clear (void)
{
	qfZoneScoped (true);
	Scene_FreeAllEntities (cl_world.scene);
	CL_ClearTEnts ();
}
