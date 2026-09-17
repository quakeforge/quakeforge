#ifndef __shader_bsp_h
#define __shader_bsp_h

#define workgroup_size 512

typedef struct command_s {//FIXME use VkDrawIndexedIndirectCommand
	uint        indexCount;
	uint        instanceCount;
	uint        firstIndex;
	uint        vertexOffset;
	uint        firstInstace;
} command_t;

typedef struct cluster_s {//FIXME dup in model.h
	uint        first;
	uint        count;
} cluster_t;

typedef struct bsp_cluster_s {
	uint        first_index;
	uint        index_count;
	uint        tex_id;
} bsp_cluster_t;

typedef struct bsp_queue_s {
	uint        cluster;
	uint        first_instance;
	uint        instance_count;
} bsp_queue_t;

#endif//__shader_bsp_h
