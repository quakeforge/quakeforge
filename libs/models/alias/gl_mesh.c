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

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/mdfour.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "QF/GL/qf_alias.h"

#include "mod_internal.h"

#include "compat.h"

// ALIAS MODEL DISPLAY LIST GENERATION ========================================

static int        *used;
static int         used_size;

// the command list holds counts and s/t values that are valid for every frame
static int        *commands;
static int         numcommands;
static int         commands_size;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
static int        *vertexorder;
static int         numorder;
static int         vertexorder_size;

static int         allverts, alltris;

static int        *stripverts;
static int        *striptris;
static int         stripcount;
static int         strip_size;


static inline void
alloc_used (int size)
{
	if (size <= used_size)
		return;
	size = (size + 1023) & ~1023;
	used = realloc (used, size * sizeof (used[0]));
	if (!used)
		Sys_Error ("gl_mesh: out of memory");
	used_size = size;
}

static inline void
add_command (int cmd)
{
	if (numcommands + 1 > commands_size) {
		commands_size += 1024;
		commands = realloc (commands, commands_size * sizeof (commands[0]));
		if (!commands)
			Sys_Error ("gl_mesh: out of memory");
	}
	commands[numcommands++] = cmd;
}

static inline void
add_vertex (int vert)
{
	if (numorder + 1 > vertexorder_size) {
		vertexorder_size += 1024;
		vertexorder = realloc (vertexorder, vertexorder_size * sizeof (vertexorder[0]));
		if (!vertexorder)
			Sys_Error ("gl_mesh: out of memory");
	}
	vertexorder[numorder++] = vert;
}

static inline void
add_strip (int vert, int tri)
{
	if (stripcount + 1 > strip_size) {
		strip_size += 1024;
		stripverts = realloc (stripverts, strip_size * sizeof (stripverts[0]));
		striptris = realloc (striptris, strip_size * sizeof (striptris[0]));
		if (!stripverts || !striptris)
			Sys_Error ("gl_mesh: out of memory");
	}
	stripverts[stripcount] = vert;
	striptris[stripcount] = tri;
	stripcount++;
}

static int
StripLength (mod_alias_ctx_t *alias_ctx, int starttri, int startv)
{
	auto mdl = alias_ctx->mdl;
	int         m1, m2, j, k;
	dtriangle_t *last, *check;

	used[starttri] = 2;

	last = &alias_ctx->triangles.a[starttri];

	stripcount = 0;
	add_strip (last->vertindex[(startv) % 3], starttri);
	add_strip (last->vertindex[(startv + 1) % 3], starttri);
	add_strip (last->vertindex[(startv + 2) % 3], starttri);

	m1 = last->vertindex[(startv + 2) % 3];
	m2 = last->vertindex[(startv + 1) % 3];

	// look for a matching triangle
nexttri:
	for (j = starttri + 1, check = &alias_ctx->triangles.a[starttri + 1];
		 j < mdl->numtris; j++, check++) {
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

			add_strip (check->vertindex[(k + 2) % 3], j);

			used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < mdl->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount - 2;
}

static int
FanLength (mod_alias_ctx_t *alias_ctx, int starttri, int startv)
{
	auto mdl = alias_ctx->mdl;
	int         m1, m2, j, k;
	dtriangle_t *last, *check;

	used[starttri] = 2;

	last = &alias_ctx->triangles.a[starttri];

	stripcount = 0;
	add_strip (last->vertindex[(startv) % 3], starttri);
	add_strip (last->vertindex[(startv + 1) % 3], starttri);
	add_strip (last->vertindex[(startv + 2) % 3], starttri);

	m1 = last->vertindex[(startv + 0) % 3];
	m2 = last->vertindex[(startv + 2) % 3];


	// look for a matching triangle
  nexttri:
	for (j = starttri + 1, check = &alias_ctx->triangles.a[starttri + 1];
		 j < mdl->numtris; j++, check++) {
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

			add_strip (m2, j);

			used[j] = 2;
			goto nexttri;
		}
	}
  done:

	// clear the temp used flags
	for (j = starttri + 1; j < mdl->numtris; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount - 2;
}

/*
	BuildTris

	Generate a list of trifans or strips
	for the model, which holds for all frames
*/
static void
BuildTris (mod_alias_ctx_t *alias_ctx)
{
	auto mdl = alias_ctx->mdl;
	float		s, t;
	int			bestlen, len, startv, type, i, j, k;
	int			besttype = 0;
	int		   *bestverts = 0, *besttris = 0;

	// build tristrips
	numorder = 0;
	numcommands = 0;
	stripcount = 0;
	alloc_used (mdl->numtris);
	memset (used, 0, used_size * sizeof (used[0]));

	for (i = 0; i < mdl->numtris; i++) {
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (type = 0; type < 2; type++) {
//  type = 1;
			for (startv = 0; startv < 3; startv++) {
				if (type == 1)
					len = StripLength (alias_ctx, i, startv);
				else
					len = FanLength (alias_ctx, i, startv);
				if (len > bestlen) {
					besttype = type;
					bestlen = len;
					if (bestverts)
						free (bestverts);
					if (besttris)
						free (besttris);
					bestverts = stripverts;
					besttris = striptris;
					stripverts = striptris = 0;
					strip_size = 0;
				}
			}
		}

		// mark the tris on the best strip as used
		for (j = 0; j < bestlen; j++)
			used[besttris[j + 2]] = 1;

		if (besttype == 1)
			add_command (bestlen + 2);
		else
			add_command (-(bestlen + 2));

		for (j = 0; j < bestlen + 2; j++) {
			int         tmp;
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			add_vertex (k);

			// emit s/t coords into the commands stream
			s = alias_ctx->stverts.a[k].s;
			t = alias_ctx->stverts.a[k].t;
			if (!alias_ctx->triangles.a[besttris[0]].facesfront
				&& alias_ctx->stverts.a[k].onseam)
				s += mdl->skinwidth / 2;	// on back side
			s = (s + 0.5) / mdl->skinwidth;
			t = (t + 0.5) / mdl->skinheight;

			memcpy (&tmp, &s, 4);
			add_command (tmp);
			memcpy (&tmp, &t, 4);
			add_command (tmp);
		}
	}

	add_command (0);					// end of list marker

	Sys_MaskPrintf (SYS_dev, "%3i tri %3i vert %3i cmd\n",
					mdl->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += mdl->numtris;

	if (bestverts)
		free (bestverts);
	if (besttris)
		free (besttris);
}

void
gl_Mod_MakeAliasModelDisplayLists (mod_alias_ctx_t *alias_ctx, void *_m,
								   int _s, int extra)
{
	auto model = alias_ctx->model;
	auto mesh = alias_ctx->mesh;
	auto mdl = alias_ctx->mdl;
	dstring_t  *cache, *fullpath;
	unsigned char model_digest[MDFOUR_DIGEST_BYTES];
	unsigned char mesh_digest[MDFOUR_DIGEST_BYTES];
	int         i, j;
	QFile      *f;
	bool        remesh = true;
	bool        do_cache = false;

	cache = dstring_new ();
	fullpath = dstring_new ();

	if (!gl_alias_render_tri) {

		if (gl_mesh_cache && gl_mesh_cache <= mdl->numtris) {
			do_cache = true;

			mdfour (model_digest, (unsigned char *) _m, _s);

			// look for a cached version
			dstring_copystr (cache, "glquake/");
			dstring_appendstr (cache, alias_ctx->mod->path);
			QFS_StripExtension (alias_ctx->mod->path + strlen ("progs/"),
							cache->str + strlen ("glquake/"));
			dstring_appendstr (cache, ".qfms");

			f = QFS_FOpenFile (cache->str);
			if (f) {
				unsigned char d1[MDFOUR_DIGEST_BYTES];
				unsigned char d2[MDFOUR_DIGEST_BYTES];
				struct mdfour md;
				int			len, vers;
				int         nc = 0, no = 0;
				int        *c = 0, *vo = 0;

				memset (d1, 0, sizeof (d1));
				memset (d2, 0, sizeof (d2));

				Qread (f, &vers, sizeof (int));
				Qread (f, &len, sizeof (int));
				Qread (f, &nc, sizeof (int));
				Qread (f, &no, sizeof (int));

				if (vers == 1 && (nc + no) == len) {
					c = malloc (((nc + 1023) & ~1023) * sizeof (c[0]));
					vo = malloc (((no + 1023) & ~1023) * sizeof (vo[0]));
					if (!c || !vo)
						Sys_Error ("gl_mesh.c: out of memory");
					Qread (f, c, nc * sizeof (c[0]));
					Qread (f, vo, no * sizeof (vo[0]));
					Qread (f, d1, MDFOUR_DIGEST_BYTES);
					Qread (f, d2, MDFOUR_DIGEST_BYTES);
					Qclose (f);

					mdfour_begin (&md);
					mdfour_update (&md, (unsigned char *) &vers, sizeof(int));
					mdfour_update (&md, (unsigned char *) &len, sizeof(int));
					mdfour_update (&md, (unsigned char *) &nc, sizeof(int));
					mdfour_update (&md, (unsigned char *) &no, sizeof(int));
					mdfour_update (&md, (unsigned char *) c, nc * sizeof (c[0]));
					mdfour_update (&md, (unsigned char *) vo, no * sizeof (vo[0]));
					mdfour_update (&md, d1, MDFOUR_DIGEST_BYTES);
					mdfour_result (&md, mesh_digest);

					if (memcmp (d2, mesh_digest, MDFOUR_DIGEST_BYTES) == 0
						&& memcmp (d1, model_digest, MDFOUR_DIGEST_BYTES) == 0) {
						remesh = false;
						numcommands = nc;
						numorder = no;
						if (numcommands > commands_size) {
							if (commands)
								free (commands);
							commands_size = (numcommands + 1023) & ~1023;
							commands = c;
						} else {
							memcpy (commands, c, numcommands * sizeof (c[0]));
							free(c);
						}
						if (numorder > vertexorder_size) {
							if (vertexorder)
								free (vertexorder);
							vertexorder_size = (numorder + 1023) & ~1023;
							vertexorder = vo;
						} else {
							memcpy (vertexorder, vo, numorder * sizeof (vo[0]));
							free (vo);
						}
					}
				}
			}
		}
		if (remesh) {
			// build it from scratch
			Sys_MaskPrintf (SYS_dev, "meshing %s...\n", alias_ctx->mod->path);

			BuildTris (alias_ctx);					// trifans or lists

			if (do_cache) {
				// save out the cached version
				dsprintf (fullpath, "%s/%s", qfs_gamedir->dir.def, cache->str);
				f = QFS_WOpen (fullpath->str, 9);

				if (f) {
					struct mdfour md;
					int         vers = 1;
					int         len = numcommands + numorder;

					mdfour_begin (&md);
					mdfour_update (&md, (unsigned char *) &vers, sizeof (int));
					mdfour_update (&md, (unsigned char *) &len, sizeof (int));
					mdfour_update (&md, (unsigned char *) &numcommands,
								   sizeof (int));
					mdfour_update (&md, (unsigned char *) &numorder, sizeof (int));
					mdfour_update (&md, (unsigned char *) commands,
								   numcommands * sizeof (commands[0]));
					mdfour_update (&md, (unsigned char *) vertexorder,
								   numorder * sizeof (vertexorder[0]));
					mdfour_update (&md, model_digest, MDFOUR_DIGEST_BYTES);
					mdfour_result (&md, mesh_digest);

					Qwrite (f, &vers, sizeof (int));
					Qwrite (f, &len, sizeof (int));
					Qwrite (f, &numcommands, sizeof (int));
					Qwrite (f, &numorder, sizeof (int));
					Qwrite (f, commands, numcommands * sizeof (commands[0]));
					Qwrite (f, vertexorder, numorder * sizeof (vertexorder[0]));
					Qwrite (f, model_digest, MDFOUR_DIGEST_BYTES);
					Qwrite (f, mesh_digest, MDFOUR_DIGEST_BYTES);
					Qclose (f);
				}
			}
		}
	} else {
		numorder = 0;
		for (i=0; i < mdl->numtris; i++) {
			add_vertex(alias_ctx->triangles.a[i].vertindex[0]);
			add_vertex(alias_ctx->triangles.a[i].vertindex[1]);
			add_vertex(alias_ctx->triangles.a[i].vertindex[2]);
		}
	}

	int numposes = alias_ctx->poseverts.size;
	int size = sizeof (gl_alias_mesh_t);
	if (extra) {
		size += sizeof (trivertx16_t[numposes * numorder]);
	} else {
		size += sizeof (trivertx_t[numposes * numorder]);
	}
	gl_alias_mesh_t *rmesh = nullptr;
	void *vertices = nullptr;

	if (!gl_alias_render_tri) {
		size += sizeof (int[numcommands]);
		rmesh = Hunk_AllocName (0, size, alias_ctx->mod->name);
		auto cmds = (int *) &rmesh[1];
		vertices = &cmds[numcommands];

		rmesh->commands = (byte *) cmds - (byte *) rmesh;
		memcpy (cmds, commands, sizeof (int[numcommands]));
	} else {
		size += sizeof (tex_coord_t[numorder]);
		rmesh = Hunk_AllocName (0, size, alias_ctx->mod->name);
		auto tex_coord = (tex_coord_t *) &rmesh[1];
		vertices = &tex_coord[numorder];

		rmesh->tex_coord = (byte *) tex_coord - (byte *) rmesh;
		for (i=0; i < numorder; i++) {
			float s, t;
			int k;
			k = vertexorder[i];
			s = alias_ctx->stverts.a[k].s;
			t = alias_ctx->stverts.a[k].t;
			if (!alias_ctx->triangles.a[i/3].facesfront
				&& alias_ctx->stverts.a[k].onseam)
				s += mdl->skinwidth / 2;	// on back side
			s = (s + 0.5) / mdl->skinwidth;
			t = (t + 0.5) / mdl->skinheight;
			tex_coord[i].st[0] = s;
			tex_coord[i].st[1] = t;
		}
	}

	auto frames = (frame_t *) ((byte *) mesh + mesh->morph.frames);
	if (extra) {
		trivertx16_t *verts = vertices;
		for (i = 0; i < numposes; i++) {
			trivertx_t *pv = alias_ctx->poseverts.a[i];
			frames[i].data = (byte *) verts - (byte *) rmesh;
			for (j = 0; j < numorder; j++) {
				trivertx16_t v;
				// convert MD16's split coordinates into something a little
				// saner. The first chunk of vertices is fully compatible with
				// IDPO alias models (even the scale). The second chunk is the
				// fractional bits of the vertex, giving 8.8. However, it's
				// easier for us to multiply everything by 256 and adjust the
				// model scale appropriately
				VectorMultAdd (pv[vertexorder[j] + mdl->numverts].v,
							   256, pv[vertexorder[j]].v, v.v);
				v.lightnormalindex =
					alias_ctx->poseverts.a[i][vertexorder[j]].lightnormalindex;
				*verts++ = v;
			}
		}
	} else {
		trivertx_t *verts = vertices;
		for (i = 0; i < numposes; i++) {
			frames[i].data = (byte *) verts - (byte *) rmesh;
			for (j = 0; j < numorder; j++) {
				*verts++ = alias_ctx->poseverts.a[i][vertexorder[j]];
			}
		}
	}
	VectorCopy (mdl->scale, rmesh->scale);
	VectorCopy (mdl->scale_origin, rmesh->scale_origin);
	rmesh->numverts = numorder;
	rmesh->numtris = mdl->numtris;
	rmesh->extra = extra;
	model->render_data = (byte *) rmesh - (byte *) model;

	dstring_delete (cache);
	dstring_delete (fullpath);
}
