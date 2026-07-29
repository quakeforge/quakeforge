#include <PropertyList.h>
#include <math.h>

#include "orbit.h"

@implementation Orbit

#define MAX_ITERATIONS 7
#define PI M_PI
#define THRESH 1e-12
#define MIN_THRESH 1e-14
#define CUBE_ROOT(X)  (exp (log (X) / 3))

/* If the eccentricity is very close to parabolic,  and the eccentric
anomaly is quite low,  you can get an unfortunate situation where
roundoff error keeps you from converging.  Consider the just-barely-
elliptical case,  where in Kepler's equation,

M = E - e sin( E)

   E and e sin( E) can be almost identical quantities.  To work
around this,  near_parabolic( ) computes E - e sin( E) by expanding
the sine function as a power series:

E - e sin( E) = E - e( E - E^3/3! + E^5/5! - ...)
= (1-e)E + e( -E^3/3! + E^5/5! - ...)

   It's a little bit expensive to do this,  and you only need do it
quite rarely.  (I only encountered the problem because I had orbits
that were supposed to be 'pure parabolic',  but due to roundoff,
they had e = 1+/- epsilon,  with epsilon _very_ small.)  So 'near_parabolic'
is only called if we've gone seven iterations without converging. */

static double near_parabolic (const double ecc_anom, const double e)
{
	double ecc_anom2 = ecc_anom * ecc_anom;
	const double anom2 = (e > 1 ? ecc_anom2 : -ecc_anom2);
	//FIXME ecc_anom * ecc_anom gets evaluated on only one side of : but is
	//used on both
	//const double anom2 = (e > 1 ? ecc_anom * ecc_anom : -ecc_anom * ecc_anom);
	double term = e * anom2 * ecc_anom / 6;
	double rval = (1 - e) * ecc_anom - term;
	unsigned n = 4;

	while (fabs (term) > 1e-15) {
		term *= anom2 / (double) (n * (n + 1));
		rval -= term;
		n += 2;
	}
	return (rval);
}

/* For a full description of this function,  see
https://www.projectpluto.com/kepler.htm . There was a long thread about
solutions to Kepler's equation on sci.astro.amateur,  and I decided to
go into excruciating detail as to how it's done below (admittedly,  some
time ago,  and changes have occurred). */

static double
kepler (const double ecc, double mean_anom)
{
	double curr, err, thresh, offset = 0;
	double delta_curr = 1;
	bool is_negative = false;
	unsigned n_iter = 0;

	if (!mean_anom) {
		return 0;
	}

	if (ecc < 1) {
		if (mean_anom < -PI || mean_anom > PI) {
			double tmod = mean_anom;
			tmod = (tmod > 0 ? tmod - PI : tmod + PI) %% (2 * PI) - PI;

			if (tmod > PI) { /* bring mean anom within -pi to +pi */
				tmod -= 2 * PI;
			} else if (tmod < -PI) {
				tmod += 2 * PI;
			}
			offset = mean_anom - tmod;
			mean_anom = tmod;
		}

		if (ecc < 0.9) {   /* low-eccentricity formula from Meeus,  p. 195 */
			curr = atan2 (sin (mean_anom), cos (mean_anom) - ecc);
			/* (usually) one or two correction steps,  and we're done */
			do {
				err = (curr - ecc * sin (curr) - mean_anom) / (1 - ecc * cos (curr));
				curr -= err;
			} while (fabs (err) > THRESH);
			return curr + offset;
		}
	}

	if (mean_anom < 0) {
		mean_anom = -mean_anom;
		is_negative = true;
	}

	curr = mean_anom;
	thresh = THRESH * fabs (1 - ecc);
	/* Due to roundoff error,  there's no way we can hope to */
	/* get below a certain minimum threshhold anyway:        */
	if (thresh < MIN_THRESH) {
		thresh = MIN_THRESH;
	}
	if (ecc > 1 && mean_anom / ecc > 3) {  /* hyperbolic, large-mean-anomaly */
		curr = log (mean_anom / ecc) + 0.85;
	} else if ((ecc > 0.8 && mean_anom < PI / 3) || ecc > 1) {  /* up to 60 degrees */
		double trial = mean_anom / fabs (1 - ecc);

		if (trial * trial > 6 * fabs (1 - ecc)) { /* cubic term is dominant */
			trial = CUBE_ROOT (6 * mean_anom);
		}
		curr = trial;
		if (thresh > THRESH) {     /* happens if e > 2 */
			thresh = THRESH;
		}
	}

	if (ecc < 1) {
		while (fabs (delta_curr) > thresh) {
			if (n_iter++ > MAX_ITERATIONS) {
				err = near_parabolic (curr, ecc) - mean_anom;
			} else {
				err = curr - ecc * sin (curr) - mean_anom;
			}
			delta_curr = -err / (1 - ecc * cos (curr));
			curr += delta_curr;
			//assert (n_iter < 20);
		}
	} else {
		while (fabs (delta_curr) > thresh) {
			if (n_iter++ > MAX_ITERATIONS && ecc < 1.01) {
				err = -near_parabolic (curr, ecc) - mean_anom;
			} else {
				err = ecc * sinh (curr) - curr - mean_anom;
			}
			delta_curr = -err / (ecc * cosh (curr) - 1);
			curr += delta_curr;
			//assert (n_iter < 20);
		}
	}
	return is_negative ? offset - curr : offset + curr;
}

-initWithEccentricity:(double)e
{
	if (!(self = [super init])) {
		return nil;
	}
	eccentricity = e;
	return self;
}

+(Orbit *) orbit:(double)e
{
	return [[[Orbit alloc] initWithEccentricity:e] autorelease];
}

-(conic_t)conicData
{
	@algebra (PGA) {
		return {
			.point = e123,
			.s = e032,
			.t = e013,
			.slr = 1,
			.ecc = (float) eccentricity,
			.width = 2,
			.color = 0xfffffff,	//white
		};
	}
}

-(dvec2)ta_sincos:(double)M
{
	double E = kepler (eccentricity, M);
	dvec2 Esc;
	double k = sqrt (fabs (1 - eccentricity) / (1 + eccentricity));
	if (eccentricity > 1) {
		// hyperbolic
		// ν = 2*atan2(sinh(E), k*cosh(E))
		Esc = sincosh (E / 2) * dvec2 (1, k);
	} else if (eccentricity < 1) {
		// elliptic
		// ν = 2*atan2(sin(E), k*cos(E))
		Esc = sincos (E / 2) * dvec2 (1, k);
	} else {
		// parabolic
		// For parabolas, E is already the tangent of half the true anomaly
		// ν = 2*atan(E)
		Esc = { E, 1 };
	}
	dvec2 sc = Esc / sqrt (Esc • Esc);
	// double the angle
	return { 2*sc.x*sc.y, sc.y*sc.y - sc.x*sc.x };
}

@end
