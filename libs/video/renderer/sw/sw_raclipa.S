/*
	r_aclipa.S

	x86 assembly-language clip routines for drawing Alias models directly
	to the screen

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

#ifdef _WIN32
# undef PIC
#endif

	.text
#define temp0		0
#define temp1		4
#define point5		8
#define pfv0		12+8+4
#define pfv1		12+8+8
#define out			12+8+12

.globl C(R_Alias_clip_bottom)
#ifdef PIC
	.type	R_Alias_clip_bottom,@function
#endif
C(R_Alias_clip_bottom):
	pushl	%esi
	pushl	%edi

	subl	$12,%esp

	movl	pfv0(%esp),%esi
	movl	pfv1(%esp),%edi

#ifdef PIC
	call	.Lpic1
.Lpic1:
	popl	%eax
	addl	$C(_GLOBAL_OFFSET_TABLE_)-1+[.-.Lpic1],%eax

	movl	C(float_point5)@GOTOFF(%eax),%edx
	movl	C(r_refdef)@GOT(%eax),%eax
#else
	leal	C(r_refdef),%eax
	movl	C(float_point5),%edx
#endif
	movl	rd_aliasvrectbottom(%eax),%eax
	movl	%edx,point5(%esp)

LDoForwardOrBackward:

	movl	fv_v+4(%esi),%edx
	movl	fv_v+4(%edi),%ecx

	cmpl	%ecx,%edx
	jl		LDoForward

	movl	fv_v+4(%esi),%ecx
	movl	fv_v+4(%edi),%edx
	movl	pfv0(%esp),%edi
	movl	pfv1(%esp),%esi

LDoForward:

	subl	%edx,%ecx
	subl	%edx,%eax
	movl	%ecx,temp1(%esp)
	movl	%eax,temp0(%esp)
	fildl	temp1(%esp)
	fildl	temp0(%esp)
	movl	out(%esp),%edx
	movl	$2,%eax

	fdivp	%st(0),%st(1)					// scale

LDo3Forward:
	fildl	fv_v+0(%esi)	// fv0v0 | scale
	fildl	fv_v+0(%edi)	// fv1v0 | fv0v0 | scale
	fildl	fv_v+4(%esi)	// fv0v1 | fv1v0 | fv0v0 | scale
	fildl	fv_v+4(%edi)	// fv1v1 | fv0v1 | fv1v0 | fv0v0 | scale
	fildl	fv_v+8(%esi)	// fv0v2 | fv1v1 | fv0v1 | fv1v0 | fv0v0 | scale
	fildl	fv_v+8(%edi)	// fv1v2 | fv0v2 | fv1v1 | fv0v1 | fv1v0 | fv0v0 |
							//  scale
	fxch	%st(5)			// fv0v0 | fv0v2 | fv1v1 | fv0v1 | fv1v0 | fv1v2 |
							//  scale
	fsubr	%st(0),%st(4)	// fv0v0 | fv0v2 | fv1v1 | fv0v1 | fv1v0-fv0v0 |
							//  fv1v2 | scale
	fxch	%st(3)			// fv0v1 | fv0v2 | fv1v1 | fv0v0 | fv1v0-fv0v0 |
							//  fv1v2 | scale
	fsubr	%st(0),%st(2)	// fv0v1 | fv0v2 | fv1v1-fv0v1 | fv0v0 |
							//  fv1v0-fv0v0 | fv1v2 | scale
	fxch	%st(1)			// fv0v2 | fv0v1 | fv1v1-fv0v1 | fv0v0 |
							//  fv1v0-fv0v0 | fv1v2 | scale
	fsubr	%st(0),%st(5)	// fv0v2 | fv0v1 | fv1v1-fv0v1 | fv0v0 |
							//  fv1v0-fv0v0 | fv1v2-fv0v2 | scale
	fxch	%st(6)			// scale | fv0v1 | fv1v1-fv0v1 | fv0v0 |
							//  fv1v0-fv0v0 | fv1v2-fv0v2 | fv0v2
	fmul	%st(0),%st(4)	// scale | fv0v1 | fv1v1-fv0v1 | fv0v0 |
							//  (fv1v0-fv0v0)*scale | fv1v2-fv0v2 | fv0v2
	addl	$12,%edi
	fmul	%st(0),%st(2)	// scale | fv0v1 | (fv1v1-fv0v1)*scale | fv0v0 |
							//  (fv1v0-fv0v0)*scale | fv1v2-fv0v2 | fv0v2
	addl	$12,%esi
	addl	$12,%edx
	fmul	%st(0),%st(5)	// scale | fv0v1 | (fv1v1-fv0v1)*scale | fv0v0 |
							//  (fv1v0-fv0v0)*scale | (fv1v2-fv0v2)*scale |
							//  fv0v2
	fxch	%st(3)			// fv0v0 | fv0v1 | (fv1v1-fv0v1)*scale | scale |
							//  (fv1v0-fv0v0)*scale | (fv1v2-fv0v2)*scale |
							//  fv0v2
	faddp	%st(0),%st(4)	// fv0v1 | (fv1v1-fv0v1)*scale | scale |
							//  fv0v0+(fv1v0-fv0v0)*scale |
							//  (fv1v2-fv0v2)*scale | fv0v2
	faddp	%st(0),%st(1)	// fv0v1+(fv1v1-fv0v1)*scale | scale |
							//  fv0v0+(fv1v0-fv0v0)*scale |
							//  (fv1v2-fv0v2)*scale | fv0v2
	fxch	%st(4)			// fv0v2 | scale | fv0v0+(fv1v0-fv0v0)*scale |
							//  (fv1v2-fv0v2)*scale | fv0v1+(fv1v1-fv0v1)*scale
	faddp	%st(0),%st(3)	// scale | fv0v0+(fv1v0-fv0v0)*scale |
							//  fv0v2+(fv1v2-fv0v2)*scale |
							//  fv0v1+(fv1v1-fv0v1)*scale
	fxch	%st(1)			// fv0v0+(fv1v0-fv0v0)*scale | scale | 
							//  fv0v2+(fv1v2-fv0v2)*scale |
							//  fv0v1+(fv1v1-fv0v1)*scale
	fadds	point5(%esp)
	fxch	%st(3)			// fv0v1+(fv1v1-fv0v1)*scale | scale | 
							//  fv0v2+(fv1v2-fv0v2)*scale |
							//  fv0v0+(fv1v0-fv0v0)*scale
	fadds	point5(%esp)
	fxch	%st(2)			// fv0v2+(fv1v2-fv0v2)*scale | scale | 
							//  fv0v1+(fv1v1-fv0v1)*scale |
							//  fv0v0+(fv1v0-fv0v0)*scale
	fadds	point5(%esp)
	fxch	%st(3)			// fv0v0+(fv1v0-fv0v0)*scale | scale | 
							//  fv0v1+(fv1v1-fv0v1)*scale |
							//  fv0v2+(fv1v2-fv0v2)*scale
	fistpl	fv_v+0-12(%edx)	// scale | fv0v1+(fv1v1-fv0v1)*scale |
							//  fv0v2+(fv1v2-fv0v2)*scale
	fxch	%st(1)			// fv0v1+(fv1v1-fv0v1)*scale | scale |
							//  fv0v2+(fv1v2-fv0v2)*scale | scale
	fistpl	fv_v+4-12(%edx)	// scale | fv0v2+(fv1v2-fv0v2)*scale
	fxch	%st(1)			// fv0v2+(fv1v2-fv0v2)*sc | scale
	fistpl	fv_v+8-12(%edx)	// scale

	decl	%eax
	jnz		LDo3Forward

	fstp	%st(0)

	addl	$12,%esp

	popl	%edi
	popl	%esi

	ret
#ifdef PIC
.Lsize1:
	.size	C(R_Alias_clip_bottom),.Lsize1-C(R_Alias_clip_bottom)
#endif
	.align 4


.globl C(R_Alias_clip_top)
#ifdef PIC
	.type	R_Alias_clip_top,@function
#endif
C(R_Alias_clip_top):
	pushl	%esi
	pushl	%edi

	subl	$12,%esp

	movl	pfv0(%esp),%esi
	movl	pfv1(%esp),%edi

#ifdef PIC
	call	.Lpic2
.Lpic2:
	popl	%eax
	addl	$C(_GLOBAL_OFFSET_TABLE_)-1+[.-.Lpic2],%eax

	movl	C(float_point5)@GOTOFF(%eax),%edx
	movl	C(r_refdef)@GOT(%eax),%eax
#else
	leal	C(r_refdef),%eax
	movl	C(float_point5),%edx
#endif
	movl	rd_aliasvrect+4(%eax),%eax
	movl	%edx,point5(%esp)
	jmp		LDoForwardOrBackward
#ifdef PIC
.Lsize2:
	.size	C(R_Alias_clip_top),.Lsize2-C(R_Alias_clip_top)
#endif
	.align 4



.globl C(R_Alias_clip_right)
#ifdef PIC
	.type	R_Alias_clip_right,@function
#endif
C(R_Alias_clip_right):
	pushl	%esi
	pushl	%edi

	subl	$12,%esp

	movl	pfv0(%esp),%esi
	movl	pfv1(%esp),%edi

#ifdef PIC
	call	.Lpic3
.Lpic3:
	popl	%eax
	addl	$C(_GLOBAL_OFFSET_TABLE_)-1+[.-.Lpic3],%eax

	movl	C(float_point5)@GOTOFF(%eax),%edx
	movl	C(r_refdef)@GOT(%eax),%eax
#else
	leal	C(r_refdef),%eax
	movl	C(float_point5),%edx
#endif
	movl	rd_aliasvrectright(%eax),%eax
	movl	%edx,point5(%esp)

LRightLeftEntry:


	movl	fv_v+4(%esi),%edx
	movl	fv_v+4(%edi),%ecx

	cmpl	%ecx,%edx
	movl	fv_v+0(%esi),%edx

	movl	fv_v+0(%edi),%ecx
	jl		LDoForward2

	movl	fv_v+0(%esi),%ecx
	movl	fv_v+0(%edi),%edx
	movl	pfv0(%esp),%edi
	movl	pfv1(%esp),%esi

LDoForward2:

	jmp		LDoForward
#ifdef PIC
.Lsize3:
	.size	C(R_Alias_clip_right),.Lsize3-C(R_Alias_clip_right)
#endif
	.align 4


.globl C(R_Alias_clip_left)
#ifdef PIC
	.type	R_Alias_clip_left,@function
#endif
C(R_Alias_clip_left):
	pushl	%esi
	pushl	%edi

	subl	$12,%esp

	movl	pfv0(%esp),%esi
	movl	pfv1(%esp),%edi

#ifdef PIC
	call	.Lpic4
.Lpic4:
	popl	%eax
	addl	$C(_GLOBAL_OFFSET_TABLE_)-1+[.-.Lpic4],%eax

	movl	C(float_point5)@GOTOFF(%eax),%edx
	movl	C(r_refdef)@GOT(%eax),%eax
#else
	leal	C(r_refdef),%eax
	movl	C(float_point5),%edx
#endif
	movl	rd_aliasvrect+0(%eax),%eax
	movl	%edx,point5(%esp)
	jmp		LRightLeftEntry
#ifdef PIC
.Lsize4:
	.size	C(R_Alias_clip_left),.Lsize4-C(R_Alias_clip_left)
#endif
	.align 4


#endif	// USE_INTEL_ASM

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
