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

[push_constant] @block PushConstants {
	vec4        gravity;
	float       dT;
};


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

[shader(GLCompute, LocalSize=[1,1,1])]
void
main_update ()
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

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;

[shader(GLCompute, LocalSize=[1,1,1])]
void
main_physics ()
{
	uint        ind = gl_GlobalInvocationID.x;
	if (ind >= particleSystem.particleCount) {
		return;
	}
	Particle    part = particleStates.particles[ind];
	Parameters  parm = particleParameters.parameters[ind];

	part.pos += dT * part.vel;
	part.vel += dT * (part.vel * parm.drag + gravity * parm.drag.w);

	part.ramp += dT * parm.ramp.x;
	part.scale += dT * parm.ramp.z;
	part.color.a -= dT * parm.ramp.a;
	part.live -= dT;

	particleStates.particles[ind] = part;
}
