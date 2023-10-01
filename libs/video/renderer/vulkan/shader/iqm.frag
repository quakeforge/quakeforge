#version 450

layout (set = 2, binding = 0) uniform sampler2D Skin;

layout (push_constant) uniform PushConstants {
	layout (offset = 72)
	uint        colorA;
	uint        colorB;
	vec4        base_color;
	vec4        fog;
};

layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec4 position;
layout (location = 2) in vec3 fnormal;
layout (location = 3) in vec3 ftangent;
layout (location = 4) in vec3 fbitangent;
layout (location = 5) in vec4 color;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 frag_emission;
layout (location = 2) out vec4 frag_normal;
layout (location = 3) out vec4 frag_position;

void
main (void)
{
	vec4        c;
	vec4        e;
	//vec3        n;
	int         i;
	vec3        normal = normalize (fnormal);
	vec3        tangent = normalize (ftangent);
	vec3        bitangent = normalize (fbitangent);
	//mat3        tbn = mat3 (tangent, bitangent, normal);

	c = texture (Skin, texcoord);// * color;
	//c = texture (Skin, vec3 (texcoord, 0)) * color;
	//c += texture (Skin, vec3 (texcoord, 1)) * unpackUnorm4x8(colorA);
	//c += texture (Skin, vec3 (texcoord, 2)) * unpackUnorm4x8(colorB);
	//e = texture (Skin, vec3 (texcoord, 3));
	//n = texture (Skin, vec3 (texcoord, 4)).xyz * 2 - 1;

	frag_color = c;
	frag_emission = vec4(0,0,0,1);//e;
	frag_normal = vec4(normal,1);//vec4(tbn * n, 1);
	frag_position = vec4 (position.xyz, gl_FragCoord.z);
}
