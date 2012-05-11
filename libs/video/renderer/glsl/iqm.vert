uniform mat4 mvp_mat;
uniform mat3 norm_mat;
uniform mat4 bonemats[80];
attribute vec4 vcolor;
attribute vec4 vweights;
attribute vec4 vbones;
attribute vec2 texcoord;
attribute vec4 vtangent;
attribute vec3 vnormal;
attribute vec3 vposition;

varying vec3 position;
varying vec3 bitangent;
varying vec3 tangent;
varying vec3 normal;
varying vec2 st;
varying vec4 color;

vec3
qmult (vec4 q, vec3 v)
{
	float       qs = q.w;
	vec3        qv = q.xyz;
	vec3        t = cross (qv, v);
	return (qs * qs) * v + 2.0 * qs * t + dot (qv, v) * qv + cross (qv, t);
}

vec3
dqtrans (vec4 q0, vec4 qe)
{//2.0 * (q0.w * qe.xyz - qe.w * q0.xyz - cross (qe.xyz, q0.xyz));
	float       qs = q0.w, Ts = qe.w;
	vec3        qv = -q0.xyz, Tv = qe.xyz;

	return 2.0 * (Ts * qv + qs * Tv + cross (Tv, qv));
}

void
main (void)
{
	mat4        m;
	vec4        q0, qe;
	vec3        sh, sc, tr, v, n, t;

	m  = bonemats[int (vbones.x)] * vweights.x;
	m += bonemats[int (vbones.y)] * vweights.y;
	m += bonemats[int (vbones.z)] * vweights.z;
	m += bonemats[int (vbones.w)] * vweights.w;
	q0 = m[0].yzwx; //swizzle for conversion betwen QF and GL
	qe = m[1].yzwx; //swizzle for conversion betwen QF and GL
	sh = m[2].xyz;
	sc = m[3].xyz;

	// extract translation from dual quaternion
	tr = dqtrans (q0, qe);
	// apply rotation and translation
	v = qmult (q0, vposition) + tr;
	// apply shear
	v.z += v.y * sh.z + v.x * sh.y;
	v.y += v.x * sh.x;
	// apply scale
	v *= sc;
	// rotate normal (won't bother with shear or scale: not super accurate,
	// but probably good enough)
	n = qmult (q0, vnormal);
	// rotate tangent (won't bother with shear or scale: not super accurate,
	// but probably good enough)
	t = qmult (q0, vtangent.xyz);
	position = v * 8.0;
	normal = norm_mat * n;
	tangent = norm_mat * t;
	bitangent = cross (normal, tangent) * vtangent.w;
	color = vcolor;
	st = texcoord;
	gl_Position = mvp_mat * vec4 (position, 1.0);
}
