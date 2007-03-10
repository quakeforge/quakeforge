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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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
#include "QF/draw.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_screen.h"
#include "QF/GL/qf_textures.h"
#include "QF/GL/qf_vid.h"

#include "r_dynamic.h"
#include "r_local.h"
#include "varrays.h"

int			r_init = 0;


/*
	R_Envmap_f

	Grab six views for environment mapping tests
*/
static void
R_Envmap_f (void)
{
	byte        buffer[256 * 256 * 4];

	qfglDrawBuffer (GL_FRONT);
	qfglReadBuffer (GL_FRONT);
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
	qfglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	QFS_WriteFile ("env0.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qfglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	QFS_WriteFile ("env1.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qfglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	QFS_WriteFile ("env2.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qfglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	QFS_WriteFile ("env3.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qfglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	QFS_WriteFile ("env4.rgb", buffer, sizeof (buffer));

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	qfglReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	QFS_WriteFile ("env5.rgb", buffer, sizeof (buffer));

	envmap = false;
	qfglDrawBuffer (GL_BACK);
	qfglReadBuffer (GL_BACK);
	GL_EndRendering ();
}

void
R_LoadSky_f (void)
{
	if (Cmd_Argc () != 2) {
		Con_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	R_LoadSkys (Cmd_Argv (1));
}

VISIBLE void
R_Init (void)
{
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f,
					"Tests the current refresh rate for the current location");
	Cmd_AddCommand ("envmap", R_Envmap_f, "No Description");
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f,
					"Load a pointfile to determine map leaks");
	Cmd_AddCommand ("loadsky", R_LoadSky_f, "Load a skybox");

	R_InitBubble ();

	GDT_Init ();
	
	texture_extension_number = R_InitGraphTextures (texture_extension_number);

	texture_extension_number = Skin_Init_Textures (texture_extension_number);

	r_init = 1;
	R_InitParticles ();
	R_InitSprites ();
}

VISIBLE void
R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
	cvar_t     *r_skyname;
	int         i;
	texture_t  *tex;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	for (i = 0; i < r_worldentity.model->numleafs; i++)
		r_worldentity.model->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	GL_BuildLightmaps (models, num_models);

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;
	for (i = 0; i < r_worldentity.model->numtextures; i++) {
		tex = r_worldentity.model->textures[i];
		if (!tex)
			continue;
		if (!strncmp (tex->name, "sky", 3)) {
			skytexturenum = i;
			R_InitSky (tex);
		}
		if (!strncmp (tex->name, "window02_1", 10))
			mirrortexturenum = i;
		tex->texturechain = NULL;
		tex->texturechain_tail = &tex->texturechain;
	}
	tex = r_notexture_mip;
	tex->texturechain = NULL;
	tex->texturechain_tail = &tex->texturechain;

	r_skyname = Cvar_FindVar ("r_skyname");
	if (r_skyname != NULL)
		R_LoadSkys (r_skyname->string);
	else
		R_LoadSkys ("none");
}

void
R_ViewChanged (float aspect)
{
}

/*
  R_TimeRefresh_f

  For program optimization
  LordHavoc: improved appearance and accuracy of timerefresh
*/
void
R_TimeRefresh_f (void)
{
	double      start, stop, time;
	int         i;

	GL_EndRendering ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView ();
		GL_EndRendering ();
	}

	stop = Sys_DoubleTime ();
	time = stop - start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
}

VISIBLE void
D_FlushCaches (void)
{
}
