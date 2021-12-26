/*
	d_part.c

	software driver module for drawing particles

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define NH_DEFINE
#include "namehack.h"

#include "QF/sys.h"

#include "d_local.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"


void
sw32_D_DrawParticle (particle_t *pparticle)
{
	vec3_t      local, transformed;
	float       zi;
	short      *pz;
	int         i, izi, pix, count, u, v;

	// transform point
	VectorSubtract (pparticle->pos, r_origin, local);

	transformed[0] = DotProduct (local, r_pright);
	transformed[1] = DotProduct (local, r_pup);
	transformed[2] = DotProduct (local, r_ppn);

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

	// project the point
	// FIXME: preadjust sw32_xcenter and sw32_ycenter
	zi = 1.0 / transformed[2];
	u = (int) (sw32_xcenter + zi * transformed[0] + 0.5);
	v = (int) (sw32_ycenter - zi * transformed[1] + 0.5);

	if ((v > sw32_d_vrectbottom_particle)
		|| (u > sw32_d_vrectright_particle)
		|| (v < sw32_d_vrecty) || (u < sw32_d_vrectx)) {
		return;
	}

	pz = sw32_d_pzbuffer + (sw32_d_zwidth * v) + u;
	izi = (int) (zi * 0x8000);

	pix = izi >> sw32_d_pix_shift;

	if (pix < sw32_d_pix_min)
		pix = sw32_d_pix_min;
	else if (pix > sw32_d_pix_max)
		pix = sw32_d_pix_max;

	switch(sw32_ctx->pixbytes)
	{
	case 1:
		{
			byte *pdest = (byte *) sw32_d_viewbuffer + sw32_d_scantable[v] + u,
				pixcolor = pparticle->icolor;
			switch (pix) {
				case 1:
					count = 1 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}
					}
					break;
				case 2:
					count = 2 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}
					}
					break;
				case 3:
					count = 3 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}

						if (pz[2] <= izi) {
							pz[2] = izi;
							pdest[2] = pixcolor;
						}
					}
					break;
				case 4:
					count = 4 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}

						if (pz[2] <= izi) {
							pz[2] = izi;
							pdest[2] = pixcolor;
						}

						if (pz[3] <= izi) {
							pz[3] = izi;
							pdest[3] = pixcolor;
						}
					}
					break;
				default:
					count = pix << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						for (i = 0; i < pix; i++) {
							if (pz[i] <= izi) {
								pz[i] = izi;
								pdest[i] = pixcolor;
							}
						}
					}
					break;
			}
		}
		break;
	case 2:
		{
			unsigned short *pdest = (unsigned short *) sw32_d_viewbuffer +
				sw32_d_scantable[v] + u,
				pixcolor = sw32_8to16table[(int) pparticle->icolor];
			switch (pix) {
				case 1:
					count = 1 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}
					}
					break;
				case 2:
					count = 2 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}
					}
					break;
				case 3:
					count = 3 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}

						if (pz[2] <= izi) {
							pz[2] = izi;
							pdest[2] = pixcolor;
						}
					}
					break;
				case 4:
					count = 4 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}

						if (pz[2] <= izi) {
							pz[2] = izi;
							pdest[2] = pixcolor;
						}

						if (pz[3] <= izi) {
							pz[3] = izi;
							pdest[3] = pixcolor;
						}
					}
					break;
				default:
					count = pix << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						for (i = 0; i < pix; i++) {
							if (pz[i] <= izi) {
								pz[i] = izi;
								pdest[i] = pixcolor;
							}
						}
					}
					break;
			}
		}
		break;
	case 4:
		{
			int *pdest = (int *) sw32_d_viewbuffer + sw32_d_scantable[v] + u,
				pixcolor = d_8to24table[(int) pparticle->icolor];
			switch (pix) {
				case 1:
					count = 1 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}
					}
					break;
				case 2:
					count = 2 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}
					}
					break;
				case 3:
					count = 3 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}

						if (pz[2] <= izi) {
							pz[2] = izi;
							pdest[2] = pixcolor;
						}
					}
					break;
				case 4:
					count = 4 << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}

						if (pz[1] <= izi) {
							pz[1] = izi;
							pdest[1] = pixcolor;
						}

						if (pz[2] <= izi) {
							pz[2] = izi;
							pdest[2] = pixcolor;
						}

						if (pz[3] <= izi) {
							pz[3] = izi;
							pdest[3] = pixcolor;
						}
					}
					break;
				default:
					count = pix << sw32_d_y_aspect_shift;

					for (; count; count--, pz += sw32_d_zwidth,
							 pdest += sw32_screenwidth) {
						for (i = 0; i < pix; i++) {
							if (pz[i] <= izi) {
								pz[i] = izi;
								pdest[i] = pixcolor;
							}
						}
					}
					break;
			}
		}
		break;
	default:
		Sys_Error("D_DrawParticles: unsupported r_pixbytes %i",
				  sw32_ctx->pixbytes);
	}
}
