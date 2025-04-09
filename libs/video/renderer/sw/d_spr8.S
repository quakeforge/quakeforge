/*
	d_spr8.S

	x86 assembly-language horizontal 8-bpp transparent span-drawing code.

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

#ifdef PIC
#undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifdef USE_INTEL_ASM

//----------------------------------------------------------------------
// 8-bpp horizontal span drawing code for polygons, with transparency.
//----------------------------------------------------------------------

	.text

// out-of-line, rarely-needed clamping code

LClampHigh0:
	movl	C(bbextents),%esi
	jmp		LClampReentry0
LClampHighOrLow0:
	jg		LClampHigh0
	xorl	%esi,%esi
	jmp		LClampReentry0

LClampHigh1:
	movl	C(bbextentt),%edx
	jmp		LClampReentry1
LClampHighOrLow1:
	jg		LClampHigh1
	xorl	%edx,%edx
	jmp		LClampReentry1

LClampLow2:
	movl	$2048,%ebp
	jmp		LClampReentry2
LClampHigh2:
	movl	C(bbextents),%ebp
	jmp		LClampReentry2

LClampLow3:
	movl	$2048,%ecx
	jmp		LClampReentry3
LClampHigh3:
	movl	C(bbextentt),%ecx
	jmp		LClampReentry3

LClampLow4:
	movl	$2048,%eax
	jmp		LClampReentry4
LClampHigh4:
	movl	C(bbextents),%eax
	jmp		LClampReentry4

LClampLow5:
	movl	$2048,%ebx
	jmp		LClampReentry5
LClampHigh5:
	movl	C(bbextentt),%ebx
	jmp		LClampReentry5


#define pspans	4+16

	.align 4
.globl C(D_SpriteDrawSpans)
C(D_SpriteDrawSpans):
	pushl	%ebp				// preserve caller's stack frame
	pushl	%edi
	pushl	%esi				// preserve register variables
	pushl	%ebx

//
// set up scaled-by-8 steps, for 8-long segments; also set up cacheblock
// and span list pointers, and 1/z step in 0.32 fixed-point
//
// FIXME: any overlap from rearranging?
	flds	C(d_sdivzstepu)
	fmuls	C(fp_8)
	movl	C(cacheblock),%edx
	flds	C(d_tdivzstepu)
	fmuls	C(fp_8)
	movl	pspans(%esp),%ebx	// point to the first span descriptor
	flds	C(d_zistepu)
	fmuls	C(fp_8)
	movl	%edx,C(pbase)			// pbase = cacheblock
	flds	C(d_zistepu)
	fmuls	C(fp_64kx64k)
	fxch	%st(3)
	fstps	C(sdivz8stepu)
	fstps	C(zi8stepu)
	fstps	C(tdivz8stepu)
	fistpl	C(izistep)
	movl	C(izistep),%eax
	rorl	$16,%eax		// put upper 16 bits in low word
	movl	sspan_t_count(%ebx),%ecx
	movl	%eax,C(izistep)

	cmpl	$0,%ecx
	jle		LNextSpan

LSpanLoop:

//
// set up the initial s/z, t/z, and 1/z on the FP stack, and generate the
// initial s and t values
//
// FIXME: pipeline FILD?
	fildl	sspan_t_v(%ebx)
	fildl	sspan_t_u(%ebx)

	fld		%st(1)			// dv | du | dv
	fmuls	C(d_sdivzstepv)	// dv*d_sdivzstepv | du | dv
	fld		%st(1)			// du | dv*d_sdivzstepv | du | dv
	fmuls	C(d_sdivzstepu)	// du*d_sdivzstepu | dv*d_sdivzstepv | du | dv
	fld		%st(2)			// du | du*d_sdivzstepu | dv*d_sdivzstepv | du | dv
	fmuls	C(d_tdivzstepu)	// du*d_tdivzstepu | du*d_sdivzstepu |
							//  dv*d_sdivzstepv | du | dv
	fxch	%st(1)			// du*d_sdivzstepu | du*d_tdivzstepu |
							//  dv*d_sdivzstepv | du | dv
	faddp	%st(0),%st(2)	// du*d_tdivzstepu |
							//  du*d_sdivzstepu + dv*d_sdivzstepv | du | dv
	fxch	%st(1)			// du*d_sdivzstepu + dv*d_sdivzstepv |
							//  du*d_tdivzstepu | du | dv
	fld		%st(3)			// dv | du*d_sdivzstepu + dv*d_sdivzstepv |
							//  du*d_tdivzstepu | du | dv
	fmuls	C(d_tdivzstepv)	// dv*d_tdivzstepv |
							//  du*d_sdivzstepu + dv*d_sdivzstepv |
							//  du*d_tdivzstepu | du | dv
	fxch	%st(1)			// du*d_sdivzstepu + dv*d_sdivzstepv |
							//  dv*d_tdivzstepv | du*d_tdivzstepu | du | dv
	fadds	C(d_sdivzorigin) // sdivz = d_sdivzorigin + dv*d_sdivzstepv +
							//  du*d_sdivzstepu; stays in %st(2) at end
	fxch	%st(4)			// dv | dv*d_tdivzstepv | du*d_tdivzstepu | du |
							//  s/z
	fmuls	C(d_zistepv)		// dv*d_zistepv | dv*d_tdivzstepv |
							//  du*d_tdivzstepu | du | s/z
	fxch	%st(1)			// dv*d_tdivzstepv |  dv*d_zistepv |
							//  du*d_tdivzstepu | du | s/z
	faddp	%st(0),%st(2)	// dv*d_zistepv |
							//  dv*d_tdivzstepv + du*d_tdivzstepu | du | s/z
	fxch	%st(2)			// du | dv*d_tdivzstepv + du*d_tdivzstepu |
							//  dv*d_zistepv | s/z
	fmuls	C(d_zistepu)		// du*d_zistepu |
							//  dv*d_tdivzstepv + du*d_tdivzstepu |
							//  dv*d_zistepv | s/z
	fxch	%st(1)			// dv*d_tdivzstepv + du*d_tdivzstepu |
							//  du*d_zistepu | dv*d_zistepv | s/z
	fadds	C(d_tdivzorigin)	// tdivz = d_tdivzorigin + dv*d_tdivzstepv +
							//  du*d_tdivzstepu; stays in %st(1) at end
	fxch	%st(2)			// dv*d_zistepv | du*d_zistepu | t/z | s/z
	faddp	%st(0),%st(1)	// dv*d_zistepv + du*d_zistepu | t/z | s/z

	flds	C(fp_64k)			// fp_64k | dv*d_zistepv + du*d_zistepu | t/z | s/z
	fxch	%st(1)			// dv*d_zistepv + du*d_zistepu | fp_64k | t/z | s/z
	fadds	C(d_ziorigin)		// zi = d_ziorigin + dv*d_zistepv +
							//  du*d_zistepu; stays in %st(0) at end
							// 1/z | fp_64k | t/z | s/z

	fld		%st(0)			// FIXME: get rid of stall on FMUL?
	fmuls	C(fp_64kx64k)
	fxch	%st(1)

//
// calculate and clamp s & t
//
	fdivr	%st(0),%st(2)	// 1/z | z*64k | t/z | s/z
	fxch	%st(1)

	fistpl	C(izi)				// 0.32 fixed-point 1/z
	movl	C(izi),%ebp

//
// set pz to point to the first z-buffer pixel in the span
//
	rorl	$16,%ebp		// put upper 16 bits in low word
	movl	sspan_t_v(%ebx),%eax
	movl	%ebp,C(izi)
	movl	sspan_t_u(%ebx),%ebp
	imull	C(d_zrowbytes)
	shll	$1,%ebp					// a word per pixel
	addl	C(d_zbuffer),%eax
	addl	%ebp,%eax
	movl	%eax,C(pz)

//
// point %edi to the first pixel in the span
//
	movl	C(d_viewbuffer),%ebp
	movl	sspan_t_v(%ebx),%eax
	pushl	%ebx		// preserve spans pointer
	movl	C(tadjust),%edx
	movl	C(sadjust),%esi
	movl	C(d_scantable)(,%eax,4),%edi	// v * d_rowbytes
	addl	%ebp,%edi
	movl	sspan_t_u(%ebx),%ebp
	addl	%ebp,%edi				// pdest = &pdestspan[scans->u];

//
// now start the FDIV for the end of the span
//
	cmpl	$8,%ecx
	ja		LSetupNotLast1

	decl	%ecx
	jz		LCleanup1		// if only one pixel, no need to start an FDIV
	movl	%ecx,C(spancountminus1)

// finish up the s and t calcs
	fxch	%st(1)			// z*64k | 1/z | t/z | s/z

	fld		%st(0)			// z*64k | z*64k | 1/z | t/z | s/z
	fmul	%st(4),%st(0)	// s | z*64k | 1/z | t/z | s/z
	fxch	%st(1)			// z*64k | s | 1/z | t/z | s/z
	fmul	%st(3),%st(0)	// t | s | 1/z | t/z | s/z
	fxch	%st(1)			// s | t | 1/z | t/z | s/z
	fistpl	C(s)			// 1/z | t | t/z | s/z
	fistpl	C(t)			// 1/z | t/z | s/z

	fildl	C(spancountminus1)

	flds	C(d_tdivzstepu)	// _d_tdivzstepu | spancountminus1
	flds	C(d_zistepu)	// _d_zistepu | _d_tdivzstepu | spancountminus1
	fmul	%st(2),%st(0)	// _d_zistepu*scm1 | _d_tdivzstepu | scm1
	fxch	%st(1)			// _d_tdivzstepu | _d_zistepu*scm1 | scm1
	fmul	%st(2),%st(0)	// _d_tdivzstepu*scm1 | _d_zistepu*scm1 | scm1
	fxch	%st(2)			// scm1 | _d_zistepu*scm1 | _d_tdivzstepu*scm1
	fmuls	C(d_sdivzstepu)	// _d_sdivzstepu*scm1 | _d_zistepu*scm1 |
							//  _d_tdivzstepu*scm1
	fxch	%st(1)			// _d_zistepu*scm1 | _d_sdivzstepu*scm1 |
							//  _d_tdivzstepu*scm1
	faddp	%st(0),%st(3)	// _d_sdivzstepu*scm1 | _d_tdivzstepu*scm1
	fxch	%st(1)			// _d_tdivzstepu*scm1 | _d_sdivzstepu*scm1
	faddp	%st(0),%st(3)	// _d_sdivzstepu*scm1
	faddp	%st(0),%st(3)

	flds	C(fp_64k)
	fdiv	%st(1),%st(0)	// this is what we've gone to all this trouble to
							//  overlap
	jmp		LFDIVInFlight1

LCleanup1:
// finish up the s and t calcs
	fxch	%st(1)			// z*64k | 1/z | t/z | s/z

	fld		%st(0)			// z*64k | z*64k | 1/z | t/z | s/z
	fmul	%st(4),%st(0)	// s | z*64k | 1/z | t/z | s/z
	fxch	%st(1)			// z*64k | s | 1/z | t/z | s/z
	fmul	%st(3),%st(0)	// t | s | 1/z | t/z | s/z
	fxch	%st(1)			// s | t | 1/z | t/z | s/z
	fistpl	C(s)			// 1/z | t | t/z | s/z
	fistpl	C(t)			// 1/z | t/z | s/z
	jmp		LFDIVInFlight1

	.align	4
LSetupNotLast1:
// finish up the s and t calcs
	fxch	%st(1)			// z*64k | 1/z | t/z | s/z

	fld		%st(0)			// z*64k | z*64k | 1/z | t/z | s/z
	fmul	%st(4),%st(0)	// s | z*64k | 1/z | t/z | s/z
	fxch	%st(1)			// z*64k | s | 1/z | t/z | s/z
	fmul	%st(3),%st(0)	// t | s | 1/z | t/z | s/z
	fxch	%st(1)			// s | t | 1/z | t/z | s/z
	fistpl	C(s)				// 1/z | t | t/z | s/z
	fistpl	C(t)				// 1/z | t/z | s/z

	fadds	C(zi8stepu)
	fxch	%st(2)
	fadds	C(sdivz8stepu)
	fxch	%st(2)
	flds	C(tdivz8stepu)
	faddp	%st(0),%st(2)
	flds	C(fp_64k)
	fdiv	%st(1),%st(0)	// z = 1/1/z
							// this is what we've gone to all this trouble to
							//  overlap
LFDIVInFlight1:

	addl	C(s),%esi
	addl	C(t),%edx
	movl	C(bbextents),%ebx
	movl	C(bbextentt),%ebp
	cmpl	%ebx,%esi
	ja		LClampHighOrLow0
LClampReentry0:
	movl	%esi,C(s)
	movl	C(pbase),%ebx
	shll	$16,%esi
	cmpl	%ebp,%edx
	movl	%esi,C(sfracf)
	ja		LClampHighOrLow1
LClampReentry1:
	movl	%edx,C(t)
	movl	C(s),%esi					// sfrac = scans->sfrac;
	shll	$16,%edx
	movl	C(t),%eax					// tfrac = scans->tfrac;
	sarl	$16,%esi
	movl	%edx,C(tfracf)

//
// calculate the texture starting address
//
	sarl	$16,%eax
	addl	%ebx,%esi
	imull	C(cachewidth),%eax		// (tfrac >> 16) * cachewidth
	addl	%eax,%esi				// psource = pbase + (sfrac >> 16) +
									//           ((tfrac >> 16) * cachewidth);

//
// determine whether last span or not
//
	cmpl	$8,%ecx
	jna		LLastSegment

//
// not the last segment; do full 8-wide segment
//
LNotLastSegment:

//
// advance s/z, t/z, and 1/z, and calculate s & t at end of span and steps to
// get there
//

// pick up after the FDIV that was left in flight previously

	fld		%st(0)			// duplicate it
	fmul	%st(4),%st(0)	// s = s/z * z
	fxch	%st(1)
	fmul	%st(3),%st(0)	// t = t/z * z
	fxch	%st(1)
	fistpl	C(snext)
	fistpl	C(tnext)
	movl	C(snext),%eax
	movl	C(tnext),%edx

	subl	$8,%ecx		// count off this segments' pixels
	movl	C(sadjust),%ebp
	pushl	%ecx		// remember count of remaining pixels
	movl	C(tadjust),%ecx

	addl	%eax,%ebp
	addl	%edx,%ecx

	movl	C(bbextents),%eax
	movl	C(bbextentt),%edx

	cmpl	$2048,%ebp
	jl		LClampLow2
	cmpl	%eax,%ebp
	ja		LClampHigh2
LClampReentry2:

	cmpl	$2048,%ecx
	jl		LClampLow3
	cmpl	%edx,%ecx
	ja		LClampHigh3
LClampReentry3:

	movl	%ebp,C(snext)
	movl	%ecx,C(tnext)

	subl	C(s),%ebp
	subl	C(t),%ecx
	
//
// set up advancetable
//
	movl	%ecx,%eax
	movl	%ebp,%edx
	sarl	$19,%edx			// sstep >>= 16;
	movl	C(cachewidth),%ebx
	sarl	$19,%eax			// tstep >>= 16;
	jz		LIsZero
	imull	%ebx,%eax			// (tstep >> 16) * cachewidth;
LIsZero:
	addl	%edx,%eax			// add in sstep
								// (tstep >> 16) * cachewidth + (sstep >> 16);
	movl	C(tfracf),%edx
	movl	%eax,C(advancetable)+4	// advance base in t
	addl	%ebx,%eax			// ((tstep >> 16) + 1) * cachewidth +
								//  (sstep >> 16);
	shll	$13,%ebp			// left-justify sstep fractional part
	movl	%ebp,C(sstep)
	movl	C(sfracf),%ebx
	shll	$13,%ecx			// left-justify tstep fractional part
	movl	%eax,C(advancetable)	// advance extra in t
	movl	%ecx,C(tstep)

	movl	C(pz),%ecx
	movl	C(izi),%ebp

	cmpw	(%ecx),%bp
	jl		Lp1
	movb	(%esi),%al			// get first source texel
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp1
	movw	%bp,(%ecx)
	movb	%al,(%edi)			// store first dest pixel
Lp1:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx			// advance tfrac fractional part by tstep frac

	sbbl	%eax,%eax			// turn tstep carry into -1 (0 if none)
	addl	C(sstep),%ebx			// advance sfrac fractional part by sstep frac
	adcl	C(advancetable)+4(,%eax,4),%esi	// point to next source texel

	cmpw	2(%ecx),%bp
	jl		Lp2
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp2
	movw	%bp,2(%ecx)
	movb	%al,1(%edi)
Lp2:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

	cmpw	4(%ecx),%bp
	jl		Lp3
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp3
	movw	%bp,4(%ecx)
	movb	%al,2(%edi)
Lp3:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

	cmpw	6(%ecx),%bp
	jl		Lp4
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp4
	movw	%bp,6(%ecx)
	movb	%al,3(%edi)
Lp4:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

	cmpw	8(%ecx),%bp
	jl		Lp5
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp5
	movw	%bp,8(%ecx)
	movb	%al,4(%edi)
Lp5:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

//
// start FDIV for end of next segment in flight, so it can overlap
//
	popl	%eax
	cmpl	$8,%eax			// more than one segment after this?
	ja		LSetupNotLast2	// yes

	decl	%eax
	jz		LFDIVInFlight2	// if only one pixel, no need to start an FDIV
	movl	%eax,C(spancountminus1)
	fildl	C(spancountminus1)

	flds	C(d_zistepu)		// _d_zistepu | spancountminus1
	fmul	%st(1),%st(0)	// _d_zistepu*scm1 | scm1
	flds	C(d_tdivzstepu)	// _d_tdivzstepu | _d_zistepu*scm1 | scm1
	fmul	%st(2),%st(0)	// _d_tdivzstepu*scm1 | _d_zistepu*scm1 | scm1
	fxch	%st(1)			// _d_zistepu*scm1 | _d_tdivzstepu*scm1 | scm1
	faddp	%st(0),%st(3)	// _d_tdivzstepu*scm1 | scm1
	fxch	%st(1)			// scm1 | _d_tdivzstepu*scm1
	fmuls	C(d_sdivzstepu)	// _d_sdivzstepu*scm1 | _d_tdivzstepu*scm1
	fxch	%st(1)			// _d_tdivzstepu*scm1 | _d_sdivzstepu*scm1
	faddp	%st(0),%st(3)	// _d_sdivzstepu*scm1
	flds	C(fp_64k)			// 64k | _d_sdivzstepu*scm1
	fxch	%st(1)			// _d_sdivzstepu*scm1 | 64k
	faddp	%st(0),%st(4)	// 64k

	fdiv	%st(1),%st(0)	// this is what we've gone to all this trouble to
							//  overlap
	jmp		LFDIVInFlight2

	.align	4
LSetupNotLast2:
	fadds	C(zi8stepu)
	fxch	%st(2)
	fadds	C(sdivz8stepu)
	fxch	%st(2)
	flds	C(tdivz8stepu)
	faddp	%st(0),%st(2)
	flds	C(fp_64k)
	fdiv	%st(1),%st(0)	// z = 1/1/z
							// this is what we've gone to all this trouble to
							//  overlap
LFDIVInFlight2:
	pushl	%eax

	cmpw	10(%ecx),%bp
	jl		Lp6
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp6
	movw	%bp,10(%ecx)
	movb	%al,5(%edi)
Lp6:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

	cmpw	12(%ecx),%bp
	jl		Lp7
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp7
	movw	%bp,12(%ecx)
	movb	%al,6(%edi)
Lp7:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

	cmpw	14(%ecx),%bp
	jl		Lp8
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp8
	movw	%bp,14(%ecx)
	movb	%al,7(%edi)
Lp8:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

	addl	$8,%edi
	addl	$16,%ecx
	movl	%edx,C(tfracf)
	movl	C(snext),%edx
	movl	%ebx,C(sfracf)
	movl	C(tnext),%ebx
	movl	%edx,C(s)
	movl	%ebx,C(t)

	movl	%ecx,C(pz)
	movl	%ebp,C(izi)

	popl	%ecx				// retrieve count

//
// determine whether last span or not
//
	cmpl	$8,%ecx				// are there multiple segments remaining?
	ja		LNotLastSegment		// yes

//
// last segment of scan
//
LLastSegment:

//
// advance s/z, t/z, and 1/z, and calculate s & t at end of span and steps to
// get there. The number of pixels left is variable, and we want to land on the
// last pixel, not step one past it, so we can't run into arithmetic problems
//
	testl	%ecx,%ecx
	jz		LNoSteps		// just draw the last pixel and we're done

// pick up after the FDIV that was left in flight previously


	fld		%st(0)			// duplicate it
	fmul	%st(4),%st(0)	// s = s/z * z
	fxch	%st(1)
	fmul	%st(3),%st(0)	// t = t/z * z
	fxch	%st(1)
	fistpl	C(snext)
	fistpl	C(tnext)

	movl	C(tadjust),%ebx
	movl	C(sadjust),%eax

	addl	C(snext),%eax
	addl	C(tnext),%ebx

	movl	C(bbextents),%ebp
	movl	C(bbextentt),%edx

	cmpl	$2048,%eax
	jl		LClampLow4
	cmpl	%ebp,%eax
	ja		LClampHigh4
LClampReentry4:
	movl	%eax,C(snext)

	cmpl	$2048,%ebx
	jl		LClampLow5
	cmpl	%edx,%ebx
	ja		LClampHigh5
LClampReentry5:

	cmpl	$1,%ecx			// don't bother 
	je		LOnlyOneStep	// if two pixels in segment, there's only one step,
							//  of the segment length
	subl	C(s),%eax
	subl	C(t),%ebx

	addl	%eax,%eax		// convert to 15.17 format so multiply by 1.31
	addl	%ebx,%ebx		//  reciprocal yields 16.48
	imull	C(reciprocal_table)-8(,%ecx,4) // sstep = (snext - s) / (spancount-1)
	movl	%edx,%ebp

	movl	%ebx,%eax
	imull	C(reciprocal_table)-8(,%ecx,4) // tstep = (tnext - t) / (spancount-1)

LSetEntryvec:
//
// set up advancetable
//
	movl	C(spr8entryvec_table)(,%ecx,4),%ebx
	movl	%edx,%eax
	pushl	%ebx				// entry point into code for RET later
	movl	%ebp,%ecx
	sarl	$16,%ecx			// sstep >>= 16;
	movl	C(cachewidth),%ebx
	sarl	$16,%edx			// tstep >>= 16;
	jz		LIsZeroLast
	imull	%ebx,%edx			// (tstep >> 16) * cachewidth;
LIsZeroLast:
	addl	%ecx,%edx			// add in sstep
								// (tstep >> 16) * cachewidth + (sstep >> 16);
	movl	C(tfracf),%ecx
	movl	%edx,C(advancetable)+4	// advance base in t
	addl	%ebx,%edx			// ((tstep >> 16) + 1) * cachewidth +
								//  (sstep >> 16);
	shll	$16,%ebp			// left-justify sstep fractional part
	movl	C(sfracf),%ebx
	shll	$16,%eax			// left-justify tstep fractional part
	movl	%edx,C(advancetable)	// advance extra in t

	movl	%eax,C(tstep)
	movl	%ebp,C(sstep)
	movl	%ecx,%edx

	movl	C(pz),%ecx
	movl	C(izi),%ebp

	ret							// jump to the number-of-pixels handler

//----------------------------------------

LNoSteps:
	movl	C(pz),%ecx
	subl	$7,%edi			// adjust for hardwired offset
	subl	$14,%ecx
	jmp		LEndSpan


LOnlyOneStep:
	subl	C(s),%eax
	subl	C(t),%ebx
	movl	%eax,%ebp
	movl	%ebx,%edx
	jmp		LSetEntryvec

//----------------------------------------

.globl	C(Spr8Entry2_8)
C(Spr8Entry2_8):
	subl	$6,%edi		// adjust for hardwired offsets
	subl	$12,%ecx
	movb	(%esi),%al
	jmp		LLEntry2_8

//----------------------------------------

.globl	C(Spr8Entry3_8)
C(Spr8Entry3_8):
	subl	$5,%edi		// adjust for hardwired offsets
	subl	$10,%ecx
	jmp		LLEntry3_8

//----------------------------------------

.globl	C(Spr8Entry4_8)
C(Spr8Entry4_8):
	subl	$4,%edi		// adjust for hardwired offsets
	subl	$8,%ecx
	jmp		LLEntry4_8

//----------------------------------------

.globl	C(Spr8Entry5_8)
C(Spr8Entry5_8):
	subl	$3,%edi		// adjust for hardwired offsets
	subl	$6,%ecx
	jmp		LLEntry5_8

//----------------------------------------

.globl	C(Spr8Entry6_8)
C(Spr8Entry6_8):
	subl	$2,%edi		// adjust for hardwired offsets
	subl	$4,%ecx
	jmp		LLEntry6_8

//----------------------------------------

.globl	C(Spr8Entry7_8)
C(Spr8Entry7_8):
	decl	%edi		// adjust for hardwired offsets
	subl	$2,%ecx
	jmp		LLEntry7_8

//----------------------------------------

.globl	C(Spr8Entry8_8)
C(Spr8Entry8_8):
	cmpw	(%ecx),%bp
	jl		Lp9
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp9
	movw	%bp,(%ecx)
	movb	%al,(%edi)
Lp9:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi
LLEntry7_8:
	cmpw	2(%ecx),%bp
	jl		Lp10
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp10
	movw	%bp,2(%ecx)
	movb	%al,1(%edi)
Lp10:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi
LLEntry6_8:
	cmpw	4(%ecx),%bp
	jl		Lp11
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp11
	movw	%bp,4(%ecx)
	movb	%al,2(%edi)
Lp11:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi
LLEntry5_8:
	cmpw	6(%ecx),%bp
	jl		Lp12
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp12
	movw	%bp,6(%ecx)
	movb	%al,3(%edi)
Lp12:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi
LLEntry4_8:
	cmpw	8(%ecx),%bp
	jl		Lp13
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp13
	movw	%bp,8(%ecx)
	movb	%al,4(%edi)
Lp13:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi
LLEntry3_8:
	cmpw	10(%ecx),%bp
	jl		Lp14
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp14
	movw	%bp,10(%ecx)
	movb	%al,5(%edi)
Lp14:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi
LLEntry2_8:
	cmpw	12(%ecx),%bp
	jl		Lp15
	movb	(%esi),%al
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp15
	movw	%bp,12(%ecx)
	movb	%al,6(%edi)
Lp15:
	addl	C(izistep),%ebp
	adcl	$0,%ebp
	addl	C(tstep),%edx
	sbbl	%eax,%eax
	addl	C(sstep),%ebx
	adcl	C(advancetable)+4(,%eax,4),%esi

LEndSpan:
	cmpw	14(%ecx),%bp
	jl		Lp16
	movb	(%esi),%al		// load first texel in segment
	cmpb	$(TRANSPARENT_COLOR),%al
	jz		Lp16
	movw	%bp,14(%ecx)
	movb	%al,7(%edi)
Lp16:

//
// clear s/z, t/z, 1/z from FP stack
//
	fstp %st(0)
	fstp %st(0)
	fstp %st(0)

	popl	%ebx				// restore spans pointer
LNextSpan:
	addl	$(sspan_t_size),%ebx // point to next span
	movl	sspan_t_count(%ebx),%ecx
	cmpl	$0,%ecx				// any more spans?
	jg		LSpanLoop			// yes
	jz		LNextSpan			// yes, but this one's empty

	popl	%ebx				// restore register variables
	popl	%esi
	popl	%edi
	popl	%ebp				// restore the caller's stack frame
	ret

#endif	// USE_INTEL_ASM

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
