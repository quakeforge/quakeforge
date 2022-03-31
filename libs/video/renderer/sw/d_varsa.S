/*
	d_varsa.S

	(description)

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "asm_i386.h"
#include "quakeasm.h"
#include "asm_draw.h"
#include "d_ifacea.h"

#ifdef PIC
#undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifdef USE_INTEL_ASM

	.data

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: put all refresh variables into one contiguous block. Make into one
// big structure, like cl or sv?

	.align	4
.globl	C(d_sdivzstepu)
.globl	C(d_tdivzstepu)
.globl	C(d_zistepu)
.globl	C(d_sdivzstepv)
.globl	C(d_tdivzstepv)
.globl	C(d_zistepv)
.globl	C(d_sdivzorigin)
.globl	C(d_tdivzorigin)
.globl	C(d_ziorigin)
C(d_sdivzstepu):	.single	0
C(d_tdivzstepu):	.single	0
C(d_zistepu):		.single	0
C(d_sdivzstepv):	.single	0
C(d_tdivzstepv):	.single	0
C(d_zistepv):		.single	0
C(d_sdivzorigin):	.single	0
C(d_tdivzorigin):	.single	0
C(d_ziorigin):		.single	0

.globl	C(sadjust)
.globl	C(tadjust)
.globl	C(bbextents)
.globl	C(bbextentt)
C(sadjust):			.long	0
C(tadjust):			.long	0
C(bbextents):		.long	0
C(bbextentt):		.long	0

.globl	C(cacheblock)
.globl	C(d_viewbuffer)
.globl	C(d_rowbytes)
.globl	C(d_height)
.globl	C(cachewidth)
.globl	C(d_zbuffer)
.globl	C(d_zrowbytes)
.globl	C(d_zwidth)
C(cacheblock):		.long	0
C(cachewidth):		.long	0
C(d_viewbuffer):	.long	0
C(d_rowbytes):		.long	0
C(d_height):		.long	0
C(d_zbuffer):		.long	0
C(d_zrowbytes):		.long	0
C(d_zwidth):		.long	0


//-------------------------------------------------------
// ASM-only variables
//-------------------------------------------------------
.globl	C(izi)
C(izi):			.long	0

.globl	C(pbase), C(s), C(t), C(sfracf), C(tfracf), C(snext), C(tnext)
.globl	C(spancountminus1), C(zi16stepu), C(sdivz16stepu), C(tdivz16stepu)
.globl	C(zi8stepu), C(sdivz8stepu), C(tdivz8stepu), C(pz)
C(s):				.long	0
C(t):				.long	0
C(snext):			.long	0
C(tnext):			.long	0
C(sfracf):			.long	0
C(tfracf):			.long	0
C(pbase):			.long	0
C(zi8stepu):		.long	0
C(sdivz8stepu):	.long	0
C(tdivz8stepu):	.long	0
C(zi16stepu):		.long	0
C(sdivz16stepu):	.long	0
C(tdivz16stepu):	.long	0
C(spancountminus1): .long	0
C(pz):				.long	0

.globl	C(izistep)
C(izistep):				.long	0

//-------------------------------------------------------
// local variables for d_draw16.s
//-------------------------------------------------------

.globl	C(reciprocal_table_16)
// 1/2, 1/3, 1/4, 1/5, 1/6, 1/7, 1/8, 1/9, 1/10, 1/11, 1/12, 1/13,
// 1/14, and 1/15 in 0.32 form
C(reciprocal_table_16):	.long	0x40000000, 0x2aaaaaaa, 0x20000000
						.long	0x19999999, 0x15555555, 0x12492492
						.long	0x10000000, 0xe38e38e, 0xccccccc, 0xba2e8ba
						.long	0xaaaaaaa, 0x9d89d89, 0x9249249, 0x8888888


//-------------------------------------------------------
// local variables for d_parta.s
//-------------------------------------------------------
.globl	C(DP_Count), C(DP_u), C(DP_v), C(DP_32768), C(DP_Color)
.globl	C(DP_Pix), C(DP_EntryTable)
C(DP_Count):	.long	0
C(DP_u):		.long	0
C(DP_v):		.long	0
C(DP_32768):	.single	32768.0
C(DP_Color):	.long	0
C(DP_Pix):		.long	0


#ifndef NeXT
	.extern C(DP_1x1)
	.extern C(DP_2x2)
	.extern C(DP_3x3)
	.extern C(DP_4x4)
#endif

C(DP_EntryTable):	.long	C(DP_1x1), C(DP_2x2), C(DP_3x3), C(DP_4x4)

//
// advancetable is 8 bytes, but points to the middle of that range so negative
// offsets will work
//
.globl	C(advancetable), C(sstep), C(tstep), C(pspantemp)
.globl	C(counttemp), C(jumptemp)
C(advancetable):	.long	0, 0
C(sstep):			.long	0
C(tstep):			.long	0

C(pspantemp):		.long	0
C(counttemp):		.long	0
C(jumptemp):		.long	0

// 1/2, 1/3, 1/4, 1/5, 1/6, and 1/7 in 0.32 form
.globl	C(reciprocal_table), C(entryvec_table)
C(reciprocal_table):	.long	0x40000000, 0x2aaaaaaa, 0x20000000
					.long	0x19999999, 0x15555555, 0x12492492

#ifndef NeXT
	.extern C(Entry2_8)
	.extern C(Entry3_8)
	.extern C(Entry4_8)
	.extern C(Entry5_8)
	.extern C(Entry6_8)
	.extern C(Entry7_8)
	.extern C(Entry8_8)
#endif

C(entryvec_table):	.long	0, C(Entry2_8), C(Entry3_8), C(Entry4_8)
				.long	C(Entry5_8), C(Entry6_8), C(Entry7_8), C(Entry8_8)

#ifndef NeXT
	.extern C(Spr8Entry2_8)
	.extern C(Spr8Entry3_8)
	.extern C(Spr8Entry4_8)
	.extern C(Spr8Entry5_8)
	.extern C(Spr8Entry6_8)
	.extern C(Spr8Entry7_8)
	.extern C(Spr8Entry8_8)
#endif
	
.globl C(spr8entryvec_table)
C(spr8entryvec_table):	.long	0, C(Spr8Entry2_8), C(Spr8Entry3_8), C(Spr8Entry4_8)
					.long	C(Spr8Entry5_8), C(Spr8Entry6_8), C(Spr8Entry7_8), C(Spr8Entry8_8)

#endif	// USE_INTEL_ASM

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
