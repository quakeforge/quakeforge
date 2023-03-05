/*
	glsl_shader.c

	Shader management based on The OpenGL Shader Wrangler
	http://prideout.net/blog/?p=11

	Copyright (C) 2013 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2013/02/26

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

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/segtext.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/GLSL/qf_vid.h"

typedef struct glsl_effect_s {
	struct glsl_effect_s *next;
	const char *name;
	segtext_t  *text;
} glsl_effect_t;

static hashtab_t *effect_tab;
ALLOC_STATE (glsl_effect_t, effects);

static glsl_effect_t *
new_effect (void)
{
	glsl_effect_t *effect;
	ALLOC (64, glsl_effect_t, effects, effect);
	return effect;
}

static const char *
effect_get_key (const void *e, void *unused)
{
	glsl_effect_t *effect = (glsl_effect_t *) e;
	return effect->name;
}

int
GLSL_RegisterEffect (const char *name, const char *src)
{
	glsl_effect_t *effect;
	segtext_t  *text;

	if (!effect_tab)
		effect_tab = Hash_NewTable (61, effect_get_key, 0, 0, 0);

	if (Hash_Find (effect_tab, name)) {
		Sys_Printf ("WARNING: ignoring duplicate '%s' effect\n", name);
		return 0;
	}
	text = Segtext_new (src);
	if (!text) {
		Sys_Printf ("WARNING: %s: problem parsing effect source\n", name);
		return 0;
	}
	effect = new_effect ();
	effect->name = strdup (name);
	effect->text = text;
	Hash_Add (effect_tab, effect);
	return 1;
}

shader_t *
GLSL_BuildShader (const char **effect_keys)
{
	const char *dot;
	const char **key;
	int         num_keys = 0;
	shader_t   *shader;
	dstring_t  *ekey;
	glsl_effect_t *effect;
	const segchunk_t *chunk;

	for (key = effect_keys; *key; key++)
		num_keys++;

	shader = malloc (sizeof (shader_t));
	shader->num_strings = num_keys;
	shader->strings = calloc (2 * num_keys, sizeof (const char *));
	shader->src = shader->strings + num_keys;

	ekey = dstring_new ();
	for (key = effect_keys; *key; key++) {
		int         num = key - effect_keys;
		dot = strchr (*key, '.');
		if (!dot) {
			Sys_Printf ("Invalid effect key: '%s'\n", *key);
			goto error;
		}
		dstring_copysubstr (ekey, *key, dot - *key);
		effect = Hash_Find (effect_tab, ekey->str);
		if (!effect) {
			Sys_Printf ("Unknown effect: '%s'\n", ekey->str);
			goto error;
		}
		dot++;
		chunk = Segtext_FindChunk (effect->text, dot);
		if (!chunk) {
			Sys_Printf ("Unknown shader key: '%s'\n", dot);
			goto error;
		}
		if (strncmp ("#version ", chunk->text, 9) == 0) {
			const char *eol = strchr (chunk->text, '\n');
			int         vline_len = eol ? eol - chunk->text + 1 : 0;
			shader->strings[num] = nva ("%.*s#line %d\n%s", vline_len,
										chunk->text,
										chunk->start_line + 1,
										chunk->text + vline_len);
		} else {
			shader->strings[num] = nva ("#line %d\n%s", chunk->start_line,
										chunk->text);
		}
		shader->src[num] = strdup (ekey->str);
	}
	dstring_delete (ekey);
	return shader;
error:
	// there is guaranteed to be a null in the array if an error occurs.
	for (key = shader->strings; *key; key++) {
		free ((char *) *key);
		free ((char *) shader->src[key - shader->strings]);
	}
	free (shader->strings);
	free (shader);
	dstring_delete (ekey);
	return 0;
}

void
GLSL_FreeShader (shader_t *shader)
{
	int         i;

	for (i = 0; i < shader->num_strings; i++) {
		free ((char *) shader->strings[i]);
		free ((char *) shader->src[i]);
	}
	free (shader->strings);
	free (shader);
}
