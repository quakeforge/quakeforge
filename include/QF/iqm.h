#ifndef __QF_iqm_h
#define __QF_iqm_h

#include "QF/qtypes.h"

#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_SMAGIC 0x45544e49
#define IQM_VERSION 2

typedef struct iqmheader_s {
    char        magic[16];
    uint32_t    version;
    uint32_t    filesize;
    uint32_t    flags;
    uint32_t    num_text, ofs_text;
    uint32_t    num_meshes, ofs_meshes;
    uint32_t    num_vertexarrays, num_vertexes, ofs_vertexarrays;
    uint32_t    num_triangles, ofs_triangles, ofs_adjacency;
    uint32_t    num_joints, ofs_joints;
    uint32_t    num_poses, ofs_poses;
    uint32_t    num_anims, ofs_anims;
    uint32_t    num_frames, num_framechannels, ofs_frames, ofs_bounds;
    uint32_t    num_comment, ofs_comment;
    uint32_t    num_extensions, ofs_extensions;
} iqmheader;

typedef struct iqmmesh_s {
    uint32_t    name;
    uint32_t    material;
    uint32_t    first_vertex, num_vertexes;
    uint32_t    first_triangle, num_triangles;
} iqmmesh;

typedef enum : uint32_t {
    IQM_POSITION     = 0,
    IQM_TEXCOORD     = 1,
    IQM_NORMAL       = 2,
    IQM_TANGENT      = 3,
    IQM_BLENDINDEXES = 4,
    IQM_BLENDWEIGHTS = 5,
    IQM_COLOR        = 6,
    IQM_CUSTOM       = 0x10
} iqmverttype;

typedef enum : uint32_t {
    IQM_BYTE   = 0,
    IQM_UBYTE  = 1,
    IQM_SHORT  = 2,
    IQM_USHORT = 3,
    IQM_INT    = 4,
    IQM_UINT   = 5,
    IQM_HALF   = 6,
    IQM_FLOAT  = 7,
    IQM_DOUBLE = 8
} iqmformat;

typedef struct iqmtriangle_s {
    uint32_t    vertex[3];
} iqmtriangle;

typedef struct iqmjointv1_s {
    uint32_t    name;
    int32_t     parent;
    float       translate[3], rotate[3], scale[3];
} iqmjointv1;

typedef struct iqmjoint_s {
    uint32_t    name;
    int32_t     parent;
    float       translate[3], rotate[4], scale[3];
} iqmjoint;

typedef struct iqmposev1_s {
    int32_t     parent;
    uint32_t    mask;
    float       channeloffset[9];
    float       channelscale[9];
} iqmposev1;

typedef struct iqmpose_s {
    int32_t     parent;
    uint32_t    mask;
    float       channeloffset[10];
    float       channelscale[10];
} iqmpose;

typedef enum : uint32_t {
    IQM_LOOP = 1<<0
} iqmanimflags;

typedef struct iqmanim_s {
    uint32_t    name;
    uint32_t    first_frame, num_frames;
    float       framerate;
    iqmanimflags flags;
} iqmanim;

typedef struct iqmvertexarray_s {
    iqmverttype type;
    uint32_t    flags;
    iqmformat   format;
    uint32_t    size;
    uint32_t    offset;
} iqmvertexarray;

typedef struct iqmbounds_s {
    float       bbmin[3], bbmax[3];
    float       xyradius, radius;
} iqmbounds;

typedef struct iqmextension_s {
    uint32_t    name;
    uint32_t    num_data, ofs_data;
    uint32_t    ofs_extensions; // pointer to next extension
} iqmextension;

// QF stuff

typedef struct {
	DualQuat_t  rt;
	quat_t      shear;
	quat_t      scale;
} iqmframe_t;

typedef struct {
	byte        indices[4];
	byte        weights[4];
} iqmblend_t;

typedef struct iqm_s {
	char       *text;
	int         num_meshes;
	iqmmesh    *meshes;
	int         num_verts;
	byte       *vertices;
	int         stride;
	int         num_elements;
	union {
		uint16_t   *elements16;
		uint32_t   *elements32;
	};
	int         num_arrays;
	iqmvertexarray *vertexarrays;
	int         num_joints;
	iqmjoint   *joints;
	mat4_t     *baseframe;
	mat4_t     *inverse_baseframe;
	int         num_frames;
	iqmframe_t **frames;
	int         num_anims;
	iqmanim    *anims;
	void       *extra_data;
} iqm_t;

#endif//__QF_iqm_h
