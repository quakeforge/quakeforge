/*
	r_part.c

	Interface for particles

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

#include "QF/alloc.h"
#include "QF/cbuf.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/script.h"
#include "QF/segtext.h"
#include "QF/sys.h"

#include "compat.h"
#include "r_internal.h"

typedef enum {
	pt_phys_done,
	pt_phys_add_vel,
	pt_phys_sub_slowgrav,
	pt_phys_add_grav,
	pt_phys_sub_grav,
	pt_phys_add_fastgrav,
	pt_phys_add_ramp,		// die
	pt_phys_fade_alpha,		// die
	pt_phys_color_ramp1,
	pt_phys_color_ramp2,
	pt_phys_color_ramp3,
	pt_phys_alpha_ramp,
	pt_phys_explode_vel,
	pt_phys_damp_vel,
	pt_phys_grow_scale,
	pt_phys_shrink_scale,	// die
	pt_phys_custom,			// die
} pt_phys_e;

typedef struct {
	const char *name;
	pt_phys_e   opcode;
	int         num_params;
} pt_phys_opcode_t;

typedef struct {
	char       *name;
	qboolean  (*func) (particle_t *part, void *data);
	void       *data;
} pt_phys_func_t;

typedef union pt_phys_op_s {
	int32_t     op;
	float       parm;
} pt_phys_op_t;

typedef struct pt_phys_script_s {
	struct pt_phys_script_s *next;
	char       *name;
	pt_phys_op_t *script;
} pt_phys_script_t;

const char particle_types[] =
#include "particles.pc"
;

static pt_phys_func_t *phys_funcs;
static int num_phys_funcs;
static int max_phys_funcs;
static pt_phys_script_t *phys_script_freelist;
static hashtab_t *phys_scripts;

static pt_phys_opcode_t part_phys_opcodes[] =
{
	{"add_vel",			pt_phys_add_vel,		0},
	{"sub_slowgrav",	pt_phys_sub_slowgrav,	0},
	{"add_grav",		pt_phys_add_grav,		0},
	{"sub_grav",		pt_phys_sub_grav,		0},
	{"add_fastgrav",	pt_phys_add_fastgrav,	0},
	{"add_ramp",		pt_phys_add_ramp,		2},
	{"fade_alpha",		pt_phys_fade_alpha,		1},
	{"color_ramp1",		pt_phys_color_ramp1,	0},
	{"color_ramp2",		pt_phys_color_ramp2,	0},
	{"color_ramp3",		pt_phys_color_ramp3,	0},
	{"alpha_ramp",		pt_phys_alpha_ramp,		1},
	{"explode_vel",		pt_phys_explode_vel,	1},
	{"damp_vel",		pt_phys_damp_vel,		1},
	{"grow_scale",		pt_phys_grow_scale,		1},
	{"shrink_scale",	pt_phys_shrink_scale,	1},

	{0, pt_phys_done, 0},
};

unsigned int	r_maxparticles, numparticles;
particle_t	   *active_particles, *free_particles, *particles, **freeparticles;
vec3_t			r_pright, r_pup, r_ppn;

/*
  R_MaxParticlesCheck

  Misty-chan: Dynamically change the maximum amount of particles on the fly.
  Thanks to a LOT of help from Taniwha, Deek, Mercury, Lordhavoc, and lots of
  others.
*/
void
R_MaxParticlesCheck (cvar_t *r_particles, cvar_t *r_particles_max)
{
/*
	Catchall. If the user changed the setting to a number less than zero *or*
	if we had a wacky cfg get past the init code check, this will make sure we
	don't have problems. Also note that grabbing the var->int_val is IMPORTANT:
	Prevents a segfault since if we grabbed the int_val of r_particles_max
	we'd sig11 right here at startup.
*/
	if (r_particles && r_particles->int_val)
		r_maxparticles = r_particles_max ? r_particles_max->int_val : 0;
	else
		r_maxparticles = 0;

/*
	Be very careful the next time we do something like this. calloc/free are
	IMPORTANT and the compiler doesn't know when we do bad things with them.
*/
	if (particles)
		free (particles);
	if (freeparticles)
		free (freeparticles);

	particles = 0;
	freeparticles = 0;

	if (r_maxparticles) {
		particles = (particle_t *) calloc (r_maxparticles, sizeof (particle_t));
		freeparticles = (particle_t **) calloc (r_maxparticles,
												sizeof (particle_t *));
	}

	vr_funcs->R_ClearParticles ();

	if (r_init)
		vr_funcs->R_InitParticles ();
}

static int  ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static int  ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int  ramp3[8] = { 0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };

static inline float
slow_grav (void)
{
	return -vr_data.frametime * vr_data.gravity * 0.0375;
}

static inline float
grav (void)
{
	return -vr_data.frametime * vr_data.gravity * 0.05;
}

static inline float
fast_grav (void)
{
	return -vr_data.frametime * vr_data.gravity;
}

static inline void
add_vel (particle_t *part)
{
	VectorMultAdd (part->org, vr_data.frametime, part->vel, part->org);
}

static inline void
sub_slowgrav (particle_t *part)
{
	part->vel[2] -= slow_grav ();
}

static inline void
add_grav (particle_t *part)
{
	part->vel[2] += grav ();
}

static inline void
sub_grav (particle_t *part)
{
	part->vel[2] -= grav ();
}

static inline void
add_fastgrav (particle_t *part)
{
	part->vel[2] += fast_grav ();
}

static inline qboolean
add_ramp (particle_t *part, float time, int max)
{
	part->ramp += vr_data.frametime * time;
	if (part->ramp >= max) {
		part->die = -1;
		return true;
	}
	return false;
}

static inline qboolean
fade_alpha (particle_t *part, float time)
{
	part->alpha -= vr_data.frametime * time;
	if (part->alpha <= 0.0) {
		part->die = -1;
		return true;
	}
	return false;
}

static inline qboolean
custom_func (particle_t *part, int op)
{
	int         func;
	func = op - pt_phys_custom;
	if (func < 0 || func >= num_phys_funcs) {
		Sys_Error ("invalid custom particle physics opcode: %d", op);
	}
	if (phys_funcs[func].func (part, phys_funcs[func].data)) {
		part->die = -1;
		return true;
	}
	return false;
}

void
R_RunParticlePhysics (particle_t *part)
{
	const pt_phys_op_t *op;
	float       parm, max;

	if (!part->physics) {
		// safety net for physicsless particles
		add_ramp (part, 1, 4);
		return;
	}
	for (op = part->physics; ; op++) {
		if (op->op >= pt_phys_custom) {
			if (custom_func (part, op->op))
				return;
		}

		switch ((pt_phys_e) op->op) {
			case pt_phys_done:
				return;
			case pt_phys_add_vel:
				add_vel (part);
				break;
			case pt_phys_sub_slowgrav:
				sub_slowgrav (part);
				break;
			case pt_phys_add_grav:
				add_grav (part);
				break;
			case pt_phys_sub_grav:
				sub_grav (part);
				break;
			case pt_phys_add_fastgrav:
				add_fastgrav (part);
				break;
			case pt_phys_add_ramp:
				parm = (++op)->parm;
				max = (++op)->parm;
				if (add_ramp (part, parm, max))
					return;
				break;
			case pt_phys_fade_alpha:
				parm = (++op)->parm;
				if (fade_alpha (part, parm))
					return;
				break;
			case pt_phys_color_ramp1:
				part->color = ramp1[((int) part->ramp) % sizeof (ramp1)];
				break;
			case pt_phys_color_ramp2:
				part->color = ramp2[((int) part->ramp) % sizeof (ramp2)];
				break;
			case pt_phys_color_ramp3:
				part->color = ramp3[((int) part->ramp) % sizeof (ramp3)];
				break;
			case pt_phys_alpha_ramp:
				parm = (++op)->parm;
				part->alpha = (parm - part->ramp) / parm;
				break;
			case pt_phys_explode_vel:
				parm = (++op)->parm;
				VectorMultAdd (part->vel, vr_data.frametime * parm, part->vel,
							   part->vel);
				break;
			case pt_phys_damp_vel:
				parm = (++op)->parm;
				part->vel[0] -= part->vel[0] * vr_data.frametime * parm;
				part->vel[1] -= part->vel[1] * vr_data.frametime * parm;
				break;
			case pt_phys_grow_scale:
				parm = (++op)->parm;
				part->scale += vr_data.frametime * parm;
				break;
			case pt_phys_shrink_scale:
				parm = (++op)->parm;
				part->scale -= vr_data.frametime * parm;
				if (part->scale <= 0.0) {
					part->die = -1;
					return;
				}
				break;
			case pt_phys_custom:
				// handled above
				break;
		}
	}
}

static pt_phys_opcode_t *
part_find_opcode (const char *name)
{
	pt_phys_opcode_t *op;

	for (op = part_phys_opcodes; op->name; op++) {
		if (strcmp (op->name, name) == 0)
			return op;
	}
	return 0;
}

static pt_phys_func_t *
part_find_function (const char *name)
{
	pt_phys_func_t *func;

	//FIXME is a hash table worth it?
	for (func = phys_funcs; func - phys_funcs < num_phys_funcs; func++) {
		if (strcmp (func->name, name) == 0)
			return func;
	}
	return 0;
}

static void
part_add_op (dstring_t *phys, int op)
{
	int         offs = phys->size;
	phys->size += sizeof (pt_phys_op_t);
	dstring_adjust (phys);
	((pt_phys_op_t *) (phys->str + offs))->op = op;
}

static void
part_add_parm (dstring_t *phys, float parm)
{
	int         offs = phys->size;
	phys->size += sizeof (pt_phys_op_t);
	dstring_adjust (phys);
	((pt_phys_op_t *) (phys->str + offs))->parm = parm;
}

static pt_phys_script_t *
new_phys_script (void)
{
	pt_phys_script_t    *phys_script;
	ALLOC (16, pt_phys_script_t, phys_script, phys_script);
	return phys_script;
}

static void
free_phys_script (pt_phys_script_t *phys_script)
{
	FREE (phys_script, phys_script);
}

static const char *
part_script_get_key (const void *_s, void *unused)
{
	const pt_phys_script_t *s = (const pt_phys_script_t *) _s;
	return s->name;
}

static void
part_script_free (void *_s, void *unused)
{
	pt_phys_script_t *s = (pt_phys_script_t *) _s;
	free (s->name);
	free (s->script);
	free_phys_script (s);
}

qboolean
R_CompileParticlePhysics (const char *name, const char *code)
{
	cbuf_args_t *args = Cbuf_ArgsNew ();
	script_t   *script = Script_New ();
	pt_phys_opcode_t *opcode;
	pt_phys_script_t *phys;
	pt_phys_func_t *func;
	dstring_t  *phys_build = dstring_new ();
	int         i;
	qboolean    ret = false;

	if (!phys_scripts) {
		phys_scripts = Hash_NewTable (61, part_script_get_key,
									  part_script_free, 0);
	}
	Script_Start (script, "name", code);
	while (Script_GetToken (script, 1)) {
		args->argc = 0;
		Cbuf_ArgsAdd (args, Script_Token (script));
		while (Script_TokenAvailable (script, 0)) {
			Script_GetToken (script, 0);
			Cbuf_ArgsAdd (args, Script_Token (script));
		}
		Sys_Printf ("%s:%d: %s\n", name, script->line, args->argv[0]->str);
		if ((func = part_find_function (args->argv[0]->str))) {
			if (args->argc > 1) {
				Sys_Printf ("%s:%d: warning: ignoring extra args\n",
							name, script->line);
			}
			part_add_op (phys_build, pt_phys_custom + (func - phys_funcs));
		} else if ((opcode = part_find_opcode (args->argv[0]->str))) {
			part_add_op (phys_build, opcode->opcode);
			if (args->argc - 1 > opcode->num_params) {
				Sys_Printf ("%s:%d: warning: ignoring extra args\n",
							name, script->line);
				args->argc = opcode->num_params + 1;
			}
			for (i = 1; i < args->argc; i++) {
				char       *end;
				float       parm = strtof (args->argv[i]->str, &end);
				if (end == args->argv[i]->str) {
					Sys_Printf ("%s:%d: warning: bad float\n",
								name, script->line);
				} else if (*end) {
					Sys_Printf ("%s:%d: warning: ignoring extra chars\n",
								name, script->line);
				}
				part_add_parm (phys_build, parm);
			}
			if (args->argc - 1 > opcode->num_params) {
				Sys_Printf ("%s:%d: warning: setting parms %d+ to zero\n",
							name, script->line, args->argc - 1);
				for (i--; i < opcode->num_params; i++)
					part_add_parm (phys_build, 0);
			}
		} else {
			Sys_Printf ("%s:%d: warning: unknown opcode/function: %s\n",
						name, script->line, args->argv[0]->str);
		}
	}
	if (phys_build->size) {
		part_add_op (phys_build, pt_phys_done);
		phys = new_phys_script ();
		phys->name = strdup (name);
		phys->script = (pt_phys_op_t *) dstring_freeze (phys_build);
		Hash_Add (phys_scripts, phys);
		ret = true;
	} else {
		dstring_delete (phys_build);
	}

	Script_Delete (script);
	Cbuf_ArgsDelete (args);
	return ret;
}

const pt_phys_op_t *
R_ParticlePhysics (const char *type)
{
	pt_phys_script_t *phys;

	if (!phys_scripts)
		return 0;
	if ((phys = Hash_Find (phys_scripts, type)))
		return phys->script;
	return 0;
}

qboolean
R_AddParticlePhysicsFunction (const char *name,
							  qboolean  (*func) (particle_t *part, void *data),
							  void *data)
{
	int         i;

	for (i = 0; i < num_phys_funcs; i++) {
		if (strcmp (name, phys_funcs[i].name) == 0)
			return false;
	}
	if (num_phys_funcs == max_phys_funcs) {
		max_phys_funcs += 16;
		phys_funcs = realloc (phys_funcs,
							  max_phys_funcs * sizeof (pt_phys_func_t));
		if (!phys_funcs) {
			Sys_Error ("R_AddParticlePhysicsFunction: out of memory");
		}
	}
	phys_funcs[num_phys_funcs].name = strdup (name);
	phys_funcs[num_phys_funcs].func = func;
	phys_funcs[num_phys_funcs].data = data;
	return true;
}

void
R_LoadParticles (void)
{
	segtext_t  *text;
	segchunk_t *chunk;

	text = Segtext_new (particle_types);
	for (chunk = text->chunk_list; chunk; chunk = chunk->next) {
		if (chunk->tag) {
			Sys_Printf ("compiling %s\n", chunk->tag);
			R_CompileParticlePhysics (chunk->tag, chunk->text);
		}
	}
}
