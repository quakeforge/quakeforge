/*
	threads.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/qtypes.h"
#include "QF/qendian.h"

#include "tools/qflight/include/light.h"
#include "tools/qflight/include/options.h"
#include "tools/qflight/include/threads.h"

#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
pthread_mutex_t *my_mutex;
#endif

void
InitThreads (void)
{
#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
	if (options.threads > 1) {
		pthread_mutexattr_t mattrib;

		my_mutex = malloc (sizeof (*my_mutex));
		if (pthread_mutexattr_init (&mattrib) == -1)
			fprintf (stderr, "pthread_mutex_attr_init failed");
	//	if (pthread_mutexattr_setkind_np (&mattrib, MUTEX_FAST_NP) == -1)
	//		fprintf (stderr, "pthread_mutexattr_setkind_np failed");
		if (pthread_mutex_init (my_mutex, &mattrib) == -1)
			fprintf (stderr, "pthread_mutex_init failed");
	}
#endif
}

void
RunThreadsOn (threadfunc_t *func)
{
#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
	if (options.threads > 1) {
		pthread_t work_threads[256];
		void *status;
		pthread_attr_t attrib;
		lightinfo_t *l[256];
		long        i;

		if (pthread_attr_init (&attrib) == -1)
			fprintf (stderr, "pthread_attr_init failed");
		if (pthread_attr_setstacksize (&attrib, 0x100000) == -1)
			fprintf (stderr, "pthread_attr_setstacksize failed");

		for (i = 0; i < options.threads; i++) {
			l[i] = malloc (sizeof (lightinfo_t));
			if (pthread_create (&work_threads[i], &attrib, func, l[i]) == -1)
				fprintf (stderr, "pthread_create failed");
		}

		for (i = 0; i < options.threads; i++) {
			if (pthread_join (work_threads[i], &status) == -1)
				fprintf (stderr, "pthread_join failed");
			free (l[i]);
		}
	} else
#endif
	{
		lightinfo_t *l = malloc (sizeof (lightinfo_t));
		func (l);
	}
}
