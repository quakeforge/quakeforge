#include "QF/script.h"
#include "QF/sys.h"

#include "SetBrush.h"
#include "Entity.h"
#include "EntityClass.h"
#include "Map.h"
#include "Preferences.h"
#include "XYView.h"
#include "ZView.h"
#include "CameraView.h"
#include "Clipper.h"
#include "QuakeEd.h"

@implementation SetBrush
/*
==================
textureAxisFromPlane
==================
*/
#if 1
vec3_t baseaxis[18] = {
	{ 0,  0,  1}, {1, 0, 0}, {0, -1,  0}, // floor
	{ 0,  0, -1}, {1, 0, 0}, {0, -1,  0}, // ceiling
	{ 1,  0,  0}, {0, 1, 0}, {0,  0, -1}, // west wall
	{-1,  0,  0}, {0, 1, 0}, {0,  0, -1}, // east wall
	{ 0,  1,  0}, {1, 0, 0}, {0,  0, -1}, // south wall
	{ 0, -1,  0}, {1, 0, 0}, {0,  0, -1} // north wall
};
#else
vec3_t baseaxis[18] = {
	{ 0,  0,  1}, { 1,  0, 0}, {0, -1,  0}, // floor
	{ 0,  0, -1}, { 1,  0, 0}, {0,  1,  0}, // ceiling
	{ 1,  0,  0}, { 0,  1, 0}, {0,  0, -1}, // west wall
	{-1,  0,  0}, { 0, -1, 0}, {0,  0, -1}, // east wall
	{ 0,  1,  0}, {-1,  0, 0}, {0,  0, -1}, // south wall
	{ 0, -1,  0}, { 1,  0, 0}, {0,  0, -1} // north wall
};
#endif

float
TextureAxisFromPlane (plane_t *pln, float *xv, float *yv)
{
	int     bestaxis;
	float   dot, best;
	int     i;

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

	return lightaxis[bestaxis >> 1];
}

#define BOGUS_RANGE 18000

/*
=================
CheckFace

Note: this will not catch 0 area polygons
=================
*/
void
CheckFace (face_t * f)
{
	int         i, j;
	float       *p1, *p2;
	float       d, edgedist;
	vec3_t      dir, edgenormal;
	winding_t   *w;

	w = f->w;
	if (!w)
		Sys_Error ("CheckFace: no winding");

	if (w->numpoints < 3)
		Sys_Error ("CheckFace: %i points", w->numpoints);

	for (i = 0; i < w->numpoints; i++) {
		p1 = w->points[i];

		for (j = 0; j < 3; j++) {
			if (p1[j] > BOGUS_RANGE || p1[j] < -BOGUS_RANGE)
				Sys_Error ("CheckFace: BUGUS_RANGE: %f", p1[j]);
		}

		j = i + 1 == w->numpoints ? 0 : i + 1;

		// check the point is on the face plane
		d = DotProduct (p1, f->plane.normal) - f->plane.dist;
		if (d < -ON_EPSILON || d > ON_EPSILON)
			Sys_Error ("CheckFace: point off plane");

		// check the edge isn't degenerate
		p2 = w->points[j];
		VectorSubtract (p2, p1, dir);

		if (VectorLength (dir) < ON_EPSILON)
			Sys_Error ("CheckFace: degenerate edge");

		CrossProduct (f->plane.normal, dir, edgenormal);
		VectorNormalize (edgenormal);
		edgedist = DotProduct (p1, edgenormal);
		edgedist += ON_EPSILON;

		// all other points must be on front side
		for (j = 0; j < w->numpoints; j++) {
			if (j == i)
				continue;
			d = DotProduct (w->points[j], edgenormal);
			if (d > edgedist)
				Sys_Error ("CheckFace: non-convex");
		}
	}
}

/*
=============================================================================

            TURN PLANES INTO GROUPS OF FACES

=============================================================================
*/

/*
==================
NewWinding
==================
*/
winding_t *
NewWinding (int points)
{
	winding_t   *w;
	size_t      size;

	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error ("NewWinding: %i points", points);

	size = (size_t) ((winding_t *) 0)->points[points];
	w = malloc (size);
	memset (w, 0, size);

	return w;
}

/*
==================
CopyWinding
==================
*/
winding_t *
CopyWinding (winding_t * w)
{
	size_t      size;
	winding_t   *c;

	size = (size_t) ((winding_t *) 0)->points[w->numpoints];
	c = malloc (size);
	memcpy (c, w, size);
	return c;
}

/*
==================
ClipWinding

Clips the winding to the plane, returning the new winding on the positive side
Frees the input winding.
==================
*/
winding_t *
ClipWinding (winding_t * in, plane_t *split)
{
	float       dists[MAX_POINTS_ON_WINDING];
	int         sides[MAX_POINTS_ON_WINDING];
	int         counts[3];
	float       dot;
	int         i, j;
	float       *p1, *p2, *mid;
	winding_t   *neww;
	int         maxpts;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < in->numpoints; i++) {
		dot = DotProduct (in->points[i], split->normal);
		dot -= split->dist;
		dists[i] = dot;
		if (dot > ON_EPSILON)
			sides[i] = SIDE_FRONT;
		else if (dot < -ON_EPSILON)
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0] && !counts[1])
		return in;
	if (!counts[0]) {
		free (in);
		return NULL;
	}
	if (!counts[1])
		return in;

	// can't use counts[0]+2 because of fp grouping errors
	maxpts = in->numpoints + 4;
	neww = NewWinding (maxpts);

	for (i = 0; i < in->numpoints; i++) {
		p1 = in->points[i];

		mid = neww->points[neww->numpoints];

		if (sides[i] == SIDE_FRONT || sides[i] == SIDE_ON) {
			VectorCopy (p1, mid);
			mid[3] = p1[3];
			mid[4] = p1[4];
			neww->numpoints++;
			if (sides[i] == SIDE_ON)
				continue;
			mid = neww->points[neww->numpoints];
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		// generate a split point
		if (i == in->numpoints - 1)
			p2 = in->points[0];
		else
			p2 = p1 + 5;

		neww->numpoints++;

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) {   // avoid round off error when possible
			if (split->normal[j] == 1)
				mid[j] = split->dist;
			else if (split->normal[j] == -1)
				mid[j] = -split->dist;
			mid[j] = p1[j] + dot * (p2[j] - p1[j]);
		}
		mid[3] = p1[3] + dot * (p2[3] - p1[3]);
		mid[4] = p1[4] + dot * (p2[4] - p1[4]);
	}

	if (neww->numpoints > maxpts)
		Sys_Error ("ClipWinding: points exceeded estimate");

	// free the original winding
	free (in);

	return neww;
}

/*
=================
BasePolyForPlane

There has GOT to be a better way of doing this...
=================
*/
winding_t *
BasePolyForPlane (face_t * f)
{
	int             i, x;
	float           max, v;
	vec3_t          org, vright, vup;
	vec3_t          xaxis, yaxis;
	winding_t       *w;
	texturedef_t    *td;
	plane_t         *p;
	float           ang, sinv, cosv;
	float           s, t, ns, nt;

	p = &f->plane;

	// find the major axis
	max = -BOGUS_RANGE;
	x = -1;
	for (i = 0; i < 3; i++) {
		v = fabs (p->normal[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}

	if (x == -1)
		Sys_Error ("BasePolyForPlane: no axis found");

	VectorCopy (vec3_origin, vup);
	switch (x) {
		case 0:
		case 1:
			vup[2] = 1;
			break;

		case 2:
			vup[0] = 1;
			break;
	}

	v = DotProduct (vup, p->normal);
	VectorMultAdd (vup, -v, p->normal, vup);
	VectorNormalize (vup);

	VectorScale (p->normal, p->dist, org);

	CrossProduct (vup, p->normal, vright);

	VectorScale (vup, 8192, vup);
	VectorScale (vright, 8192, vright);

	// project a really big axis-aligned box onto the plane
	w = NewWinding (4);
	w->numpoints = 4;

	VectorSubtract (org, vright, w->points[0]);
	VectorAdd (w->points[0], vup, w->points[0]);

	VectorAdd (org, vright, w->points[1]);
	VectorAdd (w->points[1], vup, w->points[1]);

	VectorAdd (org, vright, w->points[2]);
	VectorSubtract (w->points[2], vup, w->points[2]);

	VectorSubtract (org, vright, w->points[3]);
	VectorSubtract (w->points[3], vup, w->points[3]);

	// set texture values
	f->light = TextureAxisFromPlane (&f->plane, xaxis, yaxis);
	td = &f->texture;

	// rotate axis
	ang = td->rotate / 180 * M_PI;
	sinv = sin (ang);
	cosv = cos (ang);

	if (!(td->scale[0]))
		td->scale[0] = 1;

	if (!(td->scale[1]))
		td->scale[1] = 1;

	for (i = 0; i < 4; i++) {
		s = DotProduct (w->points[i], xaxis);
		t = DotProduct (w->points[i], yaxis);

		ns = cosv * s - sinv * t;
		nt = sinv * s + cosv * t;

		w->points[i][3] = ns / td->scale[0] + td->shift[0];
		w->points[i][4] = nt / td->scale[1] + td->shift[1];
	}

	return w;
}

/*
===========
calcWindings

recalc the faces and mins / maxs from the planes
If a face has a NULL winding, it is an overconstraining plane and
can be removed.
===========
*/
- (id) calcWindings
{
	int         i, j, k;
	float       v;
	face_t      *f;
	winding_t   *w;
	plane_t     plane;
	vec3_t      t1, t2, t3;
	BOOL        useplane[MAX_FACES];

	bmins[0] = bmins[1] = bmins[2] = 99999;
	bmaxs[0] = bmaxs[1] = bmaxs[2] = -99999;
	invalid = NO;

	[self freeWindings];

	for (i = 0; i < MAX_FACES; i++) {
		f = &faces[i];

		// calc a plane from the points
		for (j = 0; j < 3; j++) {
			t1[j] = f->planepts[0][j] - f->planepts[1][j];
			t2[j] = f->planepts[2][j] - f->planepts[1][j];
			t3[j] = f->planepts[1][j];
		}

		CrossProduct (t1, t2, f->plane.normal);
		if (VectorCompare (f->plane.normal, vec3_origin)) {
			useplane[i] = NO;
			break;
		}
		VectorNormalize (f->plane.normal);
		f->plane.dist = DotProduct (t3, f->plane.normal);

		// if the plane duplicates another plane, ignore it
		// (assume it is a brush being edited that will be fixed)
		useplane[i] = YES;
		for (j = 0; j < i; j++) {
			if (f->plane.normal[0] == faces[j].plane.normal[0]
			    && f->plane.normal[1] == faces[j].plane.normal[1]
			    && f->plane.normal[2] == faces[j].plane.normal[2]
			    && f->plane.dist == faces[j].plane.dist) {
				useplane[i] = NO;
				break;
			}
		}
	}

	for (i = 0; i < numfaces; i++) {
		if (!useplane[i])           // duplicate plane
			continue;

		f = &faces[i];

		w = BasePolyForPlane (f);
		for (j = 0; j < numfaces && w; j++) {
			if (j == i)
				continue;

			// flip the plane, because we want to keep the back side
			VectorSubtract (vec3_origin, faces[j].plane.normal, plane.normal);
			plane.dist = -faces[j].plane.dist;

			w = ClipWinding (w, &plane);
		}
		f->w = w;
		if (w) {
			CheckFace (f);
			for (j = 0; j < w->numpoints; j++) {
				for (k = 0; k < 3; k++) {
					v = w->points[j][k];
					if (fabs (v - rint (v)) < FP_EPSILON)
						v = w->points[j][k] = rint (v);
					if (v < bmins[k])
						bmins[k] = v;
					if (v > bmaxs[k])
						bmaxs[k] = v;
				}
			}
		}
	}

	if (bmins[0] == 99999) {
		invalid = YES;
		VectorCopy (vec3_origin, bmins);
		VectorCopy (vec3_origin, bmaxs);
		return nil;
	}

	return self;
}

// ============================================================================

/*
===========
initOwner:::
===========
*/
- (SetBrush *) initOwner: own
   mins: (float *)mins
   maxs: (float *)maxs
   texture: (texturedef_t *)tex
{
	[super init];

	parent = own;

	[self setTexturedef: tex];
	[self setMins: mins maxs: maxs];
	return self;
}

- (id) setMins: (float *)
   mins maxs: (float *)maxs
{
	int     i, j;
	vec3_t  pts[4][2];

	for (i = 0; i < 3; i++) {
		if (maxs[i] - mins[i] <= 0) {
			VectorCopy (mins, bmins);
			VectorCopy (maxs, bmaxs);
			invalid = YES;
			numfaces = 0;
			return self;
		}
	}

	pts[0][0][0] = mins[0];
	pts[0][0][1] = mins[1];

	pts[1][0][0] = mins[0];
	pts[1][0][1] = maxs[1];

	pts[2][0][0] = maxs[0];
	pts[2][0][1] = maxs[1];

	pts[3][0][0] = maxs[0];
	pts[3][0][1] = mins[1];

	for (i = 0; i < 4; i++) {
		pts[i][0][2] = mins[2];
		pts[i][1][0] = pts[i][0][0];
		pts[i][1][1] = pts[i][0][1];
		pts[i][1][2] = maxs[2];
	}

	numfaces = 6;
	for (i = 0; i < 4; i++) {
		j = (i + 1) % 4;
		faces[i].planepts[0][0] = pts[j][1][0];
		faces[i].planepts[0][1] = pts[j][1][1];
		faces[i].planepts[0][2] = pts[j][1][2];

		faces[i].planepts[1][0] = pts[i][1][0];
		faces[i].planepts[1][1] = pts[i][1][1];
		faces[i].planepts[1][2] = pts[i][1][2];

		faces[i].planepts[2][0] = pts[i][0][0];
		faces[i].planepts[2][1] = pts[i][0][1];
		faces[i].planepts[2][2] = pts[i][0][2];
	}

	faces[4].planepts[0][0] = pts[0][1][0];
	faces[4].planepts[0][1] = pts[0][1][1];
	faces[4].planepts[0][2] = pts[0][1][2];

	faces[4].planepts[1][0] = pts[1][1][0];
	faces[4].planepts[1][1] = pts[1][1][1];
	faces[4].planepts[1][2] = pts[1][1][2];

	faces[4].planepts[2][0] = pts[2][1][0];
	faces[4].planepts[2][1] = pts[2][1][1];
	faces[4].planepts[2][2] = pts[2][1][2];

	faces[5].planepts[0][0] = pts[2][0][0];
	faces[5].planepts[0][1] = pts[2][0][1];
	faces[5].planepts[0][2] = pts[2][0][2];

	faces[5].planepts[1][0] = pts[1][0][0];
	faces[5].planepts[1][1] = pts[1][0][1];
	faces[5].planepts[1][2] = pts[1][0][2];

	faces[5].planepts[2][0] = pts[0][0][0];
	faces[5].planepts[2][1] = pts[0][0][1];
	faces[5].planepts[2][2] = pts[0][0][2];

	[self calcWindings];
	return self;
}

- (id) parent
{
	return parent;
}

- (id) setParent: (id)p
{
	parent = p;
	return self;
}

- (id) setEntityColor: (vec3_t)color
{
	VectorCopy (color, entitycolor);
	return self;
}

- (void) freeWindings
{
	int  i;

	for (i = 0; i < MAX_FACES; i++) {
		if (faces[i].w) {
			free (faces[i].w);
			faces[i].w = NULL;
		}
	}
}

- (id) copyWithZone: (NSZone *) zone
{
	SetBrush    *new;

	[self freeWindings];

	new = [[SetBrush allocWithZone: zone] init];
	new->regioned = regioned;
	new->selected = selected;
	new->invalid = invalid;
	new->parent = parent;
	VectorCopy (bmins, new->bmins);
	VectorCopy (bmaxs, new->bmaxs);
	VectorCopy (entitycolor, new->entitycolor);
	new->numfaces = numfaces;
	memcpy (new->faces, faces, sizeof (new->faces));

	[self calcWindings];
	[new calcWindings];
	return new;
}

- (void) dealloc
{
	[self freeWindings];
	return [super dealloc];
}

/*
===========
initOwner: fromTokens
===========
*/
int  numsb;

static vec3_t *
ParseVerts (script_t *script, int *n_verts)
{
	vec3_t     *verts;
	int         i;

	if (strcmp (Script_Token (script), ":"))
		Sys_Error ("parsing map file");
	Script_GetToken (script, false);
	*n_verts = atoi (Script_Token (script));
	verts = malloc (sizeof (vec3_t) * *n_verts);

	for (i = 0; i < *n_verts; i++) {
		Script_GetToken (script, true);
		verts[i][0] = atof (Script_Token (script));
		Script_GetToken (script, true);
		verts[i][1] = atof (Script_Token (script));
		Script_GetToken (script, true);
		verts[i][2] = atof (Script_Token (script));
	}
	return verts;
}

- (id) initFromScript: (script_t *)script owner: own
{
	face_t  *f;
	vec3_t  *verts = 0;
	int     n_verts = 0;
	int     i, j;

	[self init];

	parent = own;

	f = faces;
	numfaces = 0;

	if (!Script_GetToken (script, true))
		return self;

	if (strcmp (Script_Token (script), "(")) {
		verts = ParseVerts (script, &n_verts);
	} else {
		Script_UngetToken (script);
	}

	do {
		if (!Script_GetToken (script, true))
			break;
		if (!strcmp (Script_Token (script), "}"))
			break;

		if (verts) {
			int     n_v, v;
			n_v = atoi (Script_Token (script));
			Script_GetToken (script, false);
			for (i = 0; i < n_v; i++) {
				Script_GetToken (script, false);
				v = atoi (Script_Token (script));
				if (i < 3)
					VectorCopy (verts[v], f->planepts[i]);
			}
			printf ("\n");
			Script_GetToken (script, false);
		} else {
			for (i = 0; i < 3; i++) {
				if (i != 0)
					Script_GetToken (script, true);
				if (strcmp (Script_Token (script), "("))
					Sys_Error ("parsing map file");

				for (j = 0; j < 3; j++) {
					Script_GetToken (script, false);
					f->planepts[i][j] = atoi (Script_Token (script));
				}
				Script_GetToken (script, false);

				if (strcmp (Script_Token (script), ")"))
					Sys_Error ("parsing map file");
			}
		}

		Script_GetToken (script, false);
		strcpy (f->texture.texture, Script_Token (script));
		Script_GetToken (script, false);
		f->texture.shift[0] = atof (Script_Token (script));
		Script_GetToken (script, false);
		f->texture.shift[1] = atof (Script_Token (script));
		Script_GetToken (script, false);
		f->texture.rotate = atof (Script_Token (script));
		Script_GetToken (script, false);
		f->texture.scale[0] = atof (Script_Token (script));
		Script_GetToken (script, false);
		f->texture.scale[1] = atof (Script_Token (script));

		while (Script_TokenAvailable (script, false)) {
			Script_GetToken (script, false);
			if (!strcmp (Script_Token (script), "detail"))
				; // XXX implement
			else
				Sys_Error ("parsing map file");
		}

#if 0
		flags = atoi (Script_Token (script));

		flags &= 7;

		f->texture.rotate = 0;
		f->texture.scale[0] = 1;
		f->texture.scale[1] = 1;

# define TEX_FLIPAXIS   1
# define TEX_FLIPS      2
# define TEX_FLIPT      4

		if (flags & TEX_FLIPAXIS) {
			f->texture.rotate = 90;
			if (!(flags & TEX_FLIPT))
				f->texture.scale[0] = -1;
			if (flags & TEX_FLIPS)
				f->texture.scale[1] = -1;
		} else {
			if (flags & TEX_FLIPS)
				f->texture.scale[0] = -1;
			if (flags & TEX_FLIPT)
				f->texture.scale[1] = -1;
		}
#endif /* if 0 */
		f++;
		numfaces++;
	} while (1);

	numsb++;

	[self calcWindings];

	return self;
}

- (void) writeToFile: (QFile *)file
   region: (BOOL)reg
{
	int  i, j;
	face_t          *fa;
	texturedef_t    *td;

	if (reg && regioned)
		return;
	Qprintf (file, "{\n");
	for (i = 0; i < numfaces; i++) {
		fa = &faces[i];
		for (j = 0; j < 3; j++) {
			Qprintf (file, "( %d %d %d ) ", (int) fa->planepts[j][0],
			         (int) fa->planepts[j][1], (int) fa->planepts[j][2]);
		}
		td = &fa->texture;
		Qprintf (file, "%s %d %d %d %f %f\n", td->texture, (int) td->shift[0],
		         (int) td->shift[1], (int) td->rotate, td->scale[0],
		         td->scale[1]);
	}
	Qprintf (file, "}\n");

	return;
}

/*
==============================================================================

INTERACTION

==============================================================================
*/

- (void) getMins: (vec3_t)mins
   maxs: (vec3_t)maxs
{
	VectorCopy (bmins, mins);
	VectorCopy (bmaxs, maxs);
}

- (BOOL) selected
{
	return selected;
}

- (void) setSelected: (BOOL)s
{
	selected = s;
}

- (BOOL) regioned
{
	return regioned;
}

- (void) setRegioned: (BOOL)s
{
	regioned = s;
}

/*
===========
setTexturedef
===========
*/
- (void) setTexturedef: (texturedef_t *)tex
{
	int  i;

	for (i = 0; i < MAX_FACES; i++) {
		faces[i].texture = *tex;
		faces[i].qtexture = NULL;   // recache next render
	}

	[self calcWindings];            // in case texture coords changed
}

- (void) setTexturedef: (texturedef_t *)tex
   forFace: (int)f
{
	if (f > numfaces)
		Sys_Error ("setTexturedef:forFace: bad face number %i", f);
	faces[f].texture = *tex;
	faces[f].qtexture = NULL;       // recache next render

	[self calcWindings];            // in case texture coords changed
}

/*
===========
texturedef
===========
*/
- (texturedef_t *) texturedef
{
	return &faces[0].texture;
}

- (texturedef_t *) texturedefForFace: (int)f
{
	return &faces[f].texture;
}

/*
===========
removeIfInvalid

So created veneers don't stay around
===========
*/
- (id) removeIfInvalid
{
	int  i, j;

	for (i = 0; i < numfaces; i++) {
		if (faces[i].w)
			continue;
		for (j = i + 1; j < numfaces; j++)
			faces[j - 1] = faces[j];

		i--;
		numfaces--;
	}
	for ( ; i < MAX_FACES; i++)
		faces[i].w = NULL;

	if (numfaces < 4) {
		invalid = YES;
		[self remove];
		return nil;
	}
	return self;
}

/*
===========
containsPoint

===========
*/
- (BOOL) containsPoint: (vec3_t)pt
{
	int  i;

	for (i = 0; i < numfaces; i++) {
		if (DotProduct (faces[i].plane.normal, pt) >= faces[i].plane.dist)
			return NO;
	}
	return YES;
}

/*
===========
clipRay::::::

===========
*/
- (id) clipRay: (vec3_t)p1
   : (vec3_t)p2
   : (vec3_t)frontpoint
   : (int *)f_face
   : (vec3_t)backpoint
   : (int *)b_face
{
	int     frontface, backface;
	int     i, j;
	face_t  *f;
	float   d1, d2, m;
	float   *start;

	start = p1;

	frontface = -2;
	backface = -2;

	f = faces;
	for (i = 0; i < numfaces; i++, f++) {
		if (!f->w)
			continue;               // clipped off plane
		d1 = DotProduct (p1, f->plane.normal) - f->plane.dist;
		d2 = DotProduct (p2, f->plane.normal) - f->plane.dist;
		if (d1 >= 0 && d2 >= 0) {   // the whole ray is in front of the polytope
			*f_face = -1;
			*b_face = -1;
			return self;
		}
		if (d1 > 0 && d2 < 0) {     // new front plane
			frontface = i;
			m = d1 / (d1 - d2);
			for (j = 0; j < 3; j++)
				frontpoint[j] = p1[j] + m * (p2[j] - p1[j]);

			p1 = frontpoint;
		}
		if (d1 < 0 && d2 > 0) {     // new back plane
			backface = i;
			m = d1 / (d1 - d2);
			for (j = 0; j < 3; j++)
				backpoint[j] = p1[j] + m * (p2[j] - p1[j]);

			p2 = backpoint;
		}
	}

	*f_face = frontface;
	*b_face = backface;

	return self;
}

/*
===========
hitByRay::::

===========
*/
- (void) hitByRay: (vec3_t)p1
   : (vec3_t)p2
   : (float *)time
   : (int *)face
{
	vec3_t  frontpoint, backpoint, dir;
	int     frontface, backface;

	if (regioned) {
		*time = -1;
		*face = -1;
		return;
	}

	[self clipRay: p1 : p2 : frontpoint : &frontface : backpoint : &backface];

	if (frontface == -2 && backface == -2) {
		// entire ray is inside the brush, select first face
		*time = 0;
		*face = 0;
		return;
	}

	if (frontface < 0) {    // ray started inside the polytope, don't select it
		*time = -1;
		*face = -1;
		return;
	}

	VectorSubtract (p2, p1, dir);
	VectorNormalize (dir);
	VectorSubtract (frontpoint, p1, frontpoint);
	*time = DotProduct (frontpoint, dir);

	if (*time < 0)
		Sys_Error ("hitByRay: negative t");

	*face = frontface;
}

/*
==============================================================================

DRAWING ROUTINES

==============================================================================
*/

BOOL  fakebrush;

- (id) drawConnections
{
	id          obj;
	int         c, i;
	vec3_t      dest, origin;
	vec3_t      mid;
	vec3_t      forward, right;
	const char  *targname;
	vec3_t      min, max, temp;
	const char  *targ;

	targ = [parent valueForQKey: "target"];

	if (!targ || !targ[0])
		return self;

	origin[0] = (bmins[0] + bmaxs[0]) / 2;
	origin[1] = (bmins[1] + bmaxs[1]) / 2;

	c = [map_i count];
	for (i = 0; i < c; i++) {
		obj = [map_i objectAtIndex: i];
		targname = [obj valueForQKey: "targetname"];
		if (strcmp (targ, targname))
			continue;

		[[obj objectAtIndex: 0] getMins: min maxs: max];
		dest[0] = (min[0] + max[0]) / 2;
		dest[1] = (min[1] + max[1]) / 2;

		XYmoveto (origin);
		XYlineto (dest);

		forward[0] = dest[0] - origin[0];
		forward[1] = dest[1] - origin[1];
		forward[2] = 0;

		if (!forward[0] && !forward[1])
			continue;
		VectorNormalize (forward);
		forward[0] = 8 * forward[0];
		forward[1] = 8 * forward[1];
		right[0] = forward[1];
		right[1] = -forward[0];

		mid[0] = (dest[0] + origin[0]) / 2;
		mid[1] = (dest[1] + origin[1]) / 2;

		temp[0] = mid[0] + right[0] - forward[0];
		temp[1] = mid[1] + right[1] - forward[1];
		XYmoveto (temp);
		XYlineto (mid);
		temp[0] = mid[0] - right[0] - forward[0];
		temp[1] = mid[1] - right[1] - forward[1];
		XYlineto (temp);
	}

	return self;
}

- (BOOL) fakeBrush: (SEL)call
{
	id      copy;
	face_t  face;

	if (!selected || fakebrush)
		return NO;

	if (![clipper_i getFace: &face])
		return NO;

	fakebrush = YES;
	copy = [self copy];
	copy = [copy addFace: &face];
	if (copy) {
		[copy performSelector: call];
		[copy dealloc];
	}
	fakebrush = NO;
	return YES;
}

/*
===========
XYDrawSelf
===========
*/
- (void) XYDrawSelf
{
	int         i, j;
	winding_t   *w;
	vec3_t      mid, end, s1, s2;
	const char  *val;
	float       ang;
	id          worldent, currentent;
	BOOL        keybrush;

	if ([self fakeBrush: @selector (XYDrawSelf)])
		return;

	[xyview_i addToScrollRange: bmins[0] : bmins[1]];
	[xyview_i addToScrollRange: bmaxs[0] : bmaxs[1]];

	worldent = [map_i objectAtIndex: 0];
	currentent = [map_i currentEntity];

	if (parent != worldent && self == [parent objectAtIndex: 0])
		keybrush = YES;
	else
		keybrush = NO;

	if (parent != worldent && worldent == currentent)
		linecolor (entitycolor[0], entitycolor[1], entitycolor[2]);
	else if (selected)              // selected
		linecolor (1, 0, 0);
	else if (parent == currentent)  // unselected, but in same entity
		linecolor (0, 0, 0);
	else                            // other entity green
		linecolor (0, 0.5, 0);

	if (keybrush)
		[self drawConnections];     // target line

	if (!selected &&
	    (bmaxs[0] < xy_draw_rect.origin.x
	     || bmaxs[1] < xy_draw_rect.origin.y
	     || bmins[0] > xy_draw_rect.origin.x + xy_draw_rect.size.width
	     || bmins[1] > xy_draw_rect.origin.y + xy_draw_rect.size.height))
		return;                     // off view, don't bother

	for (i = 0; i < numfaces; i++) {
		w = faces[i].w;
		if (!w)
			continue;
		if (DotProduct (faces[i].plane.normal, xy_viewnormal) > -VECTOR_EPSILON)
			continue;

		XYmoveto (w->points[w->numpoints - 1]);
		for (j = 0; j < w->numpoints; j++)
			XYlineto (w->points[j]);
	}

	if (keybrush) {
		val = [parent valueForQKey: "angle"];   // angle arrow
		if (val && val[0]) {
			ang = atof (val) * M_PI / 180;
			if (ang > 0) {          // negative values are up/down flags
				mid[0] = (bmins[0] + bmaxs[0]) / 2;
				mid[1] = (bmins[1] + bmaxs[1]) / 2;

				end[0] = mid[0] + 16 * cos (ang);
				end[1] = mid[1] + 16 * sin (ang);

				s1[0] = mid[0] + 12 * cos (ang + 0.4);
				s1[1] = mid[1] + 12 * sin (ang + 0.4);

				s2[0] = mid[0] + 12 * cos (ang - 0.4);
				s2[1] = mid[1] + 12 * sin (ang - 0.4);

				XYmoveto (mid);
				XYlineto (end);
				XYmoveto (s1);
				XYlineto (end);
				XYlineto (s2);
			}
		}
	}

	return;
}

/*
===========
ZDrawSelf
===========
*/
- (void) ZDrawSelf
{
	int         i;
	NSPoint     p;
	vec3_t      p1, p2;
	vec3_t      frontpoint, backpoint;
	int         frontface, backface;
	qtexture_t  *q;

	if ([self fakeBrush: @selector (ZDrawSelf)])
		return;

	[zview_i addToHeightRange: bmins[2]];
	[zview_i addToHeightRange: bmaxs[2]];

	if (selected) {
		[[NSColor redColor] set];
		NSFrameRect (NSMakeRect (1, bmins[2], 24, bmaxs[2] - bmins[2]));
	}

	[zview_i getPoint: &p];
	p1[0] = p.x;
	p1[1] = p.y;
	for (i = 0; i < 2; i++) {
		if (bmins[i] >= p1[i] || bmaxs[i] <= p1[i])
			return;
	}

	p1[2] = 4096;
	p2[0] = p1[0];
	p2[1] = p1[1];
	p2[2] = -4096;

	[self clipRay: p1 : p2 : frontpoint : &frontface : backpoint : &backface];

	if (frontface == -1 || backface == -1)
		return;

	q = TEX_ForName (faces[frontface].texture.texture);

	[[NSColor colorWithCalibratedRed: q->flatcolor.chan[0] / 255.0
	                           green: q->flatcolor.chan[1] / 255.0
	                            blue: q->flatcolor.chan[2] / 255.0
	                           alpha: 1.0] set];
	NSRectFill (NSMakeRect (-8, backpoint[2],
	                        17, frontpoint[2] - backpoint[2] + 1));
	[[NSColor blackColor] set];
	NSFrameRect (NSMakeRect (-12, backpoint[2],
	                         25, frontpoint[2] - backpoint[2] + 1));

	return;
}

/*
===========
CameraDrawSelf
===========
*/
- (void) CameraDrawSelf
{
	int         i, j;
	winding_t   *w;
	id          worldent, currentent;

	if ([self fakeBrush: @selector (CameraDrawSelf)])
		return;

	worldent = [map_i objectAtIndex: 0];
	currentent = [map_i currentEntity];

	if (parent != worldent && worldent == currentent)
		linecolor (entitycolor[0], entitycolor[1], entitycolor[2]);
	else if (selected)
		linecolor (1, 0, 0);
	else if (parent == [map_i currentEntity])
		linecolor (0, 0, 0);
	else
		linecolor (0, 0.5, 0);

	for (i = 0; i < numfaces; i++) {
		w = faces[i].w;
		if (!w)
			continue;
		CameraMoveto (w->points[w->numpoints - 1]);
		for (j = 0; j < w->numpoints; j++)
			CameraLineto (w->points[j]);
	}
	return;
}

/*
===========
XYRenderSelf
===========
*/
- (void) XYRenderSelf
{
	int  i;

	if ([self fakeBrush: @selector (XYRenderSelf)])
		return;

	for (i = 0; i < numfaces; i++)
		REN_DrawXYFace (&faces[i]);

	return;
}

/*
===========
CameraRenderSelf
===========
*/
- (void) CameraRenderSelf
{
	int     i;
	BOOL    olddraw;
	extern qtexture_t   badtex;
	pixel32_t           p;

	if ([self fakeBrush: @selector (CameraRenderSelf)])
		return;

	// hack to draw entity boxes as single flat color
	if (![parent modifiable]) {
		olddraw = r_drawflat;
		r_drawflat = YES;

		p = badtex.flatcolor;

		badtex.flatcolor.chan[0] = entitycolor[0] * 255;
		badtex.flatcolor.chan[1] = entitycolor[1] * 255;
		badtex.flatcolor.chan[2] = entitycolor[2] * 255;

		for (i = 0; i < numfaces; i++)
			REN_DrawCameraFace (&faces[i]);

		badtex.flatcolor = p;
		r_drawflat = olddraw;
	} else {
		for (i = 0; i < numfaces; i++)
			REN_DrawCameraFace (&faces[i]);
	}

	return;
}

/*
==============================================================================

SINGLE BRUSH ACTIONS

==============================================================================
*/

face_t  *dragface, *dragface2;

int     numcontrolpoints;
float   *controlpoints[MAX_FACES * 3];

- (BOOL) checkModifiable
{
//  int     i;

	if ([parent modifiable])
		return YES;

	// don't stretch spawned entities, move all points
#if 0
	numcontrolpoints = numfaces * 3;

	for (i = 0; i < numcontrolpoints; i++)
		controlpoints[i] = faces[i / 3].planepts[i % 3];
#endif

	return NO;
}

- (void) getZdragface: (vec3_t)dragpoint
{
	int     i, j;
	float   d;

	if (![self checkModifiable])
		return;

	numcontrolpoints = 0;

	for (i = 0; i < numfaces; i++) {
		if (!faces[i].w)
			continue;

		if (faces[i].plane.normal[2] == 1)
			d = dragpoint[2] - faces[i].plane.dist;
		else if (faces[i].plane.normal[2] == -1)
			d = -faces[i].plane.dist - dragpoint[2];
		else
			continue;

		if (d <= 0)
			continue;

		for (j = 0; j < 3; j++) {
			controlpoints[numcontrolpoints] = faces[i].planepts[j];
			numcontrolpoints++;
		}
	}
}

- (void) getXYdragface: (vec3_t)dragpoint
{
	int     i, j;
	float   d;

	numcontrolpoints = 0;

	if (![self checkModifiable])
		return;

	for (i = 0; i < numfaces; i++) {
		if (!faces[i].w)
			continue;

		if (faces[i].plane.normal[2])
			continue;

		d = DotProduct (faces[i].plane.normal, dragpoint) - faces[i].plane.dist;
		if (d <= 0)
			continue;

		for (j = 0; j < 3; j++) {
			controlpoints[numcontrolpoints] = faces[i].planepts[j];
			numcontrolpoints++;
		}
	}
}

- (void) getXYShearPoints: (vec3_t)dragpoint
{
	int         i, j, k;
	int         facectl;
	float       d;
	int         numdragplanes;
	BOOL        dragplane[MAX_FACES];
	winding_t   *w;
	face_t      *f;
	BOOL        onplane[MAX_POINTS_ON_WINDING];

	if (![self checkModifiable])
		return;
	numcontrolpoints = 0;
	numdragplanes = 0;
	for (i = 0; i < numfaces; i++) {
		dragplane[i] = NO;
		if (!faces[i].w)
			continue;
//		if (faces[i].plane.normal[2])
//			continue;

		d = DotProduct (faces[i].plane.normal, dragpoint) - faces[i].plane.dist;
		if (d <= -ON_EPSILON)
			continue;
		dragplane[i] = YES;
		numdragplanes++;
	}

	// find faces that just share an edge with a drag plane
	for (i = 0; i < numfaces; i++) {
		f = &faces[i];
		w = f->w;
		if (!w)
			continue;

		if (dragplane[i] && numdragplanes == 1) {
			for (j = 0; j < 3; j++) {
				controlpoints[numcontrolpoints] = faces[i].planepts[j];
				numcontrolpoints++;
			}
			continue;
		}

		if (!dragplane[i] && numdragplanes > 1)
			continue;

		facectl = 0;
		for (j = 0; j < w->numpoints; j++) {
			onplane[j] = NO;
			for (k = 0; k < numfaces; k++) {
				if (!dragplane[k])
					continue;
				if (k == i)
					continue;
				d = DotProduct (w->points[j], faces[k].plane.normal)
				    - faces[k].plane.dist;
				if (fabs (d) > ON_EPSILON)
					continue;
				onplane[j] = YES;
				facectl++;
				break;
			}
		}

		if (facectl == 0)
			continue;

		// find one or two static points to go with the controlpoints
		// and change the plane points
		k = 0;
		for (j = 0; j < w->numpoints; j++) {
			if (!onplane[j])
				continue;
			if (facectl >= 2 && !onplane[(j + 1) % w->numpoints])
				continue;
			if (facectl == 3 && !onplane[(j + 2) % w->numpoints])
				continue;
			VectorCopy (w->points[j], f->planepts[k]);
			controlpoints[numcontrolpoints] = f->planepts[k];
			numcontrolpoints++;
			k++;

			if (facectl >= 2) {
				VectorCopy (w->points[(j + 1) % w->numpoints], f->planepts[k]);
				controlpoints[numcontrolpoints] = f->planepts[k];
				numcontrolpoints++;
				k++;
			}
			if (facectl == 3) {
				VectorCopy (w->points[(j + 2) % w->numpoints], f->planepts[k]);
				controlpoints[numcontrolpoints] = f->planepts[k];
				numcontrolpoints++;
				k++;
			}
			break;
		}

		for ( ; j < w->numpoints && k != 3; j++) {
			if (!onplane[j]) {
				VectorCopy (w->points[j], f->planepts[k]);
				k++;
			}
		}

		for (j = 0; j < w->numpoints && k != 3; j++) {
			if (!onplane[j]) {
				VectorCopy (w->points[j], f->planepts[k]);
				k++;
			}
		}

		if (k != 3) {
//          Sys_Error ("getXYShearPoints: didn't get three points on plane");
			numcontrolpoints = 0;
			return;
		}

		for (j = 0; j < 3; j++) {
			for (k = 0; k < 3; k++)
				f->planepts[j][k] = rint (f->planepts[j][k]);
		}
	}
}

/*
==============================================================================

MULTIPLE BRUSH ACTIONS

==============================================================================
*/

vec3_t  region_min, region_max;

/*
===========
newRegion

Set the regioned flag based on if the object is containted in region_min/max
===========
*/
- (id) newRegion
{
	int         i;
	const char  *name;

	// filter away entities
	if (parent != [map_i objectAtIndex: 0]) {
		if (filter_entities) {
			regioned = YES;
			return self;
		}

		name = [parent valueForQKey: "classname"];

		if ((filter_light && !strncmp (name, "light", 5))
		    || (filter_path && !strncmp (name, "path", 4))) {
			regioned = YES;
			return self;
		}
	} else if (filter_world) {
		regioned = YES;
		return self;
	}

	if (filter_clip_brushes && !strcasecmp (faces[0].texture.texture, "clip")) {
		regioned = YES;
		return self;
	}

	if (filter_water_brushes && faces[0].texture.texture[0] == '*') {
		regioned = YES;
		return self;
	}

	for (i = 0; i < 3; i++) {
		if (region_min[i] >= bmaxs[i] || region_max[i] <= bmins[i]) {
			if (selected)
				[self deselect];
			regioned = YES;
			return self;
		}
	}

	regioned = NO;
	return self;
}

vec3_t  select_min, select_max;

- (void) selectPartial
{
	int  i;

	for (i = 0; i < 3; i++) {
		if (select_min[i] >= bmaxs[i] || select_max[i] <= bmins[i])
			return;
	}
	selected = YES;
}

- (void) selectComplete
{
	int  i;

	for (i = 0; i < 3; i++) {
		if (select_min[i] > bmins[i] || select_max[i] < bmaxs[i])
			return;
	}
	selected = YES;
}

- (void) regionPartial
{
	int  i;

	for (i = 0; i < 3; i++) {
		if (select_min[i] >= bmaxs[i] || select_max[i] <= bmins[i])
			return;
	}
	selected = YES;
}

- (void) regionComplete
{
	int  i;

	for (i = 0; i < 3; i++) {
		if (select_min[i] > bmins[i] || select_max[i] < bmaxs[i])
			return;
	}
	selected = YES;
}

id  sb_newowner;

- (void) moveToEntity
{
	id      eclass;
	float   *c;

	[parent removeObject: self];
	parent = sb_newowner;

	// hack to allow them to be copied to another map
	if ([parent respondsToSelector: @selector (valueForQKey:)]) {
		eclass = [entity_classes_i classForName: [parent valueForQKey: "classname"]];
		c = [eclass drawColor];
		[self setEntityColor: c];
	}

	[parent addObject: self];
}

vec3_t  sb_translate;

- (void) translate
{
	int  i, j;

	// move the planes
	for (i = 0; i < numfaces; i++) {
		for (j = 0; j < 3; j++)
			VectorAdd (faces[i].planepts[j], sb_translate, faces[i].planepts[j]);
	}

	[self calcWindings];
}

vec3_t  sb_mins, sb_maxs;

- (void) addToBBox
{
	int  k;

	if (numfaces < 4)
		return;

	for (k = 0; k < 3; k++) {
		if (bmins[k] < sb_mins[k])
			sb_mins[k] = bmins[k];
		if (bmaxs[k] > sb_maxs[k])
			sb_maxs[k] = bmaxs[k];
	}
}

- (void) flushTextures
{                                   // call when texture palette changes
	int  i;

	for (i = 0; i < MAX_FACES; i++)
		faces[i].qtexture = NULL;

	[self calcWindings];
}

- (void) select
{
	[map_i setCurrentEntity: parent];
	selected = YES;
}

- (void) deselect
{
	selected = NO;

	// the last selected brush determines
	if (invalid)
		printf ("WARNING: deselected invalid brush\n");
	[map_i setCurrentMinZ: bmins[2]];
	[map_i setCurrentMaxZ: bmaxs[2]];
}

- (void) remove
{
	// the last selected brush determines
	if (!invalid) {
		[map_i setCurrentMinZ: bmins[2]];
		[map_i setCurrentMaxZ: bmaxs[2]];
	}

	[parent removeObject: self];
	[self release];
}

vec3_t  sel_x, sel_y, sel_z;
vec3_t  sel_org;

- (void) transform
{
	int     i, j;
	vec3_t  old;
	float   *p;

	for (i = 0; i < numfaces; i++) {
		for (j = 0; j < 3; j++) {
			p = faces[i].planepts[j];
			VectorCopy (p, old);
			VectorSubtract (old, sel_org, old);
			p[0] = DotProduct (old, sel_x);
			p[1] = DotProduct (old, sel_y);
			p[2] = DotProduct (old, sel_z);
			VectorAdd (p, sel_org, p);
		}
	}

	[self calcWindings];
}

- (void) flipNormals                // used after an inside-out transform
                                    // (flip x/y/z)
{
	int     i;
	vec3_t  temp;

	for (i = 0; i < numfaces; i++) {
		VectorCopy (faces[i].planepts[0], temp);
		VectorCopy (faces[i].planepts[2], faces[i].planepts[0]);
		VectorCopy (temp, faces[i].planepts[2]);
	}
	[self calcWindings];
}

- (void) carveByClipper
{
	face_t  face;

	if (![clipper_i getFace: &face])
		return;

	[self addFace: &face];
}

- (void) takeCurrentTexture
{
	texturedef_t  td;

	[texturepalette_i getTextureDef: &td];
	[self setTexturedef: &td];
}

float  sb_floor_dir, sb_floor_dist;

- (void) feetToFloor
{
	float   oldz;
	vec3_t  p1, p2;
	int     frontface, backface;
	vec3_t  frontpoint, backpoint;
	float   dist;

	[cameraview_i getOrigin: p1];
	VectorCopy (p1, p2);
	oldz = p1[2] - 48;

	p1[2] = 4096;
	p2[2] = -4096;

	[self clipRay: p1 : p2 : frontpoint : &frontface : backpoint : &backface];
	if (frontface == -1)
		return;

	dist = frontpoint[2] - oldz;

	if (sb_floor_dir == 1) {
		if (dist > 0 && dist < sb_floor_dist)
			sb_floor_dist = dist;
		else if (dist < 0 && dist > sb_floor_dist)
			sb_floor_dist = dist;
	}
}

/*
===============================================================================

BRUSH SUBTRACTION

===============================================================================
*/

vec3_t  carvemin, carvemax;
int     numcarvefaces;
face_t  *carvefaces;
id      carve_in, carve_out;

// returns the new brush formed after the addition of the given plane
// nil is returned if it faced all of the original setbrush
- (id) addFace: (face_t *)f
{
	if (numfaces == MAX_FACES)
		Sys_Error ("addFace: numfaces == MAX_FACES");
	faces[numfaces] = *f;
	faces[numfaces].texture = faces[0].texture;
	faces[numfaces].qtexture = NULL;
	faces[numfaces].w = NULL;
	numfaces++;
	[self calcWindings];

	// remove any degenerate faces
	return [self removeIfInvalid];
}

- (void) clipByFace: (face_t *)fa
   front: (id *)f
   back: (id *)b
{
	id      front, back;
	face_t  fb;
	vec3_t  temp;

	fb = *fa;
	VectorCopy (fb.planepts[0], temp);
	VectorCopy (fb.planepts[2], fb.planepts[0]);
	VectorCopy (temp, fb.planepts[2]);

	front = [self copy];
	back = [self copy];

	*b = [back addFace: fa];
	*f = [front addFace: &fb];
}

- (id) carve
{
	int     i;
	id      front, back;

#if 0
	if ((i = NSMallocCheck ()))
		Sys_Error ("MallocCheck failure");
#endif

	// check bboxes
	for (i = 0; i < 3; i++) {
		if (bmins[i] >= carvemax[i] || bmaxs[i] <= carvemin[i]) {
			[carve_out addObject: self];
			return self;
		}
	}

	// carve by the planes
	back = self;
	for (i = 0; i < numcarvefaces; i++) {
		[back clipByFace: &carvefaces[i] front: &front back: &back];
		if (front)
			[carve_out addObject: front];
		if (!back)
			return nil;             // nothing completely inside
	}

	[carve_in addObject: back];
	return self;
}

/*
==================
setCarveVars
==================
*/
- (void) setCarveVars
{
	VectorCopy (bmins, carvemin);
	VectorCopy (bmaxs, carvemax);
	numcarvefaces = numfaces;
	carvefaces = faces;
}

- (int) getNumBrushFaces
{
	return numfaces;
}

- (face_t *) getBrushFace: (int)which
{
	return &faces[which];
}

@end
