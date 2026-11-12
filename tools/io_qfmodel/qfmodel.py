# vim:ts=4:et
# Custom Python Binary Layout Module for QuakeForge Model Assets

from struct import Struct, pack, unpack

qfm_s8   = 0
qfm_s16  = 1
qfm_s32  = 2
qfm_s64  = 3

qfm_u8   = 4
qfm_u16  = 5
qfm_u32  = 6
qfm_u64  = 7

qfm_s8n  = 8
qfm_s16n = 9
qfm_u8n  = 10
qfm_u16n = 11

qfm_f16  = 12
qfm_f32  = 13
qfm_f64  = 14

# Type conversions from qfm_type_t enumerations
QFM_TYPES = {
    qfm_s8:  ('b', 1,     1,                 -128,                  127),
    qfm_s16: ('h', 2,     1,               -32768,                32767),
    qfm_s32: ('i', 4,     1,          -2147483648,           2147483647),
    qfm_s64: ('l', 8,     1, -9223372036854775808,  9223372036854775807),
    qfm_u8:  ('B', 1,     1,                    0,                  255),
    qfm_u16: ('H', 2,     1,                    0,                65535),
    qfm_u32: ('I', 4,     1,                    0,           4294967295),
    qfm_u64: ('L', 8,     1,                    0, 18446744073709551615),
    qfm_s8n: ('b', 1,   127,                 -1.0,                  1.0),
    qfm_s16n:('h', 2, 32767,                 -1.0,                  1.0),
    qfm_u8n: ('B', 1,   255,                  0.0,                  1.0),
    qfm_u16n:('H', 2, 65535,                  0.0,                  1.0),
    qfm_f16: ('e', 2,     1,        -float('inf'),         float('inf')),
    qfm_f32: ('f', 4,     1,        -float('inf'),         float('inf')),
    qfm_f64: ('d', 8,     1,        -float('inf'),         float('inf')),
}

# Attribute positions from qfm_attr_t enumerations
qfm_position = 0
qfm_normal   = 1
qfm_tangent  = 2
qfm_texcoord = 3
qfm_color    = 4
qfm_joints   = 5
qfm_weights  = 6

def qfm_attrdesc(offset, stride, attr, set, morph, type, components):
    return (offset, stride | (attr << 16) | (set << 19) | (morph << 22)
                    | (type << 24) | (components << 28))

# Structure Formats (Matching qfmodel.h alignments and types)

# 2 * uint32: offset, count
LOC = Struct('<2I')

# uint32: numclips, clips_ofs, keyframes_ofs, data_ofs
ANIM = Struct('<4I')

# qfm_joint_t: translate(3f), name(I), rotate(4f), scale(3f), parent(i)
# 48 bytes
JOINT = Struct('<3fI4f3fi')

# qfm_motor_t: q(4f), t(4f), s(3f), flags(I)
# 48 bytes
MOTOR = Struct('<4f4f3fI')

# qfm_channel_t: data(I), base(f), scale(f)
# 12 bytes
CHANNEL = Struct('<I2f')

# clipdesc_t: firstframe(I), numframes(I), flags(I), name(I)
# 16 bytes
CLIPDESC = Struct('<4I')

# keyframe_t: endtime(f), data(I)
# 8 bytes
KEYFRAME = Struct('<fI')

# qfm_attrdesc_t: offset, bitfields
# 8 bytes
ATTRDESC = Struct('<2I')

# qf_mesh_t:
#  144 bytes
MESH = Struct(
    '<'
    'I'     # name
    'I'     # triangle_count
    'I'     # index_type
    'I'     # indices
    '2I'    # qfm_loc_t  adjacency
    '2I'    # qfm_loc_t  attributes
    '2I'    # qfm_loc_t  vertices
    '2I'    # qfm_loc_t  morph_attributes
    'I'     # vertex_stride
    'I'     # morph_stride
    'I'     # material
    '4I'    # anim_t     morph
    '4I'    # anim_t     skin
    '3f'    # vec3_t     scale
    '3f'    # vec3_t     scale_origin
    '3f'    # vec3_t     bounds_min
    '3f'    # vec3_t     bounds_max
    '4x'    # aligned (16) padding
)

# qf_model_t Header
# 80 bytes
HEADER = Struct(
    '<'
    '2I'    # meshes(LOC)
    '2I'    # joints(LOC)
    '2I'    # inverse(LOC)
    '2I'    # pose(LOC)
    '2I'    # channels(LOC)
    '2I'    # text(LOC)
    '4I'    # anim(ANIM)
    'H'     # crc
    '2x'    # alignment padding
    'I'     # render_data (must be 0)
    '8x'    # aligned (16) padding
)

def pad_bytes(data, alignment=16):
    padding = (alignment - (len(data) % alignment)) % alignment
    return data + (b'\x00' * padding)

def pack_motor(matrix):
    """Encodes a 4x4 matrix into a motor structure (qfm_motor_t)."""
    loc, rot, scale = matrix.decompose()
    q = rot.normalized()

    qx, qy, qz, qw = -q.x, -q.y, -q.z, q.w
    tx, ty, tz = loc.x * 0.5, loc.y * 0.5, loc.z * 0.5

    px = (ty * qz - tz * qy) - (qw * tx)
    py = (tz * qx - tx * qz) - (qw * ty)
    pz = (tx * qy - ty * qx) - (qw * tz)
    pw = -(tx * qx + ty * qy + tz * qz)

    is_nonuniform = 1 if (abs(scale.x - scale.y) > 1e-4
                          or abs(scale.x - scale.z) > 1e-4) else 0
    return MOTOR.pack(qx, qy, qz, qw, px, py, pz, pw,
                      scale.x, scale.y, scale.z, is_nonuniform)

del Struct, pack, unpack
