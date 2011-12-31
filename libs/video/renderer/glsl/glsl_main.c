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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_textures.h"

#include "gl_draw.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "r_screen.h"

mat4_t glsl_projection;
mat4_t glsl_view;
VISIBLE vec3_t r_origin, vpn, vright, vup;
VISIBLE refdef_t r_refdef;
qboolean r_cache_thrash;
int r_init;
int r_framecount;
int d_lightstylevalue[256];
int r_visframecount;
entity_t r_worldentity;
entity_t *currententity;

void
gl_overbright_f (cvar_t *var)
{
}

VISIBLE void
R_ViewChanged (float aspect)
{
	double      xmin, xmax, ymin, ymax;
	float       fovx, fovy, neard, fard;
	vec_t      *proj = glsl_projection;

	fovx = r_refdef.fov_x;
	fovy = r_refdef.fov_y;
	neard = r_nearclip->value;
	fard = r_farclip->value;

	ymax = neard * tan (fovy * M_PI / 360);		// fov_2 / 2
	ymin = -ymax;
	xmax = neard * tan (fovx * M_PI / 360);		// fov_2 / 2
	xmin = -xmax;

	proj[0] = (2 * neard) / (xmax - xmin);
	proj[4] = 0;
	proj[8] = (xmax + xmin) / (xmax - xmin);
	proj[12] = 0;

	proj[1] = 0;
	proj[5] = (2 * neard) / (ymax - ymin);
	proj[9] = (ymax + ymin) / (ymax - ymin);
	proj[13] = 0;

	proj[2] = 0;
	proj[6] = 0;
	proj[10] = (fard + neard) / (neard - fard);
	proj[14] = (2 * fard + neard) / (neard - fard);

	proj[3] = 0;
	proj[7] = 0;
	proj[11] = -1;
	proj[15] = 0;
}

void
R_SetupFrame (void)
{
	R_ClearEnts ();
	r_framecount++;

	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);
}

static void
R_SetupView (void)
{
	float       x, y, w, h;
	mat4_t      mat;
	static mat4_t z_up = {
		 0, 0, -1, 0,
		-1, 0,  0, 0,
		 0, 1,  0, 0,
		 0, 0,  0, 1,
	};

	x = r_refdef.vrect.x;
	y = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height));
	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;
	qfglViewport (x, y, w, h);

	Mat4Zero (mat);
	VectorCopy (vpn, mat + 0);
	VectorNegate (vright, mat + 4);			// we want vleft
	VectorCopy (vup, mat + 8);
	mat[15] = 1;
	Mat4Transpose (mat, mat);//AngleVectors gives the transpose of what we want
	Mat4Mult (z_up, mat, glsl_view);

	Mat4Identity (mat);
	VectorNegate (r_refdef.vieworg, mat + 12);
	Mat4Mult (glsl_view, mat, glsl_view);
}

static void
R_RenderEntities (void)
{
	entity_t   *ent;

	if (!r_drawentities->int_val)
		return;

	R_SpriteBegin ();
	for (ent = r_ent_queue; ent; ent = ent->next) {
		if (ent->model->type != mod_sprite)
			continue;
		currententity = ent;
		R_DrawSprite ();
	}
	R_SpriteEnd ();
}

static inline void
visit_leaf (mleaf_t *leaf)
{
	// deal with model fragments in this leaf
	if (leaf->efrags)
		R_StoreEfrags (leaf->efrags);
}

static inline int
get_side (mnode_t *node)
{
	// find which side of the node we are on
	plane_t    *plane = node->plane;

	if (plane->type < 3)
		return (r_origin[plane->type] - plane->dist) < 0;
	return (DotProduct (r_origin, plane->normal) - plane->dist) < 0;
}


static inline void
visit_node (mnode_t *node, int side)
{
	int         c;
	msurface_t *surf;

	// sneaky hack for side = side ? SURF_PLANEBACK : 0;
	side = (~side + 1) & SURF_PLANEBACK;
	// draw stuff
	if ((c = node->numsurfaces)) {
		surf = r_worldentity.model->surfaces + node->firstsurface;
		for (; c; c--, surf++) {
			if (surf->visframe != r_visframecount)
				continue;

			// side is either 0 or SURF_PLANEBACK
			if (side ^ (surf->flags & SURF_PLANEBACK))
				continue;               // wrong side

			//chain_surface (surf, 0, 0);
		}
	}
}


static inline int
test_node (mnode_t *node)
{
	if (node->contents < 0)
		return 0;
	if (node->visframe != r_visframecount)
		return 0;
	if (R_CullBox (node->minmaxs, node->minmaxs + 3))
		return 0;
	return 1;
}


static void
R_VisitWorldNodes (mnode_t *node)
{
#define NODE_STACK 1024
	struct {
		mnode_t    *node;
		int         side;
	}          *node_ptr, node_stack[NODE_STACK];
	mnode_t    *front;
	int         side;

	node_ptr = node_stack;

	while (1) {
		while (test_node (node)) {
			side = get_side (node);
			front = node->children[side];
			if (test_node (front)) {
				if (node_ptr - node_stack == NODE_STACK)
					Sys_Error ("node_stack overflow");
				node_ptr->node = node;
				node_ptr->side = side;
				node_ptr++;
				node = front;
				continue;
			}
			if (front->contents < 0 && front->contents != CONTENTS_SOLID)
				visit_leaf ((mleaf_t *) front);
			visit_node (node, side);
			node = node->children[!side];
		}
		if (node->contents < 0 && node->contents != CONTENTS_SOLID)
			visit_leaf ((mleaf_t *) node);
		if (node_ptr != node_stack) {
			node_ptr--;
			node = node_ptr->node;
			side = node_ptr->side;
			visit_node (node, side);
			node = node->children[!side];
			continue;
		}
		break;
	}
	if (node->contents < 0 && node->contents != CONTENTS_SOLID)
		visit_leaf ((mleaf_t *) node);
}

static void
R_DrawWorld (void)
{
	entity_t    worldent;

	memset (&worldent, 0, sizeof (worldent));
	worldent.model = r_worldentity.model;

	currententity = &worldent;

	R_VisitWorldNodes (worldent.model->nodes);
}

VISIBLE void
R_RenderView (void)
{
	R_SetupFrame ();
	R_SetupView ();
	R_DrawWorld ();
	R_RenderEntities ();
}

VISIBLE void
R_Init (void)
{
	R_InitParticles ();
	R_InitSprites ();
}

VISIBLE void
R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = worldmodel;

	R_FreeAllEntities ();
	R_ClearParticles ();
}

VISIBLE void
R_LoadSkys (const char *sky)
{
}

VISIBLE void
Fog_Update (float density, float red, float green, float blue, float time)
{
}

VISIBLE void
Fog_ParseWorldspawn (struct plitem_s *worldspawn)
{
}

VISIBLE void
Skin_Player_Model (model_t *model)
{
}

VISIBLE void
Skin_Set_Translate (int top, int bottom, void *_dest)
{
}

VISIBLE void
Skin_Do_Translation (skin_t *player_skin, int slot, skin_t *skin)
{
}

VISIBLE void
Skin_Do_Translation_Model (struct model_s *model, int skinnum, int slot,
						   skin_t *skin)
{
}

VISIBLE void
Skin_Process (skin_t *skin, struct tex_s * tex)
{
}

VISIBLE void
Skin_Init_Translation (void)
{
}

VISIBLE void
R_LineGraph (int x, int y, int *h_vals, int count)
{
}

VISIBLE int
GL_LoadTexture (const char *identifier, int width, int height, byte *data,
				qboolean mipmap, qboolean alpha, int bytesperpixel)
{
	return 0;
}

VISIBLE void
R_ClearState (void)
{
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
