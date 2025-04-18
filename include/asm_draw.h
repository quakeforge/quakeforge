/*
	asm_draw.h

	Assembler drawing routines.

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

//
// !!! note that this file must match the corresponding C structures at all
// times !!!
//

// !!! if this is changed, it must be changed in r_local.h too !!!
#define	NEAR_CLIP	0.01

// !!! if this is changed, it must be changed in r_local.h too !!!
#define	CYCLE	128

// espan_t structure
// !!! if this is changed, it must be changed in r_shared.h too !!!
#define espan_t_u    	0
#define espan_t_v	    4
#define espan_t_count   8
#define espan_t_pnext	12
#define espan_t_size    16	// sizeof(espan_t)

// sspan_t structure
// !!! if this is changed, it must be changed in d_local.h too !!!
#define sspan_t_u    	0
#define sspan_t_v	    4
#define sspan_t_count   8
#define sspan_t_size    12	// sizeof(sspan_t)

// spanpackage_t structure
// !!! if this is changed, it must be changed in d_polyset.c too !!!
#define spanpackage_t_pdest				0
#define spanpackage_t_pz				4
#define spanpackage_t_count				8
#define spanpackage_t_ptex				12
#define spanpackage_t_sfrac				16
#define spanpackage_t_tfrac				20
#define spanpackage_t_light				24
#define spanpackage_t_zi				28
#define spanpackage_t_size				32	// sizeof(spanpackage_t)

// edge_t structure
// !!! if this is changed, it must be changed in r_shared.h too !!!
#define et_u			0
#define et_u_step		4
#define et_prev			8
#define et_next			12
#define et_surfs		16
#define et_nextremove	20
#define et_nearzi		24
#define et_owner		28
#define et_size			32	// sizeof(edge_t)

// surf_t structure
// !!! if this is changed, it must be changed in r_shared.h too !!!
#define SURF_T_SHIFT	6
#define st_next			0
#define st_prev			4
#define st_spans		8
#define st_key			12
#define st_last_u		16
#define st_spanstate	20
#define st_flags		24
#define st_data			28
#define st_entity		32
#define st_nearzi		36
#define st_insubmodel	40
#define st_d_ziorigin	44
#define st_d_zistepu	48
#define st_d_zistepv	52
#define st_pad			56
#define st_size			64	// sizeof (surf_t)

// clipplane_t structure
// !!! if this is changed, it must be changed in r_local.h too !!!
#define cp_normal		0
#define cp_dist			12
#define cp_next			16
#define cp_leftedge		20
#define cp_rightedge	21
#define cp_reserved		22
#define cp_size			24	// sizeof (clipplane_t)

// medge_t structure
// !!! if this is changed, it must be changed in model.h too !!!
#define me_v				0
#define me_cachededgeoffset	8
#define me_size				12	// sizeof (medge_t)

// mvertex_t structure
// !!! if this is changed, it must be changed in model.h too !!!
#define mv_position		0
#define mv_size			12	// sizeof (mvertex_t)

// refdef_t structure
// !!! if this is changed, it must be changed in render.h too !!!
#define rd_fvrectx_adj				0
#define rd_fvrecty_adj				4
#define rd_fvrectright_adj			8
#define rd_fvrectbottom_adj			12
#define rd_aliasvrectleft			16
#define rd_aliasvrecttop			20
#define rd_aliasvrectright			24
#define rd_aliasvrectbottom			28
#define rd_vrectright				32
#define rd_vrectbottom				36
#define rd_vrectx_adj_shift20		40
#define rd_vrectright_adj_shift20	44
#define rd_fvrectx					48
#define rd_fvrecty					52
#define rd_fvrectright				56
#define rd_fvrectbottom				60
#define rd_size						64	// sizeof (refdef_t) FIXME make true

// dtriangle_t structure
// !!! if this is changed, it must be changed in model.h too !!!
#define dtri_facesfront		0
#define dtri_vertindex		4
#define dtri_size			16	// !!! if this changes, array indexing in !!!
								// !!! d_polysa.s must be changed to match !!!
#define dtri_shift			4

