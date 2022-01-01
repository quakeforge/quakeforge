#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/mathlib.h"
#include "QF/simd/vec4d.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

#define right   { 1, 0, 0 }
#define forward { 0, 1, 0 }
#define up      { 0, 0, 1 }
#define one     { 1, 1, 1, 1 }
#define half    { 0.5, 0.5, 0.5, 0.5 }
#define zero    { 0, 0, 0, 0 }
#define qident  { 0, 0, 0, 1 }
#define qtest   { 0.64, 0.48, 0, 0.6 }

#define nright   { -1,  0,  0 }
#define nforward {  0, -1,  0 }
#define nup      {  0,  0, -1 }
#define none     { -1, -1, -1, -1 }
#define nqident  {  0,  0,  0, -1 }

#define pi       {  M_PI, M_PI,  M_PI, M_PI } // lots of bits
#define pmpi     { -M_PI, M_PI, -M_PI, M_PI } // lots of bits

#define identity \
	{	{ 1, 0, 0, 0 }, \
		{ 0, 1, 0, 0 }, \
		{ 0, 0, 1, 0 }, \
		{ 0, 0, 0, 1 } }
#define rotate120 \
	{	{ 0, 1, 0, 0 }, \
		{ 0, 0, 1, 0 }, \
		{ 1, 0, 0, 0 }, \
		{ 0, 0, 0, 1 } }
#define rotate240 \
	{	{ 0, 0, 1, 0 }, \
		{ 1, 0, 0, 0 }, \
		{ 0, 1, 0, 0 }, \
		{ 0, 0, 0, 1 } }

#define s05 0.70710678118654757

#ifdef __AVX2__
typedef  struct {
	int         line;
	vec4d_t   (*op) (vec4d_t a, vec4d_t b);
	vec4d_t     a;
	vec4d_t     b;
	vec4d_t     expect;
	vec4d_t     ulp_errors;
} vec4d_test_t;
#endif

typedef  struct {
	int         line;
	vec4f_t   (*op) (vec4f_t a, vec4f_t b);
	vec4f_t     a;
	vec4f_t     b;
	vec4f_t     expect;
	vec4f_t     ulp_errors;
} vec4f_test_t;

typedef  struct {
	int         line;
	void   (*op) (mat4f_t c, const mat4f_t a, const mat4f_t b);
	mat4f_t     a;
	mat4f_t     b;
	mat4f_t     expect;
	mat4f_t     ulp_errors;
} mat4f_test_t;

typedef  struct {
	int         line;
	vec4f_t   (*op) (const mat4f_t a, vec4f_t b);
	mat4f_t     a;
	vec4f_t     b;
	vec4f_t     expect;
	vec4f_t     ulp_errors;
} mv4f_test_t;

typedef  struct {
	int         line;
	void   (*op) (mat4f_t m, vec4f_t q);
	vec4f_t     q;
	mat4f_t     expect;
	mat4f_t     ulp_errors;
} mq4f_test_t;

#ifdef __AVX2__
static vec4d_t tvtruncd (vec4d_t v, vec4d_t ignore)
{
	return vtrunc4d (v);
}

static vec4d_t tvceild (vec4d_t v, vec4d_t ignore)
{
	return vceil4d (v);
}

static vec4d_t tvfloord (vec4d_t v, vec4d_t ignore)
{
	return vfloor4d (v);
}

static vec4d_t tqconjd (vec4d_t v, vec4d_t ignore)
{
	return qconjd (v);
}
#endif

static vec4f_t tvtruncf (vec4f_t v, vec4f_t ignore)
{
	return vtrunc4f (v);
}

static vec4f_t tvceilf (vec4f_t v, vec4f_t ignore)
{
	return vceil4f (v);
}

static vec4f_t tvfloorf (vec4f_t v, vec4f_t ignore)
{
	return vfloor4f (v);
}

static vec4f_t tqconjf (vec4f_t v, vec4f_t ignore)
{
	return qconjf (v);
}

static vec4f_t tvabsf (vec4f_t v, vec4f_t ignore)
{
	return vabs4f (v);
}

static vec4f_t tvsqrtf (vec4f_t v, vec4f_t ignore)
{
	return vsqrt4f (v);
}

static vec4f_t tmagnitudef (vec4f_t v, vec4f_t ignore)
{
	return magnitudef (v);
}

static vec4f_t tmagnitude3f (vec4f_t v, vec4f_t ignore)
{
	return magnitude3f (v);
}

#define T(t...) { __LINE__, t }

#ifdef __AVX2__
static vec4d_test_t vec4d_tests[] = {
	// 3D dot products
	T(dotd, right,   right,   one  ),
	T(dotd, right,   forward, zero ),
	T(dotd, right,   up,      zero ),
	T(dotd, forward, right,   zero ),
	T(dotd, forward, forward, one  ),
	T(dotd, forward, up,      zero ),
	T(dotd, up,      right,   zero ),
	T(dotd, up,      forward, zero ),
	T(dotd, up,      up,      one  ),

	// one is 4D, so its self dot product is 4
	T(dotd, one,     one,     { 4,  4,  4,  4} ),
	T(dotd, one,    none,     {-4, -4, -4, -4} ),

	// 3D cross products
	T(crossd, right,   right,    zero    ),
	T(crossd, right,   forward,  up      ),
	T(crossd, right,   up,      nforward ),
	T(crossd, forward, right,   nup      ),
	T(crossd, forward, forward,  zero    ),
	T(crossd, forward, up,       right   ),
	T(crossd, up,      right,    forward ),
	T(crossd, up,      forward, nright   ),
	T(crossd, up,      up,       zero    ),
	// double whammy tests: cross product with an angled vector and
	// ensuring that a 4d vector (non-zero w component) does not affect
	// the result, including the result's w component remaining zero.
	T(crossd, right,   one,     { 0, -1,  1} ),
	T(crossd, forward, one,     { 1,  0, -1} ),
	T(crossd, up,      one,     {-1,  1,  0} ),
	T(crossd, one,     right,   { 0,  1, -1} ),
	T(crossd, one,     forward, {-1,  0,  1} ),
	T(crossd, one,     up,      { 1, -1,  0} ),
	// This one fails when optimizing with -mfma (which is why fma is not
	// used): ulp errors in z and w
	T(crossd, qtest,   qtest,   {0, 0, 0, 0} ),

	T(qmuld, qident,  qident,   qident  ),
	T(qmuld, qident,  right,    right   ),
	T(qmuld, qident,  forward,  forward ),
	T(qmuld, qident,  up,       up      ),
	T(qmuld, right,   qident,   right   ),
	T(qmuld, forward, qident,   forward ),
	T(qmuld, up,      qident,   up      ),
	T(qmuld, right,   right,   nqident  ),
	T(qmuld, right,   forward,  up      ),
	T(qmuld, right,   up,      nforward ),
	T(qmuld, forward, right,   nup      ),
	T(qmuld, forward, forward, nqident  ),
	T(qmuld, forward, up,       right   ),
	T(qmuld, up,      right,    forward ),
	T(qmuld, up,      forward, nright   ),
	T(qmuld, up,      up,      nqident  ),
	T(qmuld, one,     one,     { 2, 2, 2, -2 } ),
	T(qmuld, one,     { 2, 2, 2, -2 }, { 0, 0, 0, -8 } ),
	// This one fails when optimizing with -mfma (which is why fma is not
	// used): ulp error in z
	T(qmuld, qtest,   qtest,   {0.768, 0.576, 0, -0.28} ),

	// The one vector is not unit (magnitude 2), so using it as a rotation
	// quaternion results in scaling by 4. However, it still has the effect
	// of rotating 120 degrees around the axis equidistant from the three
	// orthogonal axes such that x->y->z->x
	T(qvmuld, one,    right,     { 0, 4, 0, 0 } ),
	T(qvmuld, one,    forward,   { 0, 0, 4, 0 } ),
	T(qvmuld, one,    up,        { 4, 0, 0, 0 } ),
	T(qvmuld, one,    {1,1,1,0}, { 4, 4, 4, 0 } ),
	T(qvmuld, one,    one,       { 4, 4, 4, -2 } ),
	// inverse rotation, so x->z->y->x
	T(vqmuld, right,     one,    { 0, 0, 4, 0 } ),
	T(vqmuld, forward,   one,    { 4, 0, 0, 0 } ),
	T(vqmuld, up,        one,    { 0, 4, 0, 0 } ),
	T(vqmuld, {1,1,1,0}, one,    { 4, 4, 4, 0 } ),
	T(vqmuld, one,       one,    { 4, 4, 4, -2 } ),
	// The half vector is unit.
	T(qvmuld, half,   right,     forward ),
	T(qvmuld, half,   forward,   up      ),
	T(qvmuld, half,   up,        right   ),
	T(qvmuld, half,   {1,1,1,0}, { 1, 1, 1, 0 } ),
	// inverse
	T(vqmuld, right,     half,   up      ),
	T(vqmuld, forward,   half,   right   ),
	T(vqmuld, up,        half,   forward ),
	T(vqmuld, {1,1,1,0}, half,   { 1, 1, 1, 0 } ),
	// one is a 4D vector and qvmuld is meant for 3D vectors. However, it
	// seems that the vector's w has no effect on the 3d portion of the
	// result, but the result's w is cosine of the full rotation angle
	// scaled by quaternion magnitude and vector w
	T(qvmuld, half,   one,       { 1, 1, 1, -0.5 } ),
	T(qvmuld, half,   {2,2,2,2}, { 2, 2, 2, -1 } ),
	T(qvmuld, qtest,  right,     {0.5392, 0.6144, -0.576, 0} ),
	T(qvmuld, qtest,  forward,   {0.6144, 0.1808, 0.768, 0},
	                             {0, -2.7e-17, 0, 0} ),
	T(qvmuld, qtest,  up,        {0.576, -0.768, -0.28, 0} ),
	// inverse
	T(vqmuld, one,       half,   { 1, 1, 1, -0.5 } ),
	T(vqmuld, {2,2,2,2}, half,   { 2, 2, 2, -1 } ),
	T(vqmuld, right,     qtest,  {0.5392, 0.6144, 0.576, 0} ),
	T(vqmuld, forward,   qtest,  {0.6144, 0.1808, -0.768, 0},
	                             {0, -2.7e-17, 0, 0} ),
	T(vqmuld, up,        qtest,  {-0.576, 0.768, -0.28, 0} ),

	T(qrotd, right,   right,    qident ),
	T(qrotd, right,   forward,  {    0,    0,  s05,  s05 },
	                            {0, 0, -1.1e-16, 0} ),
	T(qrotd, right,   up,       {    0, -s05,    0,  s05 },
	                            {0, 1.1e-16, 0, 0} ),
	T(qrotd, forward, right,    {    0,    0, -s05,  s05 },
	                            {0, 0, 1.1e-16, 0} ),
	T(qrotd, forward, forward,  qident ),
	T(qrotd, forward, up,       {  s05,    0,    0,  s05 },
	                            {-1.1e-16, 0, 0, 0} ),
	T(qrotd, up,      right,    {    0,  s05,    0,  s05 },
	                            {0, -1.1e-16, 0, 0} ),
	T(qrotd, up,      forward,  { -s05,    0,    0,  s05 },
	                            { 1.1e-16, 0, 0, 0} ),
	T(qrotd, up,      up,       qident ),

	T(tvtruncd, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -1, -2 } ),
	T(tvceild,  { 1.1, 2.9, -1.1, -2.9 }, {}, { 2, 3, -1, -2 } ),
	T(tvfloord, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -2, -3 } ),
	T(tqconjd,  one, {}, { -1, -1, -1, 1 } ),
};
#define num_vec4d_tests (sizeof (vec4d_tests) / (sizeof (vec4d_tests[0])))
#endif

static vec4f_test_t vec4f_tests[] = {
	// 3D dot products
	T(dotf, right,   right,   one  ),
	T(dotf, right,   forward, zero ),
	T(dotf, right,   up,      zero ),
	T(dotf, forward, right,   zero ),
	T(dotf, forward, forward, one  ),
	T(dotf, forward, up,      zero ),
	T(dotf, up,      right,   zero ),
	T(dotf, up,      forward, zero ),
	T(dotf, up,      up,      one  ),

	// one is 4D, so its self dot product is 4
	T(dotf, one,     one,     { 4,  4,  4,  4} ),
	T(dotf, one,    none,     {-4, -4, -4, -4} ),

	// 3D cross products
	T(crossf, right,   right,    zero    ),
	T(crossf, right,   forward,  up      ),
	T(crossf, right,   up,      nforward ),
	T(crossf, forward, right,   nup      ),
	T(crossf, forward, forward,  zero    ),
	T(crossf, forward, up,       right   ),
	T(crossf, up,      right,    forward ),
	T(crossf, up,      forward, nright   ),
	T(crossf, up,      up,       zero    ),
	// double whammy tests: cross product with an angled vector and
	// ensuring that a 4d vector (non-zero w component) does not affect
	// the result, including the result's w component remaining zero.
	T(crossf, right,   one,     { 0, -1,  1} ),
	T(crossf, forward, one,     { 1,  0, -1} ),
	T(crossf, up,      one,     {-1,  1,  0} ),
	T(crossf, one,     right,   { 0,  1, -1} ),
	T(crossf, one,     forward, {-1,  0,  1} ),
	T(crossf, one,     up,      { 1, -1,  0} ),
#if !defined(__SSE__) && !defined(__OPTIMIZE__)
	// when not optimizing and SSE is not available (but ok when
	// optimizing)
	T(crossf, qtest,   qtest,   {0, 0, -1.47819534e-09, 0} ),
#else
	T(crossf, qtest,   qtest,   {0, 0, 0, 0} ),
#endif

	T(qmulf, qident,  qident,   qident  ),
	T(qmulf, qident,  right,    right   ),
	T(qmulf, qident,  forward,  forward ),
	T(qmulf, qident,  up,       up      ),
	T(qmulf, right,   qident,   right   ),
	T(qmulf, forward, qident,   forward ),
	T(qmulf, up,      qident,   up      ),
	T(qmulf, right,   right,   nqident  ),
	T(qmulf, right,   forward,  up      ),
	T(qmulf, right,   up,      nforward ),
	T(qmulf, forward, right,   nup      ),
	T(qmulf, forward, forward, nqident  ),
	T(qmulf, forward, up,       right   ),
	T(qmulf, up,      right,    forward ),
	T(qmulf, up,      forward, nright   ),
	T(qmulf, up,      up,      nqident  ),
	T(qmulf, one,     one,     { 2, 2, 2, -2 } ),
	T(qmulf, one,     { 2, 2, 2, -2 }, { 0, 0, 0, -8 } ),
	T(qmulf, qtest,   qtest,   {0.768, 0.576, 0, -0.28},
#if !defined(__SSE__) && !defined(__OPTIMIZE__)
	// when not optimizing and SSE is not available (but ok when
	// optimizing)
	                           {0, 6e-8, -1.47819534e-09, 3e-8}
#elif !defined( __SSE__)
	                           {0, 6e-8, 0, 6e-8}
#else
	                           {0, 6e-8, 0, 3e-8}
#endif
	),

	// The one vector is not unit (magnitude 2), so using it as a rotation
	// quaternion results in scaling by 4. However, it still has the effect
	// of rotating 120 degrees around the axis equidistant from the three
	// orthogonal axes such that x->y->z->x
	T(qvmulf, one,    right,     { 0, 4, 0, 0 } ),
	T(qvmulf, one,    forward,   { 0, 0, 4, 0 } ),
	T(qvmulf, one,    up,        { 4, 0, 0, 0 } ),
	T(qvmulf, one,    {1,1,1,0}, { 4, 4, 4, 0 } ),
	T(qvmulf, one,    one,       { 4, 4, 4, -2 } ),
	// inverse rotation, so x->z->y->x
	T(vqmulf, right,     one,    { 0, 0, 4, 0 } ),
	T(vqmulf, forward,   one,    { 4, 0, 0, 0 } ),
	T(vqmulf, up,        one,    { 0, 4, 0, 0 } ),
	T(vqmulf, {1,1,1,0}, one,    { 4, 4, 4, 0 } ),
	T(vqmulf, one,       one,    { 4, 4, 4, -2 } ),
	//
	T(qvmulf, qtest,  right,     {0.5392, 0.6144, -0.576, 0},
	                             {0, -5.9e-8, -6e-8, 0} ),
	T(qvmulf, qtest,  forward,   {0.6144, 0.1808, 0.768, 0},
#if !defined(__SSE__) && !defined(__OPTIMIZE__)
	                             {-5.9e-8, 0, 0, 0}
#elif  !defined(__SSE__)
	                             {-5.9e-8, 3e-8, 0, 0}
#else
	                             {-5.9e-8, 1.5e-8, 0, 0}
#endif
	),
	T(qvmulf, qtest,  up,        {0.576, -0.768, -0.28, 0},
#if !defined(__SSE__) && !defined(__OPTIMIZE__)
	                             {6e-8, 0, 3e-8, 0}
#elif  !defined(__SSE__)
	                             {6e-8, 0, 6e-8, 0}
#else
	                             {6e-8, 0, 3e-8, 0}
#endif
	),
	T(vqmulf, right,     qtest,  {0.5392, 0.6144, 0.576, 0},
	                             {0, -5.9e-8, 5.9e-8, 0} ),
	T(vqmulf, forward,   qtest,  {0.6144, 0.1808, -0.768, 0},
#if !defined(__SSE__) && !defined(__OPTIMIZE__)
	                             {-5.9e-8, 0, 0, 0}
#elif  !defined(__SSE__)
	                             {-5.9e-8, 3e-8, 0, 0}
#else
	                             {-5.9e-8, 1.5e-8, 0, 0}
#endif
	),
	T(vqmulf, up,        qtest,  {-0.576, 0.768, -0.28, 0},
#if !defined(__SSE__) && !defined(__OPTIMIZE__)
	                             {-5.9e-8, 0, 3e-8, 0}
#elif  !defined(__SSE__)
	                             {-5.9e-8, 0, 6e-8, 0}
#else
	                             {-5.9e-8, 0, 3e-8, 0}
#endif
	),

	T(qrotf, right,   right,    qident ),
	T(qrotf, right,   forward,  {    0,    0,  s05,  s05 } ),
	T(qrotf, right,   up,       {    0, -s05,    0,  s05 } ),
	T(qrotf, forward, right,    {    0,    0, -s05,  s05 } ),
	T(qrotf, forward, forward,  qident ),
	T(qrotf, forward, up,       {  s05,    0,    0,  s05 } ),
	T(qrotf, up,      right,    {    0,  s05,    0,  s05 } ),
	T(qrotf, up,      forward,  { -s05,    0,    0,  s05 } ),
	T(qrotf, up,      up,       qident ),

	T(tvabsf,    pmpi, {}, pi ),
	T(tvsqrtf,  { 1, 4, 9, 16}, {}, {1, 2, 3, 4} ),
	T(tvtruncf, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -1, -2 } ),
	T(tvceilf,  { 1.1, 2.9, -1.1, -2.9 }, {}, { 2, 3, -1, -2 } ),
	T(tvfloorf, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -2, -3 } ),
	T(tqconjf,  one, {}, { -1, -1, -1, 1 } ),
	T(tmagnitudef,  {  3,  4,  12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  {  3,  4,  12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  {  3,  4, -12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  {  3,  4, -12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  {  3, -4,  12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  {  3, -4,  12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  {  3, -4, -12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  {  3, -4, -12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3,  4,  12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3,  4,  12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3,  4, -12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3,  4, -12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3, -4,  12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3, -4,  12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3, -4, -12,  84}, {}, {85, 85, 85, 85} ),
	T(tmagnitudef,  { -3, -4, -12, -84}, {}, {85, 85, 85, 85} ),
	T(tmagnitude3f, { -3, -4, -12, -84}, {}, {13, 13, 13, 13} ),
};
#define num_vec4f_tests (sizeof (vec4f_tests) / (sizeof (vec4f_tests[0])))

static mat4f_test_t mat4f_tests[] = {
	T(mmulf, identity, identity, identity ),
	T(mmulf, rotate120, identity, rotate120 ),
	T(mmulf, identity, rotate120, rotate120 ),
	T(mmulf, rotate120, rotate120, rotate240 ),
	T(mmulf, rotate120, rotate240, identity ),
	T(mmulf, rotate240, rotate120, identity ),
};
#define num_mat4f_tests (sizeof (mat4f_tests) / (sizeof (mat4f_tests[0])))

static mv4f_test_t mv4f_tests[] = {
	T(mvmulf, identity, { 1, 0, 0, 0 }, { 1, 0, 0, 0 } ),
	T(mvmulf, identity, { 0, 1, 0, 0 }, { 0, 1, 0, 0 } ),
	T(mvmulf, identity, { 0, 0, 1, 0 }, { 0, 0, 1, 0 } ),
	T(mvmulf, identity, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } ),
	T(mvmulf, rotate120, { 1, 2, 3, 4 }, { 3, 1, 2, 4 } ),
	T(mvmulf, rotate240, { 1, 2, 3, 4 }, { 2, 3, 1, 4 } ),
};
#define num_mv4f_tests (sizeof (mv4f_tests) / (sizeof (mv4f_tests[0])))

// expect filled in using non-simd QuatToMatrix (has its own tests)
static mq4f_test_t mq4f_tests[] = {
	T(mat4fquat, { 0, 0, 0, 1 } ),
	T(mat4fquat, {  0.5,  0.5,  0.5, 0.5 } ),
	T(mat4fquat, {  0.5,  0.5, -0.5, 0.5 } ),
	T(mat4fquat, {  0.5, -0.5,  0.5, 0.5 } ),
	T(mat4fquat, {  0.5, -0.5, -0.5, 0.5 } ),
	T(mat4fquat, { -0.5,  0.5,  0.5, 0.5 } ),
	T(mat4fquat, { -0.5,  0.5, -0.5, 0.5 } ),
	T(mat4fquat, { -0.5, -0.5,  0.5, 0.5 } ),
	T(mat4fquat, { -0.5, -0.5, -0.5, 0.5 } ),
};
#define num_mq4f_tests (sizeof (mq4f_tests) / (sizeof (mq4f_tests[0])))

#ifdef __AVX2__
static int
run_vec4d_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_vec4d_tests; i++) {
		__auto_type test = &vec4d_tests[i];
		vec4d_t     result = test->op (test->a, test->b);
		vec4d_t     expect = test->expect + test->ulp_errors;
		vec4l_t     res = result != expect;
		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_vec4d_tests %zd, line %d\n", i, test->line);
			printf ("a: " VEC4D_FMT "\n", VEC4_EXP(test->a));
			printf ("b: " VEC4D_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4D_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4L_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4D_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4D_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4D_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}
#endif

static int
run_vec4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_vec4f_tests; i++) {
		__auto_type test = &vec4f_tests[i];
		vec4f_t     result = test->op (test->a, test->b);
		vec4f_t     expect = test->expect + test->ulp_errors;
		vec4i_t     res = (vec4i_t) result != (vec4i_t) expect;
		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_vec4f_tests %zd, line %d\n", i, test->line);
			printf ("a: " VEC4F_FMT "\n", VEC4_EXP(test->a));
			printf ("b: " VEC4F_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4F_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4I_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4F_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4F_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4F_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}

static int
run_mat4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_mat4f_tests; i++) {
		__auto_type test = &mat4f_tests[i];
		mat4f_t     result;
		mat4f_t     expect;
		mat4i_t     res = {};

		test->op (result, test->a, test->b);
		maddf (expect, test->expect, test->ulp_errors);

		int         fail = 0;
		for (int j = 0; j < 4; j++) {
			res[j] = result[j] != expect[j];
			fail |= res[j][0] || res[j][1] || res[j][2] || res[j][3];
		}
		if (fail) {
			ret |= 1;
			printf ("\nrun_mat4f_tests %zd, line %d\n", i, test->line);
			printf ("a: " VEC4F_FMT "\n", MAT4_ROW(test->a, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 3));
			printf ("b: " VEC4F_FMT "\n", MAT4_ROW(test->b, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->b, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->b, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->b, 3));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(result, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 3));
			printf ("t: " VEC4I_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 3));
			printf ("E: " VEC4F_FMT "\n", MAT4_ROW(expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 3));
			printf ("e: " VEC4F_FMT "\n", MAT4_ROW(test->expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 3));
			printf ("u: " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 3));
		}
	}
	return ret;
}

static int
run_mv4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_mv4f_tests; i++) {
		__auto_type test = &mv4f_tests[i];
		vec4f_t     result = test->op (test->a, test->b);
		vec4f_t     expect = test->expect + test->ulp_errors;
		vec4i_t     res = result != expect;

		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_mv4f_tests %zd, line %d\n", i, test->line);
			printf ("a: " VEC4F_FMT "\n", MAT4_ROW(test->a, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 3));
			printf ("b: " VEC4F_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4F_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4I_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4F_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4F_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4F_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}

static int
run_mq4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_mq4f_tests; i++) {
		__auto_type test = &mq4f_tests[i];
		quat_t      q;
		vec_t       m[16];
		memcpy (q, &test->q, sizeof (quat_t));
		QuatToMatrix (q, m, 1, 1);
		memcpy (&test->expect, m, sizeof (mat4f_t));
	}
	for (size_t i = 0; i < num_mq4f_tests; i++) {
		__auto_type test = &mq4f_tests[i];
		mat4f_t     result;
		mat4f_t     expect;
		mat4i_t     res = {};

		test->op (result, test->q);
		maddf (expect, test->expect, test->ulp_errors);
		memcpy (expect, (void *) &test->expect, sizeof (mat4f_t));

		int         fail = 0;
		for (int j = 0; j < 4; j++) {
			res[j] = result[j] != expect[j];
			fail |= res[j][0] || res[j][1] || res[j][2] || res[j][3];
		}
		if (fail) {
			ret |= 1;
			printf ("\nrun_mq4f_tests %zd, line %d\n", i, test->line);
			printf ("q: " VEC4F_FMT "\n", VEC4_EXP(test->q));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(result, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 3));
			printf ("t: " VEC4I_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 3));
			printf ("E: " VEC4F_FMT "\n", MAT4_ROW(expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 3));
			printf ("e: " VEC4F_FMT "\n", MAT4_ROW(test->expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 3));
			printf ("u: " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 3));
		}
	}
	return ret;
}

int
main (void)
{
	int         ret = 0;
#ifdef __AVX2__
	ret |= run_vec4d_tests ();
#endif
	ret |= run_vec4f_tests ();
	ret |= run_mat4f_tests ();
	ret |= run_mv4f_tests ();
	ret |= run_mq4f_tests ();
	return ret;
}
