#include <GLSL/general.h>

void printf (string fmt, ...)
	= @intrinsic(OpExtInst, "NonSemantic.DebugPrintf", DebugPrintf);

const uint MaxParticles = 2048;

typedef struct Particle {
	vec4        pos;
	vec4        vel;
	uint        color;
	uint        ramp_base;
	int         pad;
	float       alpha;
	float       tex;
	float       ramp;
	float       scale;
	float       live;
} Particle;

typedef struct Parameters {
	vec4        drag;	// [dx, dy, dz, grav scale]
	vec4        ramp;	// [rate, max, alpha rate, scale rate]
} Parameters;

[buffer, set(0), binding(0)] @block OutStates {
	Particle particles[];
} outStates;

[buffer, set(0), binding(1)] @block OutParameters {
	Parameters parameters[];
} outParameters;

//doubles as VkDrawIndirectCommand
[buffer, set(0), binding(2)] @block OutSystem {
	uint        vertexCount;
	uint        particleCount;	//instanceCount
	uint        firstVertex;
	uint        firstInstance;
} outSystem;

[buffer, set(1), binding(0)] @block InStates {
	Particle particles[];
} inStates;

[buffer, set(1), binding(1)] @block InParameters {
	Parameters parameters[];
} inParameters;

//doubles as VkDrawIndirectCommand
[buffer, set(1), binding(2)] @block InSystem {
	uint        vertexCount;
	uint        particleCount;	//instanceCount
	uint        firstVertex;
	uint        firstInstance;
} inSystem;

[buffer, set(2), binding(0)] @block NewStates {
	Particle particles[];
} newStates;

[buffer, set(2), binding(1)] @block NewParameters {
	Parameters parameters[];
} newParameters;

//doubles as VkDrawIndirectCommand
[buffer, set(2), binding(2)] @block NewSystem {
	uint        vertexCount;
	uint        particleCount;	//instanceCount
	uint        firstVertex;
	uint        firstInstance;
} newSystem;

[buffer, set(0), binding(0)] @block ParticleStates {
	Particle particles[];
} particleStates;

[buffer, set(0), binding(1)] @block ParticleParameters {
	Parameters parameters[];
} particleParameters;

//doubles as VkDrawIndirectCommand
[buffer, set(0), binding(2)] @block ParticleSystem {
	uint        vertexCount;
	uint        particleCount;	//instanceCount
	uint        firstVertex;
	uint        firstInstance;
	uint        part_ramps[];
} particleSystem;

[push_constant] @block PhysPushConstants {
	vec4        center;
	float       gravity;
	float       min_dist;
	float       dT;
} phys;

bool
is_dead (const Particle part, const Parameters parm)
{
	if (part.live <= 0) {
		return true;
	}
	if (part.ramp >= parm.ramp.y || part.alpha <= 0 || part.scale <= 0) {
		return true;
	}
	return false;
}

@namespace update {

[shader(GLCompute, LocalSize=[1,1,1])]
void
main ()
{
	uint        j = 0;
	// compact existing partles removing dead particles
	for (uint i = 0; i < inSystem.particleCount; i++) {
		if (is_dead (inStates.particles[i], inParameters.parameters[i])) {
			continue;
		}
		outStates.particles[j] = inStates.particles[i];
		outParameters.parameters[j] = inParameters.parameters[i];
		j++;
	}
	// inject any new particles that aren't DOA
	for (uint i = 0; i < newSystem.particleCount && j < MaxParticles; i++) {
		if (is_dead (newStates.particles[i], newParameters.parameters[i])) {
			continue;
		}
		outStates.particles[j] = newStates.particles[i];
		outParameters.parameters[j] = newParameters.parameters[i];
		j++;
	}
	outSystem.vertexCount = newSystem.vertexCount;
	outSystem.particleCount = j;
	outSystem.firstVertex = newSystem.firstVertex;
	outSystem.firstInstance = newSystem.firstInstance;
}

}

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;

@namespace physics {

[shader(GLCompute, LocalSize=[64,1,1])]
void
main ()
{
	uint        ind = gl_GlobalInvocationID.x;
	if (ind >= particleSystem.particleCount) {
		return;
	}
	Particle    part = particleStates.particles[ind];
	Parameters  parm = particleParameters.parameters[ind];

	part.pos += phys.dT * part.vel;
	vec3 d = phys.center.w * part.pos.xyz - phys.center.xyz;
	float d2 = d • d;
	auto acc = vec4 (-d * phys.gravity * parm.drag.w / (d2 * sqrt (d2)), 0);
	part.vel += phys.dT * (acc + part.vel * parm.drag);

	part.ramp += phys.dT * parm.ramp.x;
	part.scale += phys.dT * parm.ramp.z;
	part.alpha -= phys.dT * parm.ramp.a;
	part.live -= phys.dT;

	if (part.ramp_base != ~0) {
		uint ind = part.ramp_base + (int) part.ramp;
		part.color = particleSystem.part_ramps[ind];
	}
	if (d2 < phys.min_dist * phys.min_dist) {
		part.live = 0;
	}

	particleStates.particles[ind] = part;
}

}

#include <GLSL/fragment.h>
#include <GLSL/atomic.h>
#include <GLSL/image.h>

@namespace frag {

[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FragCoord")] vec4 gl_FragCoord;

#include "fog.finc"
#include "oit_store.finc"

[push_constant] @block PushConstants {
	[offset(64)]
	vec4        fog;
};

[in(0)] vec4 uv_tr;
[in(1)] vec4 color;

[shader(Fragment, EarlyFragmentTests)]
[capability(MultiView)]
void
main (void)
{
	vec4        c = color;
	vec2        x = uv_tr.xy;

	float       a = 1 - x • x;
	float       z = gl_FragCoord.z;
	if (a <= 0) {
		__discard();
	}
	//c = c * a;
	c = FogBlend (c * a, frag.fog);
	StoreFrag (c, z / (1 - 0.99 * z * sqrt(a)));
}

}

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

@namespace vert {

#include <GLSL/texture.h>

[push_constant] @block PushConstants {
	mat4 Model;
};

[uniform, set(1), binding(0)] @sampler(@image(float,2D)) Palette;

@namespace in {
	[in(0)] vec4 position;
	[in(1)] vec4 velocity;
	[in(2)] uvec4 color;
	[in(3)] vec4 ramp;
	[in("ViewIndex")] int gl_ViewIndex;
	[in("VertexIndex")] int gl_VertexIndex;
}

@namespace out {
	[out(0)] vec4 uv_tr;
	[out(1)] vec4 color;

	[out("Position")] vec4 position;
}

[shader(Vertex)]
[capability(MultiView)]
void
main (void)
{
	uint        c = in.color.x;
	uint        x = c & 0x0f;
	uint        y = (c >> 4) & 0x0f;

	float vx = (in.gl_VertexIndex & 2);
	float vy = (in.gl_VertexIndex & 1);
	float s = in.ramp.z;

	vec2 v = (View[in.gl_ViewIndex] * (Model * in.velocity)).xy;
	float k = length(v);
	mat2 vs;
	if (k > 0.01) {
		v /= k;
		k = sqrt(sqrt(k));
		vs = mat2 (1 + k * v.x * v.x, k * v.x * v.y,
				   k * v.x * v.y, 1 + k * v.y * v.y);
	} else {
		vs = mat2 (1);
	}

	auto d = vec2 (1, 2) * vec2 (vx, vy) - vec2 (1, 1);
	vec4 pos = View[in.gl_ViewIndex] * (Model * in.position);
	auto p = vec4(s * (vs * d), 0, 0) + pos;

	out.position = Projection3d * p;
	out.uv_tr = vec4 (d, in.ramp.xy);
	out.color = texture (Palette, (vec2 (x, y) + 0.5) / 16.0);
}

}
