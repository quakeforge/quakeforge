/*
	threading.c

	Ruamoko threading support

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/03/01

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

#include <sys/time.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "QF/dstring.h"

#include "ruamoko/qwaq/qwaq.h"
#include "ruamoko/qwaq/threading.h"

#define always_inline inline __attribute__((__always_inline__))

int
qwaq_pipe_acquire_string (qwaq_pipe_t *pipe)
{
	int         string_id = -1;

	pthread_mutex_lock (&pipe->string_id_cond.mut);
	while (RB_DATA_AVAILABLE (pipe->string_ids) < 1) {
		pthread_cond_wait (&pipe->string_id_cond.rcond,
						   &pipe->string_id_cond.mut);
	}
	RB_READ_DATA (pipe->string_ids, &string_id, 1);
	pthread_cond_broadcast (&pipe->string_id_cond.wcond);
	pthread_mutex_unlock (&pipe->string_id_cond.mut);
	return string_id;
}

void
qwaq_pipe_release_string (qwaq_pipe_t *pipe, int string_id)
{
	pthread_mutex_lock (&pipe->string_id_cond.mut);
	while (RB_SPACE_AVAILABLE (pipe->string_ids) < 1) {
		pthread_cond_wait (&pipe->string_id_cond.wcond,
						   &pipe->string_id_cond.mut);
	}
	RB_WRITE_DATA (pipe->string_ids, &string_id, 1);
	pthread_cond_broadcast (&pipe->string_id_cond.rcond);
	pthread_mutex_unlock (&pipe->string_id_cond.mut);
}

void
qwaq_pipe_submit (qwaq_pipe_t *pipe, const int *cmd, unsigned len)
{
	pthread_mutex_lock (&pipe->pipe_cond.mut);
	while (RB_SPACE_AVAILABLE (pipe->pipe) < len) {
		pthread_cond_wait (&pipe->pipe_cond.wcond,
						   &pipe->pipe_cond.mut);
	}
	RB_WRITE_DATA (pipe->pipe, cmd, len);
	pthread_cond_broadcast (&pipe->pipe_cond.rcond);
	pthread_mutex_unlock (&pipe->pipe_cond.mut);
}

void
qwaq_pipe_receive (qwaq_pipe_t *pipe, int *result, int cmd, unsigned len)
{
	//Sys_Printf ("qwaq_wait_result: %d %d\n", cmd, len);
	pthread_mutex_lock (&pipe->pipe_cond.mut);
	while (RB_DATA_AVAILABLE (pipe->pipe) < len
		   || *RB_PEEK_DATA (pipe->pipe, 0) != cmd) {
		pthread_cond_wait (&pipe->pipe_cond.rcond,
						   &pipe->pipe_cond.mut);
	}
	RB_READ_DATA (pipe->pipe, result, len);
	pthread_cond_broadcast (&pipe->pipe_cond.wcond);
	pthread_mutex_unlock (&pipe->pipe_cond.mut);
	//Sys_Printf ("qwaq_wait_result exit: %d %d\n", cmd, len);
}

void
qwaq_init_timeout (struct timespec *timeout, int64_t time)
{
	#define SEC 1000000000L
	struct timeval now;
	gettimeofday(&now, 0);
	timeout->tv_sec = now.tv_sec;
	timeout->tv_nsec = now.tv_usec * 1000L + time;
	if (timeout->tv_nsec >= SEC) {
		timeout->tv_sec += timeout->tv_nsec / SEC;
		timeout->tv_nsec %= SEC;
	}
}

void
qwaq_init_cond (rwcond_t *cond)
{
	pthread_cond_init (&cond->rcond, 0);
	pthread_cond_init (&cond->wcond, 0);
	pthread_mutex_init (&cond->mut, 0);
}

void
qwaq_init_pipe (qwaq_pipe_t *pipe)
{
	qwaq_init_cond (&pipe->pipe_cond);
	qwaq_init_cond (&pipe->string_id_cond);

	for (int i = 0; i < STRING_ID_QUEUE_SIZE - 1; i++) {
		RB_WRITE_DATA (pipe->string_ids, &i, 1);
		pipe->strings[i].mem = &dstring_default_mem;
	}
}
