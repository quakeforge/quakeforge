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
qboolean    allowskybox;				// allow skyboxes?  --KB
void        R_InitBubble (void);

extern cvar_t	*gl_lerp_anim;
extern cvar_t	*r_netgraph;

extern void GDT_Init ();

extern entity_t r_worldentity;


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
	
	//XXX netgraphtexture = texture_extension_number;
	//XXX texture_extension_number++;

	playertextures = texture_extension_number;
	texture_extension_number += 16;//MAX_CLIENTS;
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


/*
	R_TranslatePlayerSkin

	Translates a skin texture by the per-player color lookup
*/
void
R_TranslatePlayerSkin (int playernum)
{
	int         top, bottom;
	byte        translate[256];
	unsigned    translate32[256];
	int         i, j, s;
	model_t    *model;
	aliashdr_t *paliashdr;
	byte       *original;
	unsigned    pixels[512 * 256], *out;
	unsigned    scaled_width, scaled_height;
	int         inwidth, inheight;
	byte       *inrow;
	unsigned    frac, fracstep;

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors & 15) << 4;

	for (i = 0; i < 256; i++)
		translate[i] = i;

	for (i = 0; i < 16; i++) {
		if (top < 128)					// the artists made some backwards
			// ranges.  sigh.
			translate[TOP_RANGE + i] = top + i;
		else
			translate[TOP_RANGE + i] = top + 15 - i;

		if (bottom < 128)
			translate[BOTTOM_RANGE + i] = bottom + i;
		else
			translate[BOTTOM_RANGE + i] = bottom + 15 - i;
	}

	// 
	// locate the original skin pixels
	// 
	currententity = &cl_entities[1 + playernum];
	model = currententity->model;
	if (!model)
		return;							// player doesn't have a model yet
	if (model->type != mod_alias)
		return;							// only translate skins on alias
	// models

	paliashdr = (aliashdr_t *) Mod_Extradata (model);
	s = paliashdr->mdl.skinwidth * paliashdr->mdl.skinheight;
	if (currententity->skinnum < 0
		|| currententity->skinnum >= paliashdr->mdl.numskins) {
		Con_Printf ("(%d): Invalid player skin #%d\n", playernum,
					currententity->skinnum);
		original = (byte *) paliashdr + paliashdr->texels[0];
	} else
		original =
			(byte *) paliashdr + paliashdr->texels[currententity->skinnum];
	if (s & 3)
		Sys_Error ("R_TranslateSkin: s&3");

	inwidth = paliashdr->mdl.skinwidth;
	inheight = paliashdr->mdl.skinheight;

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_upload 8
	glBindTexture (GL_TEXTURE_2D, playertextures + playernum);

#if 0
	byte        translated[320 * 200];

	for (i = 0; i < s; i += 4) {
		translated[i] = translate[original[i]];
		translated[i + 1] = translate[original[i + 1]];
		translated[i + 2] = translate[original[i + 2]];
		translated[i + 3] = translate[original[i + 3]];
	}


	// don't mipmap these, because it takes too long
	GL_Upload8 (translated, paliashdr->skinwidth, paliashdr->skinheight, false,
				false, true);
#else
	scaled_width = gl_max_size->int_val < 512 ? gl_max_size->int_val : 512;
	scaled_height = gl_max_size->int_val < 256 ? gl_max_size->int_val : 256;

	// allow users to crunch sizes down even more if they want
	scaled_width >>= gl_playermip->int_val;
	scaled_height >>= gl_playermip->int_val;

	if (VID_Is8bit ()) {				// 8bit texture upload
		byte       *out2;

		out2 = (byte *) pixels;
		memset (pixels, 0, sizeof (pixels));
		fracstep = inwidth * 0x10000 / scaled_width;
		for (i = 0; i < scaled_height; i++, out2 += scaled_width) {
			inrow = original + inwidth * (i * inheight / scaled_height);
			frac = fracstep >> 1;
			for (j = 0; j < scaled_width; j += 4) {
				out2[j] = translate[inrow[frac >> 16]];
				frac += fracstep;
				out2[j + 1] = translate[inrow[frac >> 16]];
				frac += fracstep;
				out2[j + 2] = translate[inrow[frac >> 16]];
				frac += fracstep;
				out2[j + 3] = translate[inrow[frac >> 16]];
				frac += fracstep;
			}
		}

		GL_Upload8_EXT ((byte *) pixels, scaled_width, scaled_height, false,
						false);
		return;
	}

	for (i = 0; i < 256; i++)
		translate32[i] = d_8to24table[translate[i]];

	out = pixels;
	fracstep = inwidth * 0x10000 / scaled_width;
	for (i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow = original + inwidth * (i * inheight / scaled_height);
		frac = fracstep >> 1;
		for (j = 0; j < scaled_width; j += 4) {
			out[j] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 1] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 2] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 3] = translate32[inrow[frac >> 16]];
			frac += fracstep;
		}
	}
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, scaled_width,
				  scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif
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
	mirrortexturenum = -1;
	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		if (!cl.worldmodel->textures[i])
			continue;
		if (!strncmp (cl.worldmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;
		if (!strncmp (cl.worldmodel->textures[i]->name, "window02_1", 10))
			mirrortexturenum = i;
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
