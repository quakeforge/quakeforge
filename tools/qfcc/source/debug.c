#include "qfcc.h"

static int  auxfunctions_size;
int         num_auxfunctions;
pr_auxfunction_t *auxfunctions;

static int  linenos_size;
int         num_linenos;
pr_lineno_t *linenos;

static int  locals_size;
int         num_locals;
ddef_t      *locals;

pr_auxfunction_t *
new_auxfunction (void)
{
	if (num_auxfunctions == auxfunctions_size) {
		auxfunctions_size += 1024;
		auxfunctions = realloc (auxfunctions,
								auxfunctions_size * sizeof (pr_auxfunction_t));
	}
	memset (&auxfunctions[num_auxfunctions], 0,
			sizeof (linenos[num_auxfunctions]));
	return &auxfunctions[num_auxfunctions++];
}

pr_lineno_t *
new_lineno (void)
{
	if (num_linenos == linenos_size) {
		linenos_size += 1024;
		linenos = realloc (linenos, linenos_size * sizeof (pr_lineno_t));
	}
	memset (&linenos[num_linenos], 0, sizeof (linenos[num_linenos]));
	return &linenos[num_linenos++];
}

ddef_t *
new_local (void)
{
	if (num_locals == locals_size) {
		locals_size += 1024;
		locals = realloc (locals, locals_size * sizeof (ddef_t));
	}
	memset (&locals[num_locals], 0, sizeof (locals[num_locals]));
	return &locals[num_locals++];
}
