#ifndef __lighting_h
#define __lighting_h

typedef struct LightData {
	vec4        color;		// .a is intensity
	vec4        position;	// .w = 0 -> directional, .w = 1 -> point/cone
	vec3        axis;
	uint        cone;
	vec4        attenuation;
} LightData;

typedef struct qfv_light_render_s {
	uint        mat_id:14;
	uint        map_id:5;
	uint        layer:11;
	uint             :1;
	uint        no_style:1;
	uint        style;
} qfv_light_render_t;

typedef struct qfv_light_matdata_s {
	float cascade_distance;
	float texel_size;
} qfv_light_matdata_t;

typedef struct LightQueue {
	uint        start:16;
	uint        count:16;
} LightQueue;

#ifdef __QFCC__
[buffer, readonly, set(0), binding(0)] @block ShadowMatrices {
	mat4 shadow_mats[];
};
[buffer, readonly, set(1), binding(0)] @block LightIds {
	uint        lightIds[];
};
[buffer, readonly, set(1), binding(1)] @block Lights {
	LightData   lights[];
};
[buffer, readonly, set(1), binding(2)] @block Renderer {
	qfv_light_render_t renderer[];
};
[buffer, readonly, set(1), binding(3)] @block Style {
	vec4        style[];
};
[buffer, readonly, set(1), binding(4)] @block LightMatDataBuffer {
	qfv_light_matdata_t lightmatdata[];
};
[buffer, readonly, set(1), binding(5)] @block LightEntIds {
	uint        light_entids[];
};

#ifdef SHADOW_SAMPLER
[uniform, set(3), binding(0)] SHADOW_SAMPLER shadow_map[32];
#endif

[push_constant] @block PushConstants {
	vec4        fog;
	float       spec;
	float       diff;
	float       near_plane;
	LightQueue  queue;
	uint        num_cascades;
};
#endif//__QFCC__

#endif//__lighting_h
