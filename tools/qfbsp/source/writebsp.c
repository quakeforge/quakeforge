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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include "QF/qendian.h"
#include "QF/sys.h"

#include "bsp5.h"


int		headclipnode;
int		firstface;

//===========================================================================

/*
==================
FindFinalPlane

Used to find plane index numbers for clip nodes read from child processes
==================
*/
int FindFinalPlane (dplane_t *p)
{
	int		i;
	dplane_t	*dplane;
	
	for (i=0, dplane = bsp->planes ; i<bsp->numplanes ; i++, dplane++)
	{
		if (p->type != dplane->type)
			continue;
		if (p->dist != dplane->dist)
			continue;
		if (p->normal[0] != dplane->normal[0])
			continue;
		if (p->normal[1] != dplane->normal[1])
			continue;
		if (p->normal[2] != dplane->normal[2])
			continue;
		return i;
	}
	
//
// new plane
//
	if (bsp->numplanes == MAX_MAP_PLANES)
		Sys_Error ("numplanes == MAX_MAP_PLANES");
	dplane = &bsp->planes[bsp->numplanes];
	*dplane = *p;
	bsp->numplanes++;
	
	return bsp->numplanes - 1;
}



int		planemapping[MAX_MAP_PLANES];

void WriteNodePlanes_r (node_t *node)
{
	plane_t		*plane;
	dplane_t	*dplane;

	if (node->planenum == -1)
		return;
	if (planemapping[node->planenum] == -1)
	{	// a new plane
		planemapping[node->planenum] = bsp->numplanes;
		
		if (bsp->numplanes == MAX_MAP_PLANES)
			Sys_Error ("numplanes == MAX_MAP_PLANES");
		plane = &planes[node->planenum];
		dplane = &bsp->planes[bsp->numplanes];
		dplane->normal[0] = plane->normal[0];
		dplane->normal[1] = plane->normal[1];
		dplane->normal[2] = plane->normal[2];
		dplane->dist = plane->dist;
		dplane->type = plane->type;

		bsp->numplanes++;
	}

	node->outputplanenum = planemapping[node->planenum];
	
	WriteNodePlanes_r (node->children[0]);
	WriteNodePlanes_r (node->children[1]);
}

/*
==================
WriteNodePlanes

==================
*/
void WriteNodePlanes (node_t *nodes)
{
	memset (planemapping,-1, sizeof(planemapping));
	WriteNodePlanes_r (nodes);
}

//===========================================================================

/*
==================
WriteClipNodes_r

==================
*/
int WriteClipNodes_r (node_t *node)
{
	int			i, c;
	dclipnode_t	*cn;
	int			num;
	
// FIXME: free more stuff?	
	if (node->planenum == -1)
	{
		num = node->contents;
		free (node);
		return num;
	}
	
// emit a clipnode
	c = bsp->numclipnodes;
	cn = &bsp->clipnodes[bsp->numclipnodes];
	bsp->numclipnodes++;
	cn->planenum = node->outputplanenum;
	for (i=0 ; i<2 ; i++)
		cn->children[i] = WriteClipNodes_r(node->children[i]);
	
	free (node);
	return c;
}

/*
==================
WriteClipNodes

Called after the clipping hull is completed.  Generates a disk format
representation and frees the original memory.
==================
*/
void WriteClipNodes (node_t *nodes)
{
	headclipnode = bsp->numclipnodes;
	WriteClipNodes_r (nodes);
}

//===========================================================================

/*
==================
WriteLeaf
==================
*/
void WriteLeaf (node_t *node)
{
	face_t		**fp, *f;
	dleaf_t		*leaf_p;
		
// emit a leaf
	leaf_p = &bsp->leafs[bsp->numleafs];
	bsp->numleafs++;

	leaf_p->contents = node->contents;

//
// write bounding box info
//	
	VectorCopy (node->mins, leaf_p->mins);
	VectorCopy (node->maxs, leaf_p->maxs);
	
	leaf_p->visofs = -1;	// no vis info yet
	
//
// write the marksurfaces
//
	leaf_p->firstmarksurface = bsp->nummarksurfaces;
	
	for (fp=node->markfaces ; *fp ; fp++)
	{
	// emit a marksurface
		if (bsp->nummarksurfaces == MAX_MAP_MARKSURFACES)
			Sys_Error ("nummarksurfaces == MAX_MAP_MARKSURFACES");
		f = *fp;
		do
		{
			bsp->marksurfaces[bsp->nummarksurfaces] =  f->outputnumber;
			bsp->nummarksurfaces++;
			f=f->original;		// grab tjunction split faces
		} while (f);
	}
	
	leaf_p->nummarksurfaces = bsp->nummarksurfaces - leaf_p->firstmarksurface;
}


/*
==================
WriteDrawNodes_r
==================
*/
void WriteDrawNodes_r (node_t *node)
{
	dnode_t	*n;
	int		i;

// emit a node	
	if (bsp->numnodes == MAX_MAP_NODES)
		Sys_Error ("numnodes == MAX_MAP_NODES");
	n = &bsp->nodes[bsp->numnodes];
	bsp->numnodes++;

	VectorCopy (node->mins, n->mins);
	VectorCopy (node->maxs, n->maxs);

	n->planenum = node->outputplanenum;
	n->firstface = node->firstface;
	n->numfaces = node->numfaces;

//
// recursively output the other nodes
//	
	
	for (i=0 ; i<2 ; i++)
	{
		if (node->children[i]->planenum == -1)
		{
			if (node->children[i]->contents == CONTENTS_SOLID)
				n->children[i] = -1;
			else
			{
				n->children[i] = -(bsp->numleafs + 1);
				WriteLeaf (node->children[i]);
			}
		}
		else
		{
			n->children[i] = bsp->numnodes;	
			WriteDrawNodes_r (node->children[i]);
		}
	}
}

/*
==================
WriteDrawNodes
==================
*/
void WriteDrawNodes (node_t *headnode)
{
	int		i;
	int		start;
	dmodel_t	*bm;

#if 0
	if (headnode->contents < 0)
		Sys_Error ("FinishBSPModel: empty model");
#endif

// emit a model
	if (bsp->nummodels == MAX_MAP_MODELS)
		Sys_Error ("nummodels == MAX_MAP_MODELS");
	bm = &bsp->models[bsp->nummodels];
	bsp->nummodels++;
	
	bm->headnode[0] = bsp->numnodes;
	bm->firstface = firstface;
	bm->numfaces = bsp->numfaces - firstface;	
	firstface = bsp->numfaces;
	
	start = bsp->numleafs;

	if (headnode->contents < 0)	
		WriteLeaf (headnode);
	else
		WriteDrawNodes_r (headnode);
	bm->visleafs = bsp->numleafs - start;
	
	for (i=0 ; i<3 ; i++)
	{
		bm->mins[i] = headnode->mins[i] + SIDESPACE + 1;	// remove the padding
		bm->maxs[i] = headnode->maxs[i] - SIDESPACE - 1;
	}
// FIXME: are all the children decendant of padded nodes?
}


/*
==================
BumpModel

Used by the clipping hull processes that only need to store headclipnode
==================
*/
void BumpModel (int hullnum)
{
	dmodel_t	*bm;

// emit a model
	if (bsp->nummodels == MAX_MAP_MODELS)
		Sys_Error ("nummodels == MAX_MAP_MODELS");
	bm = &bsp->models[bsp->nummodels];
	bsp->nummodels++;
	
	bm->headnode[hullnum] = headclipnode;
}

//=============================================================================

typedef struct
{
	char		identification[4];		// should be WAD2
	int			numlumps;
	int			infotableofs;
} wadinfo_t;


typedef struct
{
	int			filepos;
	int			disksize;
	int			size;					// uncompressed
	char		type;
	char		compression;
	char		pad1, pad2;
	char		name[16];				// must be null terminated
} lumpinfo_t;

QFile		*texfile;
wadinfo_t	wadinfo;
lumpinfo_t	*lumpinfo;

void CleanupName (char *in, char *out)
{
	int		i;
	
	for (i=0 ; i< 16 ; i++ )
	{
		if (!in[i])
			break;
			
		out[i] = toupper(in[i]);
	}
	
	for ( ; i< 16 ; i++ )
		out[i] = 0;
}


/*
=================
TEX_InitFromWad
=================
*/
void	TEX_InitFromWad (char *path)
{
	int			i;
	
	texfile = Qopen (path, "rb");
	Qread (texfile, &wadinfo, sizeof(wadinfo));
	if (strncmp (wadinfo.identification, "WAD2", 4))
		Sys_Error ("TEX_InitFromWad: %s isn't a wadfile",path);
	wadinfo.numlumps = LittleLong(wadinfo.numlumps);
	wadinfo.infotableofs = LittleLong(wadinfo.infotableofs);
	Qseek (texfile, wadinfo.infotableofs, SEEK_SET);
	lumpinfo = malloc(wadinfo.numlumps*sizeof(lumpinfo_t));
	Qread (texfile, lumpinfo, wadinfo.numlumps*sizeof(lumpinfo_t));
	
	for (i=0 ; i<wadinfo.numlumps ; i++)
	{
		CleanupName (lumpinfo[i].name, lumpinfo[i].name);
		lumpinfo[i].filepos = LittleLong(lumpinfo[i].filepos);
		lumpinfo[i].disksize = LittleLong(lumpinfo[i].disksize);
	}
}

/*
==================
LoadLump
==================
*/
int LoadLump (char *name, byte *dest)
{
	int		i;
	char	cname[16];
	
	CleanupName (name, cname);
	
	for (i=0 ; i<wadinfo.numlumps ; i++)
	{
		if (!strcmp(cname, lumpinfo[i].name))
		{
			Qseek (texfile, lumpinfo[i].filepos, SEEK_SET);
			Qread (texfile, dest, lumpinfo[i].disksize);
			return lumpinfo[i].disksize;
		}
	}
	
	printf ("WARNING: texture %s not found\n", name);
	return 0;
}


/*
==================
AddAnimatingTextures
==================
*/
void AddAnimatingTextures (void)
{
	int		base;
	int		i, j, k;
	char	name[32];

	base = nummiptex;
	
	for (i=0 ; i<base ; i++)
	{
		if (miptex[i][0] != '+')
			continue;
		strcpy (name, miptex[i]);

		for (j=0 ; j<20 ; j++)
		{
			if (j < 10)
				name[1] = '0'+j;
			else
				name[1] = 'A'+j-10;		// alternate animation
			

		// see if this name exists in the wadfile
			for (k=0 ; k<wadinfo.numlumps ; k++)
				if (!strcmp(name, lumpinfo[k].name))
				{
					FindMiptex (name);	// add to the miptex list
					break;
				}
		}
	}
	
	printf ("added %i texture frames\n", nummiptex - base);
}

/*
==================
WriteMiptex
==================
*/
void WriteMiptex (void)
{
	int		i, len;
	byte	*data;
	dmiptexlump_t	*l;
	char	*path;
	char	fullpath[1024];

	path = ValueForKey (&entities[0], "_wad");
	if (!path || !path[0])
	{
		path = ValueForKey (&entities[0], "wad");
		if (!path || !path[0])
		{
			printf ("WARNING: no wadfile specified\n");
			bsp->texdatasize = 0;
			return;
		}
	}

	sprintf (fullpath, "%s/%s", /*FIXME gamedir*/".", path);

	TEX_InitFromWad (fullpath);
	
	AddAnimatingTextures ();

	l = (dmiptexlump_t *)bsp->texdata;
	data = (byte *)&l->dataofs[nummiptex];
	l->nummiptex = nummiptex;
	for (i=0 ; i<nummiptex ; i++)
	{
		l->dataofs[i] = data - (byte *)l;
		len = LoadLump (miptex[i], data);
		if (data + len - bsp->texdata >= MAX_MAP_MIPTEX)
			Sys_Error ("Textures exceeded MAX_MAP_MIPTEX");
		if (!len)
			l->dataofs[i] = -1;	// didn't find the texture
		data += len;
	}

	bsp->texdatasize = data - bsp->texdata;
}

//===========================================================================


/*
==================
BeginBSPFile
==================
*/
void BeginBSPFile (void)
{
// edge 0 is not used, because 0 can't be negated
	bsp->numedges = 1;

// leaf 0 is common solid with no faces
	bsp->numleafs = 1;
	bsp->leafs[0].contents = CONTENTS_SOLID;

	firstface = 0;	
}


/*
==================
FinishBSPFile
==================
*/
void FinishBSPFile (void)
{
	QFile      *f;
	printf ("--- FinishBSPFile ---\n");
	printf ("WriteBSPFile: %s\n", bspfilename);
	
	WriteMiptex ();

	//XXX PrintBSPFileSizes ();
	f = Qopen (bspfilename, "wb");
	WriteBSPFile (bsp, f);
	Qclose (f);
}

