#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <QF/progs.h>
#include <QF/zone.h>

#define RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].int_var = EDICT_TO_PROG(p, e))
#define RETURN_STRING(p, s) ((p)->pr_globals[OFS_RETURN].int_var = PR_SetString((p), s))

float *read_result;	//FIXME: eww

static void 
bi_fixme (progs_t *pr)
{
	PR_Error (pr, "unimplemented function\n");
}

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
	G_FLOAT (pr, OFS_RETURN) = errno;
}

static void
bi_strerror (progs_t *pr)	// FIXME: make INT when qc int
{
	int err = G_FLOAT (pr, OFS_PARM0);
	RETURN_STRING (pr, strerror (err));
}

static void
bi_open (progs_t *pr)	// FIXME: make INT when qc int
{
	char *path = G_STRING (pr, OFS_PARM0);
	int flags = G_FLOAT (pr, OFS_PARM1);
	int mode = G_FLOAT (pr, OFS_PARM2);
	G_INT (pr, OFS_RETURN) = open (path, flags, mode);
}

static void
bi_close (progs_t *pr)	// FIXME: make INT when qc int
{
	int handle = G_INT (pr, OFS_PARM0);
	G_FLOAT (pr, OFS_RETURN) = close (handle);
}

static void
bi_read (progs_t *pr)	// FIXME: make INT when qc int
{
	int handle = G_INT (pr, OFS_PARM0);
	int count = G_FLOAT (pr, OFS_PARM1);
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
bi_write (progs_t *pr)	// FIXME: make INT when qc int
{
	int handle = G_INT (pr, OFS_PARM0);
	char *buffer = G_STRING (pr, OFS_PARM1);
	int count = G_FLOAT (pr, OFS_PARM2);

	G_FLOAT (pr, OFS_RETURN) = write (handle, buffer, count);
}

static void
bi_seek (progs_t *pr)	// FIXME: make INT when qc int
{
	int handle = G_INT (pr, OFS_PARM0);
	int pos = G_FLOAT (pr, OFS_PARM1);
	int whence = G_FLOAT (pr, OFS_PARM2);

	G_FLOAT (pr, OFS_RETURN) = lseek (handle, pos, whence);
}

builtin_t   builtins[] = {
	bi_fixme,
	bi_print,
	bi_GarbageCollect,
	bi_errno,
	bi_strerror,
	bi_open,
	bi_close,
	bi_read,
	bi_write,
	bi_seek,
};

void
BI_Init (progs_t *progs)
{
	progs->builtins = builtins;
	progs->numbuiltins = sizeof (builtins) / sizeof (builtins[0]);
}
