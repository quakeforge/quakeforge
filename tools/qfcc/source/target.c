/*
	target.c

	Shared data stuctures for backend targets.

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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

#include <strings.h>
#include <stdlib.h>

#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/target.h"

target_t current_target;

bool
target_set_backend (const char *tgt)
{
	if (!strcasecmp (tgt, "v6")) {
		current_target = v6_target;
		options.code.progsversion = PROG_ID_VERSION;
		options.code.max_params = PR_MAX_PARAMS;
	} else if (!strcasecmp (tgt, "v6p")) {
		current_target = v6p_target;
		options.code.progsversion = PROG_V6P_VERSION;
		options.code.max_params = PR_MAX_PARAMS;
	} else if (!strcasecmp (tgt, "ruamoko")) {
		current_target = ruamoko_target;
		options.code.progsversion = PROG_VERSION;
		options.code.max_params = -1;
	} else if (!strcasecmp (tgt, "spir-v")) {
		current_target = spirv_target;
		options.code.progsversion = PROG_VERSION;
		options.code.max_params = -1;
		options.code.spirv = true;
		options.code.no_vararg = true;
	} else {
		fprintf (stderr, "unknown target: %s\n", tgt);
		exit (1);
	}
	return true;
}
