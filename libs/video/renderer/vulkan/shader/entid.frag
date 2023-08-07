#version 450

layout (location = 0) in flat uint entid;
layout (location = 0) out uint frag_entid;

void
main (void)
{
	frag_entid = entid;
}
