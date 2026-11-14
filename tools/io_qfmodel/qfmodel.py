# vim:ts=4:et
# Custom Python Binary Layout Module for QuakeForge Model Assets

from mathutils import Vector, Quaternion
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

# qfm_frame_t: name(I), data(I), bounds_min(3f), bounds_max(3f)
# 32 bytes
FRAME = Struct('<2I3f3f')

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

# qf_model_t
# 96 bytes
MODEL = Struct(
    '<'
    '2I'    # meshes(LOC)
    '2I'    # joints(LOC)
    '2I'    # inverse(LOC)
    '2I'    # pose(LOC)
    '2I'    # channels(LOC)
    '2I'    # text(LOC)
    '4I'    # anim(ANIM)
    '3f'    # vec3_t     bounds_min
    '3f'    # vec3_t     bounds_max
    'H'     # crc
    '2x'    # alignment padding
    'I'     # render_data (must be 0)
)

def pad_bytes(data, alignment=16):
    padding = (alignment - (len(data) % alignment)) % alignment
    return data + (b'\x00' * padding)

def pack_motor(motor):
    is_nonuniform = 1 if (abs(motor.s.x - motor.s.y) > 1e-4
                          or abs(motor.s.x - motor.s.z) > 1e-4) else 0
    return MOTOR.pack(*motor.q, *motor.t, *motor.s, is_nonuniform)

del Struct, pack, unpack

class Motor:
    """mirror of qfm_motor_t from qfmodel.h."""
    def __init__(self, q, t, s):
        self.q = q # (x, y, z, w)
        self.t = t # (x, y, z, w)
        self.s = s # Vector((sx, sy, sz))

    @classmethod
    def from_trs(cls, loc, rot, scale):
        # q = qconjf(joint.rotate) -> Blender rot is (w, x, y, z)
        qx, qy, qz, qw = -rot.x, -rot.y, -rot.z, rot.w
        tx, ty, tz = loc.x * 0.5, loc.y * 0.5, loc.z * 0.5

        # p = crossf(t, sa) - c * t; p[3] = -dotf(t, sa)
        px = (ty * qz - tz * qy) - (qw * tx)
        py = (tz * qx - tx * qz) - (qw * ty)
        pz = (tx * qy - ty * qx) - (qw * tz)
        pw = -(tx * qx + ty * qy + tz * qz)
        return cls((qx, qy, qz, qw), (px, py, pz, pw), Vector(scale))

    def inverted(self):
        qx, qy, qz, qw = self.q
        tx, ty, tz, tw = self.t
        inv_s = Vector((1.0 / self.s.x, 1.0 / self.s.y, 1.0 / self.s.z))
        return Motor((-qx, -qy, -qz, qw), (-tx, -ty, -tz, tw), inv_s)

    def __matmul__(self, other):
        b1x, b1y, b1z, a1 = self.q
        c1x, c1y, c1z, d1 = self.t
        s1 = self.s

        b2x, b2y, b2z, a2 = other.q
        # Scale child translation by s1.x (matches c2 = loadxyzf(m2.t) * s1[0])
        c2x = other.t[0] * s1.x
        c2y = other.t[1] * s1.x
        c2z = other.t[2] * s1.x
        d2  = other.t[3] * s1.x

        a  = a1 * a2 - (b1x * b2x + b1y * b2y + b1z * b2z)
        bx = a1 * b2x + a2 * b1x - (b1y * b2z - b1z * b2y)
        by = a1 * b2y + a2 * b1y - (b1z * b2x - b1x * b2z)
        bz = a1 * b2z + a2 * b1z - (b1x * b2y - b1y * b2x)

        cx = a1 * c2x + a2 * c1x - (b1y * c2z - b1z * c2y) - (c1y * b2z - c1z * b2y) - d1 * b2x - d2 * b1x
        cy = a1 * c2y + a2 * c1y - (b1z * c2x - b1x * c2z) - (c1z * b2x - c1x * b2z) - d1 * b2y - d2 * b1y
        cz = a1 * c2z + a2 * c1z - (b1x * c2y - b1y * c2x) - (c1x * b2y - c1y * b2x) - d1 * b2z - d2 * b1z
        d  = a1 * d2 + a2 * d1 + (b1x * c2x + b1y * c2y + b1z * c2z) + (c1x * b2x + c1y * b2y + c1z * b2z)

        s = Vector((s1.x * other.s.x, s1.y * other.s.y, s1.z * other.s.z))
        return Motor((bx, by, bz, a), (cx, cy, cz, d), s)

    def to_matrix(self):
        qx, qy, qz, qw = self.q
        tx, ty, tz, tw = self.t

        # q = qconjf(m.q) -> (-qx, -qy, -qz, qw)
        rot = Quaternion((qw, -qx, -qy, -qz))

        # t = -2 * qmulf(m.t, m.q) (quaternion product of t and q)
        rx = -2.0 * (tw * qx + tx * qw + ty * qz - tz * qy)
        ry = -2.0 * (tw * qy - tx * qz + ty * qw + tz * qx)
        rz = -2.0 * (tw * qz + tx * qy - ty * qx + tz * qw)

        mat = rot.to_matrix().to_4x4()
        mat.col[0] *= self.s.x
        mat.col[1] *= self.s.y
        mat.col[2] *= self.s.z
        mat.translation = Vector((rx, ry, rz))
        return mat
