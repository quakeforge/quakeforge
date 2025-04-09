/*
	bi_file.c

	CSQC file builtins

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

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/quakeio.h"

#include "rua_internal.h"

typedef struct qfile_s {
	struct qfile_s *next;
	struct qfile_s **prev;
	QFile      *file;
} qfile_t;

typedef struct {
	PR_RESMAP (qfile_t) handle_map;
	qfile_t    *handles;
	dstring_t  *buffer;
} qfile_resources_t;

static qfile_t *
handle_new (qfile_resources_t *res)
{
	qfZoneScoped (true);
	return PR_RESNEW (res->handle_map);
}

static void
handle_free (qfile_resources_t *res, qfile_t *handle)
{
	qfZoneScoped (true);
	PR_RESFREE (res->handle_map, handle);
}

static void
handle_reset (qfile_resources_t *res)
{
	qfZoneScoped (true);
	PR_RESRESET (res->handle_map);
}

static inline qfile_t *
handle_get (qfile_resources_t *res, int index)
{
	qfZoneScoped (true);
	return PR_RESGET(res->handle_map, index);
}

static inline int __attribute__((pure))
handle_index (qfile_resources_t *res, qfile_t *handle)
{
	qfZoneScoped (true);
	return PR_RESINDEX(res->handle_map, handle);
}

static void
bi_qfile_clear (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	qfile_resources_t *res = (qfile_resources_t *) _res;
	qfile_t    *handle;

	for (handle = res->handles; handle; handle = handle->next)
		Qclose (handle->file);
	res->handles = 0;
	handle_reset (res);
}

static void
bi_qfile_destroy (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	qfile_resources_t *res = _res;

	PR_RESDELMAP (res->handle_map);
	dstring_delete (res->buffer);

	free (res);
}

static int
alloc_handle (qfile_resources_t *res, QFile *file)
{
	qfZoneScoped (true);
	qfile_t    *handle = handle_new (res);

	if (!handle)
		return 0;

	handle->next = res->handles;
	handle->prev = &res->handles;
	if (res->handles)
		res->handles->prev = &handle->next;
	res->handles = handle;
	handle->file = file;
	return handle_index (res, handle);
}

int
QFile_AllocHandle (progs_t *pr, QFile *file)
{
	qfZoneScoped (true);
	qfile_resources_t *res = PR_Resources_Find (pr, "QFile");

	return alloc_handle (res, file);
}

static void
bi_Qrename (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	const char *old = P_GSTRING (pr, 0);
	const char *new = P_GSTRING (pr, 1);

	R_INT (pr) = Qrename (old, new);
}

static void
bi_Qremove (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	const char *path = P_GSTRING (pr, 0);

	R_INT (pr) = Qremove (path);
}

static void
bi_Qopen (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	const char *path = P_GSTRING (pr, 0);
	const char *mode = P_GSTRING (pr, 1);
	QFile      *file;

	R_INT (pr) = 0;
	if (!(file = Qopen (path, mode)))
		return;
	if (!(R_INT (pr) = alloc_handle (res, file)))
		Qclose (file);
}

static qfile_t * __attribute__((pure))
get_handle (progs_t *pr, qfile_resources_t *res, const char *name, int handle)
{
	qfZoneScoped (true);
	qfile_t    *h = handle_get (res, handle);

	if (!h)
		PR_RunError (pr, "invalid file handle passed to %s", name + 3);
	return h;
}

QFile *
QFile_GetFile (progs_t *pr, int handle)
{
	qfZoneScoped (true);
	qfile_resources_t *res = PR_Resources_Find (pr, "QFile");
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	return h->file;
}

static void
bi_Qclose (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = handle_get (res, handle);

	if (!h)
		PR_RunError (pr, "invalid file handle passed to Qclose");
	Qclose (h->file);
	*h->prev = h->next;
	if (h->next)
		h->next->prev = h->prev;
	handle_free (res, h);
}

static void
bi_Qgetline (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	const char *s;

	s = Qgetline (h->file, res->buffer);
	if (s)
		RETURN_STRING (pr, s);
	else
		R_STRING (pr) = 0;
}

static void
bi_Qreadstring (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	int         len = P_INT (pr, 1);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	pr_string_t str = PR_NewMutableString (pr);
	dstring_t  *dstr = PR_GetMutableString (pr, str);

	dstr->size = len + 1;
	dstring_adjust (dstr);
	len = Qread (h->file, dstr->str, len);
	dstr->size = len + 1;
	dstr->str[len] = 0;
	R_STRING (pr) = str;
}

static void
check_buffer (progs_t *pr, pr_type_t *buf, int count, const char *name)
{
	qfZoneScoped (true);
	int         len;

	len = (count + sizeof (pr_type_t) - 1) / sizeof (pr_type_t);
	if (buf < pr->pr_globals || buf + len > pr->pr_globals + pr->globals_size)
		PR_RunError (pr, "%s: bad buffer", name);
}

static void
bi_Qread (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	pr_type_t  *buf = P_GPOINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "Qread");
	R_INT (pr) = Qread (h->file, buf, count);
}

static void
bi_Qwrite (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	pr_type_t  *buf = P_GPOINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "Qwrite");
	R_INT (pr) = Qwrite (h->file, buf, count);
}

static void
bi_Qputs (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	const char *str = P_GSTRING (pr, 1);

	R_INT (pr) = Qputs (h->file, str);
}
#if 0
static void
bi_Qgets (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	pr_type_t  *buf = P_GPOINTER (pr, 1);
	int         count = P_INT (pr, 2);

	check_buffer (pr, buf, count, "Qgets");
	RETURN_POINTER (pr, Qgets (h->file, (char *) buf, count));
}
#endif
static void
bi_Qgetc (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qgetc (h->file);
}

static void
bi_Qputc (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	int         c = P_INT (pr, 1);

	R_INT (pr) = Qputc (h->file, c);
}

static void
bi_Qseek (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	int         offset = P_INT (pr, 1);
	int         whence = P_INT (pr, 2);

	R_INT (pr) = Qseek (h->file, offset, whence);
}

static void
bi_Qtell (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qtell (h->file);
}

static void
bi_Qflush (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qflush (h->file);
}

static void
bi_Qeof (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qeof (h->file);
}

static void
bi_Qfilesize (progs_t *pr, void *_res)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qfilesize (h->file);
}

#define bi(x,np,params...) {#x, RUA_Secured, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t secure_builtins[] = {
	bi(Qrename, 2, p(string), p(string)),
	bi(Qremove, 1, p(string)),
	bi(Qopen,   2, p(string), p(string)),
	{0}
};

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
static builtin_t insecure_builtins[] = {
	bi(Qrename, 2, p(string), p(string)),
	bi(Qremove, 1, p(string)),
	bi(Qopen,   2, p(string), p(string)),
	{0}
};

static builtin_t builtins[] = {
	bi(Qclose,      1, p(ptr)),
	bi(Qgetline,    1, p(ptr)),
	bi(Qreadstring, 2, p(ptr), p(int)),
	bi(Qread,       3, p(ptr), p(ptr), p(int)),
	bi(Qwrite,      3, p(ptr), p(ptr), p(int)),
	bi(Qputs,       2, p(ptr), p(string)),
//	bi(Qgets,       _, _),
	bi(Qgetc,       1, p(ptr)),
	bi(Qputc,       2, p(ptr), p(int)),
	bi(Qseek,       3, p(ptr), p(int), p(int)),
	bi(Qtell,       1, p(ptr)),
	bi(Qflush,      1, p(ptr)),
	bi(Qeof,        1, p(ptr)),
	bi(Qfilesize,   1, p(ptr)),
	{0}
};

void
RUA_QFile_Init (progs_t *pr, int secure)
{
	qfZoneScoped (true);
	qfile_resources_t *res = calloc (sizeof (qfile_resources_t), 1);
	res->buffer = dstring_new ();

	PR_Resources_Register (pr, "QFile", res, bi_qfile_clear, bi_qfile_destroy);
	if (secure) {
		PR_RegisterBuiltins (pr, secure_builtins, res);
	} else {
		PR_RegisterBuiltins (pr, insecure_builtins, res);
	}
	PR_RegisterBuiltins (pr, builtins, res);
}
