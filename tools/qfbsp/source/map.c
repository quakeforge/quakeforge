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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

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
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "bsp5.h"

int         nummapbrushfaces;
int         nummapbrushes;
mbrush_t    mapbrushes[MAX_MAP_BRUSHES];

int         num_entities;
entity_t    entities[MAX_MAP_ENTITIES];

int         nummiptex;
char        miptex[MAX_MAP_TEXINFO][16];

int         numdetailbrushes;

#define	MAXTOKEN	128

char        token[MAXTOKEN];
qboolean    unget;
char       *script_p;
const char *script_file;
int         script_line;

static void __attribute__ ((format (printf, 1, 2), noreturn))
map_error (const char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	fprintf (stderr, "%s:%d: ", script_file, script_line);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
	exit (1);
}

int
FindMiptex (const char *name)
{
	int         i;

	if (strcmp (name, "hint") == 0)
		return TEX_HINT;
	if (strcmp (name, "skip") == 0)
		return TEX_SKIP;
	for (i = 0; i < nummiptex; i++) {
		if (!strcmp (name, miptex[i]))
			return i;
	}
	if (nummiptex == MAX_MAP_TEXINFO)
		map_error ("nummiptex == MAX_MAP_TEXINFO");
	strcpy (miptex[i], name);
	nummiptex++;
	return i;
}

/*
	FindTexinfo

	Returns a global texinfo number
*/
static int
FindTexinfo (texinfo_t *t)
{
	int         i, j;
	texinfo_t  *tex;

	if (t->miptex < 0)
		return t->miptex;		// it's HINT or SKIP

	// set the special flag
	if (miptex[t->miptex][0] == '*'
		|| !strncasecmp (miptex[t->miptex], "sky", 3))
		t->flags |= TEX_SPECIAL;

	tex = bsp->texinfo;
	for (i = 0; i < bsp->numtexinfo; i++, tex++) {
		if (t->miptex != tex->miptex)
			continue;
		if (t->flags != tex->flags)
			continue;

		for (j = 0; j < 8; j++)
			if (t->vecs[0][j] != tex->vecs[0][j])
				break;
		if (j != 8)
			continue;

		return i;
	}

	// allocate a new texture
	BSP_AddTexinfo (bsp, t);

	return i;
}

static void
StartTokenParsing (char *data)
{
	script_line = 1;
	script_p = data;
	unget = false;
}

static qboolean
TokenAvailable (qboolean crossline)
{
	if (unget)
		return true;
  skipspace:
	while (isspace ((unsigned char) *script_p)) {
		if (*script_p == '\n') {
			if (!crossline)
				return false;
			script_line++;
		}
		script_p++;
	}
	if (!*script_p)
		return false;
	if (*script_p == 26 || *script_p == 4) {		// end of file characters
		script_p++;
		goto skipspace;
	}

	if (script_p[0] == '/' && script_p[1] == '/') {		// comment field
		while (*script_p && *script_p != '\n')
			script_p++;
		if (!*script_p)
			return false;
		if (!crossline)
			return false;
		goto skipspace;
	}
	return true;
}

static qboolean
GetToken (qboolean crossline)
{
	char       *token_p;

	if (unget) {						// is a token allready waiting?
		unget = false;
		return true;
	}

	if (!TokenAvailable (crossline)) {
		if (!crossline)
			map_error ("line is incomplete");
		return false;
	}

	// copy token
	token_p = token;

	if (*script_p == '"') {
		script_p++;
		while (*script_p != '"') {
			if (!*script_p)
				map_error ("EOF inside quoted token");
			if (*script_p == '\n')
				script_line++;
			*token_p++ = *script_p++;
			if (token_p > &token[MAXTOKEN - 1])
				map_error ("token too large");
		}
		script_p++;
	} else
		while (*script_p && !isspace ((unsigned char) *script_p)) {
			*token_p++ = *script_p++;
			if (token_p > &token[MAXTOKEN - 1])
				map_error ("token too large");
		}

	*token_p = 0;

	return true;
}

static void
UngetToken (void)
{
	unget = true;
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

	if (strlen (token) >= MAX_KEY - 1)
		map_error ("ParseEpar: token too long");
	e->key = strdup (token);
	GetToken (false);
	if (strlen (token) >= MAX_VALUE - 1)
		map_error ("ParseEpar: token too long");
	e->value = strdup (token);
}

vec3_t      baseaxis[18] = {
	{0, 0, 1}, {1, 0, 0}, {0, -1, 0},	// floor
	{0, 0, -1}, {1, 0, 0}, {0, -1, 0},	// ceiling
	{1, 0, 0}, {0, 1, 0}, {0, 0, -1},	// west wall
	{-1, 0, 0}, {0, 1, 0}, {0, 0, -1},	// east wall
	{0, 1, 0}, {1, 0, 0}, {0, 0, -1},	// south wall
	{0, -1, 0}, {1, 0, 0}, {0, 0, -1}	// north wall
};

static void
TextureAxisFromPlane (plane_t *pln, vec3_t xv, vec3_t yv)
{
	float       dot, best;
	int         bestaxis, i;

	best = 0;
	bestaxis = 0;

	for (i = 0; i < 6; i++) {
		dot = DotProduct (pln->normal, baseaxis[i * 3]);
		if (dot > best) {
			best = dot;
			bestaxis = i;
		}
	}

	VectorCopy (baseaxis[bestaxis * 3 + 1], xv);
	VectorCopy (baseaxis[bestaxis * 3 + 2], yv);
}

static vec3_t *
ParseVerts (int *n_verts)
{
	vec3_t     *verts;
	int         i;

	if (token[0] != ':')
		map_error ("parsing brush");
	*n_verts = atoi (token + 1);
	verts = malloc (sizeof (vec3_t) * *n_verts);

	for (i = 0; i < *n_verts; i++) {
		GetToken (true);
		verts[i][0] = atof (token);
		GetToken (true);
		verts[i][1] = atof (token);
		GetToken (true);
		verts[i][2] = atof (token);
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
	vec_t       d, vecs[2][4];

	b = &mapbrushes[nummapbrushes];
	nummapbrushes++;
	b->next = mapent->brushes;
	mapent->brushes = b;

	GetToken (true);
	if (strcmp (token, "(") != 0) {
		verts = ParseVerts (&n_verts);
	} else {
		UngetToken ();
	}

	do {
		if (!GetToken (true))
			break;
		if (!strcmp (token, "}"))
			break;

		if (verts) {
			int         n_v, v;
			n_v = atoi (token);
			GetToken (false);
			for (i = 0; i < n_v; i++) {
				GetToken (false);
				v = atof (token);
				if (i < 3)
					VectorCopy (verts[v], planepts[i]);
			}
			GetToken (false);
		} else {
			// read the three point plane definition
			for (i = 0; i < 3; i++) {
				if (i != 0)
					GetToken (true);
				if (strcmp (token, "("))
					map_error ("parsing brush");

				for (j = 0; j < 3; j++) {
					GetToken (false);
					planepts[i][j] = atof (token);
				}

				GetToken (false);
				if (strcmp (token, ")"))
					map_error ("parsing brush");
			}
		}

		// convert points to a plane
		VectorSubtract (planepts[0], planepts[1], t1);
		VectorSubtract (planepts[2], planepts[1], t2);
		CrossProduct(t1, t2, plane.normal);
		VectorNormalize (plane.normal);
		plane.dist = DotProduct(planepts[1], plane.normal);

		// read the texturedef
		memset (&tx, 0, sizeof (tx));
		GetToken (false);
		tx.miptex = FindMiptex (token);
		GetToken (false);
		if ((hltexdef = !strcmp (token, "["))) {
			// S vector
			GetToken (false);
			vecs[0][0] = atof (token);
			GetToken (false);
			vecs[0][1] = atof (token);
			GetToken (false);
			vecs[0][2] = atof (token);
			GetToken (false);
			vecs[0][3] = atof (token);
			GetToken (false);
			if (strcmp (token, "]"))
				map_error ("missing ]");
			GetToken (false);
			if (strcmp (token, "["))
				map_error ("missing [");
			// T vector
			GetToken (false);
			vecs[1][0] = atof (token);
			GetToken (false);
			vecs[1][1] = atof (token);
			GetToken (false);
			vecs[1][2] = atof (token);
			GetToken (false);
			vecs[1][3] = atof (token);
			GetToken (false);
			if (strcmp (token, "]"))
				map_error ("missing ]");
		} else {
			vecs[0][3] = atof (token);
			GetToken (false);
			vecs[1][3] = atof (token);
		}

		GetToken (false);
		rotate = atof (token);
		GetToken (false);
		scale[0] = atof (token);
		GetToken (false);
		scale[1] = atof (token);

		while (TokenAvailable (false)) {
			GetToken (false);
			if (!strcmp (token, "detail"))
				b->detail = 1;
			else
				map_error ("parse error");
		}

		// if the three points are all on a previous plane, it is a duplicate
		// plane
		for (f2 = b->faces; f2; f2 = f2->next) {
			for (i = 0; i < 3; i++) {
				d = DotProduct (planepts[i], f2->plane.normal)
					- f2->plane.dist;
				if (d < -ON_EPSILON || d > ON_EPSILON)
					break;
			}
			if (i == 3)
				break;
		}
		if (f2) {
			printf ("WARNING: brush with duplicate plane\n");
			continue;
		}

		if (DotProduct (plane.normal, plane.normal) < 0.1) {
			printf ("WARNING: brush plane with no normal on line %d\n",
					script_line);
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
			// cale and store into texinfo
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
		nummapbrushfaces++;
	} while (1);
	if (verts)
		free (verts);
}

static qboolean
ParseEntity (void)
{
	if (!GetToken (true))
		return false;

	if (strcmp (token, "{"))
		map_error ("ParseEntity: { not found");

	if (num_entities == MAX_MAP_ENTITIES)
		map_error ("num_entities == MAX_MAP_ENTITIES");

	mapent = &entities[num_entities];
	num_entities++;

	do {
		if (!GetToken (true))
			map_error ("ParseEntity: EOF without closing brace");
		if (!strcmp (token, "}"))
			break;
		if (!strcmp (token, "{"))
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

	script_file = filename;

	StartTokenParsing (buf);

	num_entities = 0;

	while (ParseEntity ()) {
	}

	free (buf);

	qprintf ("--- LoadMapFile ---\n");
	qprintf ("%s\n", filename);
	qprintf ("%5i faces\n", nummapbrushfaces);
	qprintf ("%5i brushes (%i detail)\n", nummapbrushes, numdetailbrushes);
	qprintf ("%5i entities\n", num_entities);
	qprintf ("%5i textures\n", nummiptex);
	qprintf ("%5i texinfo\n", bsp->numtexinfo);
}

void
PrintEntity (entity_t *ent)
{
	epair_t    *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		printf ("%20s : %s\n", ep->key, ep->value);
}


const char *
ValueForKey (entity_t *ent, const char *key)
{
	epair_t    *ep;

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

float
FloatForKey (entity_t *ent, const char *key)
{
	const char *k;

	k = ValueForKey (ent, key);
	return atof (k);
}

void
GetVectorForKey (entity_t *ent, const char *key, vec3_t vec)
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
	epair_t    *ep;
	int         i;

	buf = dstring_newstr ();

	for (i = 0; i < num_entities; i++) {
		ep = entities[i].epairs;
		if (!ep)
			continue;					// ent got removed

		dstring_appendstr (buf, "{\n");

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			dstring_appendstr (buf, va ("\"%s\" \"%s\"\n",
										ep->key, ep->value));
		}
		dstring_appendstr (buf, "}\n");
	}
	BSP_AddEntities (bsp, buf->str, buf->size);
}
