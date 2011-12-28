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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include "QF/cvar.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/render.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_dynamic.h"

cvar_t	   *easter_eggs;

cvar_t     *cl_crossx;
cvar_t     *cl_crossy;
cvar_t     *cl_verstring;   
cvar_t     *crosshair;
cvar_t     *crosshaircolor;

cvar_t     *d_mipcap;
cvar_t     *d_mipscale;

cvar_t     *gl_affinemodels;
cvar_t     *gl_clear;
cvar_t     *gl_conspin;
cvar_t     *gl_constretch;
cvar_t     *gl_dlight_polyblend;
cvar_t     *gl_dlight_smooth;
cvar_t     *gl_driver;
cvar_t     *gl_fb_models;
cvar_t     *gl_keeptjunctions;
cvar_t     *gl_lerp_anim;
cvar_t     *gl_lightmap_align;
cvar_t     *gl_lightmap_subimage;
cvar_t     *gl_nocolors;
cvar_t	   *gl_overbright;
cvar_t     *gl_particle_mip;
cvar_t     *gl_particle_size;
cvar_t     *gl_picmip;
cvar_t     *gl_playermip;
cvar_t     *gl_reporttjunctions;
cvar_t     *gl_sky_clip;
cvar_t     *gl_sky_debug;
cvar_t     *gl_sky_multipass;
cvar_t     *gl_texsort;
cvar_t     *gl_triplebuffer;

cvar_t     *r_aliasstats;
cvar_t     *r_aliastransadj;
cvar_t     *r_aliastransbase;
cvar_t     *r_ambient;
cvar_t     *r_clearcolor;
cvar_t     *r_dlight_lightmap;
cvar_t	   *r_dlight_max;
cvar_t     *r_drawentities;
cvar_t     *r_drawexplosions;
cvar_t     *r_drawflat;
cvar_t     *r_drawviewmodel;
cvar_t     *r_dspeeds;
cvar_t     *r_dynamic;
cvar_t     *r_explosionclip;
cvar_t     *r_farclip;
cvar_t     *r_firecolor;
cvar_t     *r_graphheight;
cvar_t     *r_lightmap_components;
cvar_t     *r_maxedges;
cvar_t     *r_maxsurfs;
cvar_t     *r_mirroralpha;
cvar_t	   *r_nearclip;
cvar_t     *r_netgraph;
cvar_t     *r_netgraph_alpha;
cvar_t     *r_netgraph_box;
cvar_t     *r_norefresh;
cvar_t     *r_novis;
cvar_t     *r_numedges;
cvar_t     *r_numsurfs;
cvar_t     *r_particles;
cvar_t	   *r_particles_style;
cvar_t	   *r_particles_max;
cvar_t	   *r_particles_nearclip;
cvar_t     *r_reportedgeout;
cvar_t     *r_reportsurfout;
cvar_t     *r_shadows;
cvar_t     *r_skyname;
cvar_t     *r_speeds;
cvar_t     *r_timegraph;
cvar_t     *r_wateralpha;
cvar_t     *r_waterripple;
cvar_t     *r_waterwarp;
cvar_t     *r_zgraph;

cvar_t     *scr_centertime;
cvar_t     *scr_fov;
cvar_t     *scr_fisheye;
cvar_t     *scr_fviews;
cvar_t     *scr_ffov;
cvar_t     *scr_printspeed;
cvar_t     *scr_showpause;
cvar_t     *scr_showram;
cvar_t     *scr_showturtle;
cvar_t     *scr_viewsize;

void      (*r_viewsize_callback)(cvar_t *var);
int         r_viewsize;

float		cl_wateralpha;
quat_t      crosshair_color;

static void
crosshaircolor_f (cvar_t *var)
{
	byte       *color;
	color = (byte *) &d_8to24table[bound (0, var->int_val, 255)];
	QuatScale (color, 1.0 / 255, crosshair_color);
}

static void
r_particles_f (cvar_t *var)
{
	R_MaxParticlesCheck (var, r_particles_max);
}

static void
r_particles_max_f (cvar_t *var)
{
	R_MaxParticlesCheck (r_particles, var);
}

static void
r_lightmap_components_f (cvar_t *var)
{
	switch (var->int_val) {
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
r_farclip_f (cvar_t *var)
{
	Cvar_SetValue (r_farclip, bound (8.0, var->value, Q_MAXFLOAT));
	if (r_particles_nearclip && r_nearclip)
		Cvar_SetValue (r_particles_nearclip,
					   bound (r_nearclip->value, r_particles_nearclip->value,
							  r_farclip->value));
}

static void
r_nearclip_f (cvar_t *var)
{
	Cvar_SetValue (r_nearclip, bound (0.01, var->value, 4.0));
	if (r_particles_nearclip && r_farclip)
		Cvar_SetValue (r_particles_nearclip,
					   bound (r_nearclip->value, r_particles_nearclip->value,
							  r_farclip->value));
}

static void
r_particles_nearclip_f (cvar_t *var)
{
	Cvar_SetValue (r_particles_nearclip, bound (r_nearclip->value, var->value,
												r_farclip->value));
}

static void
scr_fisheye_f (cvar_t *var)
{
	if (var->int_val)
		Cvar_Set (scr_fov, "90");
}

static void
scr_ffov_f (cvar_t *var)
{
	if (var->value < 130)
		Cvar_Set (scr_fviews, "3");
	else if (var->value < 220)
		Cvar_Set (scr_fviews, "5");
	else
		Cvar_Set (scr_fviews, "6");
}

static void
viewsize_f (cvar_t *var)
{
	if (var->int_val < 30 || var->int_val > 120) {
		Cvar_SetValue (var, bound (30, var->int_val, 120));
	} else {
		vid.recalc_refdef = true;
		r_viewsize = bound (0, var->int_val, 100);
		if (r_viewsize_callback)
			r_viewsize_callback (var);
	}
}

void
R_Init_Cvars (void)
{
	easter_eggs = Cvar_Get ("easter_eggs", "0", CVAR_NONE, r_easter_eggs_f,
							"Enables easter eggs.");

	cl_crossx = Cvar_Get ("cl_crossx", "0", CVAR_ARCHIVE, NULL,
						  "Sets the position of the crosshair on the X-axis.");
	cl_crossy = Cvar_Get ("cl_crossy", "0", CVAR_ARCHIVE, NULL,
						  "Sets the position of the crosshair on the Y-axis.");
	cl_verstring = Cvar_Get ("cl_verstring", PACKAGE_VERSION, CVAR_NONE,
							 NULL, "Client version string");
	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE, NULL, "Crosshair "
						  "type. 0 off, 1 old white, 2 new with colors");
	crosshaircolor = Cvar_Get ("crosshaircolor", "79", CVAR_ARCHIVE,
							   crosshaircolor_f, "Color of the new crosshair");
	d_mipcap = Cvar_Get ("d_mipcap", "0", CVAR_NONE, NULL,
						 "Detail level. 0 is highest, 3 is lowest.");
	d_mipscale = Cvar_Get ("d_mipscale", "1", CVAR_NONE, NULL, "Detail level "
						   "of objects. 0 is highest, 3 is lowest.");
	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_ARCHIVE, NULL,
								"Makes texture rendering quality better if "
								"set to 1");
	gl_clear = Cvar_Get ("gl_clear", "0", CVAR_NONE, NULL, "Set to 1 to make "
						 "background black. Useful for removing HOM effect");
	gl_conspin = Cvar_Get ("gl_conspin", "0", CVAR_ARCHIVE, NULL,
						   "speed at which the console spins");
	gl_constretch = Cvar_Get ("gl_constretch", "0", CVAR_ARCHIVE, NULL,
							  "toggle console between slide and stretch");
	gl_dlight_polyblend = Cvar_Get ("gl_dlight_polyblend", "0", CVAR_ARCHIVE,
									NULL, "Set to 1 to use a dynamic light "
									"effect faster on GL");
	gl_dlight_smooth = Cvar_Get ("gl_dlight_smooth", "1", CVAR_ARCHIVE, NULL,
								 "Smooth dynamic vertex lighting");
	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_ARCHIVE, NULL,
							 "Toggles fullbright color support for models");
	gl_keeptjunctions = Cvar_Get ("gl_keeptjunctions", "1", CVAR_ARCHIVE, NULL,
								  "Set to 0 to turn off colinear vertexes "
								  "upon level load.");
	gl_lerp_anim = Cvar_Get ("gl_lerp_anim", "1", CVAR_ARCHIVE, NULL,
							 "Toggles model animation interpolation");

	gl_driver = Cvar_Get ("gl_driver", GL_DRIVER, CVAR_ROM, NULL,
						  "The OpenGL library to use. (path optional)");
	gl_lightmap_align = Cvar_Get ("gl_lightmap_align", "1", CVAR_NONE, NULL,
								  "Workaround for nvidia slow path. Set to 4 "
								  "or 16 if you have an nvidia 3d "
								  "accelerator, set to 1 otherwise.");
	gl_lightmap_subimage = Cvar_Get ("gl_lightmap_subimage", "1", CVAR_NONE,
									 NULL, "Lightmap Update method. Default 2 "
									 "updates a minimum 'dirty rectangle' "
									 "around the area changed. 1 updates "
									 "every line that changed. 0 updates the "
									 "entire lightmap.");
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL,
							"Set to 1, turns off all player colors");
	gl_overbright = Cvar_Get ("gl_overbright", "0", CVAR_NONE,
							  gl_overbright_f, "Darken lightmaps so that "
							  "dynamic lights can be overbright. 1 = 0.75 "
							  "brightness, 2 = 0.5 brightness.");
	gl_particle_mip = Cvar_Get ("gl_particle_mip", "0", CVAR_NONE, NULL,
								"Toggles particle texture mipmapping.");
	gl_particle_size = Cvar_Get ("gl_particle_size", "5", CVAR_NONE, NULL,
								 "Vertical and horizontal size of particle "
								 "textures as a power of 2. Default is 5 "
								 "(32 texel square).");
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, NULL, "Dimensions of "
						  "textures. 0 is normal, 1 is half, 2 is 1/4");
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL,
							 "Detail of player skins. 0 best, 4 worst.");
	gl_reporttjunctions = Cvar_Get ("gl_reporttjunctions", "0", CVAR_NONE,
									NULL, "None");
	gl_sky_clip = Cvar_Get ("gl_sky_clip", "2", CVAR_ARCHIVE, NULL,
							"controls amount of sky overdraw");
	gl_sky_debug = Cvar_Get ("gl_sky_debug", "0", CVAR_NONE, NULL,
							 "debugging `info' for sky clipping");
	gl_sky_divide = Cvar_Get ("gl_sky_divide", "1", CVAR_ARCHIVE, NULL,
							  "subdivide sky polys");
	gl_sky_multipass = Cvar_Get ("gl_sky_multipass", "1", CVAR_ARCHIVE, NULL,
								"controls whether the skydome is single or "
								"double pass");
	gl_texsort = Cvar_Get ("gl_texsort", "1", CVAR_NONE, NULL, "None");
	gl_triplebuffer = Cvar_Get ("gl_triplebuffer", "1", CVAR_ARCHIVE, NULL,
								"Set to 1 by default. Fixes status bar "
								"flicker on some hardware");
	r_aliasstats = Cvar_Get ("r_polymodelstats", "0", CVAR_NONE, NULL,
							 "Toggles the displays of number of polygon "
							 "models current being viewed");
	r_aliastransadj = Cvar_Get ("r_aliastransadj", "100", CVAR_NONE, NULL,
								"Determines how much of an alias model is "
								"clipped away and how much is viewable.");
	r_aliastransbase = Cvar_Get ("r_aliastransbase", "200", CVAR_NONE, NULL,
								 "Determines how much of an alias model is "
								 "clipped away and how much is viewable");
	r_ambient = Cvar_Get ("r_ambient", "0", CVAR_NONE, NULL,
						  "Determines the ambient lighting for a level");
	r_clearcolor = Cvar_Get ("r_clearcolor", "2", CVAR_NONE, NULL,
							 "This sets the color for areas outside of the "
							 "current map");
	r_dlight_lightmap = Cvar_Get ("r_dlight_lightmap", "1", CVAR_ARCHIVE,
								  NULL, "Set to 1 for high quality dynamic "
								  "lighting.");
	r_dlight_max = Cvar_Get ("r_dlight_max", "32", CVAR_ARCHIVE,
							 R_MaxDlightsCheck, "Number of dynamic lights.");
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, NULL,
							   "Toggles drawing of entities (almost "
							   "everything but the world)");
	r_drawexplosions = Cvar_Get ("r_drawexplosions", "1", CVAR_ARCHIVE, NULL,
								 "Draw explosions.");
	r_drawflat = Cvar_Get ("r_drawflat", "0", CVAR_NONE, NULL,
						   "Toggles the drawing of textures");
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_ARCHIVE, NULL,
								"Toggles view model drawing (your weapons)");
	r_dspeeds = Cvar_Get ("r_dspeeds", "0", CVAR_NONE, NULL,
						  "Toggles the display of drawing speed information");
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, NULL,
						  "Set to 0 to disable lightmap changes");
	r_explosionclip = Cvar_Get ("r_explosionclip", "0", CVAR_ARCHIVE, NULL,
								"Clip explosions.");
	r_farclip = Cvar_Get ("r_farclip", "4096", CVAR_ARCHIVE, r_farclip_f,
						  "Distance of the far clipping plane from the "
						  "player.");
	r_firecolor = Cvar_Get ("r_firecolor", "0.9 0.7 0.0", CVAR_ARCHIVE, NULL,
							"color of rocket and lava ball fires");
	r_graphheight = Cvar_Get ("r_graphheight", "32", CVAR_NONE, NULL,
							  "Set the number of lines displayed in the "
							  "various graphs");
	r_lightmap_components = Cvar_Get ("r_lightmap_components", "3", CVAR_ROM,
									  r_lightmap_components_f,
									  "Lightmap texture components. 1 "
									  "is greyscale, 3 is RGB, 4 is RGBA.");
	r_maxedges = Cvar_Get ("r_maxedges", "0", CVAR_NONE, NULL,
						   "Sets the maximum number of edges");
	r_maxsurfs = Cvar_Get ("r_maxsurfs", "0", CVAR_NONE, NULL,
						   "Sets the maximum number of surfaces");
	r_mirroralpha = Cvar_Get ("r_mirroralpha", "1", CVAR_NONE, NULL, "None");
	r_nearclip = Cvar_Get ("r_nearclip", "4", CVAR_ARCHIVE, r_nearclip_f,
						   "Distance of the near clipping plane from the "
						   "player.");
	r_netgraph = Cvar_Get ("r_netgraph", "0", CVAR_NONE, NULL,
						   "Toggle the display of a graph showing network "
						   "performance");
	r_netgraph_alpha = Cvar_Get ("r_netgraph_alpha", "0.5", CVAR_ARCHIVE, NULL,
								 "Net graph translucency");
	r_netgraph_box = Cvar_Get ("r_netgraph_box", "1", CVAR_ARCHIVE, NULL,
							   "Draw box around net graph");
	r_norefresh = Cvar_Get ("r_norefresh_", "0", CVAR_NONE, NULL,
							"Set to 1 to disable display refresh");
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL, "Set to 1 to enable "
						"runtime visibility checking (SLOW)");
	r_numedges = Cvar_Get ("r_numedges", "0", CVAR_NONE, NULL,
						   "Toggles the displaying of number of edges "
						   "currently being viewed");
	r_numsurfs = Cvar_Get ("r_numsurfs", "0", CVAR_NONE, NULL,
						   "Toggles the displaying of number of surfaces "
						   "currently being viewed");
	r_particles = Cvar_Get ("r_particles", "1", CVAR_ARCHIVE, r_particles_f,
							"Toggles drawing of particles.");
	r_particles_max = Cvar_Get ("r_particles_max", "2048", CVAR_ARCHIVE,
								r_particles_max_f, "Maximum amount of "
								"particles to display. No maximum, minimum "
								"is 0.");
	r_particles_nearclip = Cvar_Get ("r_particles_nearclip", "32",
									 CVAR_ARCHIVE, r_particles_nearclip_f,
									 "Distance of the particle near clipping "
									 "plane from the player.");
	r_particles_style = Cvar_Get ("r_particles_style", "1", CVAR_ARCHIVE,
								  r_particles_style_f, "Sets particle style. "
								  "0 for Id, 1 for QF.");
	r_reportedgeout = Cvar_Get ("r_reportedgeout", "0", CVAR_NONE, NULL,
								"Toggle the display of how many edges were "
								"not displayed");
	r_reportsurfout = Cvar_Get ("r_reportsurfout", "0", CVAR_NONE, NULL,
								"Toggle the display of how many surfaces "
								"were not displayed");
	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_ARCHIVE, NULL,
						  "Set to 1 to enable shadows for entities");
	r_skyname = Cvar_Get ("r_skyname", "none", CVAR_NONE, NULL,
						  "name of the current skybox");
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, NULL, "Display drawing "
						 "time and statistics of what is being viewed");
	r_timegraph = Cvar_Get ("r_timegraph", "0", CVAR_NONE, NULL,
							"Toggle the display of a performance graph");
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_ARCHIVE, NULL,
							 "Determine the opacity of liquids. 1 = opaque, "
							 "0 = transparent, otherwise translucent.");
	r_waterripple = Cvar_Get ("r_waterripple", "0", CVAR_NONE, NULL,
							  "Set to make liquids ripple, try setting to 5");
	r_waterwarp = Cvar_Get ("r_waterwarp", "1", CVAR_NONE, NULL,
							"Toggles whether surfaces are warped in liquid.");
	r_zgraph = Cvar_Get ("r_zgraph", "0", CVAR_NONE, NULL,
						 "Toggle the graph that reports the changes of "
						 "z-axis position");
	scr_centertime = Cvar_Get ("scr_centertime", "2", CVAR_NONE, NULL, "How "
							   "long in seconds screen hints are displayed");
	scr_fov = Cvar_Get ("fov", "90", CVAR_NONE, NULL, "Your field of view in "
						"degrees. Smaller than 90 zooms in. Don't touch in "
						"fisheye mode, use ffov instead.");
	scr_fisheye = Cvar_Get ("fisheye", "0", CVAR_NONE, scr_fisheye_f,
							"Toggles fisheye mode.");
	scr_fviews = Cvar_Get ("fviews", "6", CVAR_NONE, NULL, "The number of "
						   "fisheye views.");
	scr_ffov = Cvar_Get ("ffov", "180", CVAR_NONE, scr_ffov_f, "Your field of "
						 "view in degrees in fisheye mode.");
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", CVAR_NONE, NULL,
							   "How fast the text is displayed at the end of "
							   "the single player episodes");
	scr_showpause = Cvar_Get ("showpause", "1", CVAR_NONE, NULL,
							  "Toggles display of pause graphic");
	scr_showram = Cvar_Get ("showram", "1", CVAR_NONE, NULL,
							"Show RAM icon if game is running low on memory");
	scr_showturtle = Cvar_Get ("showturtle", "0", CVAR_NONE, NULL,
							   "Show a turtle icon if your fps is below 10");
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE, viewsize_f,
							 "Set the screen size 30 minimum, 120 maximum");
}
