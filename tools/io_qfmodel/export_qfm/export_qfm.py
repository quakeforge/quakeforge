import bpy
import struct
from mathutils import Vector, Quaternion, Matrix
from ..qfmodel import *
import re
import numpy as np
from pprint import pprint

# Specific sub-string targets for Rigify limb segments prone to non-uniform
# stretch drivers
RIGIFY_STRETCH_TARGETS = {"upper_arm", "forearm", "thigh", "shin"}
RIGIFY_FACE_NAMES = [
    "face", "eye", "lid", "brow", "lip", "cheek", "nose",
    "ear", "jaw", "tongue", "teeth", "forehead", "temple", "chin", "mouth"
]

def vmin(a, b):
    return Vector(( min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)))

def vmax(a, b):
    return Vector(( max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)))

class DynObj:
    pass

class TextBlock:
    def __init__(self):
        self.buffer = b"\x00"
        self.offsets = {"": 0}
    def add(self, text):
        if text not in self.offsets:
            self.offsets[text] = len(self.buffer)
            self.buffer += bytes(text, "utf-8") + b"\x00"
        return self.offsets[text]

class Skeleton:
    """
    Unified skeleton compiler. Forces DEF-root to index 0, topologically sorts
    all bones, and dynamically injects virtual leaf bones (.qfscale) to isolate
    non-uniform scale off of parent joints.
    """
    def __init__(self, armature, text):
        self.armature = armature
        self.def_names = []
        self.bone_to_index = {}
        self.virtual_parents = {}
        self.joints = []
        self.inverse = []
        self.poses = []
        # Mapping structure: parent_bone_name -> dict of parameters
        self.scale_split_registry = {}
        self.text = text

        self.compile_skeleton ()
        self.calculate_matrices ()

    def is_face_element(self, bname, pname):
        pname = pname.lower()
        bname = bname.lower()
        return any(x in bname or x in pname for x in RIGIFY_FACE_NAMES)

    def clean_name(self, name, pref):
        if name.startswith(pref):
            return name[len(pref):]
        return name

    def find_parent(self, bname, org2defs):
        arm_data = self.armature.data
        data_bone = arm_data.bones.get(bname)
        org_parent = data_bone.parent
        while org_parent:
            if org_parent.name in self.raw_def_names:
                return org_parent.name
            p_clean = self.clean_name(org_parent.name, "ORG-")
            if p_clean in org2defs and org2defs[p_clean]:
                for fallback_name in reversed(org2defs[p_clean]):
                    if fallback_name == bname:
                        #avoid self-parenting
                        continue
                    if fallback_name in self.raw_def_names:
                        return fallback_name
            if self.is_face_element(bname, p_clean):
                if "DEF-spine.006" in self.raw_def_names:
                    return "DEF-spine.006"
                elif "DEF-head" in self.raw_def_names:
                    return "DEF-head"
            org_parent = org_parent.parent
        return self.root_name

    def build_org2defs(self):
        arm_data = self.armature.data
        org2defs = {}
        for b in arm_data.bones:
            if b.name.startswith("ORG-"):
                org2defs[b.name[4:]] = []
        for name in self.raw_def_names:
            name_clean = name[4:]
            if name_clean in org2defs:
                org2defs[name_clean].append(name)
        for v in org2defs.values():
            v.sort()
        return org2defs

    def build_raw_parents(self):
        org2defs = self.build_org2defs()
        raw_parents = {}
        for bname in self.raw_def_names:
            if bname == self.root_name:
                raw_parents[bname] = -1
                continue
            raw_parents[bname] = self.find_parent(bname, org2defs)
        return raw_parents

    def is_stretch_limb(self, bname):
        bname = bname.lower()
        return any(target in bname for target in RIGIFY_STRETCH_TARGETS)

    def split_scale_bone(self, p_name):
        arm_data = self.armature.data
        p_bone = arm_data.bones.get(p_name)
        # Decompose rest coordinates to establish baseline scale properties safely
        loc_t, rot_t, scale_t = p_bone.matrix_local.decompose()

        # Register the scale split properties using the rest y-scale baseline
        virtual_leaf_name = f"{p_name}.qfscale"
        self.scale_split_registry[p_name] = {
            'leaf_name': virtual_leaf_name,
            'uniform_factor': scale_t.y,
            # Baseline remainder initializes to identity (1.0).
            # Actual dynamic non-uniform variations will pack per-frame in Section 2!
            'remainder_scale': (1.0, 1.0, 1.0)
        }
        return virtual_leaf_name

    def compile_skeleton(self):
        arm_data = self.armature.data
        self.raw_def_names = [b.name for b in arm_data.bones
                              if b.name.startswith("DEF-")]
        self.root_name = "DEF-root"
        if "DEF-root" in arm_data.bones:
            self.root_name = "DEF-root"
        elif "root" in arm_data.bones:
            self.root_name = "root"

        raw_parents = self.build_raw_parents()
        parent_map_names = {self.root_name: -1}

        # Build the "tree" with each bone getting a list of child bones. Leaf
        # bones will have an empty list
        children_registry = {self.root_name: []}
        for bname in self.raw_def_names:
            children_registry[bname] = []
        for child_name, p_name in raw_parents.items():
            if p_name != -1:
                if p_name not in children_registry:
                    children_registry[p_name] = []
                children_registry[p_name].append(child_name)
        # Check for non-leaf bones that are expected to have non-uniform scale
        # and "split" them so they can use uniform scale and with a leaf child
        # that handles the non-uniform scaling
        for p_name, children in children_registry.items():
            if children and self.is_stretch_limb(p_name):
                children.append(self.split_scale_bone(p_name))

        # Create the breadth-first sorted list of bones
        self.def_names.append(self.root_name)
        queue = [self.root_name]
        while queue:
            curr_p = queue.pop(0)
            for child in children_registry.get(curr_p, []):
                if child not in self.def_names:
                    self.def_names.append(child)
                    parent_map_names[child] = curr_p
                    queue.append(child)

        # Create a map from bone name to index in the breadth-first list
        self.bone_to_index = {name: idx for idx, name
                              in enumerate(self.def_names)}
        for bname in self.def_names:
            p_name = parent_map_names.get(bname, -1)
            self.virtual_parents[bname] = self.bone_to_index.get(p_name, -1)

    def calculate_matrices(self):
        arm_data = self.armature.data
        motors = {}
        for idx, bname in enumerate(self.def_names):
            # Handle virtual scale leaf bones
            if bname.endswith(".qfscale"):
                parent_origin_name = bname[:-8]
                p_idx = self.bone_to_index[parent_origin_name]

                # The virtual leaf sits at identity transforms locally relative to the parent
                loc_rest = Vector((0.0, 0.0, 0.0))
                rot_rest = Quaternion((1.0, 0.0, 0.0, 0.0))
                scale_rest = Vector((1.0, 1.0, 1.0))

                # The virtual leaf bone has identity transform, so its rest transform is
                # just a copy of the parent bone's transform
                data_bone_p = arm_data.bones[parent_origin_name]
                rest_world_p = data_bone_p.matrix_local
                rest_world = rest_world_p
            else:
                data_bone = arm_data.bones.get(bname)
                pose_bone = self.armature.pose.bones.get(bname)
                p_idx = self.virtual_parents[bname]
                rest_world = data_bone.matrix_local
                if p_idx != -1:
                    parent_name = self.def_names[p_idx]
                    data_bone_p = arm_data.bones.get(parent_name)
                    rest_world_p = data_bone_p.matrix_local
                    rest_local = rest_world_p.inverted() @ rest_world
                else:
                    rest_local = rest_world
                loc_rest, rot_rest, scale_rest = rest_local.decompose()
                rot_rest.normalize()

            motors[bname] = Motor.from_trs(loc_rest, rot_rest, scale_rest)
            if p_idx != -1:
                p_name = self.def_names[p_idx]
                motors[bname] = motors[p_name] @ motors[bname]

            bname_clean = self.clean_name(bname, "DEF-")
            self.joints.append((
                loc_rest.x, loc_rest.y, loc_rest.z,
                self.text.add(bname_clean),
                rot_rest.x, rot_rest.y, rot_rest.z, rot_rest.w,
                scale_rest.x, scale_rest.y, scale_rest.z,
                p_idx
            ))

            self.inverse.append(motors[bname].inverted())
            anim_local = self.get_TRS(bname)
            self.poses.append(anim_local)

    def get_TRS(self, bname):
        # Skip virtual joints on the first pass; we compute them from their parents
        pose = self.armature.pose
        if bname.endswith(".qfscale"):
            pose_bone = pose.bones.get(bname[:-8])
        else:
            pose_bone = pose.bones.get(bname)
        p_idx = self.virtual_parents[bname]

        local_mat = pose_bone.matrix
        if p_idx != -1:
            p_bone = pose.bones[self.def_names[p_idx]]
            local_mat = (p_bone.matrix).inverted() @ local_mat

        loc, rot, scale = local_mat.decompose()
        rot.normalize()

        if bname.endswith(".qfscale"):
            s = scale.y
            if abs(s) <= 1e-5:
                s = 1
            scale /= s
        elif bname in self.scale_split_registry:
            s = scale.y
            scale = Vector((s, s, s))
        else:
            pass
        return loc, rot, scale

    def compute_bounds(self, packed_meshes, anim_frames):
        # Gather all frame TRS lists: default pose + every frame of every action
        all_trs_frames = [self.poses]
        for act_frames in anim_frames.values():
            for flat_frame in act_frames:
                # Unpack flat 10-float-per-bone list back into (loc, rot, scale)
                trs = []
                for b_idx in range(len(self.def_names)):
                    base = b_idx * 10
                    loc = Vector(flat_frame[base : base + 3])
                    rx, ry, rz, rw = flat_frame[base + 3 : base + 7]
                    rot = Quaternion((rw, rx, ry, rz)) # Blender Quaternion is (w, x, y, z)
                    scale = Vector(flat_frame[base + 7 : base + 10])
                    trs.append((loc, rot, scale))
                all_trs_frames.append(trs)

        # Precompute (num_frames, num_joints, 4, 4) skinning matrix palette
        num_frames = len(all_trs_frames)
        num_joints = len(self.def_names)
        all_skin_mats = np.empty((num_frames, num_joints, 4, 4), dtype=np.float32)

        model_frame = DynObj()
        inf = float('inf')
        model_frame.min = [Vector((inf, inf, inf)) for _ in range(num_frames)]
        model_frame.max = [Vector((-inf, -inf, -inf)) for _ in range(num_frames)]

        for f_idx, frame_trs in enumerate(all_trs_frames):
            motors = [None] * num_joints
            for j_idx, (loc, rot, scale) in enumerate(frame_trs):
                motors[j_idx] = Motor.from_trs(loc, rot, scale)
                p_idx = self.virtual_parents[self.def_names[j_idx]]
                if p_idx != -1:
                    motors[j_idx] = motors[p_idx] @ motors[j_idx]

                skin_mot = motors[j_idx] @ self.inverse[j_idx]
                all_skin_mats[f_idx, j_idx] = skin_mot.to_matrix()

        # Deform each mesh across all frames in vectorized batches
        for pm in packed_meshes:
            mesh = pm.mesh
            N = len(mesh.raw_positions)
            if N == 0:
                continue

            verts = np.ones((N, 4), dtype=np.float32)
            verts[:, :3] = np.array(mesh.raw_positions, dtype=np.float32)
            joints = np.array(mesh.raw_joints, dtype=np.int32)     # (N, 4)
            weights = np.array(mesh.raw_weights, dtype=np.float32) # (N, 4)

            for f_idx in range(num_frames):
                mats = all_skin_mats[f_idx] # (num_joints, 4, 4)
                # Blend 4 matrices per vertex -> (N, 4, 4)
                blended = np.einsum('ni,nij->nj', weights, mats[joints].reshape(N, 4, 16)).reshape(N, 4, 4)
                deformed = np.einsum('nij,nj->ni', blended, verts)[:, :3]

                f_min = Vector(deformed.min(axis=0))
                f_max = Vector(deformed.max(axis=0))
                model_frame.min[f_idx] = vmin(model_frame.min[f_idx], f_min)
                model_frame.max[f_idx] = vmax(model_frame.max[f_idx], f_max)
        return model_frame

class Animation:
    def __init__(self, skel, fps, context):
        self.skel = skel
        self.def_names = skel.def_names
        self.armature = skel.armature
        self.scene = context.scene
        self.clips_bin = b""
        self.keyframes_bin = b""
        self.framedata_bin = b""
        self.actions = []
        self.channels_list = []
        self.channels_bin = b""
        self.frames={}

        self.compile_animations(fps)

    def collect_frames(self):
        channel_count = len(self.def_names) * 10
        limits = DynObj()
        limits.min = [float('inf')] * channel_count
        limits.max = [-float('inf')] * channel_count

        # Cache for pre-evaluated frame data blocks
        # Format: action_name -> list of frame data components
        frames = {}
        for act in self.actions:
            self.armature.animation_data.action = act
            start, end = int(act.frame_range.x), int(act.frame_range.y)
            frames[act.name] = []

            for f in range(start, end + 1):
                self.scene.frame_set(f)
                frame_components = []
                temp_frame_data = {}

                for bname in self.def_names:
                    loc, rot, scale = self.skel.get_TRS(bname)
                    temp_frame_data[bname] = (
                        loc.x, loc.y, loc.z,
                        rot.x, rot.y, rot.z, rot.w, #NOTE component order!
                        scale.x, scale.y, scale.z
                    )
                for bname in self.def_names:
                    frame_components.extend(temp_frame_data[bname])
                frames[act.name].append(frame_components)
                for ch_idx, val in enumerate(frame_components):
                    limits.min[ch_idx] = min(limits.min[ch_idx], val)
                    limits.max[ch_idx] = max(limits.max[ch_idx], val)
        return frames, limits

    def pack_channels(self, limits):
        # Pack channel descriptor metadata chunk (qfm_channel_t)
        # translate=0,4,8 | name=12(skip) | rotate=16,20,24,28 | scale=32,36,40
        CHANNEL_OFFSETS = [0, 4, 8, 16, 20, 24, 28, 32, 36, 40]
        for b_idx in range(len(self.def_names)):
            for c_idx in range(10):
                ch_idx = (b_idx * 10) + c_idx
                c_min = limits.min[ch_idx]
                c_max = limits.max[ch_idx]

                # Emit only those channels that actually contribute to the
                # animation. FIXME better epsilon?
                if c_max > c_min and c_max - c_min > 1e-5:
                    offset = (b_idx * JOINT.size) + CHANNEL_OFFSETS[c_idx]
                    delta = c_max - c_min
                    base = c_min
                    scale = delta / 65535.0
                    self.channels_list.append({
                        'offset': offset,
                        'base': base,
                        'scale': scale,
                        'ch_idx': ch_idx
                    })
                    self.channels_bin += CHANNEL.pack(offset, base, scale)

    def pack_frames(self, frames, fps):
        # Pack individual clip keyframes timeline blocks
        first_frame = 0
        frame_time_step = 1.0 / fps

        flags = 1 #loop FIXME don't hardcode
        for act in self.actions:
            num_frames = len(frames[act.name])
            self.clips_bin += CLIPDESC.pack(
                first_frame, num_frames, flags,
                self.skel.text.add(act.name)
            )
            clip_time = 0.0
            for frame_components in frames[act.name]:
                clip_time += frame_time_step
                # keyframe data offset is relative to the model, but
                # the offsets will be adjusted for that later so for now
                # they're relative to the beginning of the frame data
                offset = len(self.framedata_bin)
                self.keyframes_bin += KEYFRAME.pack(clip_time, offset)

                for i, ch in enumerate(self.channels_list):
                    val = frame_components[ch['ch_idx']]
                    quantized = int(round((val - ch['base']) / ch['scale']))
                    quantized = max(0, min(65535, quantized))
                    self.framedata_bin += struct.pack('<H', quantized)
            first_frame += num_frames

    def compile_animations(self, fps):
        self.actions = [act for act in bpy.data.actions if act.users > 0]
        if not self.actions:
            return

        self.frames, limits = self.collect_frames()
        self.pack_channels(limits)
        self.pack_frames(self.frames, fps)

class Mesh:
    def __init__(self, skel, m_obj, mesh_data):
        self.uv_layer = None
        self.pos_bytes = bytearray()
        self.norm_bytes = bytearray()
        self.tang_bytes = bytearray()
        self.uv_bytes = bytearray()
        self.joint_bytes = bytearray()
        self.weight_bytes = bytearray()
        self.indices_bytes = bytearray()
        self.raw_positions = []
        self.raw_joints = []
        self.raw_weights = []

        self.vertex_groups = m_obj.vertex_groups
        self.wmat = skel.armature.matrix_world.inverted() @ m_obj.matrix_world
        self.tmat = self.wmat.to_3x3()
        # Use the cofactor matrix instead of the inverse transpose so scaling
        # to 0 on one axis doesn't result in broken normals
        self.nmat = Matrix.Identity(3)
        self.nmat.col[0] = self.tmat.col[1].cross(self.tmat.col[2])
        self.nmat.col[1] = self.tmat.col[2].cross(self.tmat.col[0])
        self.nmat.col[2] = self.tmat.col[0].cross(self.tmat.col[1])

        self.compile_mesh (skel, mesh_data)

    def make_joints(self, v, skel):
        v_groups = sorted(v.groups,
                          key=lambda g: g.weight, reverse=True)[:4]
        j_idx = [0] * 4
        w_val = [0] * 4
        for i, g in enumerate(v_groups):
            group_name = self.vertex_groups[g.group].name

            if group_name in skel.bone_to_index:
                # REMAPPING FILTER: If this group matches a uniformized
                # parent, automatically transfer weights onto its
                # non-uniform child leaf (.qfscale)
                if group_name in skel.scale_split_registry:
                    split = skel.scale_split_registry[group_name]
                    leaf_target = split['leaf_name']
                    j_idx[i] = skel.bone_to_index[leaf_target]
                else:
                    j_idx[i] = skel.bone_to_index[group_name]

                w_val[i] = int(round(g.weight * 255.0))
        w_sum = sum(w_val)
        if w_sum > 0:
            w_val = [int(round(w * 255 / w_sum)) for w in w_val]
        return j_idx, w_val

    def add_vertex(self, v, uv, loop, skel):
        pos = self.wmat @ v.co
        # If either norm or tang become 0, it means the face has an
        # area of 0 so isn't visible anyway.
        norm = self.nmat @ loop.normal
        norm = norm.normalized() if norm else norm
        tang = self.tmat @ loop.tangent
        tang = tang.normalized() if tang else tang
        bitang_sign = loop.bitangent_sign
        uv = Vector((uv.x, 1 - uv.y))
        j_idx, w_val = self.make_joints(v, skel)
        v_key = (tuple(pos), tuple(norm), tuple(uv),
                 tuple(j_idx), tuple(w_val))
        if v_key not in self.unique_verts:
            self.unique_verts[v_key] = self.vert_counter
            self.bounds.min = vmin (self.bounds.min, pos)
            self.bounds.max = vmax (self.bounds.max, pos)
            self.pos_bytes.extend(struct.pack('<3f', *pos))
            self.norm_bytes.extend(struct.pack('<3f', *norm))
            self.tang_bytes.extend(struct.pack('<4f', *tang, bitang_sign))
            self.uv_bytes.extend(struct.pack('<2f', uv[0], uv[1]))
            self.joint_bytes.extend(struct.pack('<4B', *j_idx))
            self.weight_bytes.extend(struct.pack('<4B', *w_val))
            self.vert_counter += 1

            self.raw_positions.append(tuple(pos))
            self.raw_joints.append(tuple(j_idx))
            self.raw_weights.append(tuple(w / 255.0 for w in w_val))
        return self.unique_verts[v_key]

    def compile_mesh(self, skel, mesh_data):
        mesh_data.calc_tangents()
        if mesh_data.uv_layers.active:
            uv_layer = mesh_data.uv_layers.active.data
        self.bounds = DynObj()
        inf = float('inf')
        self.bounds.min = Vector((inf, inf, inf))
        self.bounds.max = Vector((-inf, -inf, -inf))

        self.unique_verts = {}
        self.vert_counter = 0

        for tri in mesh_data.loop_triangles:
            face_indices = []
            for loop_idx in tri.loops:
                loop = mesh_data.loops[loop_idx]
                v_idx = loop.vertex_index
                v = mesh_data.vertices[v_idx]
                uv = uv_layer[loop_idx].uv if uv_layer else Vector((0.0, 1.0))
                face_indices.append(self.add_vertex(v, uv, loop, skel))
            #FIXME despite quake being right-handed, triangles are clockwise
            face_indices.reverse()
            self.indices_bytes.extend(struct.pack('<3I', *face_indices))

class PackedMesh:
    def __init__(self, skel, m_obj, mesh_data, text):
        mesh_data.calc_loop_triangles()
        if not mesh_data.loop_triangles:
            return None
        mesh = Mesh(skel, m_obj, mesh_data)
        self.mesh = mesh
        clean_mname = m_obj.name
        self.name_ofs = text.add(clean_mname)
        self.tri_count = len(mesh_data.loop_triangles)
        self.vertices_cnt = mesh.vert_counter
        self.indices_bytes = mesh.indices_bytes
        self.pos_bytes = mesh.pos_bytes
        self.norm_bytes = mesh.norm_bytes
        self.tang_bytes = mesh.tang_bytes
        self.uv_bytes = mesh.uv_bytes
        self.joint_bytes = mesh.joint_bytes
        self.weight_bytes = mesh.weight_bytes
        self.bounds = mesh.bounds

def export_qfm(context, filepath, fps=30.0):
    """Main pipeline execution wrapper loops over selected components with animations."""
    objs = context.selected_objects
    armature = next((o for o in objs if o.type == 'ARMATURE'), None)
    #FIXME support exporting meshes without an armature
    if not armature:
        print("Error: No armature highlighted in selection.")
        return

    text = TextBlock()
    skel = Skeleton(armature, text)

    anim_data = armature.animation_data
    old_action = anim_data.action if anim_data else None
    old_frame = context.scene.frame_current
    try:
        anim = Animation(skel, fps, context)
    finally:
        if anim_data:
            anim_data.action = old_action
        context.scene.frame_current = old_frame

    mesh_objects = [o for o in objs if o.type == 'MESH']
    packed_meshes = []
    for m_obj in mesh_objects:
        ignored_modifiers = {}
        for mod in m_obj.modifiers:
            if mod.type in {'ARMATURE', 'SUBSURF'}:
                ignored_modifiers[mod] = (mod.show_viewport, mod.show_render)
                mod.show_viewport = False
                mod.show_render = False
        try:
            dg = context.evaluated_depsgraph_get()
            mesh_data = m_obj.evaluated_get(dg).to_mesh()
        finally:
            for mod, (show_view, show_rend) in ignored_modifiers.items():
                mod.show_viewport = show_view
                mod.show_render = show_rend
        pm = PackedMesh(skel, m_obj, mesh_data, text)
        if (pm):
            packed_meshes.append(pm)
        m_obj.to_mesh_clear()
    model_frame = skel.compute_bounds(packed_meshes, anim.frames)
    def_pose_bounds = DynObj()
    def_pose_bounds.min = model_frame.min[0]
    def_pose_bounds.max = model_frame.max[0]
    model_frame.min = model_frame.min[1:]
    model_frame.max = model_frame.max[1:]

    binary_payload = bytearray()

    # Text Chunk
    ofs_text = MODEL.size + len(binary_payload)
    binary_payload.extend(text.buffer)
    binary_payload = bytearray(pad_bytes(binary_payload))

    # Joints Chunk
    ofs_joints = MODEL.size + len(binary_payload)
    for j in skel.joints:
        binary_payload.extend(JOINT.pack(*j))
    binary_payload = bytearray(pad_bytes(binary_payload))

    # Inverse Bind Motors Chunk
    ofs_inverse = MODEL.size + len(binary_payload)
    for inv in skel.inverse:
        binary_payload.extend(pack_motor(inv))
    binary_payload = bytearray(pad_bytes(binary_payload))

    # Rest Posture Base Pose Chunk
    ofs_pose = MODEL.size + len(binary_payload)
    for idx, p in enumerate(skel.poses):
        loc, rot, scale = p
        joint_name = skel.joints[idx][3]
        joint_parent = skel.joints[idx][11]
        binary_payload.extend(JOINT.pack(
            loc.x, loc.y, loc.z, joint_name,
            rot.x, rot.y, rot.z, rot.w,
            scale.x, scale.y, scale.z, joint_parent
        ))
    binary_payload = bytearray(pad_bytes(binary_payload))

    # Quantized Animation Scalar Channels map chunk
    ofs_channels = MODEL.size + len(binary_payload) if len(anim.channels_bin) > 0 else 0
    binary_payload.extend(anim.channels_bin)
    binary_payload = bytearray(pad_bytes(binary_payload))

    # Animation structures alignment tracking
    ofs_clips = 0
    ofs_keyframes = 0
    ofs_framedata = 0
    ofs_frames = 0
    if len(anim.clips_bin) > 0:
        ofs_framedata = MODEL.size + len(binary_payload)
    binary_payload.extend(anim.framedata_bin)
    binary_payload = bytearray(pad_bytes(binary_payload))

    ofs_clips = MODEL.size + len(binary_payload)
    binary_payload.extend(anim.clips_bin)
    binary_payload = bytearray(pad_bytes(binary_payload))

    ofs_keyframes = MODEL.size + len(binary_payload)
    # Shift keyframe payload tracking offsets relative to global file position
    mutable_keyframes = bytearray(anim.keyframes_bin)
    for i in range(0, len(mutable_keyframes), KEYFRAME.size):
        etime, kf_offset = KEYFRAME.unpack(mutable_keyframes[i:i+KEYFRAME.size])
        mutable_keyframes[i:i+KEYFRAME.size] = KEYFRAME.pack(etime, ofs_framedata + kf_offset)
    binary_payload.extend(mutable_keyframes)
    binary_payload = bytearray(pad_bytes(binary_payload))

    frames = [FRAME.pack(0, 0, *min, *max)
              for min,max in zip(model_frame.min, model_frame.max)]
    if frames:
        ofs_frames = MODEL.size + len(binary_payload)
        binary_payload.extend(b"".join(frames))
        binary_payload = bytearray(pad_bytes(binary_payload))

    # Process individual Mesh Chunks internally
    mesh_headers = []
    local_mesh_payloads = bytearray()

    geom_stride = 20
    render_stride = 36
    for pm in packed_meshes:
        local_mesh_stream = bytearray()

        # Repack the vertex attribute arrays so they can be separated for
        # geometry-only rendering (depth, shadows) and texturing
        geom_stream = bytearray()   # Set 0: Pos(12b) + Joints(4b) + Weights(4b) = 20b Stride
        render_stream = bytearray() # Set 2: Norm(12b) + Tang(16b) + UV(8b) = 36b Stride

        for i in range(pm.vertices_cnt):
            p_chunk = pm.pos_bytes[i*12 : (i+1)*12]
            n_chunk = pm.norm_bytes[i*12 : (i+1)*12]
            t_chunk = pm.tang_bytes[i*16 : (i+1)*16]
            u_chunk = pm.uv_bytes[i*8  : (i+1)*8]
            j_chunk = pm.joint_bytes[i*4 : (i+1)*4]
            w_chunk = pm.weight_bytes[i*4 : (i+1)*4]

            geom_stream.extend(p_chunk)   # Pos (Offset 0)
            geom_stream.extend(j_chunk)   # Joints (Offset 12)
            geom_stream.extend(w_chunk)   # Weights (Offset 16)

            render_stream.extend(n_chunk) # Norm (Offset 0)
            render_stream.extend(t_chunk) # Tang (Offset 12)
            render_stream.extend(u_chunk) # UV (Offset 28)

        ofs_render_base = len(geom_stream)

        descriptors = [
            # Set 0: Geometry Stream (Stride = 20)
            qfm_attrdesc( 0, geom_stride, qfm_position, 0, 0, qfm_f32, 3),
            qfm_attrdesc(12, geom_stride, qfm_joints,   0, 0, qfm_u8,  4),#FIXME support u16
            qfm_attrdesc(16, geom_stride, qfm_weights,  0, 0, qfm_u8n, 4),

            # Set 2: Rendering Stream (Stride = 36)
            qfm_attrdesc(ofs_render_base + 0,  render_stride, qfm_normal,   2, 0, qfm_f32, 3),
            qfm_attrdesc(ofs_render_base + 12, render_stride, qfm_tangent,  2, 0, qfm_f32, 4),
            qfm_attrdesc(ofs_render_base + 28, render_stride, qfm_texcoord, 2, 0, qfm_f32, 2),
        ]
        desc_bytes = b"".join(ATTRDESC.pack(*d) for d in descriptors)

        # --- PHASE 3: CONCATENATE CHUNKS WITH RELATIVE DISPLACEMENTS ---
        # Unpack descriptors array locally inside the mesh footprint
        local_ofs_desc = MESH.size + len(local_mesh_stream)
        local_mesh_stream.extend(desc_bytes)

        # Unpack indices array locally inside the mesh footprint
        local_ofs_indices = MESH.size + len(local_mesh_stream)
        local_mesh_stream.extend(pm.indices_bytes)
        local_mesh_stream = bytearray(pad_bytes(local_mesh_stream))

        # Unpack the two flat continuous vertex blocks sequentially
        local_ofs_verts = MESH.size + len(local_mesh_stream)
        local_mesh_stream.extend(geom_stream)
        local_mesh_stream.extend(render_stream)
        local_mesh_stream = bytearray(pad_bytes(local_mesh_stream))

        local_mesh_payloads.extend(local_mesh_stream)

        # Compile struct member tuples
        mesh_base_fields      = (pm.name_ofs, pm.tri_count, 6, local_ofs_indices)
        mesh_adjacency        = (0, 0)
        mesh_attributes       = (local_ofs_desc, len(descriptors))
        mesh_vertices         = (local_ofs_verts, pm.vertices_cnt)
        mesh_morph_attributes = (0, 0)
        mesh_strides_material = (0, 0, 0)
        mesh_morph_anim       = (0, 0, 0, 0)
        mesh_skin_anim        = (0, 0, 0, 0)
        mesh_scale            = (1.0, 1.0, 1.0)
        mesh_scale_origin     = (0.0, 0.0, 0.0)
        mesh_bounds_min       = tuple(pm.bounds.min)
        mesh_bounds_max       = tuple(pm.bounds.max)

        # Pack the qf_mesh_t structure block
        mesh_headers.append(MESH.pack(
            *mesh_base_fields,
            *mesh_adjacency,
            *mesh_attributes,
            *mesh_vertices,
            *mesh_morph_attributes,
            *mesh_strides_material,
            *mesh_morph_anim,
            *mesh_skin_anim,
            *mesh_scale,
            *mesh_scale_origin,
            *mesh_bounds_min,
            *mesh_bounds_max
        ))

    # Append complete mesh layouts to file execution buffer stack
    ofs_meshes = MODEL.size + len(binary_payload)
    for mh in mesh_headers:
        binary_payload.extend(mh)
    binary_payload.extend(local_mesh_payloads)

    # Compile dynamic structural header table layout allocations
    header_bytes = MODEL.pack(
        ofs_meshes, len(packed_meshes),
        ofs_joints, len(skel.joints),
        ofs_inverse, len(skel.joints),
        ofs_pose, len(skel.joints),
        ofs_channels, len(anim.channels_list) if anim.channels_bin else 0,
        ofs_text, len(text.buffer),
        len(anim.actions), ofs_clips, ofs_keyframes, ofs_frames,
        *def_pose_bounds.min, *def_pose_bounds.max,
        0, 0 # crc, render_data
    )
    with open(filepath, "wb") as f:
        f.write(header_bytes)
        f.write(binary_payload)
    print(f"model and animations written to {filepath}")
