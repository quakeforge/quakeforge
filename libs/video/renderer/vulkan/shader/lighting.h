struct LightData {
	vec4        color;		// .a is intensity
	vec4        position;	// .w = 0 -> directional, .w = 1 -> point/cone
	vec4        direction;	// .w = -cos(cone_angle/2) (1 for omni/dir)
	vec4        attenuation;
};

#define ST_NONE     0   // no shadows
#define ST_PLANE    1   // single plane shadow map (small spotlight)
#define ST_CASCADE  2   // cascaded shadow maps
#define ST_CUBE     3   // cubemap (omni, large spotlight)

struct LightRender {
	uint        id_data;
	uint        style;
};

layout (set = 0, binding = 0) buffer ShadowMatrices {
	mat4 shadow_mats[];
};
layout (set = 1, binding = 0) buffer LightIds {
	uint        lightIds[];
};
layout (set = 1, binding = 1) buffer Lights {
	LightData   lights[];
};
layout (set = 1, binding = 2) buffer Renderer {
	LightRender renderer[];
};
layout (set = 1, binding = 3) buffer Style {
	vec4        style[];
};
layout (set = 1, binding = 4) buffer LightEntIds {
	uint        light_entids[];
};

layout (push_constant) uniform PushConstants {
	vec4        CascadeDepths;
	uint        queue;
};
