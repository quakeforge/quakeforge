/*
	sw_fpua.S

	Intel 32-bit assembly language dependent routines.

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


#ifdef USE_INTEL_ASM

#ifdef _WIN32
# undef PIC		//no such thing in win32
#endif

	.data

	.align	4
fpenv:
	.long	0, 0, 0, 0, 0, 0, 0, 0

#ifdef PIC
#define got_base(n)										\
	call	.Lpic##n									;\
.Lpic##n:												;\
	popl	%edx										;\
	addl	$C(_GLOBAL_OFFSET_TABLE_)+[.-.Lpic##n],%edx

#define got_var(v) v@GOTOFF(%edx)

#define F_BEGIN(name)			\
.globl C(name)					;\
	.type	C(name),@function	;\
C(name):

#define F_END(name) .size	C(name),.-C(name)

#else
#define got_base(n)
#define got_var(v) v

#define F_BEGIN(name)			\
.globl C(name)					;\
C(name):

#define F_END(name)

#endif

	.text

F_BEGIN(MaskExceptions)
	got_base(1)
	fnstenv	got_var(fpenv)
	orl		$0x3F,got_var(fpenv)
	fldenv	got_var(fpenv)

	ret
F_END(MaskExceptions)

#if 0
F_BEGIN(unmaskexceptions)
	got_base(2)
	fnstenv	got_var(fpenv)
	andl		$0xFFFFFFE0,got_var(fpenv)
	fldenv	got_var(fpenv)

	ret
F_END(unmaskexceptions)
#endif

	.data

	.align	4
.globl	C(ceil_cw), C(single_cw), C(full_cw), C(cw), C(pushed_cw)
C(ceil_cw):	.long	0
C(single_cw):	.long	0
C(full_cw):	.long	0
C(cw):			.long	0
C(pushed_cw):	.long	0
#ifdef PIC
.type	C(ceil_cw),@object
.type	C(single_cw),@object
.type	C(full_cw),@object
.type	C(cw),@object
.type	C(pushed_cw),@object
.size	C(ceil_cw),4
.size	C(single_cw),4
.size	C(full_cw),4
.size	C(cw),4
.size	C(pushed_cw),4
#endif

	.text

F_BEGIN(R_LowFPPrecision)
	got_base(3)
	fldcw	got_var(C(single_cw))

	ret
F_END(R_LowFPPrecision)


F_BEGIN(R_HighFPPrecision)
	got_base(4)
	fldcw	got_var(C(full_cw))

	ret
F_END(R_HighFPPrecision)


F_BEGIN(R_SetFPCW)
	got_base(7)
	fnstcw	got_var(C(cw))
	movl	got_var(C(cw)),%eax
	andb	$0xF0,%ah
	orb		$0x03,%ah	// round mode, 64-bit precision
	movl	%eax,got_var(C(full_cw))

	andb	$0xF0,%ah
	orb		$0x0C,%ah	// chop mode, single precision
	movl	%eax,got_var(C(single_cw))

	andb	$0xF0,%ah
	orb		$0x08,%ah	// ceil mode, single precision
	movl	%eax,got_var(C(ceil_cw))

	ret
F_END(R_SetFPCW)
#endif /* USE_INTEL_ASM */

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
