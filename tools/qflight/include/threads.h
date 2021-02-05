/*
	threads.h

	Thread management

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

#ifndef __threads_h
#define __threads_h

/** \defgroup qflight_threads Light thread handling.
	\ingroup qflight
*/
///@{

#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)

#include <pthread.h>

extern pthread_mutex_t *my_mutex;

#define	LOCK								\
	do {									\
		if (options.threads > 1)			\
			pthread_mutex_lock (my_mutex);	\
	} while (0)

#define	UNLOCK									\
	do {										\
		if (options.threads > 1)				\
			pthread_mutex_unlock (my_mutex);	\
	} while (0)

#else

#define	LOCK
#define	UNLOCK

#endif

extern int numthreads;

typedef void *(threadfunc_t) (void *);

void InitThreads (void);
void RunThreadsOn (threadfunc_t func);

///@}

#endif// __threads_h
