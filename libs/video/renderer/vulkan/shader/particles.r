const uint MaxParticles = 2048;

typedef struct Particle {
	vec4        pos;
	vec4        vel;
	vec4        color;
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
} particleSystem;

[push_constant] @block PhysPushConstants {
	vec4        gravity;
	float       dT;
} phys;

bool
is_dead (const Particle part, const Parameters parm)
{
	if (part.live <= 0) {
		return true;
	}
	if (part.ramp >= parm.ramp.y || part.color.a <= 0 || part.scale <= 0) {
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

[shader(GLCompute, LocalSize=[1,1,1])]
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
	part.vel += phys.dT * (part.vel * parm.drag + phys.gravity * parm.drag.w);

	part.ramp += phys.dT * parm.ramp.x;
	part.scale += phys.dT * parm.ramp.z;
	part.color.a -= phys.dT * parm.ramp.a;
	part.live -= phys.dT;

	particleStates.particles[ind] = part;
}

}

#include <GLSL/general.h>
#include <GLSL/fragment.h>
#include <GLSL/atomic.h>
#include <GLSL/image.h>
[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FragCoord")] vec4 gl_FragCoord;
#include "fog.finc"

#include "oit_store.finc"

@namespace frag {

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
	if (a <= 0) {
		__discard();
	}
	//c = c * a;
	c = FogBlend (c * a, frag.fog);
	StoreFrag (c, gl_FragCoord.z);
}

}

@namespace geom {

#include <GLSL/geometry.h>

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

[in] @block gl_PerVertex {
	[builtin("Position")] vec4 gl_Position;
	[builtin("PointSize")] float gl_PointSize;
	[builtin("ClipDistance")] float gl_ClipDistance[1];
	[builtin("CullDistance")] float gl_CullDistance[1];
} gl_in[];
[in("PrimitiveId")] int gl_PrimitiveIDIn;
[in("InvocationId")] int gl_InvocationID;
[in("ViewIndex")] int gl_ViewIndex;
[out] @block gl_PerVertex {
	[builtin("Position")] vec4 gl_Position;
	[builtin("PointSize")] float gl_PointSize;
	[builtin("ClipDistance")] float gl_ClipDistance[1];
	[builtin("CullDistance")] float gl_CullDistance[1];
};
[out("PrimitiveId")] int gl_PrimitiveID;
[out("Layer")] int gl_Layer;
[out("ViewportIndex")] int gl_ViewportIndex;

[in(0)] vec4 velocity[];
[in(1)] vec4 color[];
[in(2)] vec4 ramp[];

[out(0)] vec4 uv_tr;
[out(1)] vec4 o_color;

[shader(Geometry,
		InputPoints,
		OutputTriangleStrip,
		OutputVertices=4,
		Invocations=1)]
[capability(MultiView)]
[capability(Geometry)]
void
main (void)
{
	vec4        pos = gl_in[0].gl_Position;
	vec4        tr = vec4 (0, 0, ramp[0].xy);
	float       s = ramp[0].z;
	vec4        d, p;
	vec4        c = color[0];

	d = vec4 (-1, 1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	d = vec4 (-1, -1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	d = vec4 (1, 1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	d = vec4 (1, -1, 0, 0);
	p = pos + s * d;
	gl_Position = Projection3d * p;
	uv_tr = d + tr;
	o_color = c;
	EmitVertex ();

	EndPrimitive ();
}

}
