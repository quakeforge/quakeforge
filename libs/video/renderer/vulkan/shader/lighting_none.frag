#version 450
#extension GL_GOOGLE_include_directive : enable

float
shadow (uint mapid, uint layer, uint mat_id, vec3 pos)
{
	return 1;
}

#include "lighting_main.finc"
