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
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/varrays.h"

#include "client.h"
#include "glquake.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "render.h"

varray_t2f_c4f_v3f_t varray[MAX_VARRAY_VERTS];

qboolean    VID_Is8bit (void);
qboolean	allowskybox;				// allow skyboxes?  --KB
void        R_InitBubble (void);

extern cvar_t	*gl_lerp_anim;
extern cvar_t	*r_netgraph;

extern void GDT_Init ();


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


void
R_LoadSky_f (void)
{
	if (Cmd_Argc () != 2) {
		Con_Printf ("loadsky <name> : load a skybox\n");
		return;
	}

	R_LoadSkys (Cmd_Argv (1));
}


void
R_Init (void)
{
	allowskybox = false;				// server will decide if this is
										// allowed  --KB

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f,
					"Tests the current refresh rate for the current location");
	Cmd_AddCommand ("envmap", R_Envmap_f, "No Description");
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f,
					"Load a pointfile to determine map leaks");
	Cmd_AddCommand ("loadsky", R_LoadSky_f, "Load a skybox");

	R_InitBubble ();
	
	GDT_Init ();
	
	netgraphtexture = texture_extension_number;
	texture_extension_number++;

	playertextures = texture_extension_number;
	texture_extension_number += MAX_CLIENTS;
	player_fb_textures = texture_extension_number;
	texture_extension_number += MAX_CACHED_SKINS;

	glEnableClientState (GL_COLOR_ARRAY);
	glEnableClientState (GL_VERTEX_ARRAY);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);

//	glInterleavedArrays(GL_T2F_C4F_N3F_V3F, 0, varray);
	glTexCoordPointer (2, GL_FLOAT, sizeof(varray[0]), varray[0].texcoord);
	glColorPointer (4, GL_FLOAT, sizeof(varray[0]), varray[0].color);
	glVertexPointer (3, GL_FLOAT, sizeof(varray[0]), varray[0].vertex);
}


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

	stop = Sys_DoubleTime ();
	time = stop - start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128 / time);

	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
}


void
D_FlushCaches (void)
{
}
