#include "QF/qtypes.h"
#include "QF/render.h"

#include "r_local.h"

qboolean    r_inhibit_viewmodel;
qboolean    r_force_fullscreen;
qboolean    r_paused;
double      r_realtime;
dlight_t    r_dlights[MAX_DLIGHTS];
entity_t   *r_view_model;
entity_t   *r_player_entity;

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
