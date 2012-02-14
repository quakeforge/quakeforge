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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include "QF/sys.h"

#include "d_local.h"
#include "r_internal.h"


void
D_DrawParticle (particle_t *pparticle)
{
	vec3_t      local, transformed;
	float       zi;
	short      *pz;
	int         i, izi, pix, count, u, v;

	// transform point
	VectorSubtract (pparticle->org, r_origin, local);

	transformed[0] = DotProduct (local, r_pright);
	transformed[1] = DotProduct (local, r_pup);
	transformed[2] = DotProduct (local, r_ppn);

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

	// project the point
	// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int) (xcenter + zi * transformed[0] + 0.5);
	v = (int) (ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) ||
		(u > d_vrectright_particle) || (v < d_vrecty) || (u < d_vrectx)) {
		return;
	}

	pz = d_pzbuffer + (d_zwidth * v) + u;
	izi = (int) (zi * 0x8000);

	pix = izi >> d_pix_shift;

	if (pix < d_pix_min)
		pix = d_pix_min;
	else if (pix > d_pix_max)
		pix = d_pix_max;

	switch(r_pixbytes)
	{
	case 1:
		{
			byte *pdest = (byte *) d_viewbuffer + d_scantable[v] + u,
				pixcolor = pparticle->color;
			switch (pix) {
				case 1:
					count = 1 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}
					}
					break;
				case 2:
					count = 2 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = 3 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = 4 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = pix << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
			unsigned short *pdest = (unsigned short *) d_viewbuffer +
				d_scantable[v] + u,
				pixcolor = d_8to16table[(int) pparticle->color];
			switch (pix) {
				case 1:
					count = 1 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}
					}
					break;
				case 2:
					count = 2 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = 3 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = 4 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = pix << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
			int *pdest = (int *) d_viewbuffer + d_scantable[v] + u,
				pixcolor = d_8to24table[(int) pparticle->color];
			switch (pix) {
				case 1:
					count = 1 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
						if (pz[0] <= izi) {
							pz[0] = izi;
							pdest[0] = pixcolor;
						}
					}
					break;
				case 2:
					count = 2 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = 3 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = 4 << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
					count = pix << d_y_aspect_shift;

					for (; count; count--, pz += d_zwidth,
							 pdest += screenwidth) {
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
		Sys_Error("D_DrawParticles: unsupported r_pixbytes %i", r_pixbytes);
	}
}
