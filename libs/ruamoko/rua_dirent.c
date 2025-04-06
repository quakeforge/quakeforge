/*
	bi_dirent.c

	CSQC dirent builtins

	Copyright (C) 2025       Bill Currie <bill@taniwha.org>

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
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>

#include "QF/progs.h"

#include "rua_internal.h"

typedef struct {
	pr_string_t name;
	pr_int_t    type;
	pr_int_t    err;
} dirinfo_t;

typedef struct bi_dir_s {
	struct bi_dir_s *next;
	struct bi_dir_s **prev;
	DIR *dir;
} bi_dir_t;

typedef struct {
	progs_t    *pr;
	PR_RESMAP (bi_dir_t) dir_map;
	bi_dir_t   *dirs;
} dirent_resources_t;

static bi_dir_t *
dirent_dir_new (dirent_resources_t *res)
{
	qfZoneScoped (true);
	return PR_RESNEW (res->dir_map);
}

static void
dirent_dir_free (dirent_resources_t *res, bi_dir_t *dir)
{
	qfZoneScoped (true);
	PR_RESFREE (res->dir_map, dir);
}

static void
dirent_dir_reset (dirent_resources_t *res)
{
	qfZoneScoped (true);
	PR_RESRESET (res->dir_map);
}

static inline bi_dir_t *
dirent_dir_get (dirent_resources_t *res, int index)
{
	qfZoneScoped (true);
	return PR_RESGET (res->dir_map, index);
}

static inline int __attribute__((pure))
dirent_dir_index (dirent_resources_t *res, bi_dir_t *dir)
{
	return PR_RESINDEX (res->dir_map, dir);
}

static void
bi_dirent_clear (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	dirent_resources_t *res = _res;

	for (auto dir = res->dirs; dir; dir = dir->next) {
		closedir (dir->dir);
	}
	dirent_dir_reset (res);
}

static void
bi_dirent_destroy (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	dirent_resources_t *res = _res;
	PR_RESDELMAP (res->dir_map);
	free (res);
}

#define bi(x) static void bi_##x (progs_t *pr, void *_res)

bi(opendir)
{
	auto res = (dirent_resources_t *) _res;
	R_INT (pr) = 0;

	DIR *dir = opendir (P_GSTRING (pr, 0));

	if (dir) {
		auto bi_dir = dirent_dir_new (res);
		*bi_dir = (bi_dir_t) {
			.next = res->dirs,
			.prev = &res->dirs,
			.dir = dir,
		};
		if (res->dirs) {
			res->dirs->prev = &bi_dir->next;
		}
		res->dirs = bi_dir;

		R_INT (pr) = dirent_dir_index (res, bi_dir);
	}
}

static bi_dir_t *__attribute__((pure))
_get_dir (dirent_resources_t *res, const char *name, int index)
{
	qfZoneScoped (true);
	auto bi_dir = dirent_dir_get (res, index);

	if (!bi_dir) {
		PR_RunError (res->pr, "invalid DIR index passed to %s", name + 3);
	}
	return bi_dir;
}
#define get_dir(index) _get_dir(res,__FUNCTION__,index)

bi(closedir)
{
	qfZoneScoped (true);
	auto res = (dirent_resources_t *) _res;
	auto bi_dir = get_dir (P_INT (pr, 0));
	closedir (bi_dir->dir);
	*bi_dir->prev = bi_dir->next;
	if (bi_dir->next) {
		bi_dir->next->prev = bi_dir->prev;
	}
	dirent_dir_free (res, bi_dir);
}

bi(readdir)
{
	qfZoneScoped (true);
	auto res = (dirent_resources_t *) _res;
	auto bi_dir = get_dir (P_INT (pr, 0));

	errno = 0;
	auto dirent = readdir (bi_dir->dir);
	if (!dirent) {
		R_PACKED (pr, dirinfo_t) = (dirinfo_t) {
			.err = errno,
		};
	} else {
		int type = 0; // assume regular file
		if (dirent->d_type == DT_DIR) {
			type = 1;
		} else if (dirent->d_type != DT_REG && dirent->d_type != DT_UNKNOWN) {
			// cowardly avoid dealing with anything but regular and unknown
			// (assumed to be regular)
			type = 2;
		}

		R_PACKED (pr, dirinfo_t) = (dirinfo_t) {
			.name = PR_SetTempString (pr, dirent->d_name),
			.type = type,
		};
	}
}

#undef bi
#define bi(x,np,params...) {#x, RUA_Secured, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t secure_builtins[] = {
	bi(opendir,    1, p(string)),
	bi(closedir,   1, p(int)),
	bi(readdir,    1, p(int)),
	{0}
};

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
static builtin_t insecure_builtins[] = {
	bi(opendir,    1, p(string)),
	bi(closedir,   1, p(int)),
	bi(readdir,    1, p(int)),
	{0}
};

void
RUA_Dirent_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	dirent_resources_t *res = calloc (1, sizeof (dirent_resources_t));

	res->pr = pr;
	PR_Resources_Register (pr, "Dirent", res, bi_dirent_clear,
						   bi_dirent_destroy);
	if (secure) {
		PR_RegisterBuiltins (pr, secure_builtins, res);
	} else {
		PR_RegisterBuiltins (pr, insecure_builtins, res);
	}
}
