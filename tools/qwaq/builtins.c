#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <QF/progs.h>
#include <QF/zone.h>

#define RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].integer_var = EDICT_TO_PROG(p, e))
#define RETURN_STRING(p, s) ((p)->pr_globals[OFS_RETURN].integer_var = PR_SetString((p), s))

int *read_result;	//FIXME: eww

static void
bi_print (progs_t *pr)
{
	char *str;

	str = G_STRING (pr, (OFS_PARM0));
	fprintf (stdout, "%s", str);
}

static void
bi_GarbageCollect (progs_t *pr)
{
	PR_GarbageCollect (pr);
}

static void
bi_errno (progs_t *pr)
{
	G_INT (pr, OFS_RETURN) = errno;
}

static void
bi_strerror (progs_t *pr)
{
	int err = G_INT (pr, OFS_PARM0);
	RETURN_STRING (pr, strerror (err));
}

static void
bi_open (progs_t *pr)
{
	char *path = G_STRING (pr, OFS_PARM0);
	int flags = G_INT (pr, OFS_PARM1);
	int mode = G_INT (pr, OFS_PARM2);
	G_INT (pr, OFS_RETURN) = open (path, flags, mode);
}

static void
bi_close (progs_t *pr)
{
	int handle = G_INT (pr, OFS_PARM0);
	G_INT (pr, OFS_RETURN) = close (handle);
}

static void
bi_read (progs_t *pr)
{
	int handle = G_INT (pr, OFS_PARM0);
	int count = G_INT (pr, OFS_PARM1);
	int res;
	char *buffer;

	buffer = Hunk_TempAlloc (count);
	if (!buffer)
		PR_Error (pr, "%s: couldn't allocate %d bytes", "bi_read", count);
	res = read (handle, buffer, count);
	if (res != -1)		// FIXME: this just won't work :/
		RETURN_STRING (pr, buffer);
	*read_result = res;
}

static void
bi_write (progs_t *pr)
{
	int handle = G_INT (pr, OFS_PARM0);
	char *buffer = G_STRING (pr, OFS_PARM1);
	int count = G_INT (pr, OFS_PARM2);

	G_INT (pr, OFS_RETURN) = write (handle, buffer, count);
}

static void
bi_seek (progs_t *pr)
{
	int handle = G_INT (pr, OFS_PARM0);
	int pos = G_INT (pr, OFS_PARM1);
	int whence = G_INT (pr, OFS_PARM2);

	G_INT (pr, OFS_RETURN) = lseek (handle, pos, whence);
}

static void
bi_traceon (progs_t *pr)
{
	    pr->pr_trace = true;
}

static void
bi_traceoff (progs_t *pr)
{   
	    pr->pr_trace = false;
}

static void
bi_printf (progs_t *pr)
{
	const char *fmt = G_STRING (pr, OFS_PARM0);
	char c;
	int count = 0;
	float *v;

	while ((c = *fmt++)) {
		if (c == '%' && count < 7) {
			switch (c = *fmt++) {
				case 'i':
					fprintf (stdout, "%i", G_INT (pr, OFS_PARM1 + count++ * 3));
					break;
				case 'f':
					fprintf (stdout, "%f", G_FLOAT (pr, OFS_PARM1 + count++ * 3));
					break;
				case 's':
					fputs (G_STRING (pr, OFS_PARM1 + count++ * 3), stdout);
					break;
				case 'v':
					v = G_VECTOR (pr, OFS_PARM1 + count++ * 3);
					fprintf (stdout, "'%f %f %f'", v[0], v[1], v[2]);
					break;
				default:
					fputc ('%', stdout);
					fputc (c, stdout);
					count = 7;
					break;
			}
		} else {
			fputc (c, stdout);
		}
	}
}

void
BI_Init (progs_t *progs)
{
	progs->builtins = 0;
	progs->numbuiltins = 0;

	PR_AddBuiltin (progs, "print", bi_print, 1);
	PR_AddBuiltin (progs, "GarbageCollect", bi_GarbageCollect, 2);
	PR_AddBuiltin (progs, "errno", bi_errno, 3);
	PR_AddBuiltin (progs, "strerror", bi_strerror, 4);
	PR_AddBuiltin (progs, "open", bi_open, 5);
	PR_AddBuiltin (progs, "close", bi_close, 6);
	PR_AddBuiltin (progs, "read", bi_read, 7);
	PR_AddBuiltin (progs, "write", bi_write, 8);
	PR_AddBuiltin (progs, "seek", bi_seek, 9);
	PR_AddBuiltin (progs, "traceon", bi_traceon, 10);
	PR_AddBuiltin (progs, "traceoff", bi_traceoff, 11);
	PR_AddBuiltin (progs, "printf", bi_printf, 12);
}
