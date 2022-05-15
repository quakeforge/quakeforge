/*
	bspfile.h

	BSP (Binary Space Partitioning) file definitions

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifndef __QF_bspfile_h
#define __QF_bspfile_h

#include "QF/qtypes.h"
#include "QF/quakeio.h"

/** \defgroup formats_bsp BSP Files

	BSP files are used for quake's maps and some pick-up items (health and
	ammo boxes in particular). The maps can be quite complicated, usually with
	many sub-models, while the pick-up items are usually simple boxes.

	QuakeForge supports two formats for BSP files "BSP 29", (the original
	format, with many 16-bit fields), and "BSP 2", all fields 32 bits. Both
	formats are little-endian.

	BSP files use a right-handed coordinate system with +Z up but clockwise
	winding for face normals.

	The tools used to create a bsp file include qfbsp, qfvis and qflight (or
	similar tools from other projects).
*/

/** \defgroup bsp_limits BSP File limits
	\ingroup formats_bsp

	Other than MAX_MAP_HULLS, these apply only to BSP 29 files as the fields
	holding the values are all 16 bits.
*/
///@{

#define MAX_MAP_HULLS			4		///< fixed length array

#define MAX_MAP_PLANES			32767
/** \name nodes and leafs (sic)
	negative shorts are contents but contents "max" is -15, so -32768 to -17
	are available
*/
///@{
#define MAX_MAP_NODES			65520
#define MAX_MAP_CLIPNODES		65520
#define MAX_MAP_LEAFS			65520
///@}
#define MAX_MAP_VERTS			65535
#define MAX_MAP_FACES			65535
#define MAX_MAP_MARKSURFACES	65535

#define MAP_PVS_BYTES (MAX_MAP_LEAFS / 8)
///@}

/** \defgroup bsp_lumps BSP File lumps
	\ingroup formats_bsp

	All data in a BSP file is separated into lumps. The lump structure gives
	the file relative offset of the beginning of the data lump, and the size
	of the lump in bytes.
*/
///@{

/** Individual data lump descriptor.
*/
typedef struct lump_s {
	uint32_t fileofs;					///< File relative offset in bytes
	uint32_t filelen;					///< Size of lump in bytes
} lump_t;

#define LUMP_ENTITIES		0		///< \ref bsp_entities
#define LUMP_PLANES			1		///< \ref bsp_planes
#define LUMP_TEXTURES		2		///< \ref bsp_textures
#define LUMP_VERTEXES		3		///< \ref bsp_vertices
#define LUMP_VISIBILITY		4		///< \ref bsp_visibility
#define LUMP_NODES			5		///< \ref bsp_nodes
#define LUMP_TEXINFO		6		///< \ref bsp_texinfo
#define LUMP_FACES			7		///< \ref bsp_face
#define LUMP_LIGHTING		8		///< \ref bsp_lighting
#define LUMP_CLIPNODES		9		///< \ref bsp_clipnodes
#define LUMP_LEAFS			10		///< \ref bsp_leafs
#define LUMP_MARKSURFACES	11		///< \ref bsp_marksurfaces
#define LUMP_EDGES			12		///< \ref bsp_edges
#define LUMP_SURFEDGES		13		///< \ref bsp_surfedges
#define LUMP_MODELS			14		///< \ref bsp_model
#define HEADER_LUMPS		15		///< Number of lumps
///@}

/** \defgroup bsp_header BSP File header
	\ingroup formats_bsp
*/
///@{
/** Holdes version and lump offset information.

	Always at offset 0 of the BSP file.
*/
typedef struct dheader_s {
	uint32_t    version;			///< little-endian or 4-char string
	lump_t      lumps[HEADER_LUMPS];///< Identical between BSP 2 and BSP 29
} dheader_t;

#define BSPVERSION		29			///< little-endian uint32_t
#define BSP2VERSION		"BSP2"		///< 4-char unterminated string
#define Q2BSPVERSION	38			///< Not supported
#define TOOLVERSION		2			///< Not used
///@}

/** \defgroup bsp_model BSP File sub-model
	\ingroup formats_bsp

	The sub-model lump is an array of sub-model descriptors. Every BSP file
	has at least one sub-model, with sub-model index 0 being the main model.
*/
///@{
typedef struct dmodel_s {
	/// \name Bounding box for the model
	///@{
	float       mins[3];			///< minimum X, Y, Z
	float       maxs[3];			///< maximum X, Y, Z
	///@}
	float       origin[3];			///< unclear
	/// Index of the first node for each hull. The first headnode index
	/// is for the draw nodes (\ref bsp_nodes), while subsequent headnodes
	/// are for the clipnodes (\ref bsp_clipnodes).
	/// The engine builds a clip node tree for headnods[0] from the draw
	/// nodes at load time.
	uint32_t    headnode[MAX_MAP_HULLS];
	/// The number of visible leafs in the model.
	/// \note Does *not* include leaf 0 (the infinite solid leaf in most maps)
	uint32_t    visleafs;
	/// \name Visible faces
	/// A sub-model's visible faces are all in one contiguous block.
	///@{
	uint32_t    firstface;			///< Index of first visible face in model
	uint32_t    numfaces;			///< Number of visible faces in model
	///@}
} dmodel_t;
///@}

/** \defgroup bsp_textures BSP File texture data
	\ingroup formats_bsp

	The texture data in a BSP file forms a sub-file with its own structure.
*/
///@{

/** Header for the textures lump.

	The size is variable as there is one dataofs element for each miptex
	block.
*/
typedef struct dmiptexlump_s {
	/// Number of miptex blocks in the textures lump.
	uint32_t    nummiptex;
	/// Offsets are relative to the beginning of the dmiptexlump_t structure
	/// (ie, the textures lump). One for each miptex block. \ref miptex_s
	uint32_t    dataofs[4];
} dmiptexlump_t;

#define MIPTEXNAME  16				///< Names have a max length of 15 chars
#define MIPLEVELS   4				///< Number of mip levels in all miptex
/** Header for individual mip-mapped texture.

	The beginning of the name of the texture specifies some of the texture's
	properties:
		- sky       The texture is used for the dual-layer skies. Expected
		            to be 256x128. Sky face get special treatment by the
					renderer.
		- *         "Water" face. Texture-warped by the renderer. Usually
		            unlit (ie, always full-bright).
		- +         Member of an animation group. The second character of
					the texture's name determines to which animation sequence
					the texture belongs and the texture's position in that
					sequence.  The rest of the name specifies the group name.
					\ref bsp_texture_animation

	The texture may be rectangular, but the size must be a multiple of 16
	pixels in both directions.
*/
typedef struct miptex_s {
	char        name[MIPTEXNAME];		///< Texture name. Nul-terminated.
	uint32_t    width;					///< Width of the full-size texture
	uint32_t    height;					///< Height of the full-size texture
	/// Offsets are relative to the beginning of the individual miptex block.
	uint32_t    offsets[MIPLEVELS];
} miptex_t;
///@}

/** \defgroup bsp_vertices BSP File vertex lump
	\ingroup formats_bsp

	Array of unique vertices.

	Vertices are shared between edges and faces.
*/
///@{
typedef struct dvertex_s {
	float   point[3];				///< X, Y, Z
} dvertex_t;
///@}


/** \defgroup bsp_planes BSP File plane lump
	\ingroup formats_bsp

	Array of unique planes.

	Planes are shared between nodes and faces.
*/
///@{
typedef struct dplane_s {
	float   normal[3];				///< Plane normal
	float   dist;					///< Distance of the plane from the origin
	int32_t type;					///< \ref bsp_plane_definition
} dplane_t;
///@}

/** \defgroup bsp_plane_definition Plane definitions
	\ingroup bsp_planes
	Planes are always canonical in that thier normals always point along
	a positive axis for axial planes ((1, 0, 0), (0, 1, 0), or (0, 0, 1)),
	or the largest component is positive.
*/
///@{
/// \name Axial planes
///@{
#define PLANE_X		0				///< Plane normal points along +X axis
#define PLANE_Y		1				///< Plane normal points along +Y axis
#define PLANE_Z		2				///< Plane normal points along +Z axis
///@}
/// \name Non-axial planes. Specifies the closest axis.
///@{
#define PLANE_ANYX	3				///< Plane normal close to +X
#define PLANE_ANYY	4				///< Plane normal close to +Y
#define PLANE_ANYZ	5				///< Plane normal close to +Z
///@}
///@}

/** \defgroup bsp_nodes BSP nodes lump
	\ingroup formats_bsp

	These nodes form the visible BSP. While not all nodes will have faces,
	most (or at least many) will, as the nodes are used for depth-sorting the
	faces while traversing the BSP tree using front-depth-first-in-order
	traversal.

	They can also be used for collision detection as the leaf nodes indicate
	the contents of the volume. The engine takes advantage of this to
	construct hull 0 (disk space was still very precious in 1996)
*/
///@{

/** Spit a convex region of space into two convex regions.

	The node's plane divides the space into a front side and a back side,
	where a point is in front of the plane if its dot product with the plane
	normal is positive or 0, and ond behind (in back of) the plane if the
	dot product is negative.
*/
typedef struct dnode_s { //BSP2 version (bsp 29 version is in bspfile.c)
	uint32_t    planenum;		///< Index of plane defining this node
	/// Indices of the child nodes on the front [0] and back [1] side of this
	/// node. Negative indicies indicate leaf nodes, where the leaf index is
	/// -(child + 1) (or child is -(leaf + 1)).
	int32_t     children[2];
	/// \name Node bounding box
	///@{
	float       mins[3];		///< minimum X, Y, Z
	float       maxs[3];		///< maximum X, Y, Z
	///@}
	/// \name Node plane faces
	/// List of \ref bsp_face faces on the node's plane. Used for
	/// depth-sorting the faces while traversing the visible BSP tree.
	///@{
	uint32_t    firstface;	///< index of first face on nonde plane
	uint32_t    numfaces;	///< numer of faces on node plane (both sides)
	///@}
} dnode_t;
///@}

/** \defgroup bsp_clipnodes BSP clipping nodes lump
	\ingroup formats_bsp

	Compact BSP tree for collision detection.
*/
///@{
typedef struct dclipnode_s { //BSP2 version (bsp 29 version is in bspfile.c)
	uint32_t    planenum;		///< Index of plane defining this node
	/// Indices of the child nodes on the front [0] and back [1] side of this
	/// node. Negative indicies indicate leaf contents.
	/// \ref bsp_leaf_contents
	int32_t     children[2];
} dclipnode_t;
///@}

/** \defgroup bsp_texinfo BSP texture information lump
	\ingroup formats_bsp

	Texture mapping information for visible faces. An individual texinfo may
	be shared by many faces.
*/
///@{
typedef struct texinfo_s {
	/// The texture plane basis vectors, with the \a s basis vector in index
	/// 0 and the \a t basis vector in index 1.
	/// For each index, the first three elements are the X, Y, Z components
	/// of the basis vector, and the fourth component is the offset. Can be
	/// viewed as a 2 row x 4 column matrix (\a M) that is multiplied by
	/// a homogeneos vector (\a v) for a face vertex to determine that
	/// vertex's UV coordinates (ie, \a M \a v)
	float       vecs[2][4];
	/// Index of the miptex block in the texture data lump
	/// (\ref bsp_textures). If the referenced miptex is a member of
	/// an animation group, then the entire group is used by this texinfo.
	uint32_t    miptex;
	uint32_t    flags;			///< \ref bsp_texinfo_flags
} texinfo_t;
///@}

/** \defgroup bsp_texinfo_flags BSP texture information flags
	\ingroup bsp_texinfo
*/
///@{
#define TEX_SPECIAL 1			///< sky or slime, no lightmap, 256 subdivision
#define TEX_MISSING 2			///< this texinfo does not have a texture (N/U)
///@}

/** \defgroup bsp_edges BSP edges lump
	\ingroup formats_bsp

	\note Edge 0 is never used as negative edge indices are indicate
	counterclockwise use of the edge in a face.
*/
///@{
typedef struct dedge_s { //BSP2 version (bsp 29 version is in bspfile.c)
	uint32_t    v[2];			///< vertex indices in winding order
} dedge_t;
///@}

/** \defgroup bsp_face BSP visible faces
	\ingroup formats_bsp
*/
///@{
typedef struct dface_s { //BSP2 version (bsp 29 version is in bspfile.c)
	uint32_t    planenum;		///< Index of the plane containing this face
	/// Indicates the side of the plane considered to be the front for this
	/// face. 0 indicates the postive normal side, 1 indicates the negative
	/// normal side (ie, the face is on the back side of the plane).
	int32_t     side;

	/// \name List of edge indices in clockwise order
	/// \ref bsp_surfedges
	/// Negative edge indices indicate that the edge is being used in its
	/// reverse direction (ie, \a v[1] to \a v[0] instead of \a v[0] to
	/// \a v[1]).
	/// \note This is a double-indirection.
	///@{
	uint32_t    firstedge;			///< Index of first edge index
	uint32_t    numedges;			///< Number of edge indices
	///@}
	/// Index of texinfo block describing texture mapping and miptex reference
	/// for this face.
	uint32_t    texinfo;

	/// \name lighting info
	///@{
#define MAXLIGHTMAPS 4
	/// List of lighting styles affecting the lightmaps affecting this face.
	/// 255 indicates both no style thus no lightmap and the end of the list.
	byte        styles[MAXLIGHTMAPS];
	/// Offset into the lighting data (ref bsp_lighting) of the first lightmap
	/// affecting this face. The number of lightmaps affecting this face is
	/// inferred from the \a styles array. The size of each lightmap is
	/// determined from the texture extents of the face as (E / 16 + 1)
	/// (integer division). Thus the total lightmap data for this face is
	/// given by (nummaps * (Es / 16 + 1) * (Et / 16 + 1)) bytes.
	/// The lightmap data is 8-bit, ranging from 0 to 255.
	///
	/// -1 indicates no lightmap data.
	uint32_t    lightofs;
	///@}
} dface_t;
///@}

/** \defgroup bsp_leafs BSP leafs lump
	\ingroup formats_bsp

	leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
	all other leafs need visibility info
*/
///@{

/** A convex volume of space delimited by the surrounding node planes.

	With the exception of the generic solid leaf at index 0, *ALL* leafs are
	convex polyhedra. However, they will generally not be manifold as
	otherwise it would not be possible to see into or out of a leaf, nor for
	an entity to pass in or out of one.
*/
typedef struct dleaf_s { //BSP2 version (bsp 29 version is in bspfile.c)
	/// The contents of the leaf. \ref bsp_leaf_contents
	int32_t     contents;
	/// Offset into the \ref bsp_visibility data block for this leaf's
	/// visibility data. -1 if the leaf has no visibility data.
	int32_t     visofs;

	/// \name Bounding box for frustum culling
	///@{
	float       mins[3];			///< minimum X, Y, Z
	float       maxs[3];			///< maximum X, Y, Z
	///@}

	/// \name Visible faces of the leaf
	/// List of face indices that form the visible faces of the leaf.
	/// \note This is a double-indirection.
	///@{
	uint32_t    firstmarksurface;	///< index of first visible face index
	uint32_t    nummarksurfaces;	///< number of indices
	///@}

#define NUM_AMBIENTS    4
	/// automatic ambient sounds \ref bsp_leaf_ambient
	byte        ambient_level[NUM_AMBIENTS];
} dleaf_t;
///@}

/** \defgroup bsp_leaf_contents BSP leaf contents
	\ingroup bsp_leafs
*/
///@{
#define CONTENTS_EMPTY			-1	///< Open, transparent, passable space
#define CONTENTS_SOLID			-2	///< Solid, opaque, impassible space
#define CONTENTS_WATER			-3	///< Buoyant, translucent, passable space
#define CONTENTS_SLIME			-4	///< Like water, but toxic
#define CONTENTS_LAVA			-5	///< Like water, but hot
#define CONTENTS_SKY			-6	///< FIXME sky, but...
#define CONTENTS_ORIGIN			-7	///< removed at csg time
#define CONTENTS_CLIP			-8	///< changed to CONTENTS_SOLID

#define CONTENTS_CURRENT_0		-9	///< changed to CONTENTS_WATER
#define CONTENTS_CURRENT_90		-10	///< changed to CONTENTS_WATER
#define CONTENTS_CURRENT_180	-11	///< changed to CONTENTS_WATER
#define CONTENTS_CURRENT_270	-12	///< changed to CONTENTS_WATER
#define CONTENTS_CURRENT_UP		-13	///< changed to CONTENTS_WATER
#define CONTENTS_CURRENT_DOWN	-14	///< changed to CONTENTS_WATER
///@}

/** \defgroup bsp_leaf_ambient BSP leaf ambient sounds
	\ingroup bsp_leafs
*/
///@{
#define AMBIENT_WATER   0
#define AMBIENT_SKY     1
#define AMBIENT_SLIME   2
#define AMBIENT_LAVA    3
///@}

/** \defgroup bsp_entities Entity data string
	\ingroup formats_bsp

	Single string containing all the entity dictionaries (edicts).

	The format is extremely simple in that is is a series of blocks delimited
	by { and }. Blocks cannot be nested. A { after a { but before a } will be
	treated as either a key or a value. A } after a } but before a { is not
	valid. { must be the first non-whitespace character in the string. } must
	be the last non-whitespace character in the string if there is a {.

	The contents of a block consist of a series of key-value strinng pairs.
	Unless the string contains spaces or punctuaiont (one of "{}()':") double
	quote (") around the string are optional. Keys and values are separated
	by whitespace (optional if quotes are used) but must be on the same line,
	but it is best to use a space.  Key-value pairs may be on separate lines
	(prefered for readability).

	Key strings are Quake-C entity field names. Value strings are interpreted
	based on the type of the field.

	Below is a sample edict

	\verbatim
	{
	"origin" "-2528 -384 512"
	"angle" "270"
	"classname" "info_player_start"
	}
	\endverbatim

	As an extention, QuakeForge supports \ref property-list entity string data
	if the first non-whitespace character is a (. QuakeForge progs can register
	a custom parser for the resulting property list, but the default parser
	supports only a single array of dictionaries containing only simple
	key-value string pairs.
*/

/** \defgroup bsp_lighting Lighting data
	\ingroup formats_bsp

	Pre-computed lightmap data. Each byte represents a single lightmap
	pixel, thus a lightmap is luminance with values ranging from 0 for
	full-dark to 255 for full-bright. The pixels for each lightmap (up
	to four) for each visible face form a linear stream.

	Computed using tools such as qflight.
*/

/** \defgroup bsp_marksurfaces BSP Leaf faces
	\ingroup formats_bsp

	Array of face indices representing the faces on the leafs. The face
	indices are grouped by leaf. Because whether a face gets split depends
	on how the nodes split space, a face may be shared by multiple leafs.
	FIXME needs confirmation, but maps with more leaf face indices than
	faces in the face array have observed.
*/

/** \defgroup bsp_surfedges BSP face edges
	\ingroup formats_bsp

	Array of edge indices specifying the edges used in face polyons.

	The edges are grouped by face polygon, and are listed in clockwise order
	from that face's perspective (when looking at the face from that face's
	front side).

	As actual edges are reused and thus shared by faces, negative edge
	indices indicate that the edge is to be reversed. That is, instead of
	going from \a v[0] to \a v[1] for edges referenced by positive indices,
	go from \a v[1] to \a v[0] for edges referenced by negative indices.
	\ref bsp_edges
*/

/** \defgroup bsp_visibility BSP visibility data
	\ingroup formats_bsp

	The primary culling data in a Quake BSP. Conceptually, it is just an
	array of bit-sets representing the leafs visible from each leaf.
	However, leaf 0 is not represented in the data at all: there is now row
	for leaf 0 (first row is for leaf 1's visible leafs), and the first bit
	in each row represents leaf 1.

	As well, the data within a row is compressed: runs of 0-bytes
	(representing eight non-visible leafs) are represented as a 0 followed
	by a single count byte. For example, a run of eight 0-bytes will appear
	as 0x00 0x08, and a single 0 byte will appear as 0x00 0x01.

	Computed using tools such as \a qfvis.
*/

/** \defgroup bsp_texture_animation BSP Texture Animation
	\ingroup formats_bsp

	Textures in BSP files can be animated. Animated textures are those
	with miptex names in a special format:

	\verbatim
	+Sname
	\endverbatim

	That is, the miptex name starts with a +, followed by a single sequence
	id character (0-9 or A-J (or a-j, case-insensitive)) then the name of the
	animation group is formed by the remaining characters. Thus an animation
	group can have one or two sequences with one to ten frames per sequence.
	The two sequences are the main sequence (0-9) and the alternate sequence
	(A-J). Gaps in a sequence are not allowed: there must always be a +0 or +A
	miptex in the sequence, and there must be a texture for every digit or
	letter up to the highest used. However, the miptex blocks can be in any
	order within the textures block: the engine searches for them and sorts
	them.

	Single-frame sequences are useful for simple static texture switchinng
	based on game state (entity frame: 0 for main, non-zero for alternate).
	Multi-frame sequences are played back at ten frames per second.

	For example, the folowing sets up an animitation with a 4-frame main
	sequence and a 2-frame alternate sequence. Perhaps a roaring fire in
	the main sequence and glowing coals in the alternate.

	\verbatim
	+0fire  // main sequence
	+1fire  // main sequence
	+2fire  // main sequence
	+3fire  // main sequence
	+Afire  // alternate sequence
	+Bfire  // alternate sequence
	\endverbatim
*/

/** \defgroup bspfile BSP file manipulation
	\ingroup utils
*/
///@{
/** Memory representation of the BSP file.
	All data is in native endian format.
*/
typedef struct bsp_s {
	int         own_header;			///< \a header is owned by the structure
	dheader_t  *header;				///< the bsp header block

	int         own_models;			///< \a models is owned by the structure
	size_t      nummodels;			///< number of models
	dmodel_t   *models;				///< Array of sub-models

	int         own_visdata;		///< \a visdata is owned by the structure
	size_t      visdatasize;		///< number of bytes in visdata
	byte       *visdata;			///< \ref bsp_visibility

	int         own_lightdata;		///< \a lightdata is owned by the structure
	size_t      lightdatasize;		///< number of bytes in lightdata
	byte       *lightdata;			///< \ref bsp_lighting

	int         own_texdata;		///< \a texdata is owned by the structure
	size_t      texdatasize;		///< number of bytes in texture data
	byte       *texdata;			///< \ref bsp_textures

	int         own_entdata;		///< \a entdata is owned by the structure
	size_t      entdatasize;		///< number of bytes in entdata string
	char       *entdata;			///< \ref bsp_entities

	int         own_leafs;			///< \a leafs is owned by the structure
	size_t      numleafs;			///< number of leafs
	dleaf_t    *leafs;				///< array of leafs

	int         own_planes;			///< \a planes is owned by the structure
	size_t      numplanes;			///< number of planes
	dplane_t   *planes;				///< array of planes

	int         own_vertexes;		///< \a vertexes is owned by the structure
	size_t      numvertexes;		///< number of vertices
	dvertex_t  *vertexes;			///< array of vertices

	int         own_nodes;			///< \a nodes is owned by the structure
	size_t      numnodes;			///< number of nodes
	dnode_t    *nodes;				///< array of nodes

	int         own_texinfo;		///< \a texinfo is owned by the structure
	size_t      numtexinfo;			///< number of texinfo blocks
	texinfo_t  *texinfo;			///< array of texinfo blocks

	int         own_faces;			///< \a faces is owned by the structure
	size_t      numfaces;			///< number of faces
	dface_t    *faces;				///< array of faces

	int         own_clipnodes;		///< \a clipnodes is owned by the structure
	size_t      numclipnodes;		///< number of clipnodes
	dclipnode_t *clipnodes;			///< array of clipnodes

	int         own_edges;			///< \a edges is owned by the structure
	size_t      numedges;			///< number of edges
	dedge_t    *edges;				///< array of edges

	int         own_marksurfaces;///< \a marksurfaces is owned by the structure
	size_t      nummarksurfaces;	///< number of marksurface indices
	uint32_t   *marksurfaces;		///< array of marksurfaces indices

	int         own_surfedges;		///< \a surfedge is owned by the structure
	size_t      numsurfedges;		///< number of surfedge indices
	int32_t    *surfedges;			///< array of surfedge indices
} bsp_t;

/**	Create a bsp structure from a memory buffer.
	The returned structure will be setup to point into the supplied buffer.
	All structures within the bsp will be byte-swapped. For this reason, if
	a callback is provided, the callback be called after byteswapping the
	header, but before byteswapping any data in the lumps.

	\param mem		The buffer holding the bsp data.
	\param size		The size of the buffer. This is used for sanity checking.
	\param cb		Pointer to the callback function.
	\param cbdata	Pointer to extra data for the callback.
	\return			Initialized bsp structure.

	\note The caller maintains ownership of the memory buffer. BSP_Free will
	free only the bsp structure itself. However, if the caller wishes to
	relinquish ownership of the buffer, set bsp_t::own_header to true.
*/
bsp_t *LoadBSPMem (void *mem, size_t size, void (*cb) (const bsp_t *, void *),
				   void *cbdata);
/** Load a bsp file into memory.
	The return structre is set up the same as for LoadBSPMem, with the
	exception that the BSP data memory is owned by the structure and will be
	freed by BSP_Free.

	\param file		The open file from which to read the BSP data.
	\param size		The number of bytes to read. The BSP data may be a section
					in a larger file (eg, in a pak file).
	\return			Initialized bsp structure.
*/
bsp_t *LoadBSPFile (QFile *file, size_t size);
/** Write the bsp data to the given file.
	Does any necessary byte-swapping to ensure the written data is in
	little-endian format. Automatically selects BSP-2 or BSP-29 based on the
	data: if any value is too large for BSP-29, BSP-2 will be written,
	otherwise BSP-29 will be written.

	\param bsp		The bsp data to be written to the given file.
	\param file		The file to which the data will be written.

	\note \a file is *not* closes. It is the caller's responsibility to close
	\a file.
*/
void WriteBSPFile (const bsp_t *bsp, QFile *file);

/** Create a fresh BSP struct ready to be populated with data.
	Initially, none of the data pointers (which start as null) are owned by
	the bsp, but as data is added using the Add functions, the pointers will
	become owned.

	\return			Pointer to a blank-slate BSP strucutre.

	\note	Mixing use of an Add function with a private pointer for the
	same data will result in undefined behavior.
*/
bsp_t *BSP_New (void);

/** Free the BSP data.

	Only memory blocks owned by the BSP structure (and the structure itselve)
	will be freed.

	\param bsp		Pointer to the BSP structure to be freed.
*/
void BSP_Free (bsp_t *bsp);

/** Add a single plane to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param plane	Pointer to the single plane to be added to the BSP
					structure.
*/
void BSP_AddPlane (bsp_t *bsp, const dplane_t *plane);

/** Add a single leaf to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param leaf		Pointer to the single leaf to be added to the BSP
					structure.
*/
void BSP_AddLeaf (bsp_t *bsp, const dleaf_t *leaf);

/** Add a single vertex to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param vertex	Pointer to the single vertex to be added to the BSP
					structure.
*/
void BSP_AddVertex (bsp_t *bsp, const dvertex_t *vertex);

/** Add a single node to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param node		Pointer to the single node to be added to the BSP
					structure.
*/
void BSP_AddNode (bsp_t *bsp, const dnode_t *node);

/** Add a single texinfo to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param texinfo	Pointer to the single texinfo to be added to the BSP
					structure.
*/
void BSP_AddTexinfo (bsp_t *bsp, const texinfo_t *texinfo);

/** Add a single face to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param face		Pointer to the single face to be added to the BSP
					structure.
*/
void BSP_AddFace (bsp_t *bsp, const dface_t *face);

/** Add a single clipnode to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param clipnode	Pointer to the single clipnode to be added to the BSP
					structure.
*/
void BSP_AddClipnode (bsp_t *bsp, const dclipnode_t *clipnode);

/** Add a single marksurface to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param marksurface	The single marksurface to be added to the BSP
					structure.
*/
void BSP_AddMarkSurface (bsp_t *bsp, int marksurface);

/** Add a single surfedge to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param surfedge	Pointer to the single surfedge to be added to the BSP
					structure.
*/
void BSP_AddSurfEdge (bsp_t *bsp, int surfedge);

/** Add a single edge to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param edge		Pointer to the single edge to be added to the BSP
					structure.
*/
void BSP_AddEdge (bsp_t *bsp, const dedge_t *edge);

/** Add a single model to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param model	Pointer to the single model to be added to the BSP
					structure.
*/
void BSP_AddModel (bsp_t *bsp, const dmodel_t *model);

/** Add lighmap data to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param lightdata Pointer to the lightmap data to be added to the BSP
					structure. ref bsp_lighting
	\param lightdatasize The number of bytes in the lightmap data

	\note Call only once!
*/
void BSP_AddLighting (bsp_t *bsp, const byte *lightdata, size_t lightdatasize);

/** Add visibilityd data to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param visdata	Pointer to the visibility data to be added to the BSP
					structure. \ref bsp_visibility
	\param visdatasize The number of bytes in the visibility data

	\note Call only once!
*/
void BSP_AddVisibility (bsp_t *bsp, const byte *visdata, size_t visdatasize);

/** Add an entity data string to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param entdata	Pointer to the entity data string to be added to the BSP
					structure. \ref bsp_entities
	\param entdatasize The number of bytes in the entity data string

	\note Call only once!
*/
void BSP_AddEntities (bsp_t *bsp, const char *entdata, size_t entdatasize);

/** Add texture data to the BSP data.

	\param bsp		Pointer to the BSP structure to be modified.
	\param texdata	Pointer to the texture data to be added to the BSP
					structure. \ref bsp_textures
	\param texdatasize The number of bytes in the texture data

	\note Call only once!
*/
void BSP_AddTextures (bsp_t *bsp, const byte *texdata, size_t texdatasize);

///@}

#endif//__QF_bspfile_h
