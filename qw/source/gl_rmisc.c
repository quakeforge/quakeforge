/*
	gl_rmisc.c

	(description)

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

	$Id$
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

#include <stdio.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "glquake.h"
#include "r_dynamic.h"
#include "skin.h"
#include "QF/sys.h"
#include "vid.h"

qboolean VID_Is8bit (void);
void R_InitBubble (void);

cvar_t	*gl_fires;

cvar_t	*r_netgraph_alpha;
cvar_t	*r_netgraph_box;

extern cvar_t	*gl_lerp_anim;

extern cvar_t	*r_netgraph;

qboolean	allowskybox;				// allow skyboxes?  --KB

/*
	R_Textures_Init
*/
void
R_Textures_Init (void)
{
	int         x, y, m;
	byte       *dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip =
		Hunk_AllocName (sizeof (texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2,
						"notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof (texture_t);

	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m = 0; m < 4; m++) {
		dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++) {
			for (x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
		}
	}
}

/*
	R_Envmap_f

	Grab six views for environment mapping tests
*/
void
R_Envmap_f (void)
{
	byte        buffer[256 * 256 * 4];

	glDrawBuffer (GL_FRONT);
	glReadBuffer (GL_FRONT);
	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env0.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env1.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env2.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env3.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env4.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env5.rgb", buffer, sizeof (buffer));

	envmap = false;
	glDrawBuffer (GL_BACK);
	glReadBuffer (GL_BACK);
	GL_EndRendering ();
}

/*
   R_LoadSky_f
*/
void
R_LoadSky_f (void)
{
	if (Cmd_Argc () != 2) {
		Con_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	R_LoadSkys (Cmd_Argv (1));
}


/*
	R_Init
*/
void
R_Init (void)
{
	allowskybox = false;				// server will decide if this is
										// allowed  --KB

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f, "Tests the current refresh rate for the current location");
	Cmd_AddCommand ("envmap", R_Envmap_f, "FIXME: What on earth does this do? No Description");
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f, "Load a pointfile to determine map leaks");
	Cmd_AddCommand ("loadsky", R_LoadSky_f, "Load a skybox");

	R_InitBubble ();

	R_InitParticles ();

	netgraphtexture = texture_extension_number;
	texture_extension_number++;

	playertextures = texture_extension_number;
	texture_extension_number += MAX_CLIENTS;
	player_fb_textures = texture_extension_number;
	texture_extension_number += MAX_CACHED_SKINS;
}

void
R_Init_Cvars (void)
{
	r_norefresh = Cvar_Get ("r_norefresh", "0", CVAR_NONE, 0, "Set to 1 to disable display refresh");
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, 0, "Toggles drawing of entities (almost everything but the world)");
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_ARCHIVE, 0, "Toggles drawing of view models (your weapons)");
	r_shadows = Cvar_Get ("r_shadows", "0", CVAR_ARCHIVE, 0, "Set to 1 to enable shadows for entities");
	r_wateralpha = Cvar_Get ("r_wateralpha", "1", CVAR_NONE, 0, "Determine opacity of liquids. 1 = solid, 0 = transparent, otherwise translucent.");
	/* FIXME what does r_waterripple use for units? */
	r_waterripple = Cvar_Get ("r_waterripple", "0", CVAR_NONE, 0, "Set to make liquids ripple, a good setting is 5");
	r_dynamic = Cvar_Get ("r_dynamic", "1", CVAR_NONE, 0, "Set to 0 to disable lightmap changes");
	r_novis = Cvar_Get ("r_novis", "0", CVAR_NONE, 0, "Set to 1 to enable runtime visibility checking (SLOW)");
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, 0, "Display drawing time and statistics of what is being viewed");
	r_netgraph = Cvar_Get ("r_netgraph", "0", CVAR_ARCHIVE, 0, "Graph network stats");
	r_netgraph_alpha = Cvar_Get ("r_netgraph_alpha", "0.5", CVAR_ARCHIVE, 0, "Net graph translucency");
	r_netgraph_box = Cvar_Get ("r_netgraph_box", "1", CVAR_ARCHIVE, 0, "Draw box around net graph");
	r_particles = Cvar_Get ("r_particles", "1", CVAR_ARCHIVE, 0, "whether or not to draw particles");
	r_skyname = Cvar_Get ("r_skyname", "none", CVAR_NONE, 0, "name of the current skybox");

	gl_affinemodels = Cvar_Get ("gl_affinemodels", "0", CVAR_ARCHIVE, 0, "Makes texture rendering quality better if set to 1");
	gl_clear = Cvar_Get ("gl_clear", "0", CVAR_NONE, 0, "Set to 1 to make background black. Useful for removing HOM effect");
	gl_dlight_lightmap = Cvar_Get ("gl_dlight_lightmap", "1", CVAR_ARCHIVE, 0, "Set to 1 for high quality dynamic lighting.");
	gl_dlight_polyblend = Cvar_Get ("gl_dlight_polyblend", "0", CVAR_ARCHIVE, 0, "Set to 1 to use a dynamic light effect faster on GL");
	gl_dlight_smooth = Cvar_Get ("gl_dlight_smooth", "1", CVAR_ARCHIVE, 0, "Smooth dynamic vertex lighting");
	gl_fb_bmodels = Cvar_Get ("gl_fb_bmodels", "1", CVAR_ARCHIVE, 0, "Toggles fullbright color support for bmodels");
	gl_fb_models = Cvar_Get ("gl_fb_models", "1", CVAR_ARCHIVE, 0, "Toggles fullbright color support for models");
	gl_fires = Cvar_Get ("gl_fires", "0", CVAR_ARCHIVE, 0, "Toggles lavaball and rocket fireballs");
	gl_keeptjunctions = Cvar_Get ("gl_keeptjunctions", "1", CVAR_ARCHIVE, 0, "Set to 0 to turn off colinear vertexes upon level load");
	gl_lerp_anim = Cvar_Get ("gl_lerp_anim", "1", CVAR_ARCHIVE, 0, "Toggles model animation interpolation");
	gl_multitexture = Cvar_Get ("gl_multitexture", "0", CVAR_ARCHIVE, 0, "Use multitexture when available");
	gl_nocolors = Cvar_Get ("gl_nocolors", "0", CVAR_NONE, 0, "Set to 1, turns off all player colors");
	gl_playermip = Cvar_Get ("gl_playermip", "0", CVAR_NONE, 0, "Detail of player skins. 0 best, 4 worst.");
	gl_sky_clip = Cvar_Get ("gl_sky_clip", "0", CVAR_ARCHIVE, 0, "controls whether sky is drawn first (0) or later (1)");
	gl_sky_divide = Cvar_Get ("gl_sky_divide", "1", CVAR_ARCHIVE, 0, "subdivide sky polys");
	gl_skymultipass = Cvar_Get ("gl_skymultipass", "1", CVAR_ARCHIVE, 0, "controls whether the skydome is single or double pass");
}

/*
	R_NewMap
*/
void
R_NewMap (void)
{
	int         i;
	cvar_t     *r_skyname;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	GL_BuildLightmaps ();

	// identify sky texture
	skytexturenum = -1;
	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		if (!cl.worldmodel->textures[i])
			continue;
		if (!strncmp (cl.worldmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;
		cl.worldmodel->textures[i]->texturechain = NULL;
	}
	r_skyname = Cvar_FindVar ("r_skyname");
	if (r_skyname != NULL)
		R_LoadSkys (r_skyname->string);
	else
		R_LoadSkys ("none");
}


/*
	R_TimeRefresh_f

	For program optimization
*/
// LordHavoc: improved appearance and accuracy of timerefresh
void
R_TimeRefresh_f (void)
{
	int         i;
	double      start, stop, time;

//  glDrawBuffer  (GL_FRONT);
	glFinish ();
	GL_EndRendering ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
		r_refdef.viewangles[1] = i / 128.0 * 360.0;
		R_RenderView ();
		glFinish ();
		GL_EndRendering ();
	}

//  glFinish ();
	stop = Sys_DoubleTime ();
	time = stop - start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128 / time);

//  glDrawBuffer  (GL_BACK);
//  GL_EndRendering ();
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
}

void
D_FlushCaches (void)
{
}
