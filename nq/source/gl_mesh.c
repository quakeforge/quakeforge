/*
	gl_mesh.c

	gl_mesh.c: triangle model functions

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

#include "QF/console.h"
#include "QF/mdfour.h"
#include "model.h"
#include "QF/quakefs.h"

/*
	ALIAS MODEL DISPLAY LIST GENERATION
*/

model_t    *aliasmodel;
aliashdr_t *paliashdr;

qboolean    used[8192];

// the command list holds counts and s/t values that are valid for
// every frame
int         commands[8192];
int         numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int         vertexorder[8192];
int         numorder;

int         allverts, alltris;

int         stripverts[128];
int         striptris[128];
int         stripcount;

/*
	StripLength
*/
int
StripLength (int starttri, int startv)
{
	int         m1, m2;
	int         j;
	mtriangle_t *last, *check;
	int         k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 2) % 3];
	m2 = last->vertindex[(startv + 1) % 3];

	// look for a matching triangle
  nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1];
		 j < pheader->mdl.numtris; j++, check++) {
		if (check->facesfront != last->facesfront)
			continue;
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[(k + 2) % 3];
			else
				m1 = check->vertindex[(k + 2) % 3];

			stripverts[stripcount + 2] = check->vertindex[(k + 2) % 3];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
  done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->mdl.numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
	FanLength
*/
int
FanLength (int starttri, int startv)
{
	int         m1, m2;
	int         j;
	mtriangle_t *last, *check;
	int         k;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv) % 3];
	stripverts[1] = last->vertindex[(startv + 1) % 3];
	stripverts[2] = last->vertindex[(startv + 2) % 3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv + 0) % 3];
	m2 = last->vertindex[(startv + 2) % 3];


	// look for a matching triangle
  nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1];
		 j < pheader->mdl.numtris; j++, check++) {
		if (check->facesfront != last->facesfront)
			continue;
		for (k = 0; k < 3; k++) {
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[(k + 2) % 3];

			stripverts[stripcount + 2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}
  done:

	// clear the temp used flags
	for (j = starttri + 1; j < pheader->mdl.numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}


/*
	BuildTris

	Generate a list of trifans or strips
	for the model, which holds for all frames
*/
void
BuildTris (void)
{
	int         i, j, k;
	int         startv;
	float       s, t;
	int         len, bestlen, besttype = 0;
	int         bestverts[1024];
	int         besttris[1024];
	int         type;

	// 
	// build tristrips
	// 
	numorder = 0;
	numcommands = 0;
	memset (used, 0, sizeof (used));
	for (i = 0; i < pheader->mdl.numtris; i++) {
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0; type < 2; type++)
//  type = 1;
		{
			for (startv = 0; startv < 3; startv++) {
				if (type == 1)
					len = StripLength (i, startv);
				else
					len = FanLength (i, startv);
				if (len > bestlen) {
					besttype = type;
					bestlen = len;
					for (j = 0; j < bestlen + 2; j++)
						bestverts[j] = stripverts[j];
					for (j = 0; j < bestlen; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j = 0; j < bestlen; j++)
			used[besttris[j]] = 1;

		if (besttype == 1)
			commands[numcommands++] = (bestlen + 2);
		else
			commands[numcommands++] = -(bestlen + 2);

		for (j = 0; j < bestlen + 2; j++) {
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->mdl.skinwidth / 2;	// on back side
			s = (s + 0.5) / pheader->mdl.skinwidth;
			t = (t + 0.5) / pheader->mdl.skinheight;

			*(float *) &commands[numcommands++] = s;
			*(float *) &commands[numcommands++] = t;
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	Con_DPrintf ("%3i tri %3i vert %3i cmd\n", pheader->mdl.numtris, numorder,
				 numcommands);

	allverts += numorder;
	alltris += pheader->mdl.numtris;
}


/*
	GL_MakeAliasModelDisplayLists
*/
void
GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m, int _s)
{
	int         i, j;
	int        *cmds;
	trivertx_t *verts;
	char        cache[MAX_QPATH], fullpath[MAX_OSPATH];
	QFile      *f;
	unsigned char model_digest[MDFOUR_DIGEST_BYTES];
	unsigned char mesh_digest[MDFOUR_DIGEST_BYTES];
	qboolean    remesh = true;

	aliasmodel = m;
	paliashdr = hdr;					// (aliashdr_t *)Mod_Extradata (m);

	mdfour (model_digest, (unsigned char *) _m, _s);

	// 
	// look for a cached version
	// 
	strcpy (cache, "glquake/");
	COM_StripExtension (m->name + strlen ("progs/"),
						cache + strlen ("glquake/"));
	strncat (cache, ".ms2", sizeof (cache) - strlen (cache));

	COM_FOpenFile (cache, &f);
	if (f) {
		unsigned char d1[MDFOUR_DIGEST_BYTES];
		unsigned char d2[MDFOUR_DIGEST_BYTES];
		struct mdfour md;
		int         c[8192];
		int         nc;
		int         vo[8192];
		int         no;

		memset (d1, 0, sizeof (d1));
		memset (d2, 0, sizeof (d2));
		Qread (f, &nc, 4);
		Qread (f, &no, 4);
		if (nc <= 8192 && no <= 8192) {
			Qread (f, &c, nc * sizeof (c[0]));
			Qread (f, &vo, no * sizeof (vo[0]));
			Qread (f, d1, MDFOUR_DIGEST_BYTES);
			Qread (f, d2, MDFOUR_DIGEST_BYTES);
			Qclose (f);

			mdfour_begin (&md);
			mdfour_update (&md, (unsigned char *) &nc, 4);
			mdfour_update (&md, (unsigned char *) &no, 4);
			mdfour_update (&md, (unsigned char *) &c, nc * sizeof (c[0]));
			mdfour_update (&md, (unsigned char *) &vo, no * sizeof (vo[0]));
			mdfour_update (&md, d1, MDFOUR_DIGEST_BYTES);
			mdfour_result (&md, mesh_digest);

			if (memcmp (d2, mesh_digest, MDFOUR_DIGEST_BYTES) == 0
				&& memcmp (d1, model_digest, MDFOUR_DIGEST_BYTES) == 0) {
				remesh = false;
				numcommands = nc;
				numorder = no;
				memcpy (commands, c, numcommands * sizeof (c[0]));
				memcpy (vertexorder, vo, numorder * sizeof (vo[0]));
			}
		}
	}
	if (remesh) {
		// 
		// build it from scratch
		// 
		Con_Printf ("meshing %s...\n", m->name);

		BuildTris ();					// trifans or lists

		// 
		// save out the cached version
		// 
		snprintf (fullpath, sizeof (fullpath), "%s/%s", com_gamedir, cache);
		f = Qopen (fullpath, "wbz9");
		if (!f) {
			COM_CreatePath (fullpath);
			f = Qopen (fullpath, "wb");
		}

		if (f) {
			struct mdfour md;

			mdfour_begin (&md);
			mdfour_update (&md, (unsigned char *) &numcommands, 4);
			mdfour_update (&md, (unsigned char *) &numorder, 4);
			mdfour_update (&md, (unsigned char *) &commands,
						   numcommands * sizeof (commands[0]));
			mdfour_update (&md, (unsigned char *) &vertexorder,
						   numorder * sizeof (vertexorder[0]));
			mdfour_update (&md, model_digest, MDFOUR_DIGEST_BYTES);
			mdfour_result (&md, mesh_digest);

			Qwrite (f, &numcommands, 4);
			Qwrite (f, &numorder, 4);
			Qwrite (f, &commands, numcommands * sizeof (commands[0]));
			Qwrite (f, &vertexorder, numorder * sizeof (vertexorder[0]));
			Qwrite (f, model_digest, MDFOUR_DIGEST_BYTES);
			Qwrite (f, mesh_digest, MDFOUR_DIGEST_BYTES);
			Qclose (f);
		}
	}
	// save the data out

	paliashdr->poseverts = numorder;

	cmds = Hunk_Alloc (numcommands * 4);
	paliashdr->commands = (byte *) cmds - (byte *) paliashdr;
	memcpy (cmds, commands, numcommands * 4);

	verts = Hunk_Alloc (paliashdr->numposes * paliashdr->poseverts

						* sizeof (trivertx_t));
	paliashdr->posedata = (byte *) verts - (byte *) paliashdr;
	for (i = 0; i < paliashdr->numposes; i++)
		for (j = 0; j < numorder; j++)
			*verts++ = poseverts[i][vertexorder[j]];
}
