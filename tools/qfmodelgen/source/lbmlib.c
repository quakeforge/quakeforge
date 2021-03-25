/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

// lbmlib.c
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "QF/qendian.h"
#include "QF/quakeio.h"
#include "QF/sys.h"

#include "tools/qfmodelgen/include/lbmlib.h"

static int
LoadFile (const char *fname, byte **buf)
{
	QFile      *file;
	byte       *src;
	int         len;

	*buf = 0;
	file = Qopen (fname, "rt");
	if (!file)
		return 0;
	len = Qfilesize (file);
	src = malloc (len + 1);
	src[Qfilesize (file)] = 0;
	Qread (file, src, len);
	Qclose (file);
	*buf = src;
	return len;
}

static QFile *
SafeOpenWrite (const char *filename)
{
	QFile	*f;

	f = Qopen(filename, "wb");

	if (!f)
		Sys_Error ("Error opening %s: %s",filename,strerror(errno));

	return f;
}

static void
SafeWrite (QFile *f, void *buffer, int count)
{
	if (Qwrite (f, buffer, count) != count)
		Sys_Error ("File read failure");
}

static void
SaveFile (const char *filename, void *buffer, int count)
{
	QFile	*f;

	f = SafeOpenWrite (filename);
	SafeWrite (f, buffer, count);
	Qclose (f);
}

/*
============================================================================

						LBM STUFF

============================================================================
*/

#define FORMID ('F'+('O'<<8)+((int)'R'<<16)+((int)'M'<<24))
#define ILBMID ('I'+('L'<<8)+((int)'B'<<16)+((int)'M'<<24))
#define PBMID  ('P'+('B'<<8)+((int)'M'<<16)+((int)' '<<24))
#define BMHDID ('B'+('M'<<8)+((int)'H'<<16)+((int)'D'<<24))
#define BODYID ('B'+('O'<<8)+((int)'D'<<16)+((int)'Y'<<24))
#define CMAPID ('C'+('M'<<8)+((int)'A'<<16)+((int)'P'<<24))


bmhd_t  bmhd;

static int
Align (int l)
{
	if (l&1)
		return l+1;
	return l;
}

/*
================
LBMRLEdecompress

Source must be evenly aligned!
================
*/
static byte *
LBMRLEDecompress (byte *source, byte *unpacked, int bpwidth)
{
	int     count;
	byte    b, rept;

	count = 0;

	do {
		rept = *source++;

		if (rept > 0x80) {
			rept = (rept ^ 0xff) + 2;
			b = *source++;
			memset (unpacked, b, rept);
			unpacked += rept;
		} else if (rept < 0x80) {
			rept++;
			memcpy (unpacked, source, rept);
			unpacked += rept;
			source += rept;
		} else
			rept = 0;               // rept of 0x80 is NOP

		count += rept;

	} while (count < bpwidth);

	if (count > bpwidth)
		Sys_Error ("Decompression exceeded width!\n");


	return source;
}

#define BPLANESIZE      128
byte    bitplanes[9][BPLANESIZE];       // max size 1024 by 9 bit planes

/*
=================
MungeBitPlanes8

Asm version destroys the bit plane data!
=================
*/
static void
MungeBitPlanes8 (int width, byte *dest)
{
	int		i, ind = 0;

	while (width--) {
		for (i = 0; i < 8; i++) {
			*dest++ = (((bitplanes[7][ind] << i) & 128) >> 0)
				| (((bitplanes[6][ind] << i) & 128) >> 1)
				| (((bitplanes[5][ind] << i) & 128) >> 2)
				| (((bitplanes[4][ind] << i) & 128) >> 3)
				| (((bitplanes[3][ind] << i) & 128) >> 4)
				| (((bitplanes[2][ind] << i) & 128) >> 5)
				| (((bitplanes[1][ind] << i) & 128) >> 6)
				| (((bitplanes[0][ind] << i) & 128) >> 7);
		}
		ind++;
	}
#if 0
asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*7 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*6 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*5 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*4 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*3 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*2 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*1 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}

static void
MungeBitPlanes4 (int width, byte *dest)
{
	int		i, ind = 0;

	while (width--) {
		for (i = 0; i < 8; i++) {
			*dest++ = (((bitplanes[3][ind] << i) & 128) >> 4)
				| (((bitplanes[2][ind] << i) & 128) >> 5)
				| (((bitplanes[1][ind] << i) & 128) >> 6)
				| (((bitplanes[0][ind] << i) & 128) >> 7);
		}
		ind++;
	}
#if 0

asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     xor     al,al
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*3 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*2 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*1 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}

static void
MungeBitPlanes2 (int width, byte *dest)
{
	int		i, ind = 0;

	while (width--) {
		for (i = 0; i < 8; i++) {
			*dest++ = (((bitplanes[1][ind] << i) & 128) >> 6)
					 | (((bitplanes[0][ind] << i) & 128) >> 7);
		}
		ind++;
	}
#if 0
asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     xor     al,al
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*1 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}

static void
MungeBitPlanes1 (int width, byte *dest)
{
	int		i, ind = 0;

	while (width--) {
		for (i = 0; i < 8; i++) {
			*dest++ = (((bitplanes[0][ind] << i) & 128) >> 7);
		}
		ind++;
	}
#if 0
asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     xor     al,al
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}

void
LoadLBM (char *filename, byte **picture, byte **palette)
{
	byte    *LBMbuffer, *picbuffer, *cmapbuffer;
	byte    *LBM_P, *LBMEND_P;
	byte    *pic_p;
	byte    *body_p;
	unsigned rowsize;
	int		 y, p, planes;
	int		 formtype, formlength;
	int		 chunktype, chunklength;

	void (*mungecall) (int, byte *);

// quiet compiler warnings
	picbuffer = NULL;
	cmapbuffer = NULL;
	mungecall = NULL;

// load the LBM
	LoadFile (filename, &LBMbuffer);

// parse the LBM header
	LBM_P = LBMbuffer;
	if (*(uint32_t *) LBMbuffer != LittleLong (FORMID))
	   Sys_Error ("No FORM ID at start of file!\n");

	LBM_P += 4;
	formlength = BigLong (*(uint32_t *) LBM_P );
	LBM_P += 4;
	LBMEND_P = LBM_P + Align (formlength);

	formtype = LittleLong (*(uint32_t *) LBM_P);

	if (formtype != ILBMID && formtype != PBMID)
		Sys_Error ("Unrecognized form type: %c%c%c%c\n", formtype & 0xff,
			   (formtype >> 8) & 0xff, (formtype >> 16) & 0xff,
			   (formtype >> 24) & 0xff);

	LBM_P += 4;

// parse chunks
	while (LBM_P < LBMEND_P) {
		chunktype = LBM_P[0] + (LBM_P[1] << 8) + (LBM_P[2] << 16)
			+ (LBM_P[3] << 24);
		LBM_P += 4;
		chunklength = LBM_P[3] + (LBM_P[2] << 8) + (LBM_P[1] << 16)
			+ (LBM_P[0] << 24);
		LBM_P += 4;

		switch (chunktype) {
		case BMHDID:
			memcpy (&bmhd, LBM_P, sizeof (bmhd));
			bmhd.w = BigShort (bmhd.w);
			bmhd.h = BigShort (bmhd.h);
			bmhd.x = BigShort (bmhd.x);
			bmhd.y = BigShort (bmhd.y);
			bmhd.pageWidth = BigShort (bmhd.pageWidth);
			bmhd.pageHeight = BigShort (bmhd.pageHeight);
			break;

		case CMAPID:
			cmapbuffer = malloc (768);
			memset (cmapbuffer, 0, 768);
			memcpy (cmapbuffer, LBM_P, chunklength);
			break;

		case BODYID:
			body_p = LBM_P;

			pic_p = picbuffer = malloc (bmhd.w * bmhd.h);
			if (formtype == PBMID) {
			// unpack PBM
				for (y = 0; y < bmhd.h; y++, pic_p += bmhd.w) {
					if (bmhd.compression == cm_rle1)
						body_p = LBMRLEDecompress ((byte *) body_p , pic_p,
												   bmhd.w);
					else if (bmhd.compression == cm_none) {
						memcpy (pic_p,body_p,bmhd.w);
						body_p += Align(bmhd.w);
					}
				}
			} else {
			// unpack ILBM
				planes = bmhd.nPlanes;
				if (bmhd.masking == ms_mask)
					planes++;
				rowsize = (bmhd.w + 15) / 16 * 2;
				switch (bmhd.nPlanes) {
				case 1:
					mungecall = MungeBitPlanes1;
					break;
				case 2:
					mungecall = MungeBitPlanes2;
					break;
				case 4:
					mungecall = MungeBitPlanes4;
					break;
				case 8:
					mungecall = MungeBitPlanes8;
					break;
				default:
					Sys_Error ("Can't munge %i bit planes!\n",bmhd.nPlanes);
				}

				for (y = 0; y < bmhd.h; y++, pic_p += bmhd.w) {
					for (p = 0; p < planes; p++)
						if (bmhd.compression == cm_rle1)
							body_p = LBMRLEDecompress ((byte *) body_p,
													   bitplanes[p], rowsize);
						else if (bmhd.compression == cm_none) {
							memcpy (bitplanes[p], body_p, rowsize);
							body_p += rowsize;
						}

					mungecall (bmhd.w , pic_p);
				}
			}
			break;
		}

		LBM_P += Align (chunklength);
	}

	free (LBMbuffer);

	*picture = picbuffer;
	*palette = cmapbuffer;
}

/*
============================================================================

							WRITE LBM

============================================================================
*/

void
WriteLBMfile (char *filename, byte *data, int width, int height, byte *palette)
{
	byte    *lbm, *lbmptr;
	uint32_t    *formlength, *bmhdlength, *cmaplength, *bodylength;
	int    length;
	bmhd_t  basebmhd;

	lbm = lbmptr = malloc (width*height+1000);

// start FORM
	*lbmptr++ = 'F';
	*lbmptr++ = 'O';
	*lbmptr++ = 'R';
	*lbmptr++ = 'M';

	formlength = (uint32_t*)lbmptr;
	lbmptr+=4;                      // leave space for length

	*lbmptr++ = 'P';
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = ' ';

// write BMHD
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = 'H';
	*lbmptr++ = 'D';

	bmhdlength = (uint32_t *) lbmptr;
	lbmptr += 4;						// leave space for length

	memset (&basebmhd, 0, sizeof (basebmhd));
	basebmhd.w = BigShort ((short) width);
	basebmhd.h = BigShort ((short) height);
	basebmhd.nPlanes = 8;
	basebmhd.xAspect = 5;
	basebmhd.yAspect = 6;
	basebmhd.pageWidth = BigShort ((short) width);
	basebmhd.pageHeight = BigShort ((short) height);

	memcpy (lbmptr, &basebmhd, sizeof (basebmhd));
	lbmptr += sizeof (basebmhd);

	length = lbmptr - (byte *) bmhdlength - 4;
	*bmhdlength = BigLong (length);
	if (length & 1)
		*lbmptr++ = 0;						// pad chunk to even offset

// write CMAP
	*lbmptr++ = 'C';
	*lbmptr++ = 'M';
	*lbmptr++ = 'A';
	*lbmptr++ = 'P';

	cmaplength = (uint32_t *) lbmptr;
	lbmptr += 4;							// leave space for length

	memcpy (lbmptr, palette, 768);
	lbmptr += 768;

	length = lbmptr - (byte *) cmaplength - 4;
	*cmaplength = BigLong (length);
	if (length & 1)
		*lbmptr++ = 0;						// pad chunk to even offset

// write BODY
	*lbmptr++ = 'B';
	*lbmptr++ = 'O';
	*lbmptr++ = 'D';
	*lbmptr++ = 'Y';

	bodylength = (uint32_t *) lbmptr;
	lbmptr += 4;							// leave space for length

	memcpy (lbmptr, data, width * height);
	lbmptr += width * height;

	length = lbmptr - (byte *) bodylength - 4;
	*bodylength = BigLong(length);
	if (length & 1)
		*lbmptr++ = 0;          // pad chunk to even offset

// done
	length = lbmptr - (byte *) formlength - 4;
	*formlength = BigLong (length);
	if (length & 1)
		*lbmptr++ = 0;          // pad chunk to even offset

// write output file
	SaveFile (filename, lbm, lbmptr - lbm);
	free (lbm);
}

