/*
	Copyright (C) 1996-1997  Id Software, Inc.

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
#include <errno.h>

#include "QF/dstring.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/wad.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/options.h"
#include "tools/qfbsp/include/writebsp.h"

/**	\addtogroup qfbsp_writebsp
*/
//@{

int         headclipnode;
int         firstface;


int
FindFinalPlane (const dplane_t *p)
{
	const dplane_t *dplane;
	int         i;

	for (i = 0, dplane = bsp->planes; i < bsp->numplanes; i++, dplane++) {
		if (p->type != dplane->type)
			continue;
		if (p->dist != dplane->dist)
			continue;
		if (!VectorCompare (p->normal, dplane->normal))
			continue;
		return i;
	}

	// new plane
	if (bsp->numplanes == MAX_MAP_PLANES)
		Sys_Error ("numplanes == MAX_MAP_PLANES");
	BSP_AddPlane (bsp, p);

	return bsp->numplanes - 1;
}

int         planemapping[MAX_MAP_PLANES];

/**	Recursively write the nodes' planes to the bsp file.

	If the plane has already been written, do not write it again. Set the
	node's output plane number.

	\param node		The current node of which to write the plane.
*/
static void
WriteNodePlanes_r (node_t *node)
{
	dplane_t    dplane;
	plane_t    *plane;

	if (node->planenum == -1)
		return;
	if (planemapping[node->planenum] == -1) {	// a new plane
		planemapping[node->planenum] = bsp->numplanes;

		plane = &planes[node->planenum];

		VectorCopy (plane->normal, dplane.normal);
		dplane.dist = plane->dist;
		dplane.type = plane->type;
		BSP_AddPlane (bsp, &dplane);
	}

	node->outputplanenum = planemapping[node->planenum];

	WriteNodePlanes_r (node->children[0]);
	WriteNodePlanes_r (node->children[1]);
}

void
WriteNodePlanes (node_t *nodes)
{
	memset (planemapping, -1, sizeof (planemapping));
	WriteNodePlanes_r (nodes);
}

/**	Recursively write clip nodes to the bsp file.

	\note The node will be freed.

	\param node		The node to be written to the bsp file.
*/
static int
WriteClipNodes_r (node_t *node)
{
	dclipnode_t cn;
	int         num, c, i;

	// FIXME: free more stuff?
	if (node->planenum == -1) {
		num = node->contents;
		free (node);
		return num;
	}
	// emit a clipnode
	c = bsp->numclipnodes;
	BSP_AddClipnode (bsp, &cn);
	cn.planenum = node->outputplanenum;
	for (i = 0; i < 2; i++)
		cn.children[i] = WriteClipNodes_r (node->children[i]);
	bsp->clipnodes[c] = cn;

	free (node);
	return c;
}

void
WriteClipNodes (node_t *nodes)
{
	headclipnode = bsp->numclipnodes;
	WriteClipNodes_r (nodes);
}

/**	Write a leaf node to the bsp file.

	\param node		The leaf node to be written to the bsp file.
*/
static void
WriteLeaf (const node_t *node)
{
	dleaf_t     leaf_p;
	face_t    **fp, *f;

	// emit a leaf
	leaf_p.contents = node->contents;

	// write bounding box info
	VectorCopy (node->mins, leaf_p.mins);
	VectorCopy (node->maxs, leaf_p.maxs);

	leaf_p.visofs = -1;					// no vis info yet

	// write the marksurfaces
	leaf_p.firstmarksurface = bsp->nummarksurfaces;

	for (fp = node->markfaces; *fp; fp++) {
		// emit a marksurface
		if (bsp->nummarksurfaces == MAX_MAP_MARKSURFACES)
			Sys_Error ("nummarksurfaces == MAX_MAP_MARKSURFACES");
		f = *fp;
		if (f->texturenum < 0)
			continue;
		do {
			BSP_AddMarkSurface (bsp, f->outputnumber);
			f = f->original;			// grab tjunction split faces
		} while (f);
	}

	leaf_p.nummarksurfaces = bsp->nummarksurfaces - leaf_p.firstmarksurface;

	memset (leaf_p.ambient_level, 0, sizeof (leaf_p.ambient_level));
	BSP_AddLeaf (bsp, &leaf_p);
}

/**	Recursively write the draw nodes of the map bsp.

	\param node		The current node to be written.
*/
static void
WriteDrawNodes_r (const node_t *node)
{
	static dnode_t dummy;
	dnode_t    *n;
	int         i;
	int         nodenum = bsp->numnodes;

	// emit a node
	if (bsp->numnodes == MAX_MAP_NODES)
		Sys_Error ("numnodes == MAX_MAP_NODES");
	BSP_AddNode (bsp, &dummy);
	n = &bsp->nodes[nodenum];

	VectorCopy (node->mins, n->mins);
	VectorCopy (node->maxs, n->maxs);

	n->planenum = node->outputplanenum;
	n->firstface = node->firstface;
	n->numfaces = node->numfaces;

	// recursively output the other nodes
	for (i = 0; i < 2; i++) {
		n = &bsp->nodes[nodenum];
		if (node->children[i]->planenum == -1) {
			if (node->children[i]->contents == CONTENTS_SOLID)
				n->children[i] = -1;
			else {
				n->children[i] = -(bsp->numleafs + 1);
				WriteLeaf (node->children[i]);
			}
		} else {
			n->children[i] = bsp->numnodes;
			WriteDrawNodes_r (node->children[i]);
		}
	}
}

void
WriteDrawNodes (const node_t *headnode)
{
	dmodel_t    bm;
	int         start, i;

#if 0
	if (headnode->contents < 0)
		Sys_Error ("FinishBSPModel: empty model");
#endif

	// emit a model
	bm.headnode[0] = bsp->numnodes;
	for (i = 1; i < MAX_MAP_HULLS; i++)
		bm.headnode[i] = 0;
	bm.firstface = firstface;
	bm.numfaces = bsp->numfaces - firstface;
	firstface = bsp->numfaces;

	start = bsp->numleafs;

	if (headnode->contents < 0)
		WriteLeaf (headnode);
	else
		WriteDrawNodes_r (headnode);
	bm.visleafs = bsp->numleafs - start;

	for (i = 0; i < 3; i++) {
		bm.mins[i] = headnode->mins[i] + SIDESPACE + 1;   // remove the padding
		bm.maxs[i] = headnode->maxs[i] - SIDESPACE - 1;
	}
	VectorZero (bm.origin);
	BSP_AddModel (bsp, &bm);
	// FIXME: are all the children decendant of padded nodes?
}

void
BumpModel (int hullnum)
{
	static dmodel_t bm;

	// emit a model
	bm.headnode[hullnum] = headclipnode;
	BSP_AddModel (bsp, &bm);
}

typedef struct wadlist_s {
	struct wadlist_s *next;
	const char *path;
	wad_t      *wad;
} wadlist_t;

wadlist_t  *wadlist;

/**	Load a texture wad file.
	\param path		The path to the wad file.
*/
static int
TEX_InitFromWad (const char *path)
{
	wadlist_t  *wl;
	wad_t      *wad;

	wad = wad_open (path);
#ifdef HAVE_ZLIB
	if (!wad)
		wad = wad_open (path = va (0, "%s.gz", path));
#endif
	if (!wad)
		return -1;
	printf ("wadfile: %s\n", path);

	wl = calloc (1, sizeof (wadlist_t));
	wl->path = strdup (path);

	wl->wad = wad;

	wl->next = wadlist;
	wadlist = wl;
	return 0;
}

/**	Read a lump's data.

	Searches all loaded wad files for the lump.

	\param name		The name of the lump for which to search.
	\param dest		The destination buffer to which the lump's data will be
					written.
	\return			The size of the lump's data or 0 if the lump was not
					found.
*/
static int
LoadLump (const char *name, dstring_t *dest)
{
	int         r;
	int         ofs = dest->size;
	wadlist_t  *wl;
	lumpinfo_t *lump;
	QFile      *texfile;

	for (wl = wadlist; wl; wl = wl->next) {
		if ((lump = wad_find_lump (wl->wad, name))) {
			dest->size += lump->disksize;
			dstring_adjust (dest);
			texfile = Qopen (wl->path, "rbz");
			if (!texfile) {
				printf ("couldn't open %s\n", wl->path);
				continue;
			}
			r = Qseek (texfile, lump->filepos, SEEK_SET);
			if (r == -1) {
				printf ("seek error: %s:%s %d\n", wl->path,
						lump->name, lump->filepos);
				Qclose (texfile);
				continue;
			}
			r = Qread (texfile, dest->str + ofs, lump->disksize);
			Qclose (texfile);
			if (r != lump->disksize) {
				printf ("seek error: %s:%s %d\n", wl->path,
						lump->name, lump->disksize);
				continue;
			}
			return lump->disksize;
		}
	}

	printf ("WARNING: texture %s not found\n", name);
	return 0;
}

/**	Search loaded miptex for animated textures and load their animations.
*/
static void
AddAnimatingTextures (void)
{
	int         base, i, j;
	char        name[32];
	wadlist_t  *wl;

	base = nummiptexnames;

	name[sizeof (name) - 1] = 0;

	for (i = 0; i < base; i++) {
		if (miptexnames[i][0] != '+')
			continue;
		strncpy (name, miptexnames[i], sizeof (name) - 1);

		for (j = 0; j < 20; j++) {
			if (j < 10)
				name[1] = '0' + j;
			else
				name[1] = 'A' + j - 10;	// alternate animation

			// see if this name exists in the wadfile
			for (wl = wadlist; wl; wl = wl->next) {
				if (wad_find_lump (wl->wad, name)) {
					FindMiptex (name);	// add to the miptex list
					break;
				}
			}
		}
	}

	if (nummiptexnames - base)
		printf ("added %i texture frames\n", nummiptexnames - base);
}

/**	Write the miptex data to the bsp file.

	The miptex data is read from the wad files specified by the \c wad or
	\c _wad key of the world entity. The files are a semi-colon separated list
	(this is a QuakeForge extension). The \c wadpath is used to search for the
	files.
*/
static void
WriteMiptex (void)
{
	dstring_t  *data;
	char       *wad_list, *wad, *w;
	char       *path_list, *path, *p;
	dstring_t  *fullpath;
	dmiptexlump_t *l;
	int         i, len, res = -1;

	wad_list = (char *) ValueForKey (&entities[0], "_wad");
	if (!wad_list || !wad_list[0]) {
		wad_list = (char *) ValueForKey (&entities[0], "wad");
		if (!wad_list || !wad_list[0]) {
			printf ("WARNING: no wadfile specified\n");
			bsp->texdatasize = 0;
			return;
		}
	}
	fullpath = dstring_new ();
	wad = wad_list = strdup (wad_list);
	w = strchr (wad, ';');
	if (w)
		*w++ = 0;
	while (1) {
		path = path_list = strdup (options.wadpath);
		p = strchr (path, ';');
		if (p)
			*p++ = 0;
		while (1) {
			dsprintf (fullpath, "%s%s%s", path, path[0] ? "/" : "", wad);
			res = TEX_InitFromWad (fullpath->str);
			if (!res)
				break;
			path = p;
			if (!path || !*path)
				break;
			p = strchr (path, ';');
			if (p)
				*p++ = 0;
		}
		free (path_list);
		if (res == -1)
			Sys_Error ("couldn't open %s[.gz]", wad);

		wad = w;
		if (!wad || !*wad)
			break;
		w = strchr (wad, ';');
		if (w)
			*w++ = 0;
	}
	free (wad_list);
	dstring_delete (fullpath);

	AddAnimatingTextures ();

	data = dstring_new ();
	data->size = sizeof (dmiptexlump_t);
	dstring_adjust (data);

	l = (dmiptexlump_t *) data->str;
	l->nummiptex = nummiptexnames;
	data->size = (char *) &l->dataofs[nummiptexnames] - data->str;
	dstring_adjust (data);

	for (i = 0; i < nummiptexnames; i++) {
		l = (dmiptexlump_t *) data->str;
		l->dataofs[i] = data->size;
		len = LoadLump (miptexnames[i], data);
		l = (dmiptexlump_t *) data->str;
		if (!len)
			l->dataofs[i] = -1;			// didn't find the texture
	}
	BSP_AddTextures (bsp, (byte *) data->str, data->size);
}

void
BeginBSPFile (void)
{
	static dedge_t edge;
	static dleaf_t leaf;
	// edge 0 is not used, because 0 can't be negated
	BSP_AddEdge (bsp, &edge);

	// leaf 0 is common solid with no faces
	leaf.contents = CONTENTS_SOLID;
	leaf.visofs = -1;
	BSP_AddLeaf (bsp, &leaf);

	firstface = 0;
}

void
FinishBSPFile (void)
{
	QFile      *f;

	printf ("--- FinishBSPFile ---\n");

	WriteMiptex ();

// XXX	PrintBSPFileSizes ();
	printf ("WriteBSPFile: %s\n", options.bspfile);
	f = Qopen (options.bspfile, "wb");
	if (!f)
		Sys_Error ("couldn't open %s. %s", options.bspfile, strerror(errno));
	WriteBSPFile (bsp, f);
	Qclose (f);
}

//@}
