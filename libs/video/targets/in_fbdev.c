/*
	in_fbdev.c

	fix this!

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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"

static int
fd_blocking (int fd, int on)
{
	int x;

#if defined(_POSIX_SOURCE) || !defined(FIONBIO)
# if !defined(O_NONBLOCK)
#  if defined(O_NDELAY)
#   define O_NONBLOCK O_NDELAY
#  endif
# endif
	if ((x = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;
	if (on)
		x &= ~O_NONBLOCK;
	else
		x |= O_NONBLOCK;

	return fcntl(fd, F_SETFL, x);
#else
	x = !on;

	return ioctl(fd, FIONBIO, &x);
#endif
}

static struct termios old_tty, new_tty;
static int tty_fd = 0;

void
IN_LL_Init (void)
{
	fd_blocking(0, 0);
	tcgetattr(tty_fd, &old_tty);
	new_tty = old_tty;
	new_tty.c_cc[VMIN] = 1;
	new_tty.c_cc[VTIME] = 0;
	new_tty.c_lflag &= ~ICANON;
	new_tty.c_iflag &= ~IXON;
	tcsetattr(tty_fd, TCSADRAIN, &new_tty);
}

void
IN_LL_Init_Cvars (void)
{
}

void
IN_LL_Shutdown (void)
{
}

void
IN_LL_ProcessEvents (void)
{
	int k, down;
	char buf[4];

	if (read(0, buf, 1) == 1) {
		k = buf[0];
		switch (k) {
			case '\r':
			case '\n':
				k = QFK_RETURN;
				break;
			case '\033':
				if (read(0, buf, 2) != 2)
					break;
				switch (buf[1]) {
					case 'A':
						k = QFK_UP;
						break;
					case 'B':
						k = QFK_DOWN;
						break;
					case 'C':
						k = QFK_RIGHT;
						break;
					case 'D':
						k = QFK_LEFT;
						break;
				}
				break;
		}
		down = 1;
		Key_Event(k, buf[0], down);
		Key_Event(k, buf[0], !down);
	}
}

void
IN_LL_Grab_Input (int grab)
{
}

void
IN_LL_ClearStates (void)
{
}
