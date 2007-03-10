/*
	ver_check.c

	Version number comparisons

	Copyright (C) 1995 Ian Jackson <iwj10@cus.cam.ac.uk>
	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include <ctype.h>
#include <stdlib.h>

#include "QF/qtypes.h"
#include "QF/ver_check.h"


/*
	ver_compare

	Compare two ASCII version strings. If the first is greater than the second,
	return a positive number. If the second is greater, return a negative. If
	they are equal, return zero.
*/
VISIBLE int
ver_compare (const char *value, const char *reference)
{
	const char *valptr, *refptr;
	int         vc, rc;
	long        vl, rl;

	if (!value)
		value = "";
	if (!reference)
		reference = "";

	for (;;) {
		valptr = value;
		// Scan past any non-digits
		while (*valptr && !isdigit ((byte) *valptr))
			valptr++;

		refptr = reference;
		// get past non-digits
		while (*refptr && !isdigit ((byte) *refptr))
			refptr++;

		for (;;) {
			vc = (value == valptr) ? 0 : *value++;
			rc = (reference == refptr) ? 0 : *reference++;

			if ((!vc) && (!rc))
				break;

			if (vc && !isalpha (vc))
				vc += 256;				// ASCII charset
			if (rc && !isalpha (rc))
				rc += 256;

			if (vc != rc)
				return (vc - rc);
		}

		value = valptr;
		reference = refptr;

		vl = rl = 0;

		if (isdigit ((byte) *valptr))
			vl = strtol (value, (char **) &value, 10);

		if (isdigit ((byte) *refptr))
			rl = strtol (reference, (char **) &reference, 10);

		if (vl != rl)
			return (vl - rl);

		if ((!*value) && (!*reference))
			return 0;

		if (!*value)
			return -1;

		if (!*reference)
			return 1;
	}
}
