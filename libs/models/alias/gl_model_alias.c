/*
	gl_model.c

	model loading and caching

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

// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/compat.h"
#include "QF/console.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "glquake.h"

byte        player_8bit_texels[320 * 200];

extern model_t *loadmodel;

/*
	ALIAS MODELS
*/

extern aliashdr_t *pheader;

extern stvert_t stverts[MAXALIASVERTS];
extern mtriangle_t triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
extern trivertx_t *poseverts[MAXALIASFRAMES];
extern int  posenum;

//=========================================================

/*
	Mod_FloodFillSkin

	Fill background pixels so mipmapping doesn't have haloes - Ed
*/

typedef struct {
	short       x, y;
} floodfill_t;

extern unsigned int d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void
Mod_FloodFillSkin (byte * skin, int skinwidth, int skinheight)
{
	byte        fillcolor = *skin;		// assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int         inpt = 0, outpt = 0;
	int         filledcolor = -1;
	int         i;

	if (filledcolor == -1) {
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0))	// alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}
	// can't fill to filled color or to transparent color (used as visited
	// marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
		//printf ("not filling skin from %d to %d\n", fillcolor, filledcolor);
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt) {
		int         x = fifo[outpt].x, y = fifo[outpt].y;
		int         fdc = filledcolor;
		byte       *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
			FLOODFILL_STEP (-1, -1, 0);
		if (x < skinwidth - 1)
			FLOODFILL_STEP (1, 1, 0);
		if (y > 0)
			FLOODFILL_STEP (-skinwidth, 0, -1);
		if (y < skinheight - 1)
			FLOODFILL_STEP (skinwidth, 0, 1);
		skin[x + skinwidth * y] = fdc;
	}
}

int         Mod_Fullbright (byte * skin, int width, int height, char *name);

void       *
Mod_LoadSkin (byte * skin, int skinsize, int snum, int gnum, qboolean group)
{
	char        name[32];
	int         fbtexnum;

	Mod_FloodFillSkin (skin, pheader->mdl.skinwidth, pheader->mdl.skinheight);
	// save 8 bit texels for the player model to remap
	if (!strcmp (loadmodel->name, "progs/player.mdl")) {
		byte       *texels;
		if (skinsize > sizeof (player_8bit_texels))
			Sys_Error ("Player skin too large");
		texels = Hunk_AllocName (skinsize, loadname);
		pheader->texels[snum] = texels - (byte *) pheader;
		memcpy (texels, skin, skinsize);
		memcpy (player_8bit_texels, skin, skinsize);
	}

	if (group) {
		snprintf (name, sizeof (name), "fb_%s_%i_%i", loadmodel->name, snum,
				  gnum);
	} else {
		snprintf (name, sizeof (name), "fb_%s_%i", loadmodel->name, snum);
	}
	fbtexnum =
		Mod_Fullbright (skin + 1, pheader->mdl.skinwidth,
						pheader->mdl.skinheight, name);
	if ((loadmodel->hasfullbrights = (fbtexnum))) {
		pheader->gl_fb_texturenum[snum][gnum] = fbtexnum;
	}
	if (group) {
		snprintf (name, sizeof (name), "%s_%i_%i", loadmodel->name, snum, gnum);
	} else {
		snprintf (name, sizeof (name), "%s_%i", loadmodel->name, snum);
	}
	pheader->gl_texturenum[snum][gnum] =
		GL_LoadTexture (name, pheader->mdl.skinwidth,
						pheader->mdl.skinheight, skin, true, false, 1);
	// alpha param was true for non group skins
	return skin + skinsize;
}

/*
	Mod_LoadAllSkins
*/
void       *
Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype, int *pskinindex)
{
	int         i, j, k;
	int         skinsize;
	byte       *skin;
	daliasskingroup_t *pinskingroup;
	int         groupskins;
	daliasskininterval_t *pinskinintervals;

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	skinsize = pheader->mdl.skinwidth * pheader->mdl.skinheight;

	for (i = 0; i < numskins; i++) {
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			skin = (byte *) (pskintype + 1);
			skin = Mod_LoadSkin (skin, skinsize, i, 0, false);

			for (j = 1; j < 4; j++) {
				pheader->gl_texturenum[i][j] = pheader->gl_texturenum[i][j - 1];
				pheader->gl_fb_texturenum[i][j] =
					pheader->gl_fb_texturenum[i][j - 1];
			}
		} else {
			// animating skin group.  yuck.
			// Con_Printf("Animating Skin Group, if you get this message
			// please notify warp@debian.org\n");
			pskintype++;
			pinskingroup = (daliasskingroup_t *) pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *) (pinskingroup + 1);

			pskintype = (void *) (pinskinintervals + groupskins);
			skin = (byte *) pskintype;

			for (j = 0; j < groupskins; j++) {
				skin = Mod_LoadSkin (skin, skinsize, i, j & 3, true);
			}
			k = j;
			for ( /* */ ; j < 4; j++) {
				pheader->gl_texturenum[i][j] = pheader->gl_texturenum[i][j - k];
				pheader->gl_fb_texturenum[i][j] =
					pheader->gl_fb_texturenum[i][j - k];
			}
		}
		pskintype = (daliasskintype_t *) skin;
	}

	return pskintype;
}

/*
	Mod_LoadAliasFrame
*/
void *
Mod_LoadAliasFrame (void *pin, maliasframedesc_t *frame)
{
	trivertx_t *pinframe;
	int         i;
	daliasframe_t *pdaliasframe;

	pdaliasframe = (daliasframe_t *) pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i = 0; i < 3; i++) {
		// byte values, don't worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->mdl.numverts;

	return (void *) pinframe;
}

/*
	Mod_LoadAliasGroup
*/
void *
Mod_LoadAliasGroup (void *pin, maliasframedesc_t *frame)
{
	daliasgroup_t *pingroup;
	int         i, numframes;
	daliasinterval_t *pin_intervals;
	void       *ptemp;

	pingroup = (daliasgroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *) (pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		poseverts[posenum] = (trivertx_t *) ((daliasframe_t *) ptemp + 1);
		posenum++;
		ptemp =	(trivertx_t *) ((daliasframe_t *) ptemp + 1) + pheader->mdl.numverts;
	}
	return ptemp;
}
