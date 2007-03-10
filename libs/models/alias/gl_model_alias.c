/*
	gl_model_alias.c

	alias model loading and caching for gl

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
// models are the only shared resource between a client and server running
// on the same machine.

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

#include "QF/image.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/qf_textures.h"

#include "compat.h"

VISIBLE byte		player_8bit_texels[640 * 400];


// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses

typedef struct {
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) { \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} else if (pos[off] != 255) \
		fdc = pos[off]; \
}

/*
	Mod_FloodFillSkin

	Fill background pixels so mipmapping doesn't have halos - Ed
*/
static void
Mod_FloodFillSkin (byte * skin, int skinwidth, int skinheight)
{
	byte        fillcolor = *skin;		// assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int			filledcolor = -1, inpt = 0, outpt = 0;
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
	// can't fill to filled color or transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
//		Sys_Printf ("not filling skin from %d to %d\n", fillcolor, filledcolor);
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt) {
		int		x = fifo[outpt].x, y = fifo[outpt].y, fdc = filledcolor;
		byte   *pos = &skin[x + skinwidth * y];

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

void *
Mod_LoadSkin (byte * skin, int skinsize, int snum, int gnum, qboolean group,
			  maliasskindesc_t *skindesc)
{
	byte   *pskin;
	char	name[32], modname[MAX_QPATH + 4];
	int		fb_texnum = 0, texnum = 0;

	pskin = Hunk_AllocName (skinsize, loadname);
	skindesc->skin = (byte *) pskin - (byte *) pheader;

	memcpy (pskin, skin, skinsize);

	Mod_FloodFillSkin (pskin, pheader->mdl.skinwidth, pheader->mdl.skinheight);
	// save 8 bit texels for the player model to remap
	if (strequal (loadmodel->name, "progs/player.mdl")) {
		if (skinsize > (int) sizeof (player_8bit_texels))
			Sys_Error ("Player skin too large");
		memcpy (player_8bit_texels, pskin, skinsize);
	}

	QFS_StripExtension (loadmodel->name, modname);

	if (!loadmodel->fullbright) {
		if (group) {
			snprintf (name, sizeof (name), "fb_%s_%i_%i", modname,
					  snum, gnum);
		} else {
			snprintf (name, sizeof (name), "fb_%s_%i", modname, snum);
		}
		fb_texnum = Mod_Fullbright (pskin, pheader->mdl.skinwidth,
									pheader->mdl.skinheight, name);
	}
	if (group) {
		snprintf (name, sizeof (name), "%s_%i_%i", modname, snum,
				  gnum);
	} else {
		snprintf (name, sizeof (name), "%s_%i", modname, snum);
	}
	texnum = GL_LoadTexture (name, pheader->mdl.skinwidth,
							 pheader->mdl.skinheight, pskin, true, false, 1);
	skindesc->texnum = texnum;
	skindesc->fb_texnum = fb_texnum;
	loadmodel->hasfullbrights = fb_texnum;
	// alpha param was true for non group skins
	return skin + skinsize;
}

void *
Mod_LoadAliasFrame (void *pin, int *posenum, maliasframedesc_t *frame,
					int extra)
{
	daliasframe_t  *pdaliasframe;
	int				i;
	trivertx_t	   *pinframe;

	pdaliasframe = (daliasframe_t *) pin;

	strncpy (frame->name, pdaliasframe->name, sizeof (frame->name));
	frame->firstpose = (*posenum);
	frame->numposes = 1;

	for (i = 0; i < 3; i++) {
		// byte values, don't worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		aliasbboxmins[i] = min (frame->bboxmin.v[i], aliasbboxmins[i]);
		aliasbboxmaxs[i] = max (frame->bboxmax.v[i], aliasbboxmaxs[i]);
	}

	pinframe = (trivertx_t *) (pdaliasframe + 1);

	poseverts[(*posenum)] = pinframe;
	(*posenum)++;

	if (extra)
		pinframe += pheader->mdl.numverts * 2;
	else
		pinframe += pheader->mdl.numverts;

	return (void *) pinframe;
}

void *
Mod_LoadAliasGroup (void *pin, int *posenum, maliasframedesc_t *frame,
					int extra)
{
	daliasgroup_t	 *pingroup;
	daliasinterval_t *pin_intervals;
	int				  i, numframes;
	void			 *ptemp;

	pingroup = (daliasgroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = (*posenum);
	frame->numposes = numframes;

	for (i = 0; i < 3; i++) {
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		aliasbboxmins[i] = min (frame->bboxmin.v[i], aliasbboxmins[i]);
		aliasbboxmaxs[i] = max (frame->bboxmax.v[i], aliasbboxmaxs[i]);
	}

	pin_intervals = (daliasinterval_t *) (pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++) {
		poseverts[(*posenum)] = (trivertx_t *) ((daliasframe_t *) ptemp + 1);
		(*posenum)++;
		if (extra)
			ptemp =	(trivertx_t *) ((daliasframe_t *) ptemp + 1)
				+ pheader->mdl.numverts * 2;
		else
			ptemp =	(trivertx_t *) ((daliasframe_t *) ptemp + 1)
				+ pheader->mdl.numverts;
	}
	return ptemp;
}

void
Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr)
{
	if (strequal (m->name, "progs/eyes.mdl")) {
		hdr->mdl.scale_origin[2] -= (22 + 8);
		VectorScale (hdr->mdl.scale, 2, hdr->mdl.scale);
	}
}

static void
Mod_LoadExternalSkin (maliasskindesc_t *pskindesc, char *filename)
{
	tex_t		*tex, *glow;
	char		*ptr;

	ptr = strrchr (filename, '/');
	if (!ptr)
		ptr = filename;

	tex = LoadImage (filename);
	if (!tex)
		tex = LoadImage (va ("textures/%s", ptr + 1));
	if (tex) {
		pskindesc->texnum = GL_LoadTexture (filename, tex->width, tex->height,
											tex->data, true, false,
											tex->format > 2 ? tex->format : 1);

		pskindesc->fb_texnum = 0;

		glow = LoadImage (va ("%s_luma", filename));
		if (!glow)
			glow = LoadImage (va ("%s_glow", filename));
		if (!glow)
			glow = LoadImage (va ("textures/%s_luma", ptr + 1));
		if (!glow)
			glow = LoadImage (va ("textures/%s_glow", ptr + 1));
		if (glow)
			pskindesc->fb_texnum =
				GL_LoadTexture (va ("fb_%s", filename), glow->width,
								glow->height, glow->data, true, true,
								glow->format > 2 ? glow->format : 1);
		else if (tex->format < 3)
			pskindesc->fb_texnum = Mod_Fullbright (tex->data, tex->width,
												   tex->height,
												   va ("fb_%s", filename));
	}
}

void
Mod_LoadExternalSkins (model_t *mod)
{
	char			   filename[MAX_QPATH + 4], modname[MAX_QPATH + 4];
	int				   i, j;
	maliasskindesc_t  *pskindesc;
	maliasskingroup_t *pskingroup;

	QFS_StripExtension (mod->name, modname);

	for (i = 0; i < pheader->mdl.numskins; i++) {
		pskindesc = ((maliasskindesc_t *)
					 ((byte *) pheader + pheader->skindesc)) + i;
		if (pskindesc->type == ALIAS_SKIN_SINGLE) {
			snprintf (filename, sizeof (filename), "%s_%i", modname, i);
			Mod_LoadExternalSkin (pskindesc, filename);
		} else {
			pskingroup = (maliasskingroup_t *)
				((byte *) pheader + pskindesc->skin);

			for (j = 0; j < pskingroup->numskins; j++) {
				snprintf (filename, sizeof (filename), "%s_%i_%i",
						  modname, i, j);
				Mod_LoadExternalSkin (pskingroup->skindescs + j, filename);
			}
		}
	}
}
