/*
	snd_mixa.S

	x86 assembly-language sound code

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

	.text

	.extern C(snd_scaletable)
	.extern C(paintbuffer)
	.extern C(snd_linear_count)
	.extern C(snd_p)
	.extern C(snd_vol)
	.extern C(snd_out)

//----------------------------------------------------------------------
// 8-bit sound-mixing code
//----------------------------------------------------------------------

#define ch		4+16
#define sc		8+16
#define count	12+16

.globl C(NOT_SND_PaintChannelFrom8)
#ifdef PIC
	.type	C(NOT_SND_PaintChannelFrom8),@function
#endif
C(NOT_SND_PaintChannelFrom8):
	pushl	%esi				// preserve register variables
	pushl	%edi
	pushl	%ebx
	pushl	%ebp

//	int 	data;
//	short	*lscale, *rscale;
//	unsigned char *sfx;
//	int		i;

	movl	ch(%esp),%ebx
	movl	sc(%esp),%esi

//	if (ch->leftvol > 255)
//		ch->leftvol = 255;
//	if (ch->rightvol > 255)
//		ch->rightvol = 255;
	movl	ch_leftvol(%ebx),%eax
	movl	ch_rightvol(%ebx),%edx
	cmpl	$255,%eax
	jna		LLeftSet
	movl	$255,%eax
LLeftSet:
	cmpl	$255,%edx
	jna		LRightSet
	movl	$255,%edx
LRightSet:

//	lscale = snd_scaletable[ch->leftvol >> 3];
//	rscale = snd_scaletable[ch->rightvol >> 3];
//	sfx = (signed char *)sc->data + ch->pos;
//	ch->pos += count;
	andl	$0xF8,%eax
	addl	$(sfxc_data),%esi
	andl	$0xF8,%edx
	movl	ch_pos(%ebx),%edi
	movl	count(%esp),%ecx
	addl	%edi,%esi
	shll	$7,%eax
	addl	%ecx,%edi
	addl	%ecx,%esi
	shll	$7,%edx
	movl	%edi,ch_pos(%ebx)

#ifdef PIC
	call	.Lpic1
.Lpic1:	popl	%ebx
	addl	$C(_GLOBAL_OFFSET_TABLE_)+[.-.Lpic1],%ebx

	movl	C(snd_scaletable)@GOT(%ebx,%eax),%eax
	movl	C(snd_scaletable)@GOT(%ebx,%edx),%edx

	movl	C(paintbuffer)@GOT(%ebx),%ebx
#else
	leal	C(snd_scaletable)(%eax),%eax
	leal	C(snd_scaletable)(%edx),%edx

	leal	C(paintbuffer),%ebx
#endif
	pushl	%ebx			// save paintbuffer offset

	testl	$1,%ecx
	leal	0(%ebx,%ecx,psp_size),%ecx
	movl	$0,%ebx
	movb	-1(%esi),%bl
	jz		LMix8Loop

	movl	(%eax,%ebx,4),%edi
	movl	(%edx,%ebx,4),%ebp
	addl	psp_left-psp_size(%ecx),%edi
	addl	psp_right-psp_size(%ecx),%ebp
	movl	%edi,psp_left-psp_size(%ecx)
	movl	%ebp,psp_right-psp_size(%ecx)
	movb	-2(%esi),%bl

	subl	$psp_size,%ecx
	decl	%esi
	cmpl	(%esp),%ecx
	jz		LDone

//	for (i=0 ; i<count ; i++)
//	{
LMix8Loop:

//		data = sfx[i];
//		paintbuffer[i].left += lscale[data];
//		paintbuffer[i].right += rscale[data];
	movl	(%eax,%ebx,4),%edi
	movl	(%edx,%ebx,4),%ebp
	addl	psp_left-psp_size(%ecx),%edi
	addl	psp_right-psp_size(%ecx),%ebp
	movb	-2(%esi),%bl
	movl	%edi,psp_left-psp_size(%ecx)
	movl	%ebp,psp_right-psp_size(%ecx)

	movl	(%eax,%ebx,4),%edi
	movl	(%edx,%ebx,4),%ebp
	movb	-3(%esi),%bl
	addl	psp_left-psp_size*2(%ecx),%edi
	addl	psp_right-psp_size*2(%ecx),%ebp
	movl	%edi,psp_left-psp_size*2(%ecx)
	movl	%ebp,psp_right-psp_size*2(%ecx)

//	}
	subl	$2*psp_size,%ecx
	subl	$2,%esi
	cmpl	(%esp),%ecx
	jnz		LMix8Loop

LDone:
	addl	$4,%esp			// remove paintbuffer offset
	popl	%ebp
	popl	%ebx
	popl	%edi
	popl	%esi

	ret
#ifdef PIC
.Lfe1:
	.size	C(NOT_SND_PaintChannelFrom8),.Lfe1-C(NOT_SND_PaintChannelFrom8)
#endif

//----------------------------------------------------------------------
// Transfer of stereo buffer to 16-bit DMA buffer code
//----------------------------------------------------------------------

.globl C(SND_WriteLinearBlastStereo16)
#ifdef PIC
	.type	C(SND_WriteLinearBlastStereo16),@function
#endif
C(SND_WriteLinearBlastStereo16):
	pushl	%esi				// preserve register variables
	pushl	%edi
	pushl	%ebx

#ifdef PIC
	call	.Lpic2
.Lpic2:
	popl	%ebx
	addl	$C(_GLOBAL_OFFSET_TABLE_)+[.-.Lpic2],%ebx
#endif

//	int		i;
//	int		val;
#ifdef PIC
    movl	C(snd_linear_count)@GOTOFF(%ebx),%ecx
	movl	C(snd_vol)@GOTOFF(%ebx),%esi
	movl	C(snd_out)@GOTOFF(%ebx),%edi
	movl	C(snd_p)@GOTOFF(%ebx),%ebx
#else
	movl	C(snd_linear_count),%ecx
	movl	C(snd_vol),%esi
	movl	C(snd_out),%edi
	movl	C(snd_p),%ebx
#endif

//	for (i=0 ; i<snd_linear_count ; i+=2)
//	{
LWLBLoopTop:

//		val = (snd_p[i]*snd_vol)>>8;
//		if (val > 0x7fff)
//			snd_out[i] = 0x7fff;
//		else if (val < (short)0x8000)
//			snd_out[i] = (short)0x8000;
//		else
//			snd_out[i] = val;
	movl	-8(%ebx,%ecx,4),%eax
	imull	%esi,%eax
	sarl	$8,%eax
	cmpl	$0x7FFF,%eax
	jg		LClampHigh
	cmpl	$0xFFFF8000,%eax
	jnl		LClampDone
	movl	$0xFFFF8000,%eax
	jmp		LClampDone
LClampHigh:
	movl	$0x7FFF,%eax
LClampDone:

//		val = (snd_p[i+1]*snd_vol)>>8;
//		if (val > 0x7fff)
//			snd_out[i+1] = 0x7fff;
//		else if (val < (short)0x8000)
//			snd_out[i+1] = (short)0x8000;
//		else
//			snd_out[i+1] = val;
	movl	-4(%ebx,%ecx,4),%edx
	imull	%esi,%edx
	sarl	$8,%edx
	cmpl	$0x7FFF,%edx
	jg		LClampHigh2
	cmpl	$0xFFFF8000,%edx
	jnl		LClampDone2
	movl	$0xFFFF8000,%edx
	jmp		LClampDone2
LClampHigh2:
	movl	$0x7FFF,%edx
LClampDone2:
	shll	$16,%edx
	andl	$0xFFFF,%eax
	orl		%eax,%edx
	movl	%edx,-4(%edi,%ecx,2)

//	}
	subl	$2,%ecx
	jnz		LWLBLoopTop

//	snd_p += snd_linear_count;

	popl	%ebx
	popl	%edi
	popl	%esi

	ret
#ifdef PIC
.Lfe2:
	.size	C(SND_WriteLinearBlastStereo16),.Lfe2-C(SND_WriteLinearBlastStereo16)
#endif


#endif	// USE_INTEL_ASM

