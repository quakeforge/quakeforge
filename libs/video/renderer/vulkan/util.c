/*
	util.c

	Copyright (C) 2019      Bill Currie <bill@taniwha.org>

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

#include "util.h"

int
count_strings (const char **str)
{
	int         count = 0;

	if (str) {
		while (*str++) {
			count++;
		}
	}
	return count;
}

void
merge_strings (const char **out, const char **in1, const char **in2)
{
	if (in1) {
		while (*in1) {
			*out++ = *in1++;
		}
	}
	if (in2) {
		while (*in2) {
			*out++ = *in2++;
		}
	}
}

void
prune_strings (const char * const *reference, const char **strings,
			   uint32_t *count)
{
	for (int i = *count; i-- > 0; ) {
		const char *str = strings[i];
		const char * const *ref;
		for (ref = reference; *ref; ref++) {
			if (!strcmp (*ref, str)) {
				break;
			}
		}
		if (!*ref) {
			memmove (strings + i, strings + i + 1,
					 (--(*count) - i) * sizeof (const char **));
		}
	}
}
