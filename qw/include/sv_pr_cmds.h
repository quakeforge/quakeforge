/*
	sv_pr_cmds.h

	server side QuakeC builtins

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

	$Id$
*/

#ifndef __sv_pr_cmds_h
#define __sv_pr_cmds_h

void PF_error (progs_t *pr);
void PF_objerror (progs_t *pr);
void PF_makevectors (progs_t *pr);
void PF_setorigin (progs_t *pr);
void PF_setsize (progs_t *pr);
void PF_setmodel (progs_t *pr);
void PF_bprint (progs_t *pr);
void PF_sprint (progs_t *pr);
void PF_centerprint (progs_t *pr);
void PF_normalize (progs_t *pr);
void PF_vlen (progs_t *pr);
void PF_vectoyaw (progs_t *pr);
void PF_vectoangles (progs_t *pr);
void PF_random (progs_t *pr);
void PF_ambientsound (progs_t *pr);
void PF_sound (progs_t *pr);
void PF_break (progs_t *pr);
void PF_traceline (progs_t *pr);
void PF_checkpos (progs_t *pr);
int PF_newcheckclient (progs_t *pr, int check);
void PF_checkclient (progs_t *pr);
void PF_stuffcmd (progs_t *pr);
void PF_localcmd (progs_t *pr);
void PF_cvar (progs_t *pr);
void PF_cvar_set (progs_t *pr);
void PF_findradius (progs_t *pr);
void PF_dprint (progs_t *pr);
void PF_ftos (progs_t *pr);
void PF_fabs (progs_t *pr);
void PF_vtos (progs_t *pr);
void PF_Spawn (progs_t *pr);
void PF_Remove (progs_t *pr);
void PF_Find (progs_t *pr);
void PR_CheckEmptyString (progs_t *pr, const char *s);
void PF_precache_file (progs_t *pr);
void PF_precache_sound (progs_t *pr);
void PF_precache_model (progs_t *pr);
void PF_coredump (progs_t *pr);
void PF_traceon (progs_t *pr);
void PF_traceoff (progs_t *pr);
void PF_eprint (progs_t *pr);
void PF_walkmove (progs_t *pr);
void PF_droptofloor (progs_t *pr);
void PF_lightstyle (progs_t *pr);
void PF_rint (progs_t *pr);
void PF_floor (progs_t *pr);
void PF_ceil (progs_t *pr);
void PF_checkbottom (progs_t *pr);
void PF_pointcontents (progs_t *pr);
void PF_nextent (progs_t *pr);
void PF_aim (progs_t *pr);
void PF_changeyaw (progs_t *pr);
void PF_WriteBytes (progs_t *pr);
void PF_WriteByte (progs_t *pr);
void PF_WriteChar (progs_t *pr);
void PF_WriteShort (progs_t *pr);
void PF_WriteLong (progs_t *pr);
void PF_WriteAngle (progs_t *pr);
void PF_WriteCoord (progs_t *pr);
void PF_WriteAngleV (progs_t *pr);
void PF_WriteCoordV (progs_t *pr);
void PF_WriteString (progs_t *pr);
void PF_WriteEntity (progs_t *pr);
void PF_makestatic (progs_t *pr);
void PF_setspawnparms (progs_t *pr);
void PF_changelevel (progs_t *pr);
void PF_logfrag (progs_t *pr);
void PF_infokey (progs_t *pr);
void PF_stof (progs_t *pr);
void PF_multicast (progs_t *pr);
void PF_Fixme (progs_t *pr);

#endif // __sv_pr_cmds_h
