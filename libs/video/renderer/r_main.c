#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/qtypes.h"
#include "QF/render.h"

#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"

qboolean    r_inhibit_viewmodel;
qboolean    r_force_fullscreen;
qboolean    r_paused;
double      r_realtime;
double      r_frametime;
dlight_t    r_dlights[MAX_DLIGHTS];
entity_t   *r_view_model;
entity_t   *r_player_entity;
lightstyle_t r_lightstyle[MAX_LIGHTSTYLES];
int         r_lineadj;
qboolean    r_active;
float       r_time1;

fire_t r_fires[MAX_FIRES];


dlight_t *
R_AllocDlight (int key)
{
	int         i;
	dlight_t   *dl;

	// first look for an exact key match
	if (key) {
		dl = r_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof (*dl));
				dl->key = key;
				dl->color[0] = dl->color[1] = dl->color[2] = 1;
				return dl;
			}
		}
	}
	// then look for anything else
	dl = r_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < r_realtime) {
			memset (dl, 0, sizeof (*dl));
			dl->key = key;
			dl->color[0] = dl->color[1] = dl->color[2] = 1;
			return dl;
		}
	}

	dl = &r_dlights[0];
	memset (dl, 0, sizeof (*dl));
	dl->key = key;
	return dl;
}

void
R_DecayLights (double frametime)
{
	int         i;
	dlight_t   *dl;

	dl = r_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < r_realtime || !dl->radius)
			continue;

		dl->radius -= frametime * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

void
R_ClearDlights (void)
{
	memset (r_dlights, 0, sizeof (r_dlights));
}

void
R_ClearFires (void)
{
	memset (r_fires, 0, sizeof (r_fires));
}

/*
	R_AddFire

	Nifty ball of fire GL effect.  Kinda a meshing of the dlight and
	particle engine code.
*/
void
R_AddFire (vec3_t start, vec3_t end, entity_t *ent)
{
	float       len;
	fire_t     *f;
	vec3_t      vec;
	int         key;

	if (!gl_fires->int_val)
		return;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	key = ent->keynum;

	if (len) {
		f = R_AllocFire (key);
		VectorCopy (end, f->origin);
		VectorCopy (start, f->owner);
		f->size = 10;
		f->die = r_realtime + 0.5;
		f->decay = 1;
		VectorCopy (r_firecolor->vec, f->color);
	}
}

/*
	R_AllocFire

	Clears out and returns a new fireball
*/
fire_t     *
R_AllocFire (int key)
{
	int         i;
	fire_t     *f;

	if (key)							// first try to find/reuse a keyed
										// spot
	{
		f = r_fires;
		for (i = 0; i < MAX_FIRES; i++, f++)
			if (f->key == key) {
				memset (f, 0, sizeof (*f));
				f->key = key;
				return f;
			}
	}

	f = r_fires;						// no match, look for a free spot
	for (i = 0; i < MAX_FIRES; i++, f++) {
		if (f->die < r_realtime) {
			memset (f, 0, sizeof (*f));
			f->key = key;
			return f;
		}
	}

	f = &r_fires[0];
	memset (f, 0, sizeof (*f));
	f->key = key;
	return f;
}
