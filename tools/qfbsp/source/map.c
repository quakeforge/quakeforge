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
#include <errno.h>
#include <ctype.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/map.h"

/**	\addtogroup qfbsp_map
*/
//@{

int         nummapbrushfaces;
int         nummapbrushes;

#define ENTITIES_CHUNK 16
int         num_entities;
int         max_entities;
entity_t   *entities;

#define MIPTEXNAME_CHUNK 16
int         nummiptexnames;
int         maxmiptexnames;
const char **miptexnames;
hashtab_t  *miptex_hash;

int         numdetailbrushes;

script_t   *map_script;

static void __attribute__ ((format (PRINTF, 1, 2), noreturn))
map_error (const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	fprintf (stderr, "%s:%d: ", map_script->file, map_script->line);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
	exit (1);
}

static void __attribute__ ((format (PRINTF, 1, 2)))
map_warning (const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	fprintf (stderr, "%s:%d: warning:", map_script->file, map_script->line);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
}

static const char *
miptex_getkey (const void *key, void *unused)
{
	intptr_t    index = (intptr_t) key;
	return miptexnames[index - 1];
}

int
FindMiptex (const char *name)
{
	intptr_t    index;
	char        mpname[MIPTEXNAME];

	strncpy (mpname, name, MIPTEXNAME - 1);
	mpname[MIPTEXNAME - 1] = 0;
	if (strcmp (name, "hint") == 0)
		return TEX_HINT;
	if (strcmp (name, "skip") == 0)
		return TEX_SKIP;
	if (!miptex_hash)
		miptex_hash = Hash_NewTable (1023, miptex_getkey, 0, 0, 0);
	if (miptexnames) {
		index = (intptr_t) Hash_Find (miptex_hash, mpname);
		if (index)
			return index - 1;
	}
	if (nummiptexnames == maxmiptexnames) {
		maxmiptexnames += MIPTEXNAME_CHUNK;
		miptexnames = realloc (miptexnames,
							   maxmiptexnames * sizeof (const char *));
	}
	index = nummiptexnames++;
	miptexnames[index] = strdup (mpname);
	Hash_Add (miptex_hash, (void *) (index + 1));
	return index;
}

/*
	FindTexinfo

	Returns a global texinfo number
*/
static int
FindTexinfo (texinfo_t *t)
{
	size_t      i;
	int         j;
	texinfo_t  *tex;

	if (t->miptex == ~0u)
		return t->miptex;		// it's HINT or SKIP

	// set the special flag
	if (miptexnames[t->miptex][0] == '*'
		|| !strncasecmp (miptexnames[t->miptex], "sky", 3))
		t->flags |= TEX_SPECIAL;

	tex = bsp->texinfo;
	for (i = 0; i < bsp->numtexinfo; i++, tex++) {
		if (t->miptex != tex->miptex)
			continue;
		if (t->flags != tex->flags)
			continue;

		for (j = 0; j < 8; j++)
			if (t->vecs[j / 4][j % 4] != tex->vecs[j / 4][j % 4])
				break;
		if (j != 8)
			continue;

		return i;
	}

	// allocate a new texture
	BSP_AddTexinfo (bsp, t);

	return i;
}

entity_t   *mapent;

static void
ParseEpair (void)
{
	epair_t    *e;

	e = malloc (sizeof (epair_t));
	memset (e, 0, sizeof (epair_t));
	e->next = mapent->epairs;
	mapent->epairs = e;
	e->line = map_script->line;

	//FIXME no error checking
	e->key = strdup (map_script->token->str);
	Script_GetToken (map_script, false);
	e->value = strdup (map_script->token->str);
}

typedef struct wall_s {
	vec3_t      n;
	vec3_t      xv;
	vec3_t      yv;
} wall_t;

// Quake's view of the world is +x forward, +y left, +z up
// north/south/east/west is from the original qbsp source.
// front/back/left/right is from inside the cube defined by the faces
// xv and yv are the st axes for the texture, using 0,0 as top-left with
// y (t) going down the texture.
static wall_t baseaxis[] = {
	{ .n = { 0, 0, 1}, .xv = {1, 0, 0}, .yv = {0, -1, 0} },//floor
	{ .n = { 0, 0,-1}, .xv = {1, 0, 0}, .yv = {0, -1, 0} },//ceiling
	{ .n = { 1, 0, 0}, .xv = {0, 1, 0}, .yv = {0, 0, -1} },//west wall  (back)
	{ .n = {-1, 0, 0}, .xv = {0, 1, 0}, .yv = {0, 0, -1} },//east wall  (front)
	{ .n = { 0, 1, 0}, .xv = {1, 0, 0}, .yv = {0, 0, -1} },//south wall (right)
	{ .n = { 0,-1, 0}, .xv = {1, 0, 0}, .yv = {0, 0, -1} },//north wall (left)
};

static void
TextureAxisFromPlane (const plane_t *pln, vec3_t xv, vec3_t yv)
{
	float       dot, best;
	int         bestaxis, i;

	best = 0;
	bestaxis = 0;

	for (i = 0; i < 6; i++) {
		dot = DotProduct (pln->normal, baseaxis[i].n);
		if (dot > best) {
			best = dot;
			bestaxis = i;
		}
	}

	VectorCopy (baseaxis[bestaxis].xv, xv);
	VectorCopy (baseaxis[bestaxis].yv, yv);
}

static vec3_t *
ParseVerts (int *n_verts)
{
	vec3_t     *verts;
	int         i;
	const char *token;

	token = Script_Token (map_script);
	if (token[0] != ':')
		map_error ("parsing brush");
	// It's normally ":count", but somebody might have done ": count"
	if (!token[1]) {
		Script_GetToken (map_script, false);
		token = Script_Token (map_script);
	} else {
		token++;
	}
	*n_verts = atoi (token);
	verts = malloc (sizeof (vec3_t) * *n_verts);

	for (i = 0; i < *n_verts; i++) {
		Script_GetToken (map_script, true);
		verts[i][0] = atof (map_script->token->str);
		Script_GetToken (map_script, true);
		verts[i][1] = atof (map_script->token->str);
		Script_GetToken (map_script, true);
		verts[i][2] = atof (map_script->token->str);
	}

	return verts;
}

static void
ParseBrush (void)
{
	float       rotate, scale[2];
	float       ang, sinv, cosv, ns, nt;
	int         sv, tv;
	int         i, j, n_verts = 0, hltexdef;
	mbrush_t   *b;
	mface_t    *f, *f2;
	plane_t     plane;
	texinfo_t   tx;
	vec3_t      planepts[3];
	vec3_t      t1, t2, *verts = 0;
	vec_t       vecs[2][4];

	nummapbrushes++;
	b = calloc (1, sizeof (mbrush_t));
	b->next = mapent->brushes;
	mapent->brushes = b;
	b->line = map_script->line;

	Script_GetToken (map_script, true);
	if (strcmp (map_script->token->str, "(") != 0) {
		verts = ParseVerts (&n_verts);
	} else {
		Script_UngetToken (map_script);
	}

	do {
		if (!Script_GetToken (map_script, true))
			break;
		if (!strcmp (map_script->token->str, "}"))
			break;

		int face_line = map_script->line;
		if (verts) {
			int         n_v, v;
			n_v = atoi (map_script->token->str);
			Script_GetToken (map_script, false);
			for (i = 0; i < n_v; i++) {
				Script_GetToken (map_script, false);
				v = atof (map_script->token->str);
				if (i < 3)
					VectorCopy (verts[v], planepts[i]);
			}
			Script_GetToken (map_script, false);
		} else {
			// read the three point plane definition
			for (i = 0; i < 3; i++) {
				if (i != 0)
					Script_GetToken (map_script, true);
				if (strcmp (map_script->token->str, "("))
					map_error ("parsing brush");

				for (j = 0; j < 3; j++) {
					Script_GetToken (map_script, false);
					planepts[i][j] = atof (map_script->token->str);
				}

				Script_GetToken (map_script, false);
				if (strcmp (map_script->token->str, ")"))
					map_error ("parsing brush");
			}
		}

		// convert points to a plane
		VectorSubtract (planepts[0], planepts[1], t1);
		VectorSubtract (planepts[2], planepts[1], t2);
		CrossProduct(t1, t2, plane.normal);
		_VectorNormalize (plane.normal);
		plane.dist = DotProduct(planepts[1], plane.normal);

		// read the texturedef
		memset (&tx, 0, sizeof (tx));
		Script_GetToken (map_script, false);
		tx.miptex = FindMiptex (map_script->token->str);
		Script_GetToken (map_script, false);
		if ((hltexdef = !strcmp (map_script->token->str, "["))) {
			// S vector
			Script_GetToken (map_script, false);
			vecs[0][0] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			vecs[0][1] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			vecs[0][2] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			vecs[0][3] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			if (strcmp (map_script->token->str, "]"))
				map_error ("missing ]");
			Script_GetToken (map_script, false);
			if (strcmp (map_script->token->str, "["))
				map_error ("missing [");
			// T vector
			Script_GetToken (map_script, false);
			vecs[1][0] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			vecs[1][1] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			vecs[1][2] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			vecs[1][3] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			if (strcmp (map_script->token->str, "]"))
				map_error ("missing ]");
		} else {
			vecs[0][3] = atof (map_script->token->str);
			Script_GetToken (map_script, false);
			vecs[1][3] = atof (map_script->token->str);
		}

		Script_GetToken (map_script, false);
		rotate = atof (map_script->token->str);
		Script_GetToken (map_script, false);
		scale[0] = atof (map_script->token->str);
		Script_GetToken (map_script, false);
		scale[1] = atof (map_script->token->str);

		while (Script_TokenAvailable (map_script, false)) {
			Script_GetToken (map_script, false);
			if (!strcmp (map_script->token->str, "detail"))
				b->detail = 1;
			else
				map_error ("parse error");
		}

		// convex objects can't have duplicate planes
		for (f2 = b->faces; f2; f2 = f2->next) {
			if (PlaneEqual (&f2->plane, &plane)) {
				break;
			}
		}
		if (f2) {
			map_warning ("brush with duplicate plane (of plane at %d)",
						 f2->line);
			continue;
		}

		if (DotProduct (plane.normal, plane.normal) < 0.1) {
			map_warning ("brush plane with no normal");
			continue;
		}

		if (!hltexdef) {
			// fake proper texture vectors from QuakeEd style
			TextureAxisFromPlane (&plane, vecs[0], vecs[1]);
		}

		if (!scale[0])
			scale[0] = 1;
		if (!scale[1])
			scale[1] = 1;

		// rotate axis
		if (rotate == 0) {
			sinv = 0;
			cosv = 1;
		} else if (rotate == 90) {
			sinv = 1;
			cosv = 0;
		} else if (rotate == 180) {
			sinv = 0;
			cosv = -1;
		} else if (rotate == 270) {
			sinv = -1;
			cosv = 0;
		} else {
			ang = rotate / 180 * M_PI;
			sinv = sin (ang);
			cosv = cos (ang);
		}

		if (vecs[0][0])
			sv = 0;
		else if (vecs[0][1])
			sv = 1;
		else
			sv = 2;

		if (vecs[1][0])
			tv = 0;
		else if (vecs[1][1])
			tv = 1;
		else
			tv = 2;

		for (i = 0; i < 2; i++) {
			// rotate
			ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
			nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
			vecs[i][sv] = ns;
			vecs[i][tv] = nt;
			// scale and store into texinfo
			for (j = 0; j < 3; j++)
				tx.vecs[i][j] = vecs[i][j] / scale[i];
			tx.vecs[i][3] = vecs[i][3];
		}

		f = calloc (1, sizeof (mface_t));
		f->next = b->faces;
		b->faces = f;
		f->plane = plane;
		// unique the texinfo
		f->texinfo = FindTexinfo (&tx);
		f->line = face_line;
		nummapbrushfaces++;
	} while (1);
	if (verts)
		free (verts);
}

static bool
ParseEntity (void)
{
	if (!Script_GetToken (map_script, true))
		return false;

	if (strcmp (map_script->token->str, "{"))
		map_error ("ParseEntity: { not found");

	if (num_entities == max_entities) {
		max_entities += ENTITIES_CHUNK;
		entities = realloc (entities, max_entities * sizeof (entity_t));
	}

	mapent = &entities[num_entities];
	num_entities++;
	memset (mapent, 0, sizeof (entity_t));
	mapent->line = map_script->line;

	do {
		if (!Script_GetToken (map_script, true))
			map_error ("ParseEntity: EOF without closing brace");
		if (!strcmp (map_script->token->str, "}"))
			break;
		if (!strcmp (map_script->token->str, "{"))
			ParseBrush ();
		else
			ParseEpair ();
	} while (1);

	if (!strcmp ("am_detail", ValueForKey (mapent, "classname"))) {
		mbrush_t   *b, *lb;
		// set detail flag
		for (lb = b = mapent->brushes; b; lb = b, b = b->next) {
			b->detail = 1;
			numdetailbrushes++;
		}
		// add to worldspawn
		lb->next = entities->brushes;
		entities->brushes = mapent->brushes;
		num_entities--;
		memset (mapent, 0, sizeof (entity_t));
		return true;
	} else {
		mbrush_t   *b;
		for (b = mapent->brushes; b; b = b->next)
			if (b->detail)
				numdetailbrushes++;
	}

	GetVectorForKey (mapent, "origin", mapent->origin);
	return true;
}

void
LoadMapFile (const char *filename)
{
	char       *buf;
	QFile      *file;
	int         bytes;

	file = Qopen (filename, "rt");
	if (!file)
		Sys_Error ("couldn't open %s. %s", filename, strerror(errno));
	bytes = Qfilesize (file);
	buf = malloc (bytes + 1);
	bytes = Qread (file, buf, bytes);
	buf[bytes] = 0;
	Qclose (file);

	map_script = Script_New ();
	// qbsp treats {foo as one token, not two. The original intent, but the
	// quake community takes advantage of it for transparent textures
	map_script->single = "";
	Script_Start (map_script, filename, buf);

	num_entities = 0;

	if (Script_GetToken (map_script, 1)) {
		if (strequal (map_script->token->str, "QuakeForge-map")) {
			if (!Script_TokenAvailable (map_script, 1))
				map_error ("Unexpected EOF reading %s\n", filename);
		} else {
			Script_UngetToken (map_script);
			while (ParseEntity ())
				;
		}
	}

	Script_Delete (map_script);
	map_script = 0;
	free (buf);

	qprintf ("--- LoadMapFile ---\n");
	qprintf ("%s\n", filename);
	qprintf ("%5i faces\n", nummapbrushfaces);
	qprintf ("%5i brushes (%i detail)\n", nummapbrushes, numdetailbrushes);
	qprintf ("%5i entities\n", num_entities);
	qprintf ("%5i textures\n", nummiptexnames);
	qprintf ("%5zd texinfo\n", bsp->numtexinfo);
}

void
PrintEntity (const entity_t *ent)
{
	const epair_t *ep;

	printf ("%20s : %d\n", "map source line", ent->line);
	for (ep = ent->epairs; ep; ep = ep->next)
		printf ("%20s : %s\n", ep->key, ep->value);
}


const char *
ValueForKey (const entity_t *ent, const char *key)
{
	const epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp (ep->key, key))
			return ep->value;
	return "";
}

void
SetKeyValue (entity_t *ent, const char *key, const char *value)
{
	epair_t    *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp (ep->key, key)) {
			free (ep->value);
			ep->value = strdup (value);
			return;
		}
	ep = malloc (sizeof (*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = strdup (key);
	ep->value = strdup (value);
}

void
GetVectorForKey (const entity_t *ent, const char *key, vec3_t vec)
{
	const char *k;
	double      v1, v2, v3;

	k = ValueForKey (ent, key);
	v1 = v2 = v3 = 0;
	// scanf into doubles, then assign, so it is vec_t size independent
	sscanf (k, "%lf %lf %lf", &v1, &v2, &v3);
	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;
}

void
WriteEntitiesToString (void)
{
	dstring_t  *buf;
	const epair_t *ep;
	int         i;

	buf = dstring_newstr ();

	for (i = 0; i < num_entities; i++) {
		ep = entities[i].epairs;
		if (!ep)
			continue;					// ent got removed

		dstring_appendstr (buf, "{\n");

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			dstring_appendstr (buf, va (0, "\"%s\" \"%s\"\n",
										ep->key, ep->value));
		}
		dstring_appendstr (buf, "}\n");
	}
	BSP_AddEntities (bsp, buf->str, buf->size);
}

//@}
