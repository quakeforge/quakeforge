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
entity_t   *r_view_model;
entity_t   *r_player_entity;
int         r_lineadj;
qboolean    r_active;
float       r_time1;

byte        color_white[4] = { 255, 255, 255, 0 };	// alpha will be explicitly set
byte        color_black[4] = { 0, 0, 0, 0 };		// alpha will be explicitly set

fire_t r_fires[MAX_FIRES];


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
