/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

// modelgen.c: generates a .mdl file from a base triangle file (.tri), a
// texture containing front and back skins (.lbm), and a series of frame
// triangle files (.tri). Result is stored in
// /raid/quake/models/<scriptname>.mdl.

//#include <sys/stat.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef _WIN32
# include <direct.h>
#endif

#include "QF/dstring.h"
#include "QF/modelgen.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"
#include "QF/script.h"
#include "QF/sys.h"

#include "compat.h"

#include "tools/qfmodelgen/include/lbmlib.h"
#include "tools/qfmodelgen/include/trilib.h"

#define MAXVERTS		2048
#define MAXFRAMES		256
#define MAXSKINS		100
#define MAXTRIANGLES	2048

typedef struct {
	aliasframetype_t	type;		// single frame or group of frames
	void				*pdata;		// either a daliasframe_t or group info
	float				interval;	// used only for frames in groups
	int					numgroupframes;	// used only by group headers
	char				name[16];
} aliaspackage_t;

typedef struct {
	aliasskintype_t		type;		// single skin or group of skiins
	void				*pdata;		// either a daliasskinframe_t or group info
	float				interval;	// used only for skins in groups
	int					numgroupskins;	// used only by group headers
} aliasskinpackage_t;

typedef struct {
	int		numnormals;
	float	normals[20][3];
} vertexnormals;

typedef struct {
	vec3_t		v;
	int			lightnormalindex;
} trivert_t;

//============================================================================

trivert_t	verts[MAXFRAMES][MAXVERTS];
mdl_t	model;

char	file1[1024];
char	skinname[1024];
char	qbasename[1024];
float	scale, scale_up = 1.0;
vec3_t	mins, maxs;
vec3_t	framesmins, framesmaxs;
vec3_t		adjust;

aliaspackage_t	frames[MAXFRAMES];

aliasskinpackage_t	skins[MAXSKINS];

//
// base frame info
//
vec3_t		baseverts[MAXVERTS];
stvert_t	stverts[MAXVERTS];
dtriangle_t	triangles[MAXTRIANGLES];
int			degenerate[MAXTRIANGLES];

char		cdpartial[256];
char		cddir[256];

int			framecount, skincount;
qboolean		cdset;
int			degeneratetris;
int			firstframe = 1;
float		totsize, averagesize;

vertexnormals	vnorms[MAXVERTS];

#define NUMVERTEXNORMALS	162

float	avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"			//FIXME should this be moved to QF?
};

trivertx_t	tarray[MAXVERTS];

char	outname[1024];

script_t scr;

/*
qdir will hold the path up to the quake directory, including the slash

  f:\quake\
  /raid/quake/

gamedir will hold qdir + the game directory (id1, id2, etc)
*/

char		qdir[1024];
char		gamedir[1024];

static void
SetQdirFromPath (char *path)
{
	char	temp[1024];
	char	*c;

	if (!(path[0] == '/' || path[0] == '\\' || path[1] == ':'))
	{	// path is partial
		if (getcwd (temp, sizeof (temp)) == NULL)
			Sys_Error ("Can't get CWD");
		strcat (temp, path);
		path = temp;
	}

	// search for "quake" in path

	for (c=path ; *c ; c++)
		if (!strncasecmp (c, "quake", 5)) {
			strncpy (qdir, path, c+6-path);
			printf ("qdir: %s\n", qdir);
			c += 6;
			while (*c) {
				if (*c == '/' || *c == '\\') {
					strncpy (gamedir, path, c+1-path);
					printf ("gamedir: %s\n", gamedir);
					return;
				}
				c++;
			}
			Sys_Error ("No gamedir in %s", path);
			return;
		}
	Sys_Error ("SetQdirFromPath: no 'quake' in %s", path);
}

static const char *
ExpandPath (const char *path)
{
	static char full[1024];

	//FIXME buffer overflow central
	//if (!qdir)
	//	Sys_Error ("ExpandPath called without qdir set");
	if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
		return path;
	sprintf (full, "%s%s", qdir, path);
	return full;
}

static void
ClearModel (void)
{
	memset (&model, 0, sizeof (model));
	model.synctype = ST_RAND;	// default
	framecount = skincount = 0;

	scale = 0;
	scale_up = 1.0;

	VectorZero (adjust);
	VectorZero (mins);
	VectorZero (maxs);
	VectorZero (framesmins);
	VectorZero (framesmaxs);

	degeneratetris = 0;
	cdset = false;
	firstframe = 1;
	totsize = 0.0;
}

static void
WriteFrame (QFile *modelouthandle, int framenum)
{
	int				j, k;
	float			v;
	daliasframe_t	aframe;
	trivert_t	   *pframe;

	pframe = verts[framenum];

	strcpy (aframe.name, frames[framenum].name);

	for (j = 0; j < 3; j++) {
		aframe.bboxmin.v[j] = 255;
		aframe.bboxmax.v[j] = 0;
	}

	for (j = 0; j < model.numverts; j++) {
		// all of these are byte values, so no need to deal with endianness
		tarray[j].lightnormalindex = pframe[j].lightnormalindex;

		if (tarray[j].lightnormalindex > NUMVERTEXNORMALS)
			Sys_Error ("invalid lightnormalindex %d\n",
				   tarray[j].lightnormalindex);

		for (k = 0; k < 3; k++) {
		// scale to byte values & min/max check
			v = (pframe[j].v[k] - model.scale_origin[k]) / model.scale[k];

			tarray[j].v[k] = v;

			if (tarray[j].v[k] < aframe.bboxmin.v[k])
				aframe.bboxmin.v[k] = tarray[j].v[k];
			if (tarray[j].v[k] > aframe.bboxmax.v[k])
				aframe.bboxmax.v[k] = tarray[j].v[k];
		}
	}

	Qwrite (modelouthandle, &aframe, sizeof (aframe));
	Qwrite (modelouthandle, &tarray[0], model.numverts * sizeof(tarray[0]));
}

static void
WriteGroupBBox (QFile *modelouthandle, int numframes, int curframe)
{
	int				i, j, k;
	daliasgroup_t	dagroup;
	trivert_t		*pframe;

	dagroup.numframes = LittleLong (numframes);

	for (i = 0; i < 3; i++) {
		dagroup.bboxmin.v[i] = 255;
		dagroup.bboxmax.v[i] = 0;
	}

	for (i = 0; i < numframes; i++) {
		pframe = (trivert_t *) frames[curframe].pdata;

		for (j = 0; j < model.numverts; j++) {
			for (k = 0; k < 3; k++) {
			// scale to byte values & min/max check
				tarray[j].v[k] = (pframe[j].v[k] - model.scale_origin[k]) /
					model.scale[k];
				if (tarray[j].v[k] < dagroup.bboxmin.v[k])
					dagroup.bboxmin.v[k] = tarray[j].v[k];
				if (tarray[j].v[k] > dagroup.bboxmax.v[k])
					dagroup.bboxmax.v[k] = tarray[j].v[k];
			}
		}

		curframe++;
	}

	Qwrite (modelouthandle, &dagroup, sizeof(dagroup));
}

static void
WriteModelFile (QFile *modelouthandle)
{
	int		i, curframe, curskin;
	float	dist[3];
	mdl_t	modeltemp;

	// Calculate the bounding box for this model
	for (i = 0; i < 3; i++) {
		printf ("framesmins[%d]: %f, framesmaxs[%d]: %f\n",
				i, framesmins[i], i, framesmaxs[i]);
		if (fabs (framesmins[i]) > fabs (framesmaxs[i]))
			dist[i] = framesmins[i];
		else
			dist[i] = framesmaxs[i];

		model.scale[i] = (framesmaxs[i] - framesmins[i]) / 255.9;
		model.scale_origin[i] = framesmins[i];
	}

	model.boundingradius = sqrt (dist[0] * dist[0] +
								 dist[1] * dist[1] +
								 dist[2] * dist[2]);

	// write out the model header
	modeltemp.ident = LittleLong (IDHEADER_MDL);
	modeltemp.version = LittleLong (ALIAS_VERSION_MDL);
	modeltemp.boundingradius = LittleFloat (model.boundingradius);

	for (i = 0; i < 3; i++) {
		modeltemp.scale[i] = LittleFloat (model.scale[i]);
		modeltemp.scale_origin[i] = LittleFloat (model.scale_origin[i]);
		modeltemp.eyeposition[i] = LittleFloat (model.eyeposition[i] +
												adjust[i]);
	}

	modeltemp.flags = LittleLong (model.flags);
	modeltemp.numskins = LittleLong (model.numskins);
	modeltemp.skinwidth = LittleLong (model.skinwidth);
	modeltemp.skinheight = LittleLong (model.skinheight);
	modeltemp.numverts = LittleLong (model.numverts);
	modeltemp.numtris = LittleLong (model.numtris - degeneratetris);
	modeltemp.numframes = LittleLong (model.numframes);
	modeltemp.synctype = LittleFloat (model.synctype);
	averagesize = totsize / model.numtris;
	modeltemp.size = LittleFloat (averagesize);

	Qwrite (modelouthandle, &modeltemp, sizeof (model));

// write out the skins
	curskin = 0;

	for (i = 0; i < model.numskins; i++) {
		Qwrite (modelouthandle, &skins[curskin].type,
				sizeof (skins[curskin].type));

		Qwrite (modelouthandle, skins[curskin].pdata,
				model.skinwidth * model.skinheight);

		curskin++;
	}

// write out the base model (the s & t coordinates for the vertices)
	for (i = 0; i < model.numverts; i++) {
		if (stverts[i].onseam == 3) {
			stverts[i].onseam = LittleLong (ALIAS_ONSEAM);
		} else {
			stverts[i].onseam = LittleLong (0);
		}

		stverts[i].s = LittleLong (stverts[i].s);
		stverts[i].t = LittleLong (stverts[i].t);
	}

	Qwrite (modelouthandle, stverts, model.numverts * sizeof(stverts[0]));

// write out the triangles
	for (i = 0; i < model.numtris; i++) {
		int			j;
		dtriangle_t	tri;

		if (!degenerate[i]) {
			tri.facesfront = LittleLong (triangles[i].facesfront);

			for (j = 0; j < 3; j++) {
				tri.vertindex[j] = LittleLong (triangles[i].vertindex[j]);
			}

			Qwrite (modelouthandle, &tri, sizeof(tri));
		}
	}

// write out the frames
	curframe = 0;

	for (i = 0; i < model.numframes; i++) {
		Qwrite (modelouthandle, &frames[curframe].type,
				sizeof (frames[curframe].type));

		if (frames[curframe].type == ALIAS_SINGLE) {
		// single (non-grouped) frame
			WriteFrame (modelouthandle, curframe);
			curframe++;
		} else {
			int					j, numframes, groupframe;
			float				totinterval;

			groupframe = curframe;
			curframe++;
			numframes = frames[groupframe].numgroupframes;

		// set and write the group header
			WriteGroupBBox (modelouthandle, numframes, curframe);

		// write the interval array
			totinterval = 0.0;

			for (j = 0; j < numframes; j++) {
				daliasinterval_t	temp;

				totinterval += frames[groupframe+1+j].interval;
				temp.interval = LittleFloat (totinterval);

				Qwrite (modelouthandle, &temp, sizeof(temp));
			}

			for (j = 0; j < numframes; j++) {
				WriteFrame (modelouthandle, curframe);
				curframe++;
			}
		}
	}
}

static void
WriteModel (void)
{
	QFile		*modelouthandle;

// write the model output file
	if (!framecount) {
		printf ("no frames grabbed, no file generated\n");
		return;
	}

	if (!skincount)
		Sys_Error ("frames with no skins\n");

	QFS_StripExtension (outname, outname);
	strcat (outname, ".mdl");

	printf ("---------------------\n");
	printf ("writing %s:\n", outname);
	modelouthandle = Qopen (outname, "wb");

	WriteModelFile (modelouthandle);

	printf ("%4d frame(s)\n", model.numframes);
	printf ("%4d ungrouped frame(s), including group headers\n", framecount);
	printf ("%4d skin(s)\n", model.numskins);
	printf ("%4d degenerate triangles(s) removed\n", degeneratetris);
	printf ("%4d triangles emitted\n", model.numtris - degeneratetris);
	printf ("pixels per triangle %f\n", averagesize);

	printf ("file size: %d\n", (int) Qtell (modelouthandle));
	printf ("---------------------\n");

	Qclose (modelouthandle);

	ClearModel ();
}

/*
============
SetSkinValues

Called for the base frame
============
*/
static void
SetSkinValues (void)
{
	float		basex, basey, v;
	int			width, height, iwidth, iheight, skinwidth, i;

	for (i = 0; i < 3; i++) {
		mins[i] = 9999999;
		maxs[i] = -9999999;
	}

	for (i = 0; i < model.numverts; i++) {
		int		j;

		stverts[i].onseam = 0;

		for (j = 0; j < 3; j++) {
			v = baseverts[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}

	for (i = 0; i < 3; i++) {
		mins[i] = floor(mins[i]);
		maxs[i] = ceil(maxs[i]);
	}

	width = maxs[0] - mins[0];
	height = maxs[2] - mins[2];

	printf ("width: %i  height: %i\n", width, height);

	scale = 8;
	if (width*scale >= 150)
		scale = 150.0 / width;
	if (height*scale >= 190)
		scale = 190.0 / height;
	iwidth = ceil(width*scale) + 4;
	iheight = ceil(height*scale) + 4;

	printf ("scale: %f\n", scale);
	printf ("iwidth: %i  iheight: %i\n", iwidth, iheight);

// determine which side of each triangle to map the texture to
	for (i = 0; i < model.numtris; i++) {
		int		j;
		vec3_t	vtemp1, vtemp2, normal;

		VectorSubtract (baseverts[triangles[i].vertindex[0]],
						baseverts[triangles[i].vertindex[1]],
						vtemp1);
		VectorSubtract (baseverts[triangles[i].vertindex[2]],
						baseverts[triangles[i].vertindex[1]],
						vtemp2);
		CrossProduct (vtemp1, vtemp2, normal);

		if (normal[1] > 0) {
			basex = iwidth + 2;
			triangles[i].facesfront = 0;
		} else {
			basex = 2;
			triangles[i].facesfront = 1;
		}
		basey = 2;

		for (j = 0; j < 3; j++) {
			float		*pbasevert;
			stvert_t	*pstvert;

			pbasevert = baseverts[triangles[i].vertindex[j]];
			pstvert = &stverts[triangles[i].vertindex[j]];

			if (triangles[i].facesfront) {
				pstvert->onseam |= 1;
			} else {
				pstvert->onseam |= 2;
			}

			if ((triangles[i].facesfront) || ((pstvert->onseam & 1) == 0)) {
			// we want the front s value for seam vertices
				pstvert->s = ((pbasevert[0] - mins[0]) * scale + basex) + 0.5;
				pstvert->t = ((maxs[2] - pbasevert[2]) * scale + basey) + 0.5;
			}
		}
	}

// make the width a multiple of 4; some hardware requires this, and it ensures
// dword alignment for each scan
	skinwidth = iwidth*2;
	model.skinwidth = (skinwidth + 3) & ~3;
	model.skinheight = iheight;

	printf ("skin width: %i (unpadded width %i)  skin height: %i\n",
			model.skinwidth, skinwidth, model.skinheight);
}

static void
Cmd_Base (void)
{
	triangle_t	*ptri;
	int			 time1, i, j, k;

	Script_GetToken (&scr, false);
	strcpy (qbasename, scr.token->str);

	sprintf (file1, "%s/%s.tri", cdpartial, scr.token->str);
	ExpandPath/*AndArchive*/ (file1);

	sprintf (file1, "%s/%s.tri", cddir, scr.token->str);
	time1 = Sys_FileExists (file1);
	if (time1 == -1)
		Sys_Error ("%s doesn't exist", file1);

// load the base triangles
	LoadTriangleList (file1, &ptri, &model.numtris);
	printf("NUMBER OF TRIANGLES (including degenerate triangles): %d\n",
			model.numtris);

// run through all the base triangles, storing each unique vertex in the base
// vertex list and setting the indirect triangles to point to the base vertices
	for (i = 0; i < model.numtris; i++) {
		if (_VectorCompare (ptri[i].verts[0], ptri[i].verts[1])
			|| _VectorCompare (ptri[i].verts[1], ptri[i].verts[2])
			|| _VectorCompare (ptri[i].verts[2], ptri[i].verts[0])) {
			degeneratetris++;
			degenerate[i] = 1;
		} else {
			degenerate[i] = 0;
		}

		for (j = 0; j < 3; j++) {
			for (k=0 ; k<model.numverts ; k++)
				if (_VectorCompare (ptri[i].verts[j], baseverts[k]))
					break;	// this vertex is already in the base vertex list

			if (k == model.numverts) {
			// new vertex
				VectorCopy (ptri[i].verts[j], baseverts[model.numverts]);
				model.numverts++;
			}

			triangles[i].vertindex[j] = k;
		}
	}

	printf ("NUMBER OF VERTEXES: %i\n", model.numverts);

// calculate s & t for each vertex, and set the skin width and height
	SetSkinValues ();
}

static void
Cmd_Skin (void)
{
	byte	*ppal, *pskinbitmap, *ptemp1, *ptemp2;
	int		time1, i;

	Script_GetToken (&scr, false);
	strcpy (skinname, scr.token->str);

	sprintf (file1, "%s/%s.lbm", cdpartial, scr.token->str);
	ExpandPath/*AndArchive*/ (file1);

	sprintf (file1, "%s/%s.lbm", cddir, scr.token->str);
	time1 = Sys_FileExists (file1);
	if (time1 == -1)
		Sys_Error ("%s not found", file1);

	if (Script_TokenAvailable (&scr, false)) {
		Script_GetToken (&scr, false);
		skins[skincount].interval = atof (scr.token->str);
		if (skins[skincount].interval <= 0.0)
			Sys_Error ("Non-positive interval");
	} else {
		skins[skincount].interval = 0.1;
	}

// load in the skin .lbm file
	LoadLBM (file1, &pskinbitmap, &ppal);

// now copy the part of the texture we care about, since LBMs are always
// loaded as 320x200 bitmaps
	skins[skincount].pdata =
			malloc (model.skinwidth * model.skinheight);

	if (!skins[skincount].pdata)
		Sys_Error ("couldn't get memory for skin texture");

	ptemp1 = skins[skincount].pdata;
	ptemp2 = pskinbitmap;

	for (i = 0; i < model.skinheight; i++) {
		memcpy (ptemp1, ptemp2, model.skinwidth);
		ptemp1 += model.skinwidth;
		ptemp2 += 320;
	}

	skincount++;

	if (skincount > MAXSKINS)
		Sys_Error ("Too many skins; increase MAXSKINS");
}

static void
GrabFrame (char *frame, int isgroup)
{
	int			 numtris, time1, i, j;
	triangle_t	*ptri;
	trivert_t	*ptrivert;

	sprintf (file1, "%s/%s.tri", cdpartial, frame);
	ExpandPath/*AndArchive*/ (file1);

	sprintf (file1, "%s/%s.tri", cddir, frame);
	time1 = Sys_FileExists (file1);
	if (time1 == -1)
		Sys_Error ("%s does not exist", file1);

	printf ("grabbing %s\n", file1);
	frames[framecount].interval = 0.1;
	strcpy (frames[framecount].name, frame);

// load the frame
	LoadTriangleList (file1, &ptri, &numtris);

	if (numtris != model.numtris)
		Sys_Error ("number of triangles doesn't match\n");

// set the intervals
	if (isgroup && Script_TokenAvailable (&scr, false)) {
		Script_GetToken (&scr, false);
		frames[framecount].interval = atof (scr.token->str);
		if (frames[framecount].interval <= 0.0)
			Sys_Error ("Non-positive interval %s %f", scr.token->str,
					frames[framecount].interval);
	} else {
		frames[framecount].interval = 0.1;
	}

// allocate storage for the frame's vertices
	ptrivert = verts[framecount];

	frames[framecount].pdata = ptrivert;
	frames[framecount].type = ALIAS_SINGLE;

	for (i = 0; i < model.numverts ; i++) {
		vnorms[i].numnormals = 0;
	}

// store the frame's vertices in the same order as the base. This assumes the
// triangles and vertices in this frame are in exactly the same order as in the
// base
	for (i = 0; i < numtris; i++) {
		vec3_t	vtemp1, vtemp2, normal;
		float	ftemp;

		if (degenerate[i])
			continue;

		if (firstframe) {
			VectorSubtract (ptri[i].verts[0], ptri[i].verts[1], vtemp1);
			VectorSubtract (ptri[i].verts[2], ptri[i].verts[1], vtemp2);
			VectorScale (vtemp1, scale_up, vtemp1);
			VectorScale (vtemp2, scale_up, vtemp2);
			CrossProduct (vtemp1, vtemp2, normal);

			totsize += sqrt (normal[0] * normal[0] +
							 normal[1] * normal[1] +
							 normal[2] * normal[2]) / 2.0;
		}

		VectorSubtract (ptri[i].verts[0], ptri[i].verts[1], vtemp1);
		VectorSubtract (ptri[i].verts[2], ptri[i].verts[1], vtemp2);
		CrossProduct (vtemp1, vtemp2, normal);

		VectorNormalize (normal);

	// rotate the normal so the model faces down the positive x axis
		ftemp = normal[0];
		normal[0] = -normal[1];
		normal[1] = ftemp;

		for (j = 0; j < 3; j++) {
			int		k;
			int		vertindex;

			vertindex = triangles[i].vertindex[j];

		// rotate the vertices so the model faces down the positive x axis
		// also adjust the vertices to the desired origin
			ptrivert[vertindex].v[0] = ((-ptri[i].verts[j][1]) * scale_up) +
				adjust[0];
			ptrivert[vertindex].v[1] = (ptri[i].verts[j][0] * scale_up) +
				adjust[1];
			ptrivert[vertindex].v[2] = (ptri[i].verts[j][2] * scale_up) +
				adjust[2];

			for (k = 0; k < 3; k++) {
				if (ptrivert[vertindex].v[k] < framesmins[k])
					framesmins[k] = ptrivert[vertindex].v[k];

				if (ptrivert[vertindex].v[k] > framesmaxs[k])
					framesmaxs[k] = ptrivert[vertindex].v[k];
			}

			VectorCopy (normal,
						vnorms[vertindex].
						normals[vnorms[vertindex].numnormals]);

			vnorms[vertindex].numnormals++;
		}
	}

// calculate the vertex normals, match them to the template list, and store the
// index of the best match
	for (i = 0; i < model.numverts; i++) {
		vec3_t	v;
		float	maxdot;
		int		maxdotindex, j;

		if (vnorms[i].numnormals > 0) {
			for (j = 0; j < 3; j++) {
				int		k;

				v[j] = 0;

				for (k = 0; k < vnorms[i].numnormals; k++) {
					v[j] += vnorms[i].normals[k][j];
				}

				v[j] /= vnorms[i].numnormals;
			}
		} else {
			Sys_Error ("Vertex with no non-degenerate triangles attached");
		}

		VectorNormalize (v);

		maxdot = -999999.0;
		maxdotindex = -1;

		for (j = 0; j < NUMVERTEXNORMALS; j++) {
			float	dot;

			dot = DotProduct (v, avertexnormals[j]);
			if (dot > maxdot) {
				maxdot = dot;
				maxdotindex = j;
			}
		}

		ptrivert[i].lightnormalindex = maxdotindex;
	}

	framecount++;

	if (framecount >= MAXFRAMES)
		Sys_Error ("Too many frames; increase MAXFRAMES");

	free (ptri);
	firstframe = 0;
}

static void
Cmd_Frame (int isgroup)
{
	while (Script_TokenAvailable (&scr, false)) {
		Script_GetToken (&scr, false);
		GrabFrame (scr.token->str, isgroup);

		if (!isgroup)
			model.numframes++;
	}
}

static void
Cmd_SkinGroupStart (void)
{
	int			groupskin;

	groupskin = skincount++;
	if (skincount >= MAXFRAMES)
		Sys_Error ("Too many skins; increase MAXSKINS");

	skins[groupskin].type = ALIAS_SKIN_GROUP;
	skins[groupskin].numgroupskins = 0;

	while (1) {
		if (!Script_GetToken (&scr, true))
			Sys_Error ("End of file during group");

		if (!strcmp (scr.token->str, "$skin")) {
			Cmd_Skin ();
			skins[groupskin].numgroupskins++;
		} else if (!strcmp (scr.token->str, "$skingroupend")) {
			break;
		} else {
			Sys_Error ("$skin or $skingroupend expected\n");
		}
	}

	if (skins[groupskin].numgroupskins == 0)
		Sys_Error ("Empty group\n");
}

static void
Cmd_FrameGroupStart (void)
{
	int			groupframe;

	groupframe = framecount++;
	if (framecount >= MAXFRAMES)
		Sys_Error ("Too many frames; increase MAXFRAMES");

	frames[groupframe].type = ALIAS_GROUP;
	frames[groupframe].numgroupframes = 0;

	while (1) {
		if (!Script_GetToken (&scr, true))
			Sys_Error ("End of file during group");

		if (!strcmp (scr.token->str, "$frame")) {
			Cmd_Frame (1);
		} else if (!strcmp (scr.token->str, "$framegroupend")) {
			break;
		} else {
			Sys_Error ("$frame or $framegroupend expected\n");
		}
	}

	frames[groupframe].numgroupframes += framecount - groupframe - 1;

	if (frames[groupframe].numgroupframes == 0)
		Sys_Error ("Empty group\n");
}

static void
Cmd_Origin (void)
{
// rotate points into frame of reference so model points down the positive x
// axis
	Script_GetToken (&scr, false);
	adjust[1] = -atof (scr.token->str);

	Script_GetToken (&scr, false);
	adjust[0] = atof (scr.token->str);

	Script_GetToken (&scr, false);
	adjust[2] = -atof (scr.token->str);
}

static void
Cmd_Eyeposition (void)
{
// rotate points into frame of reference so model points down the positive x
// axis
	Script_GetToken (&scr, false);
	model.eyeposition[1] = atof (scr.token->str);

	Script_GetToken (&scr, false);
	model.eyeposition[0] = -atof (scr.token->str);

	Script_GetToken (&scr, false);
	model.eyeposition[2] = atof (scr.token->str);
}

static void
Cmd_ScaleUp (void)
{
	Script_GetToken (&scr, false);
	scale_up = atof (scr.token->str);
}

static void
Cmd_Flags (void)
{
	Script_GetToken (&scr, false);
	model.flags = atoi (scr.token->str);
}

static void
Cmd_Modelname (void)
{
	WriteModel ();
	Script_GetToken (&scr, false);
	strcpy (outname, scr.token->str);
}

static void
ParseScript (void)
{
	while (1) {
		do {	// look for a line starting with a $ command
			if (!Script_GetToken (&scr, true))
				return;
			if (scr.token->str[0] == '$')
				break;
			while (Script_TokenAvailable (&scr, false))
				Script_GetToken (&scr, false);
		} while (1);

		if (!strcmp (scr.token->str, "$modelname")) {
			Cmd_Modelname ();
		} else if (!strcmp (scr.token->str, "$base")) {
			Cmd_Base ();
		} else if (!strcmp (scr.token->str, "$cd")) {
			if (cdset)
				Sys_Error ("Two $cd in one model");
			cdset = true;
			Script_GetToken (&scr, false);
			strcpy (cdpartial, scr.token->str);
			strcpy (cddir, ExpandPath(scr.token->str));
		} else if (!strcmp (scr.token->str, "$sync")) {
			model.synctype = ST_SYNC;
		} else if (!strcmp (scr.token->str, "$origin")) {
			Cmd_Origin ();
		} else if (!strcmp (scr.token->str, "$eyeposition")) {
			Cmd_Eyeposition ();
		} else if (!strcmp (scr.token->str, "$scale")) {
			Cmd_ScaleUp ();
		} else if (!strcmp (scr.token->str, "$flags")) {
			Cmd_Flags ();
		} else if (!strcmp (scr.token->str, "$frame")) {
			Cmd_Frame (0);
		} else if (!strcmp (scr.token->str, "$skin")) {
			Cmd_Skin ();
			model.numskins++;
		} else if (!strcmp (scr.token->str, "$framegroupstart")) {
			Cmd_FrameGroupStart ();
			model.numframes++;
		} else if (!strcmp (scr.token->str, "$skingroupstart")) {
			Cmd_SkinGroupStart ();
			model.numskins++;
		} else {
			Sys_Error ("bad command %s\n", scr.token->str);
		}
	}
}

int
main (int argc, char **argv)
{
	int         i, bytes;
	dstring_t  *path;
	QFile      *file;
	char       *buf;

	if (argc != 2)
		Sys_Error ("usage: modelgen file.qc");

	i = 1;

// load the script
	path = dstring_strdup (argv[i]);
	QFS_DefaultExtension (path, ".qc");
	SetQdirFromPath (path->str);

	file = Qopen (path->str, "rt");
	if (!file)
		Sys_Error ("couldn't open %s. %s", path->str, strerror(errno));
	bytes = Qfilesize (file);
	buf = malloc (bytes + 1);
	bytes = Qread (file, buf, bytes);
	buf[bytes] = 0;
	Qclose (file);
	Script_Start (&scr, path->str, buf);


// parse it
	memset (&model, 0, sizeof (model));

	for (i = 0; i < 3; i++) {
		framesmins[i] = 9999999;
		framesmaxs[i] = -9999999;
	}

	ClearModel ();
	strcpy (outname, argv[1]);

	ParseScript ();
	WriteModel ();

	return 0;
}
