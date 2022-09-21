/*
	screen.c

	master for refresh, status bar, console, chat, notify, etc

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/png.h"
#include "QF/pcx.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"
#include "QF/ui/view.h"

#include "compat.h"
#include "r_internal.h"
#include "sbar.h"

// only the refresh window will be updated unless these variables are flagged
int         scr_copytop;
byte       *draw_chars;			// 8*8 graphic characters FIXME location
qboolean    r_cache_thrash;		// set if surface cache is thrashing
int        *r_node_visframes;	//FIXME per renderer
int        *r_leaf_visframes;	//FIXME per renderer
int        *r_face_visframes;	//FIXME per renderer

qboolean    scr_skipupdate;
static qboolean scr_initialized;// ready to draw

static framebuffer_t *fisheye_cube_map;
static framebuffer_t *warp_buffer;

static float fov_x, fov_y;
static float tan_fov_x, tan_fov_y;
static scene_t *scr_scene;//FIXME don't want this here

static mat4f_t box_rotations[] = {
	[BOX_FRONT] = {
		{ 1, 0, 0, 0},
		{ 0, 1, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_RIGHT] = {
		{ 0,-1, 0, 0},
		{ 1, 0, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_BEHIND] = {
		{-1, 0, 0, 0},
		{ 0,-1, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_LEFT] = {
		{ 0, 1, 0, 0},
		{-1, 0, 0, 0},
		{ 0, 0, 1, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_TOP] = {
		{ 0, 0, 1, 0},
		{ 0, 1, 0, 0},
		{-1, 0, 0, 0},
		{ 0, 0, 0, 1}
	},
	[BOX_BOTTOM] = {
		{ 0, 0,-1, 0},
		{ 0, 1, 0, 0},
		{ 1, 0, 0, 0},
		{ 0, 0, 0, 1}
	},
};

static void
set_vrect (const vrect_t *vrectin, vrect_t *vrect, int lineadj)
{
	float       size;
	int         h;

	size = min (r_viewsize, 100);
	if (r_data->force_fullscreen) {
		// intermission is always full screen
		size = 100.0;
		lineadj = 0;
	}
	size /= 100.0;

	h = vrectin->height - lineadj;

	vrect->width = vrectin->width * size + 0.5;
	if (vrect->width < 96) {
		size = 96.0 / vrectin->width;
		vrect->width = 96;				// min for icons
	}
	vrect->width &= ~7;

	vrect->height = vrectin->height * size + 0.5;
	if (vrect->height > h)
		vrect->height = h;
	vrect->height &= ~1;

	vrect->x = (vrectin->width - vrect->width) / 2;
	vrect->y = (h - vrect->height) / 2;
}

void//FIXME remove when sw warp is cleaned up
R_SetVrect (const vrect_t *vrectin, vrect_t *vrect, int lineadj)
{
	set_vrect (vrectin, vrect, lineadj);
}

void
SCR_SetFOV (float fov)
{

	refdef_t   *refdef = r_data->refdef;

	double      f = fov * M_PI / 360;
	double      c = cos(f);
	double      s = sin(f);
	double      w = refdef->vrect.width;
	double      h = refdef->vrect.height;

	fov_x = fov;
	fov_y = 360 * atan2 (s * h, c * w) / M_PI;
	tan_fov_x = s / c;
	tan_fov_y = (s * h) / (c * w);
	r_funcs->set_fov (tan_fov_x, tan_fov_y);
}

static void
SCR_CalcRefdef (void)
{
	view_t     *view = r_data->scr_view;
	const vrect_t *rect = &r_data->refdef->vrect;

	r_data->vid->recalc_refdef = 0;

	view_setgeometry (view, rect->x, rect->y, rect->width, rect->height);

	// force a background redraw
	r_data->scr_fullupdate = 0;
}

static void
render_scene (void)
{
	r_framecount++;
	EntQueue_Clear (r_ent_queue);
	r_funcs->render_view ();
	r_funcs->draw_entities (r_ent_queue);
	r_funcs->draw_particles (&r_psystem);
	r_funcs->draw_transparent ();
}

static void
render_side (int side)
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

	refdef_t   *refdef = r_data->refdef;
	R_SetFrustum (refdef->frustum, &refdef->frame, 90, 90);

	r_funcs->bind_framebuffer (&fisheye_cube_map[side]);
	render_scene ();

	memcpy (r_refdef.camera, camera, sizeof (camera));
	memcpy (r_refdef.camera_inverse, camera_inverse, sizeof (camera_inverse));
}

void
SCR_UpdateScreen (transform_t *camera, double realtime, SCR_Func *scr_funcs)
{
	if (scr_skipupdate || !scr_initialized) {
		return;
	}

	if (r_timegraph || r_speeds || r_dspeeds) {
		r_time1 = Sys_DoubleTime ();
	}

	if (scr_fisheye && !fisheye_cube_map) {
		fisheye_cube_map = r_funcs->create_cube_map (r_data->vid->height);
	}

	refdef_t   *refdef = r_data->refdef;
	if (camera) {
		Transform_GetWorldMatrix (camera, refdef->camera);
		Transform_GetWorldInverse (camera, refdef->camera_inverse);
	} else {
		mat4fidentity (refdef->camera);
		mat4fidentity (refdef->camera_inverse);
	}

	// FIXME pre-rotate the camera 90 degrees about the z axis such that the
	// camera forward vector (camera Y) points along the world +X axis and the
	// camera right vector (camera X) points along the world -Y axis. This
	// should not be necessary here but is due to AngleVectors (and thus
	// AngleQuat for compatibility) treating X as forward and Y as left (or -Y
	// as right). Fixing this would take an audit of the usage of both, but is
	// probably worthwhile in the long run.
	refdef->frame.mat[0] = -refdef->camera[1];
	refdef->frame.mat[1] =  refdef->camera[0];
	refdef->frame.mat[2] =  refdef->camera[2];
	refdef->frame.mat[3] =  refdef->camera[3];
	R_SetFrustum (refdef->frustum, &refdef->frame, fov_x, fov_y);

	r_data->realtime = realtime;
	scr_copytop = r_data->scr_copyeverything = 0;

	if (r_data->vid->recalc_refdef) {
		SCR_CalcRefdef ();
	}

	R_RunParticles (r_data->frametime);
	R_AnimateLight ();
	if (scr_scene && scr_scene->worldmodel) {
		scr_scene->viewleaf = 0;
		vec4f_t     position = refdef->frame.position;
		scr_scene->viewleaf = Mod_PointInLeaf (position, scr_scene->worldmodel);
		r_dowarpold = r_dowarp;
		if (r_waterwarp) {
			r_dowarp = scr_scene->viewleaf->contents <= CONTENTS_WATER;
		}
		if (r_dowarp && !warp_buffer) {
			warp_buffer = r_funcs->create_frame_buffer (r_data->vid->width,
														r_data->vid->height);
		}
		R_MarkLeaves (scr_scene->viewleaf, r_node_visframes, r_leaf_visframes,
					  r_face_visframes);
	}
	R_PushDlights (vec3_origin);

	r_funcs->begin_frame ();
	if (r_dowarp) {
		r_funcs->bind_framebuffer (warp_buffer);
	}
	if (scr_fisheye && fisheye_cube_map) {
		int         side = fisheye_cube_map->width;
		vrect_t     feye = { 0, 0, side, side };
		r_funcs->set_viewport (&feye);
		r_funcs->set_fov (1, 1);	//FIXME shouldn't be every frame (2d stuff)
		switch (scr_fviews) {
			case 6: render_side (BOX_BEHIND);
			case 5: render_side (BOX_BOTTOM);
			case 4: render_side (BOX_TOP);
			case 3: render_side (BOX_LEFT);
			case 2: render_side (BOX_RIGHT);
			default:render_side (BOX_FRONT);
		}
		r_funcs->bind_framebuffer (0);
		r_funcs->set_viewport (&r_refdef.vrect);
		r_funcs->post_process (fisheye_cube_map);
	} else {
		r_funcs->set_viewport (&r_refdef.vrect);
		render_scene ();
		if (r_dowarp) {
			r_funcs->bind_framebuffer (0);
			r_funcs->post_process (warp_buffer);
		}
	}
	r_funcs->set_2d (0);
	view_draw (r_data->scr_view);
	r_funcs->set_2d (1);
	while (*scr_funcs) {
		(*scr_funcs) ();
		scr_funcs++;
	}
	r_funcs->end_frame ();
}

static void
update_vrect (void)
{
	r_data->vid->recalc_refdef = 1;

	vrect_t     vrect;
	refdef_t   *refdef = r_data->refdef;

	vrect.x = 0;
	vrect.y = 0;
	vrect.width = r_data->vid->width;
	vrect.height = r_data->vid->height;

	set_vrect (&vrect, &refdef->vrect, r_data->lineadj);
	SCR_SetFOV (scr_fov);
}

void
SCR_SetFullscreen (qboolean fullscreen)
{
	if (r_data->force_fullscreen == fullscreen) {
		return;
	}

	r_data->force_fullscreen = fullscreen;
	update_vrect ();
}

void
SCR_SetBottomMargin (int lines)
{
	r_data->lineadj = lines;
	update_vrect ();
}

typedef struct scr_capture_s {
	dstring_t  *name;
	QFile      *file;
} scr_capture_t;

static void
scr_write_caputre (tex_t *tex, void *data)
{
	scr_capture_t *cap = data;

	if (tex) {
		WritePNG (cap->file, tex);
		free (tex);
		Sys_Printf ("Wrote %s/%s\n", qfs_userpath, cap->name->str);
	} else {
		Sys_Printf ("Capture failed\n");
	}
	Qclose (cap->file);
	dstring_delete (cap->name);
	free (cap);
}

static void
ScreenShot_f (void)
{
	dstring_t  *name = dstring_new ();
	QFile      *file;

	// find a file name to save it to
	if (!(file = QFS_NextFile (name, va (0, "%s/qf", qfs_gamedir->dir.shots),
							   ".png"))) {
		Sys_Printf ("SCR_ScreenShot_f: Couldn't create a PNG file: %s\n",
					name->str);
		dstring_delete (name);
	} else {
		scr_capture_t *cap = malloc (sizeof (scr_capture_t));
		cap->file = file;
		cap->name = name;
		r_funcs->capture_screen (scr_write_caputre, cap);
	}
}

/*
	SCR_SizeUp_f

	Keybinding command
*/
static void
SCR_SizeUp_f (void)
{
	if (scr_viewsize < 120) {
		scr_viewsize = scr_viewsize + 10;
		r_data->vid->recalc_refdef = 1;
	}
}

/*
	SCR_SizeDown_f

	Keybinding command
*/
static void
SCR_SizeDown_f (void)
{
	scr_viewsize = scr_viewsize - 10;
	r_data->vid->recalc_refdef = 1;
}

static void
viewsize_listener (void *data, const cvar_t *cvar)
{
	update_vrect ();
}

void
SCR_Init (void)
{
	r_data->scr_view->xlen = r_data->vid->width;
	r_data->scr_view->ylen = r_data->vid->height;

	// register our commands
	Cmd_AddCommand ("screenshot", ScreenShot_f, "Take a screenshot, "
					"saves as qfxxxx.png in the QF directory");
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f, "Increases the screen size");
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f, "Decreases the screen size");

	scr_initialized = true;

	r_ent_queue = EntQueue_New (mod_num_types);

	cvar_t     *var = Cvar_FindVar ("viewsize");
	Cvar_AddListener (var, viewsize_listener, 0);
	update_vrect ();
}

void
SCR_NewScene (scene_t *scene)
{
	scr_scene = scene;
	if (scene) {
		mod_brush_t *brush = &scr_scene->worldmodel->brush;
		int         count = brush->numnodes + brush->modleafs
							+ brush->numsurfaces;
		int         size = count * sizeof (int);
		r_node_visframes = Hunk_AllocName (0, size, "visframes");
		r_leaf_visframes = r_node_visframes + brush->numnodes;
		r_face_visframes = r_leaf_visframes + brush->modleafs;
		r_funcs->set_fov (tan_fov_x, tan_fov_y);
		r_funcs->R_NewScene (scene);
	} else {
		r_funcs->R_ClearState ();
	}
}
