/*
	r_varsa.S

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
# undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifdef USE_INTEL_ASM

	.data

//-------------------------------------------------------
// ASM-only variables
//-------------------------------------------------------
.globl	C(float_1), C(float_particle_z_clip), C(float_point5)
.globl	C(float_minus_1), C(float_0)
C(float_0):				.single	0.0
C(float_1):				.single	1.0
C(float_minus_1):			.single	-1.0
C(float_particle_z_clip):	.single	PARTICLE_Z_CLIP
C(float_point5):			.single	0.5

.globl	C(fp_16), C(fp_64k), C(fp_1m), C(fp_64kx64k)
.globl	C(fp_1m_minus_1)
.globl	C(fp_8) 
C(fp_1m):					.single	1048576.0
C(fp_1m_minus_1):			.single	1048575.0
C(fp_64k):					.single	65536.0
C(fp_8):					.single	8.0
C(fp_16):					.single	16.0
C(fp_64kx64k):				.long	0x4f000000	// (float)0x8000*0x10000


.globl	C(FloatZero), C(Float2ToThe31nd), C(FloatMinus2ToThe31nd)
C(FloatZero):				.long	0
C(Float2ToThe31nd):		.long	0x4f000000
C(FloatMinus2ToThe31nd):	.long	0xcf000000

.globl	C(r_ceil_cw), C(r_single_cw)
C(r_ceil_cw):		.long	0
C(r_single_cw):	.long	0

.globl	C(r_bmodelactive)
C(r_bmodelactive):		.long	0

	.text
.global C(R_InitVars)
C(R_InitVars):
	movl	C(ceil_cw), %eax
	movl	C(single_cw), %edx
	movl	%eax, C(r_ceil_cw)
	movl	%edx, C(r_single_cw)
	ret

#endif	// USE_INTEL_ASM

