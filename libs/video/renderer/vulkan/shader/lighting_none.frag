#version 450
#extension GL_GOOGLE_include_directive : enable
#include "lighting.h"

float
shadow (uint mapid, uint layer, uint mat_id, vec3 pos, vec3 lpos)
{
	return 1;
}

#include "lighting_main.finc"
