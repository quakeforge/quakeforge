#include "QF/cvar.h"

extern cvar_t *gl_sky_divide;		// FIXME
extern void R_ForceLightUpdate (void);

cvar_t     *cl_crossx;
cvar_t     *cl_crossy;
cvar_t     *cl_verstring;
cvar_t     *crosshair;
cvar_t     *crosshaircolor;

cvar_t     *d_mipcap;
cvar_t     *d_mipscale;
cvar_t     *d_subdiv16;

cvar_t     *gl_affinemodels;
cvar_t     *gl_clear;
cvar_t     *gl_conalpha;
cvar_t     *gl_conspin;
cvar_t     *gl_constretch;
cvar_t     *gl_cull; // FIXME
cvar_t     *gl_dlight_lightmap; 
cvar_t     *gl_dlight_polyblend; 
cvar_t     *gl_dlight_smooth; 
cvar_t     *gl_fb_bmodels;
cvar_t     *gl_fb_models;
cvar_t     *gl_fires;
cvar_t     *gl_keeptjunctions;
cvar_t     *gl_lightmap_components;
cvar_t     *gl_lightmode;
cvar_t     *gl_max_size;
cvar_t     *gl_nocolors;
cvar_t     *gl_picmip;
cvar_t     *gl_playermip;
cvar_t     *gl_polyblend; // FIXME
cvar_t     *gl_reporttjunctions;
cvar_t     *gl_sky_clip;
cvar_t     *gl_skymultipass;
cvar_t     *gl_smoothmodels; // FIXME
cvar_t     *gl_texsort;
cvar_t     *gl_triplebuffer;

cvar_t     *r_aliasstats;
cvar_t     *r_aliastransadj;
cvar_t     *r_aliastransbase;
cvar_t     *r_ambient;
cvar_t     *r_clearcolor;
cvar_t     *r_drawentities;
cvar_t     *r_drawflat;
cvar_t     *r_draworder;
cvar_t     *r_drawviewmodel;
cvar_t     *r_dspeeds;
cvar_t     *r_dynamic;
cvar_t     *r_fullbright;
cvar_t     *r_graphheight;
cvar_t     *r_lightmap;
cvar_t     *r_maxedges;
cvar_t     *r_maxsurfs;
cvar_t     *r_mirroralpha;
cvar_t     *r_netgraph;
cvar_t     *r_norefresh;
cvar_t     *r_novis;
cvar_t     *r_numedges;
cvar_t     *r_numsurfs;
cvar_t     *r_particles;
cvar_t     *r_reportedgeout;
cvar_t     *r_reportsurfout;
cvar_t     *r_shadows;
cvar_t     *r_skyname;
cvar_t     *r_speeds;
cvar_t     *r_timegraph;
cvar_t     *r_wateralpha;
cvar_t     *r_waterripple;
cvar_t     *r_waterwarp;

cvar_t     *scr_centertime;
cvar_t     *scr_consize;
cvar_t     *scr_conspeed;
cvar_t     *scr_fov;
cvar_t     *scr_printspeed;
cvar_t     *scr_showpause;
cvar_t     *scr_showram;
cvar_t     *scr_showturtle;
cvar_t     *scr_viewsize;

unsigned char lighthalf_v[3];
qboolean    lighthalf;

void
gl_lightmode_callback (cvar_t *cvar)
{
	if (cvar->int_val) {
		lighthalf_v[0] = lighthalf_v[1] = lighthalf_v[2] = 128;
		lighthalf = 1;
	} else {
		lighthalf_v[0] = lighthalf_v[1] = lighthalf_v[2] = 255;
		lighthalf = 0;
	}

	R_ForceLightUpdate ();
}

void
R_Init_Cvars (void)
{

	cl_crossx =	Cvar_Get ("cl_crossx", "0", CVAR_ARCHIVE, NULL,
						  "Sets the position of the crosshair on the X-axis");
	cl_crossx = Cvar_Get ("cl_crossx", "0", CVAR_ARCHIVE, NULL,
						  "Sets the position of the crosshair on the X-axis.");
	cl_crossy = Cvar_Get ("cl_crossy", "0", CVAR_ARCHIVE, NULL,
						  "Sets the position of the crosshair on the Y-axis");
	cl_crossy = Cvar_Get ("cl_crossy", "0", CVAR_ARCHIVE, NULL,
						  "Sets the position of the crosshair on the Y-axis.");
	cl_verstring = Cvar_Get ("cl_verstring", PROGRAM " " VERSION, CVAR_NONE,
							 NULL, "Client version string");
	crosshair =
		Cvar_Get ("crosshair", "0", CVAR_ARCHIVE, NULL,
				  "Crosshair type. 0 off, 1 old without color, 2 new with colors");
	crosshaircolor = Cvar_Get ("crosshaircolor", "79", CVAR_ARCHIVE, NULL,
							   "Color of the new crosshair");
	d_mipcap = Cvar_Get ("d_mipcap", "0", CVAR_NONE, NULL,
						 "Detail level. 0 is highest, 3 is lowest.");
	d_mipscale =
		Cvar_Get ("d_mipscale", "1", CVAR_NONE, NULL,
				  "Detail level of objects. 0 is highest, 3 is lowest");
	d_subdiv16 = Cvar_Get ("d_subdiv16", "1", CVAR_NONE, NULL,
						   "Set to enable extreme perspective correction");
	gl_affinemodels =
		Cvar_Get ("gl_affinemodels", "0", CVAR_NONE, NULL,
				  "Makes texture rendering quality better if set to 1");
	gl_clear = Cvar_Get ("gl_clear", "0", CVAR_NONE, NULL,
						 "Set to 1 to make background black. Useful for removing HOM effect"); 
	gl_conalpha = Cvar_Get ("gl_conalpha", "0.6", CVAR_ARCHIVE, NULL,
							"alpha value for the console background");
	gl_conspin = Cvar_Get ("gl_conspin", "0", CVAR_ARCHIVE, NULL,
						   "speed at which the console spins");
	gl_constretch = Cvar_Get ("gl_constretch", "0", CVAR_ARCHIVE, NULL,
							  "whether slide the console or stretch it");
	gl_cull = Cvar_Get ("gl_cull", "1", CVAR_NONE, NULL, "None");
    gl_dlight_lightmap = 
        Cvar_Get ("gl_dlight_lightmap", "1", CVAR_ARCHIVE, NULL, 
                  "Set to 1 for high quality dynamic lighting."); 
    gl_dlight_polyblend = 
        Cvar_Get ("gl_dlight_polyblend", "0", CVAR_ARCHIVE, NULL, 
                  "Set to 1 to use a dynamic light effect faster on GL"); 
    gl_dlight_smooth = 
        Cvar_Get ("gl_dlight_smooth", "1", CVAR_ARCHIVE, NULL, 
                  "Smooth dynamic vertex lighting"); 
	gl_fb_bmodels = Cvar_Get ("gl_fb_bmodels", "1", CVAR_ARCHIVE, NULL,
							  "Toggles fullbright color support for bmodels");
	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_ARCHIVE, NULL,
							 "Toggles fullbright color support for models..  "
							 "This is very handy, but costs me 2 FPS.. (=:]");
	gl_fires = Cvar_Get ("gl_fires", "0", CVAR_ARCHIVE, NULL,
						 "Toggles lavaball and rocket fireballs");
	gl_keeptjunctions = Cvar_Get ("gl_keeptjunctions", "1", CVAR_NONE, NULL, "None");
	gl_lightmap_components =
		Cvar_Get ("gl_lightmap_components", "4", CVAR_ROM, NULL,
				  "Lightmap texture components. 1 is greyscale, 3 is RGB, 4 is RGBA.");
	gl_lightmode =
		Cvar_Get ("gl_lightmode", "1", CVAR_ARCHIVE, gl_lightmode_callback,
				  "Lighting mode (0 = GLQuake style, 1 = new style)");
	gl_max_size = Cvar_Get ("gl_max_size", "1024", CVAR_NONE, NULL,
							"Texture dimension"); 
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, NULL,
							"Set to 1, turns off all player colors");
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_NONE, NULL,
						  "Dimensions of displayed textures. 0 is normal, 1 is half, 2 is 1/4"); 
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, NULL,
							 "Detail of player skins. 0 best, 4 worst.");
	gl_reporttjunctions = Cvar_Get ("gl_reporttjunctions", "0", CVAR_NONE, NULL, "None");
	gl_sky_clip = 
        Cvar_Get ("gl_sky_clip", "0", CVAR_ARCHIVE, NULL, 
                  "controls whether sky is drawn first (0) or later (1)"); 
	gl_skymultipass = Cvar_Get ("gl_skymultipass", "1", CVAR_NONE, NULL,
								"controls wether the skydome is single or double pass");
	gl_smoothmodels = Cvar_Get ("gl_smoothmodels", "1", CVAR_NONE, NULL, "None");
	gl_texsort = Cvar_Get ("gl_texsort", "1", CVAR_NONE, NULL, "None");
	gl_triplebuffer = Cvar_Get ("gl_triplebuffer", "1", CVAR_ARCHIVE, NULL, "Set to 1 by default. Fixes status bar flicker on some hardware");

	r_aliasstats = Cvar_Get ("r_polymodelstats", "0", CVAR_NONE, NULL, "None");
	r_aliastransadj = Cvar_Get ("r_aliastransadj", "100", CVAR_NONE, NULL, "None");
	r_aliastransbase = Cvar_Get ("r_aliastransbase", "200", CVAR_NONE, NULL, "None");
	r_ambient = Cvar_Get ("r_ambient", "0", CVAR_NONE, NULL, "None");
	r_clearcolor = Cvar_Get ("r_clearcolor", "2", CVAR_NONE, NULL, "None");
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, NULL, "None");
	r_drawflat = Cvar_Get ("r_drawflat", "0", CVAR_NONE, NULL, "None");
	r_draworder = Cvar_Get ("r_draworder", "0", CVAR_NONE, NULL, "None");
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_NONE, NULL, "None");
	r_dspeeds = Cvar_Get ("r_dspeeds", "0", CVAR_NONE, NULL, "None");
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, NULL, "None");
	r_fullbright = Cvar_Get ("r_fullbright", "0", CVAR_NONE, NULL, "None");
	r_graphheight = Cvar_Get ("r_graphheight", "10", CVAR_NONE, NULL, "None");
	r_lightmap = Cvar_Get ("r_lightmap", "0", CVAR_NONE, NULL, "None");
	r_maxedges = Cvar_Get ("r_maxedges", "0", CVAR_NONE, NULL, "None");
	r_maxsurfs = Cvar_Get ("r_maxsurfs", "0", CVAR_NONE, NULL, "None");
	r_mirroralpha = Cvar_Get ("r_mirroralpha", "1", CVAR_NONE, NULL, "None");
	r_netgraph = Cvar_Get ("r_netgraph", "0", CVAR_NONE, NULL, "None");
	r_norefresh = Cvar_Get ("r_norefresh", "0", CVAR_NONE, NULL, "None");
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, NULL, "None");
	r_numedges = Cvar_Get ("r_numedges", "0", CVAR_NONE, NULL, "None");
	r_numsurfs = Cvar_Get ("r_numsurfs", "0", CVAR_NONE, NULL, "None");
	r_particles = Cvar_Get ("r_particles", "1", CVAR_ARCHIVE, NULL, "whether or not to draw particles");
	r_reportedgeout = Cvar_Get ("r_reportedgeout", "0", CVAR_NONE, NULL, "None");
	r_reportsurfout = Cvar_Get ("r_reportsurfout", "0", CVAR_NONE, NULL, "None");
	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_NONE, NULL, "None");
	r_skyname = Cvar_Get ("r_skyname", "none", CVAR_NONE, NULL, "name of the current skybox");
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, NULL, "None");
	r_timegraph = Cvar_Get ("r_timegraph", "0", CVAR_NONE, NULL, "None");
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, NULL, "None");
	r_waterripple = Cvar_Get ("r_waterripple", "0", CVAR_NONE, NULL, "None");
	r_waterwarp = Cvar_Get ("r_waterwarp", "1", CVAR_NONE, NULL, "None");

	scr_centertime = Cvar_Get ("scr_centertime", "2", CVAR_NONE, NULL, "How long in seconds the screen hints are displayed on the screen");
	scr_consize = Cvar_Get ("scr_consize", "0.5", CVAR_ARCHIVE, NULL, "fraction of the screen the console covers when down");
	scr_conspeed = Cvar_Get ("scr_conspeed", "300", CVAR_NONE, NULL, "How quickly in the console screen scrolls up and down");
	scr_fov = Cvar_Get ("fov", "90", CVAR_NONE, NULL, "field of view. 90 is normal, smaller numbers zoom");
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", CVAR_NONE, NULL, "How fast the text is displayed at the end of the single player episodes");
	scr_showpause = Cvar_Get ("showpause", "1", CVAR_NONE, NULL, "Toggles display of pause graphic");
	scr_showram = Cvar_Get ("showram", "1", CVAR_NONE, NULL, "Show ram icon when low on ram in game");
	scr_showturtle = Cvar_Get ("showturtle", "0", CVAR_NONE, NULL, "Show turtle icon when fps is lower than 10");
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE, NULL, "Set the screen size 30 minimum, 120 maximum");
}
