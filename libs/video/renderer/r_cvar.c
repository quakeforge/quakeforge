/*
	r_cvar.c

	renderer cvar definitions

	Copyright (C) 2000       Bill Currie
	                         Ragnvald Maartmann-Moe IV

	Author: Bill Currie
	        Ragnvald Maartmann-Moe IV
	Date: 2001/5/17

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

#include "QF/cvar.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/render.h"

#include "compat.h"
#include "r_internal.h"

int cl_crossx;
static cvar_t cl_crossx_cvar = {
	.name = "cl_crossx",
	.description =
		"Sets the position of the crosshair on the X-axis.",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_crossx },
};
int cl_crossy;
static cvar_t cl_crossy_cvar = {
	.name = "cl_crossy",
	.description =
		"Sets the position of the crosshair on the Y-axis.",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_crossy },
};
char *cl_verstring;
static cvar_t cl_verstring_cvar = {
	.name = "cl_verstring",
	.description =
		"Client version string",
	.default_value = PACKAGE_VERSION,
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &cl_verstring },
};
int crosshair;
static cvar_t crosshair_cvar = {
	.name = "crosshair",
	.description =
		"Crosshair type. 0 off, 1 old white, 2 new with colors",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &crosshair },
};
int crosshaircolor;
static cvar_t crosshaircolor_cvar = {
	.name = "crosshaircolor",
	.description =
		"Color of the new crosshair",
	.default_value = "79",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &crosshaircolor },
};

float d_mipcap;
static cvar_t d_mipcap_cvar = {
	.name = "d_mipcap",
	.description =
		"Detail level. 0 is highest, 3 is lowest.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &d_mipcap },
};
float d_mipscale;
static cvar_t d_mipscale_cvar = {
	.name = "d_mipscale",
	.description =
		"Detail level of objects. 0 is highest, 3 is lowest.",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &d_mipscale },
};

int r_aliasstats;
static cvar_t r_aliasstats_cvar = {
	.name = "r_polymodelstats",
	.description =
		"Toggles the displays of number of polygon models current being viewed",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_aliasstats },
};
float r_aliastransadj;
static cvar_t r_aliastransadj_cvar = {
	.name = "r_aliastransadj",
	.description =
		"Determines how much of an alias model is clipped away and how much is"
		" viewable.",
	.default_value = "100",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &r_aliastransadj },
};
float r_aliastransbase;
static cvar_t r_aliastransbase_cvar = {
	.name = "r_aliastransbase",
	.description =
		"Determines how much of an alias model is clipped away and how much is"
		" viewable",
	.default_value = "200",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &r_aliastransbase },
};
int r_clearcolor;
static cvar_t r_clearcolor_cvar = {
	.name = "r_clearcolor",
	.description =
		"This sets the color for areas outside of the current map",
	.default_value = "2",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_clearcolor },
};
int r_dlight_lightmap;
static cvar_t r_dlight_lightmap_cvar = {
	.name = "r_dlight_lightmap",
	.description =
		"Set to 1 for high quality dynamic lighting.",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_dlight_lightmap },
};
int r_dlight_max;
static cvar_t r_dlight_max_cvar = {
	.name = "r_dlight_max",
	.description =
		"Number of dynamic lights.",
	.default_value = "32",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_dlight_max },
};
int r_drawentities;
static cvar_t r_drawentities_cvar = {
	.name = "r_drawentities",
	.description =
		"Toggles drawing of entities (almost everything but the world)",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_drawentities },
};
int r_drawexplosions;
static cvar_t r_drawexplosions_cvar = {
	.name = "r_drawexplosions",
	.description =
		"Draw explosions.",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_drawexplosions },
};
int r_drawviewmodel;
static cvar_t r_drawviewmodel_cvar = {
	.name = "r_drawviewmodel",
	.description =
		"Toggles view model drawing (your weapons)",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_drawviewmodel },
};
int r_dspeeds;
static cvar_t r_dspeeds_cvar = {
	.name = "r_dspeeds",
	.description =
		"Toggles the display of drawing speed information",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_dspeeds },
};
int r_dynamic;
static cvar_t r_dynamic_cvar = {
	.name = "r_dynamic",
	.description =
		"Set to 0 to disable lightmap changes",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_dynamic },
};
int r_explosionclip;
static cvar_t r_explosionclip_cvar = {
	.name = "r_explosionclip",
	.description =
		"Clip explosions.",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_explosionclip },
};
float r_farclip;
static cvar_t r_farclip_cvar = {
	.name = "r_farclip",
	.description =
		"Distance of the far clipping plane from the player.",
	.default_value = "4096",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &r_farclip },
};
vec4f_t r_firecolor;
static cvar_t r_firecolor_cvar = {
	.name = "r_firecolor",
	.description =
		"color of rocket and lava ball fires",
	.default_value = "[0.9, 0.7, 0.0]",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_vector, .value = &r_firecolor },
};
int r_flatlightstyles;
static cvar_t r_flatlightstyles_cvar = {
	.name = "r_flatlightstyles",
	.description =
		"Disable animated lightmaps. 2 = use peak, 1 = use average, anything "
		"else = normal",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_flatlightstyles },
};
int r_graphheight;
static cvar_t r_graphheight_cvar = {
	.name = "r_graphheight",
	.description =
		"Set the number of lines displayed in the various graphs",
	.default_value = "32",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_graphheight },
};
int r_lightmap_components;
static cvar_t r_lightmap_components_cvar = {
	.name = "r_lightmap_components",
	.description =
		"Lightmap texture components. 1 is greyscale, 3 is RGB, 4 is RGBA.",
	.default_value = "3",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &r_lightmap_components },
};
int r_maxedges;
static cvar_t r_maxedges_cvar = {
	.name = "r_maxedges",
	.description =
		"Sets the maximum number of edges",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_maxedges },
};
int r_maxsurfs;
static cvar_t r_maxsurfs_cvar = {
	.name = "r_maxsurfs",
	.description =
		"Sets the maximum number of surfaces",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_maxsurfs },
};
float r_mirroralpha;
static cvar_t r_mirroralpha_cvar = {
	.name = "r_mirroralpha",
	.description =
		"None",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &r_mirroralpha },
};
float r_nearclip;
static cvar_t r_nearclip_cvar = {
	.name = "r_nearclip",
	.description =
		"Distance of the near clipping plane from the player.",
	.default_value = "4",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &r_nearclip },
};
int r_norefresh;
static cvar_t r_norefresh_cvar = {
	.name = "r_norefresh_",
	.description =
		"Set to 1 to disable display refresh",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_norefresh },
};
int r_novis;
static cvar_t r_novis_cvar = {
	.name = "r_novis",
	.description =
		"Set to 1 to enable runtime visibility checking (SLOW)",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_novis },
};
int r_numedges;
static cvar_t r_numedges_cvar = {
	.name = "r_numedges",
	.description =
		"Toggles the displaying of number of edges currently being viewed",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_numedges },
};
int r_numsurfs;
static cvar_t r_numsurfs_cvar = {
	.name = "r_numsurfs",
	.description =
		"Toggles the displaying of number of surfaces currently being viewed",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_numsurfs },
};
int r_reportedgeout;
static cvar_t r_reportedgeout_cvar = {
	.name = "r_reportedgeout",
	.description =
		"Toggle the display of how many edges were not displayed",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_reportedgeout },
};
int r_reportsurfout;
static cvar_t r_reportsurfout_cvar = {
	.name = "r_reportsurfout",
	.description =
		"Toggle the display of how many surfaces were not displayed",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_reportsurfout },
};
int r_shadows;
static cvar_t r_shadows_cvar = {
	.name = "r_shadows",
	.description =
		"Set to 1 to enable shadows for entities",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &r_shadows },
};
char *r_skyname;
static cvar_t r_skyname_cvar = {
	.name = "r_skyname",
	.description =
		"name of the current skybox",
	.default_value = "none",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &r_skyname },
};
int r_speeds;
static cvar_t r_speeds_cvar = {
	.name = "r_speeds",
	.description =
		"Display drawing time and statistics of what is being viewed",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_speeds },
};
int r_timegraph;
static cvar_t r_timegraph_cvar = {
	.name = "r_timegraph",
	.description =
		"Toggle the display of a performance graph",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_timegraph },
};
float r_wateralpha;
static cvar_t r_wateralpha_cvar = {
	.name = "r_wateralpha",
	.description =
		"Determine the opacity of liquids. 1 = opaque, 0 = transparent, "
		"otherwise translucent.",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &r_wateralpha },
};
float r_waterripple;
static cvar_t r_waterripple_cvar = {
	.name = "r_waterripple",
	.description =
		"Set to make liquids ripple, try setting to 5",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &r_waterripple },
};
int r_waterwarp;
static cvar_t r_waterwarp_cvar = {
	.name = "r_waterwarp",
	.description =
		"Toggles whether surfaces are warped in liquid.",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_waterwarp },
};
int r_zgraph;
static cvar_t r_zgraph_cvar = {
	.name = "r_zgraph",
	.description =
		"Toggle the graph that reports the changes of z-axis position",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_zgraph },
};

float scr_fov;
static cvar_t scr_fov_cvar = {
	.name = "fov",
	.description =
		"Your field of view in degrees. Smaller than 90 zooms in. Don't touch "
		"in fisheye mode, use ffov instead.",
	.default_value = "90",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scr_fov },
};
int scr_fisheye;
static cvar_t scr_fisheye_cvar = {
	.name = "fisheye",
	.description =
		"Toggles fisheye mode.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_fisheye },
};
int scr_fviews;
static cvar_t scr_fviews_cvar = {
	.name = "fviews",
	.description =
		"The number of fisheye views.",
	.default_value = "6",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_fviews },
};
float scr_ffov;
static cvar_t scr_ffov_cvar = {
	.name = "ffov",
	.description =
		"Your field of view in degrees in fisheye mode.",
	.default_value = "180",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scr_ffov },
};
int scr_showpause;
static cvar_t scr_showpause_cvar = {
	.name = "showpause",
	.description =
		"Toggles display of pause graphic",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_showpause },
};
int scr_showram;
static cvar_t scr_showram_cvar = {
	.name = "showram",
	.description =
		"Show RAM icon if game is running low on memory",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_showram },
};
int scr_showturtle;
static cvar_t scr_showturtle_cvar = {
	.name = "showturtle",
	.description =
		"Show a turtle icon if your fps is below 10",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &scr_showturtle },
};
int scr_viewsize;
static cvar_t scr_viewsize_cvar = {
	.name = "viewsize",
	.description =
		"Set the screen size 30 minimum, 120 maximum",
	.default_value = "100",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &scr_viewsize },
};

int         r_viewsize;

quat_t      crosshair_color;

static void
set_crosshair_color (int col, const viddef_t *vid)
{
	byte       *color;
	color = &vid->palette32[bound (0, col, 255) * 4];
	QuatScale (color, 1.0 / 255, crosshair_color);
}

static void
crosshaircolor_update (void *data, const viddef_t *vid)
{
	set_crosshair_color (crosshaircolor, vid);
}

static void
crosshaircolor_f (void *data, const cvar_t *cvar)
{
	if (!r_data->vid->palette32) {
		// palette not initialized yet
		return;
	}
	set_crosshair_color (crosshaircolor, r_data->vid);
}

static void
r_lightmap_components_f (void *data, const cvar_t *cvar)
{
	switch (r_lightmap_components) {
		case 1:
			mod_lightmap_bytes = 1;
			break;
		case 3:
		case 4:
		default:
			mod_lightmap_bytes = 3;
			break;
	}
}

static void
r_farclip_f (void *data, const cvar_t *cvar)
{
	r_farclip = bound (8.0, r_farclip, Q_MAXFLOAT);
	r_particles_nearclip = bound (r_nearclip, r_particles_nearclip, r_farclip);
	r_data->vid->recalc_refdef = true;
}

static void
r_nearclip_f (void *data, const cvar_t *cvar)
{
	r_nearclip = bound (0.01, r_nearclip, 4.0);
	r_particles_nearclip = bound (r_nearclip, r_particles_nearclip, r_farclip);
	r_data->vid->recalc_refdef = true;
}

static void
scr_fov_f (void *data, const cvar_t *cvar)
{
	// bound field of view
	scr_fov = bound (0, scr_fov, 170);
	SCR_SetFOV (scr_fov);
}

static void
scr_fisheye_f (void *data, const cvar_t *cvar)
{
	if (scr_fisheye)
		Cvar_Set ("fov", "90");
}

static void
scr_ffov_f (void *data, const cvar_t *cvar)
{
	if (scr_ffov < 130)
		Cvar_Set ("fviews", "3");
	else if (scr_ffov < 220)
		Cvar_Set ("fviews", "5");
	else
		Cvar_Set ("fviews", "6");
}

static void
viewsize_f (void *data, const cvar_t *cvar)
{
	scr_viewsize = bound (30, scr_viewsize, 120);
	r_data->vid->recalc_refdef = true;
	r_viewsize = bound (0, scr_viewsize, 100);
	if (r_data->viewsize_callback)
		r_data->viewsize_callback (scr_viewsize);
}

static void
r_dlight_max_f (void *data, const cvar_t *cvar)
{
	R_MaxDlightsCheck (r_dlight_max);
}

void
R_Init_Cvars (void)
{
	Cvar_Register (&cl_crossx_cvar, 0, 0);
	Cvar_Register (&cl_crossy_cvar, 0, 0);
	Cvar_Register (&cl_verstring_cvar, 0, 0);
	Cvar_Register (&crosshair_cvar, 0, 0);
	Cvar_Register (&crosshaircolor_cvar, crosshaircolor_f, 0);
	VID_OnPaletteChange_AddListener (crosshaircolor_update, 0);
	Cvar_Register (&d_mipcap_cvar, 0, 0);
	Cvar_Register (&d_mipscale_cvar, 0, 0);
	Cvar_Register (&r_aliasstats_cvar, 0, 0);
	Cvar_Register (&r_aliastransadj_cvar, 0, 0);
	Cvar_Register (&r_aliastransbase_cvar, 0, 0);
	Cvar_Register (&r_clearcolor_cvar, 0, 0);
	Cvar_Register (&r_dlight_lightmap_cvar, 0, 0);
	Cvar_Register (&r_dlight_max_cvar, r_dlight_max_f, 0);
	Cvar_Register (&r_drawentities_cvar, 0, 0);
	Cvar_Register (&r_drawexplosions_cvar, 0, 0);
	Cvar_Register (&r_drawviewmodel_cvar, 0, 0);
	Cvar_Register (&r_dspeeds_cvar, 0, 0);
	Cvar_Register (&r_dynamic_cvar, 0, 0);
	Cvar_Register (&r_explosionclip_cvar, 0, 0);
	Cvar_Register (&r_farclip_cvar, r_farclip_f, 0);
	Cvar_Register (&r_firecolor_cvar, 0, 0);
	Cvar_Register (&r_flatlightstyles_cvar, 0, 0);
	Cvar_Register (&r_graphheight_cvar, 0, 0);
	Cvar_Register (&r_lightmap_components_cvar, r_lightmap_components_f, 0);
	Cvar_Register (&r_maxedges_cvar, 0, 0);
	Cvar_Register (&r_maxsurfs_cvar, 0, 0);
	Cvar_Register (&r_mirroralpha_cvar, 0, 0);
	Cvar_Register (&r_nearclip_cvar, r_nearclip_f, 0);
	Cvar_Register (&r_norefresh_cvar, 0, 0);
	Cvar_Register (&r_novis_cvar, 0, 0);
	Cvar_Register (&r_numedges_cvar, 0, 0);
	Cvar_Register (&r_numsurfs_cvar, 0, 0);
	Cvar_Register (&r_reportedgeout_cvar, 0, 0);
	Cvar_Register (&r_reportsurfout_cvar, 0, 0);
	Cvar_Register (&r_shadows_cvar, 0, 0);
	Cvar_Register (&r_skyname_cvar, 0, 0);
	Cvar_Register (&r_speeds_cvar, 0, 0);
	Cvar_Register (&r_timegraph_cvar, 0, 0);
	Cvar_Register (&r_wateralpha_cvar, 0, 0);
	Cvar_Register (&r_waterripple_cvar, 0, 0);
	Cvar_Register (&r_waterwarp_cvar, 0, 0);
	Cvar_Register (&r_zgraph_cvar, 0, 0);
	Cvar_Register (&scr_fov_cvar, scr_fov_f, 0);
	Cvar_Register (&scr_fisheye_cvar, scr_fisheye_f, 0);
	Cvar_Register (&scr_fviews_cvar, 0, 0);
	Cvar_Register (&scr_ffov_cvar, scr_ffov_f, 0);
	Cvar_Register (&scr_showpause_cvar, 0, 0);
	Cvar_Register (&scr_showram_cvar, 0, 0);
	Cvar_Register (&scr_showturtle_cvar, 0, 0);
	Cvar_Register (&scr_viewsize_cvar, viewsize_f, 0);

	r_data->graphheight = &r_graphheight;
	r_data->scr_viewsize = &scr_viewsize;

	R_Particles_Init_Cvars ();
}
