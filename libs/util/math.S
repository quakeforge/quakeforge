/*
	math.S

	x86 assembly-language math routines.

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
// #include "quakeasm.h"


#ifdef USE_INTEL_ASM

#ifdef _WIN32
# undef PIC		//no such thing in win32
#endif

	.data

	.align	4
Ljmptab:	.long	Lcase0, Lcase1, Lcase2, Lcase3
			.long	Lcase4, Lcase5, Lcase6, Lcase7

	.text

	.extern C(BOPS_Error)

#ifdef PIC
#define EMINS	8+4
#define EMAXS	8+8
#define P		8+12
#else
#define EMINS	4+4
#define EMAXS	4+8
#define P		4+12
#endif

	.align 2
.globl C(BoxOnPlaneSide)
	EXPORT(BoxOnPlaneSide)
#ifdef PIC
	.type	C(BoxOnPlaneSide),@function
#endif
C(BoxOnPlaneSide):
	pushl	%ebx
#ifdef PIC
	pushl	%esi
	call	.Lpic
.Lpic:
	popl	%esi
	addl	$C(_GLOBAL_OFFSET_TABLE_)+[.-.Lpic],%esi
#endif


	movl	P(%esp),%edx
	movl	EMINS(%esp),%ecx
	xorl	%eax,%eax
	movl	EMAXS(%esp),%ebx
	movb	pl_signbits(%edx),%al
	cmpb	$8,%al
	jge		Lerror
	flds	pl_normal(%edx)		// p->normal[0]
	fld		%st(0)				// p->normal[0] | p->normal[0]
#ifdef PIC
	jmp		*Ljmptab@GOTOFF(%esi,%eax,4)
#else
	jmp		*Ljmptab(,%eax,4)
#endif


//dist1= p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
//dist2= p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
Lcase0:
	fmuls	(%ebx)				// p->normal[0]*emaxs[0] | p->normal[0]
	flds	pl_normal+4(%edx)	// p->normal[1] | p->normal[0]*emaxs[0] |
								//  p->normal[0]
	fxch	%st(2)				// p->normal[0] | p->normal[0]*emaxs[0] |
								//  p->normal[1]
	fmuls	(%ecx)				// p->normal[0]*emins[0] |
								//  p->normal[0]*emaxs[0] | p->normal[1]
	fxch	%st(2)				// p->normal[1] | p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fld		%st(0)				// p->normal[1] | p->normal[1] |
								//  p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fmuls	4(%ebx)				// p->normal[1]*emaxs[1] | p->normal[1] |
								//  p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	flds	pl_normal+8(%edx)	// p->normal[2] | p->normal[1]*emaxs[1] |
								//  p->normal[1] | p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fxch	%st(2)				// p->normal[1] | p->normal[1]*emaxs[1] |
								//  p->normal[2] | p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fmuls	4(%ecx)				// p->normal[1]*emins[1] |
								//  p->normal[1]*emaxs[1] |
								//  p->normal[2] | p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fxch	%st(2)				// p->normal[2] | p->normal[1]*emaxs[1] |
								//  p->normal[1]*emins[1] |
								//  p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fld		%st(0)				// p->normal[2] | p->normal[2] |
								//  p->normal[1]*emaxs[1] |
								//  p->normal[1]*emins[1] |
								//  p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fmuls	8(%ebx)				// p->normal[2]*emaxs[2] |
								//  p->normal[2] |
								//  p->normal[1]*emaxs[1] |
								//  p->normal[1]*emins[1] |
								//  p->normal[0]*emaxs[0] |
								//  p->normal[0]*emins[0]
	fxch	%st(5)				// p->normal[0]*emins[0] |
								//  p->normal[2] |
								//  p->normal[1]*emaxs[1] |
								//  p->normal[1]*emins[1] |
								//  p->normal[0]*emaxs[0] |
								//  p->normal[2]*emaxs[2]
	faddp	%st(0),%st(3)		//p->normal[2] |
								// p->normal[1]*emaxs[1] |
								// p->normal[1]*emins[1]+p->normal[0]*emins[0]|
								// p->normal[0]*emaxs[0] |
								// p->normal[2]*emaxs[2]
	fmuls	8(%ecx)				//p->normal[2]*emins[2] |
								// p->normal[1]*emaxs[1] |
								// p->normal[1]*emins[1]+p->normal[0]*emins[0]|
								// p->normal[0]*emaxs[0] |
								// p->normal[2]*emaxs[2]
	fxch	%st(1)				//p->normal[1]*emaxs[1] |
								// p->normal[2]*emins[2] |
								// p->normal[1]*emins[1]+p->normal[0]*emins[0]|
								// p->normal[0]*emaxs[0] |
								// p->normal[2]*emaxs[2]
	faddp	%st(0),%st(3)		//p->normal[2]*emins[2] |
								// p->normal[1]*emins[1]+p->normal[0]*emins[0]|
								// p->normal[0]*emaxs[0]+p->normal[1]*emaxs[1]|
								// p->normal[2]*emaxs[2]
	fxch	%st(3)				//p->normal[2]*emaxs[2] +
								// p->normal[1]*emins[1]+p->normal[0]*emins[0]|
								// p->normal[0]*emaxs[0]+p->normal[1]*emaxs[1]|
								// p->normal[2]*emins[2]
	faddp	%st(0),%st(2)		//p->normal[1]*emins[1]+p->normal[0]*emins[0]|
								// dist1 | p->normal[2]*emins[2]

	jmp		LSetSides

//dist1= p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
//dist2= p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
Lcase1:
	fmuls	(%ecx)				// emins[0]
	flds	pl_normal+4(%edx)
	fxch	%st(2)
	fmuls	(%ebx)				// emaxs[0]
	fxch	%st(2)
	fld		%st(0)
	fmuls	4(%ebx)				// emaxs[1]
	flds	pl_normal+8(%edx)
	fxch	%st(2)
	fmuls	4(%ecx)				// emins[1]
	fxch	%st(2)
	fld		%st(0)
	fmuls	8(%ebx)				// emaxs[2]
	fxch	%st(5)
	faddp	%st(0),%st(3)
	fmuls	8(%ecx)				// emins[2]
	fxch	%st(1)
	faddp	%st(0),%st(3)
	fxch	%st(3)
	faddp	%st(0),%st(2)

	jmp		LSetSides

//dist1= p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
//dist2= p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
Lcase2:
	fmuls	(%ebx)				// emaxs[0]
	flds	pl_normal+4(%edx)
	fxch	%st(2)
	fmuls	(%ecx)				// emins[0]
	fxch	%st(2)
	fld		%st(0)
	fmuls	4(%ecx)				// emins[1]
	flds	pl_normal+8(%edx)
	fxch	%st(2)
	fmuls	4(%ebx)				// emaxs[1]
	fxch	%st(2)
	fld		%st(0)
	fmuls	8(%ebx)				// emaxs[2]
	fxch	%st(5)
	faddp	%st(0),%st(3)
	fmuls	8(%ecx)				// emins[2]
	fxch	%st(1)
	faddp	%st(0),%st(3)
	fxch	%st(3)
	faddp	%st(0),%st(2)

	jmp		LSetSides

//dist1= p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
//dist2= p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
Lcase3:
	fmuls	(%ecx)				// emins[0]
	flds	pl_normal+4(%edx)
	fxch	%st(2)
	fmuls	(%ebx)				// emaxs[0]
	fxch	%st(2)
	fld		%st(0)
	fmuls	4(%ecx)				// emins[1]
	flds	pl_normal+8(%edx)
	fxch	%st(2)
	fmuls	4(%ebx)				// emaxs[1]
	fxch	%st(2)
	fld		%st(0)
	fmuls	8(%ebx)				// emaxs[2]
	fxch	%st(5)
	faddp	%st(0),%st(3)
	fmuls	8(%ecx)				// emins[2]
	fxch	%st(1)
	faddp	%st(0),%st(3)
	fxch	%st(3)
	faddp	%st(0),%st(2)

	jmp		LSetSides

//dist1= p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
//dist2= p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
Lcase4:
	fmuls	(%ebx)				// emaxs[0]
	flds	pl_normal+4(%edx)
	fxch	%st(2)
	fmuls	(%ecx)				// emins[0]
	fxch	%st(2)
	fld		%st(0)
	fmuls	4(%ebx)				// emaxs[1]
	flds	pl_normal+8(%edx)
	fxch	%st(2)
	fmuls	4(%ecx)				// emins[1]
	fxch	%st(2)
	fld		%st(0)
	fmuls	8(%ecx)				// emins[2]
	fxch	%st(5)
	faddp	%st(0),%st(3)
	fmuls	8(%ebx)				// emaxs[2]
	fxch	%st(1)
	faddp	%st(0),%st(3)
	fxch	%st(3)
	faddp	%st(0),%st(2)

	jmp		LSetSides

//dist1= p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
//dist2= p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
Lcase5:
	fmuls	(%ecx)				// emins[0]
	flds	pl_normal+4(%edx)
	fxch	%st(2)
	fmuls	(%ebx)				// emaxs[0]
	fxch	%st(2)
	fld		%st(0)
	fmuls	4(%ebx)				// emaxs[1]
	flds	pl_normal+8(%edx)
	fxch	%st(2)
	fmuls	4(%ecx)				// emins[1]
	fxch	%st(2)
	fld		%st(0)
	fmuls	8(%ecx)				// emins[2]
	fxch	%st(5)
	faddp	%st(0),%st(3)
	fmuls	8(%ebx)				// emaxs[2]
	fxch	%st(1)
	faddp	%st(0),%st(3)
	fxch	%st(3)
	faddp	%st(0),%st(2)

	jmp		LSetSides

//dist1= p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
//dist2= p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
Lcase6:
	fmuls	(%ebx)				// emaxs[0]
	flds	pl_normal+4(%edx)
	fxch	%st(2)
	fmuls	(%ecx)				// emins[0]
	fxch	%st(2)
	fld		%st(0)
	fmuls	4(%ecx)				// emins[1]
	flds	pl_normal+8(%edx)
	fxch	%st(2)
	fmuls	4(%ebx)				// emaxs[1]
	fxch	%st(2)
	fld		%st(0)
	fmuls	8(%ecx)				// emins[2]
	fxch	%st(5)
	faddp	%st(0),%st(3)
	fmuls	8(%ebx)				// emaxs[2]
	fxch	%st(1)
	faddp	%st(0),%st(3)
	fxch	%st(3)
	faddp	%st(0),%st(2)

	jmp		LSetSides

//dist1= p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
//dist2= p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
Lcase7:
	fmuls	(%ecx)				// emins[0]
	flds	pl_normal+4(%edx)
	fxch	%st(2)
	fmuls	(%ebx)				// emaxs[0]
	fxch	%st(2)
	fld		%st(0)
	fmuls	4(%ecx)				// emins[1]
	flds	pl_normal+8(%edx)
	fxch	%st(2)
	fmuls	4(%ebx)				// emaxs[1]
	fxch	%st(2)
	fld		%st(0)
	fmuls	8(%ecx)				// emins[2]
	fxch	%st(5)
	faddp	%st(0),%st(3)
	fmuls	8(%ebx)				// emaxs[2]
	fxch	%st(1)
	faddp	%st(0),%st(3)
	fxch	%st(3)
	faddp	%st(0),%st(2)

LSetSides:

//	sides = 0;
//	if (dist1 >= p->dist)
//		sides = 1;
//	if (dist2 < p->dist)
//		sides |= 2;

	faddp	%st(0),%st(2)		// dist1 | dist2
	fcomps	pl_dist(%edx)
	xorl	%ecx,%ecx
	fnstsw	%ax
	fcomps	pl_dist(%edx)
	andb	$1,%ah
	xorb	$1,%ah
	addb	%ah,%cl

	fnstsw	%ax
	andb	$1,%ah
	addb	%ah,%ah
	addb	%ah,%cl

//	return sides;

#ifdef PIC
	popl	%esi
#endif
	popl	%ebx
	movl	%ecx,%eax	// return status

	ret


Lerror:
#ifdef PIC
	call	C(BOPS_Error)@PLT
.Lfe1:
	.size	C(BoxOnPlaneSide),.Lfe1-C(BoxOnPlaneSide)
#else
	call	C(BOPS_Error)
#endif

#endif	// USE_INTEL_ASM

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
