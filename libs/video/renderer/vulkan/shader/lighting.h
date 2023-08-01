struct LightData {
	vec4        color;		// .a is intensity
	vec4        position;	// .w = 0 -> directional, .w = 1 -> point/cone
	vec4        direction;	// .w = -cos(cone_angle/2) (1 for omni/dir)
	vec4        attenuation;
};

layout (constant_id = 0) const int MaxLights = 768;

layout (set = 1, binding = 0) uniform Lights {
	LightData   lights[MaxLights];
	int         lightCount;
};
