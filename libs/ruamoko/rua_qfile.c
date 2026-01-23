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

#define _GNU_SOURCE		//FIXME should be elsewhere for portability

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
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

#ifdef _WIN32
#define PIPE_SIZE 65536		// default size on linux
#endif

typedef struct {
	int         pid;
	int         pipein;		// from the child's perspective
	int         pipeout;	// from the child's perspective
	int         pipeerr;	// from the child's perspective
} qpipe_t;

typedef struct qfile_s {
	struct qfile_s *next;
	struct qfile_s **prev;
	QFile      *file;
} qfile_t;

typedef struct {
	PR_RESMAP (qfile_t) handle_map;
	qfile_t    *handles;
	dstring_t  *buffer;
	progs_t    *pr;
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

#define bi(x) static void bi_##x (progs_t *pr, void *_res)

bi (qfile_clear)
{
	qfZoneScoped (true);
	qfile_resources_t *res = (qfile_resources_t *) _res;
	qfile_t    *handle;

	for (handle = res->handles; handle; handle = handle->next)
		Qclose (handle->file);
	res->handles = 0;
	handle_reset (res);
}

bi (qfile_destroy)
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

static void
close_handle (qfile_resources_t *res, int handle)
{
	qfile_t    *h = handle_get (res, handle);

	if (!h)
		PR_RunError (res->pr, "invalid file handle passed to Qclose");
	Qclose (h->file);
	*h->prev = h->next;
	if (h->next)
		h->next->prev = h->prev;
	handle_free (res, h);
}

int
QFile_AllocHandle (progs_t *pr, QFile *file)
{
	qfZoneScoped (true);
	qfile_resources_t *res = PR_Resources_Find (pr, "QFile");

	return alloc_handle (res, file);
}

bi (Qrename)
{
	qfZoneScoped (true);
	const char *old = P_GSTRING (pr, 0);
	const char *new = P_GSTRING (pr, 1);

	R_INT (pr) = Qrename (old, new);
}

bi (Qremove)
{
	qfZoneScoped (true);
	const char *path = P_GSTRING (pr, 0);

	R_INT (pr) = Qremove (path);
}

bi (Qopen)
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

static int
qf_pipe (int pipefd[2])
{
#ifdef _WIN32
	return _pipe (pipefd, PIPE_SIZE, _O_NOINHERIT|_O_BINARY);
#else
	return pipe2 (pipefd, O_CLOEXEC);
#endif
}

bi (Qpipe)
{
	qfZoneScoped (true);
	auto res = (qfile_resources_t *) _res;
	auto arg_list = &P_STRUCT (pr, pr_string_t, 0);
	int  argc = P_INT (pr, 1);
	int  do_stdin = P_INT (pr, 2);
	int  do_stdout = P_INT (pr, 3);
	int  do_stderr = P_INT (pr, 4);

	char *argv[argc + 1];
	for (int i = 0; i < argc; i++) {
		argv[i] = (char *) PR_GetString (pr, arg_list[i]);
	}
	argv[argc] = nullptr;

	//FIXME should probably be elsewhere for portability
	int pipefd_stdin[2] = {-1, -1};
	int pipefd_stdout[2] = {-1, -1};
	int pipefd_stderr[2] = {-1, -1};
	if (do_stdin) {
		if (qf_pipe (pipefd_stdin) == -1) {
			goto pipe_error;
		}
	}
	if (do_stdout) {
		if (qf_pipe (pipefd_stdout) == -1) {
			goto pipe_error;
		}
	}
	if (do_stderr) {
		if (qf_pipe (pipefd_stderr) == -1) {
			goto pipe_error;
		}
	}

	auto ret = &R_PACKED (pr, qpipe_t);
	*ret = (qpipe_t) {};
	if (do_stdin) {
		auto qf = Qdopen (pipefd_stdin[1], "wb");
		if (!(ret->pipein = alloc_handle (res, qf))) {
			Qclose (qf);
			goto fd_error;
		}
	}
	if (do_stdout) {
		auto qf = Qdopen (pipefd_stdout[0], "rb");
		if (!(ret->pipeout = alloc_handle (res, qf))) {
			Qclose (qf);
			goto fd_error;
		}
	}
	if (do_stderr) {
		auto qf = Qdopen (pipefd_stderr[0], "wb");
		if (!(ret->pipeerr = alloc_handle (res, qf))) {
			Qclose (qf);
			goto fd_error;
		}
	}

#ifdef _WIN32
	int saved_fd[3] = {};
	if (do_stdin) {
		saved_fd[0] = dup (STDIN_FILENO);
		dup2 (pipefd_stdin[0], STDIN_FILENO);
		close (pipefd_stdin[0]);
	}
	if (do_stdout) {
		saved_fd[1] = dup (STDOUT_FILENO);
		dup2 (pipefd_stdout[1], STDOUT_FILENO);
		close (pipefd_stdout[1]);
	}
	if (do_stderr) {
		saved_fd[2] = dup (STDERR_FILENO);
		dup2 (pipefd_stderr[1], STDERR_FILENO);
		close (pipefd_stderr[1]);
	}
	pid_t child_pid = _spawnvp (_P_NOWAIT, argv[0], (const char **)argv);
	if (do_stdin) {
		dup2 (saved_fd[0], STDIN_FILENO);
		close (saved_fd[0]);
	}
	if (do_stdout) {
		dup2 (saved_fd[1], STDOUT_FILENO);
		close (saved_fd[1]);
	}
	if (do_stderr) {
		dup2 (saved_fd[2], STDERR_FILENO);
		close (saved_fd[2]);
	}
#else
	pid_t child_pid = fork ();
	if (child_pid == -1) {
		goto pipe_error;
	}
	if (child_pid == 0) {
		if (do_stdin) {
			dup2 (pipefd_stdin[0], STDIN_FILENO);
		}
		if (do_stdout) {
			dup2 (pipefd_stdout[1], STDOUT_FILENO);
		}
		if (do_stderr) {
			dup2 (pipefd_stderr[1], STDERR_FILENO);
		}
		close_range (3, ~0u, CLOSE_RANGE_UNSHARE);
		execvp (argv[0], argv);
		exit (errno);
	} else {
		close (pipefd_stdin[0]);
		close (pipefd_stdout[1]);
		close (pipefd_stderr[1]);
	}
#endif
	ret->pid = child_pid;
	return;
fd_error:
	if (ret->pipein) {
		close_handle (res, ret->pipein);
	}
	if (ret->pipeout) {
		close_handle (res, ret->pipeout);
	}
	if (ret->pipeerr) {
		close_handle (res, ret->pipeerr);
	}
	*ret = (qpipe_t) {};
	return;
pipe_error:
	close (pipefd_stdin[0]);
	close (pipefd_stdin[1]);
	close (pipefd_stdout[0]);
	close (pipefd_stdout[1]);
	close (pipefd_stderr[0]);
	close (pipefd_stderr[1]);
	R_var (pr, ivec4) = (pr_ivec4_t) {};
	return;
}

bi (Qwait)
{
	int pid = P_INT (pr, 0);
#ifdef _WIN32
	_cwait (&R_INT (pr), pid, 0);
#else
	waitpid (pid, &R_INT (pr), 0);
#endif
}

bi (Qgetcwd)
{
	auto res = (qfile_resources_t *) _res;
	if (!(res->buffer->size = res->buffer->truesize)) {
		res->buffer->size = 1024;
		dstring_adjust (res->buffer);
	}
	char *s;
	while (!(s = getcwd (res->buffer->str, res->buffer->size))) {
		res->buffer->size += 1024;
		dstring_adjust (res->buffer);
	}
	RETURN_STRING (pr, s);
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

bi (Qclose)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	close_handle (res, handle);
}

bi (Qgetline)
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

bi (Qreadstring)
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

bi (Qread)
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

bi (Qwrite)
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

bi (Qputs)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	const char *str = P_GSTRING (pr, 1);

	R_INT (pr) = Qputs (h->file, str);
}
#if 0
bi (Qgets)
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
bi (Qgetc)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qgetc (h->file);
}

bi (Qputc)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	int         c = P_INT (pr, 1);

	R_INT (pr) = Qputc (h->file, c);
}

bi (Qseek)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);
	int         offset = P_INT (pr, 1);
	int         whence = P_INT (pr, 2);

	R_INT (pr) = Qseek (h->file, offset, whence);
}

bi (Qtell)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qtell (h->file);
}

bi (Qflush)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qflush (h->file);
}

bi (Qeof)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qeof (h->file);
}

bi (Qfilesize)
{
	qfZoneScoped (true);
	__auto_type res = (qfile_resources_t *) _res;
	int         handle = P_INT (pr, 0);
	qfile_t    *h = get_handle (pr, res, __FUNCTION__, handle);

	R_INT (pr) = Qfilesize (h->file);
}

#undef bi
#define bi(x,np,params...) {#x, RUA_Secured, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t secure_builtins[] = {
	bi(Qrename, 2, p(string), p(string)),
	bi(Qremove, 1, p(string)),
	bi(Qopen,   2, p(string), p(string)),
	bi(Qpipe,   5, p(ptr), p(int), p(int), p(int), p(int)),
	bi(Qwait,   1, p(int)),
	bi(Qgetcwd, 0),
	{0}
};

#undef bi
#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
static builtin_t insecure_builtins[] = {
	bi(Qrename, 2, p(string), p(string)),
	bi(Qremove, 1, p(string)),
	bi(Qopen,   2, p(string), p(string)),
	bi(Qpipe,   5, p(ptr), p(int), p(int), p(int), p(int)),
	bi(Qwait,   1, p(int)),
	bi(Qgetcwd, 0),
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
	res->pr = pr;

	PR_Resources_Register (pr, "QFile", res, bi_qfile_clear, bi_qfile_destroy);
	if (secure) {
		PR_RegisterBuiltins (pr, secure_builtins, res);
	} else {
		PR_RegisterBuiltins (pr, insecure_builtins, res);
	}
	PR_RegisterBuiltins (pr, builtins, res);
}
