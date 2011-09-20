# vim:ts=4:et
# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

from struct import unpack
from pprint import pprint

import bpy
from bpy_extras.object_utils import object_data_add
from mathutils import Vector,Matrix

from . import quakepal

class MDL:
    class Skin:
        def __init__(self):
            pass
        def read(self, mdl, sub=0):
            self.width, self.height = mdl.skinwidth, mdl.skinheight
            if sub:
                self.type = 0
                self.read_pixels(mdl)
                return self
            self.type = mdl.read_int()
            if self.type:
                # skin group
                num = mdl.read_int()
                self.times = mdl.read_float(num)
                self.skins = []
                for i in range(num):
                    self.skins.append(MDL.Skin().read(mdl, 1))
                    num -= 1
                return self
            self.read_pixels(mdl)
            return self

        def read_pixels(self, mdl):
            size = self.width * self.height
            self.pixels = mdl.read_bytes(size)

    class STVert:
        def __init__(self):
            pass
        def read(self, mdl):
            self.onseam = mdl.read_int()
            self.s, self.t = mdl.read_int(2)
            return self

    class Tri:
        def __init__(self):
            pass
        def read(self, mdl):
            self.facesfront = mdl.read_int()
            self.verts = mdl.read_int(3)
            return self

    class Frame:
        def __init__(self):
            pass
        def read(self, mdl, numverts, sub=0):
            if sub:
                self.type = 0
            else:
                self.type = mdl.read_int()
            if self.type:
                num = mdl.read_int()
                self.read_bounds(mdl)
                self.times = mdl.read_float(num)
                self.frames = []
                for i in range(num):
                    self.frames.append(MDL.Frame().read(mdl, numverts, 1))
                return self
            self.read_bounds(mdl)
            self.read_name(mdl)
            self.read_verts(mdl, numverts)
            return self

        def read_name(self, mdl):
            if mdl.version == 6:
                name = mdl.read_string(16)
            else:
                name = ""
            if "\0" in name:
                name = name[:name.index("\0")]
            self.name = name

        def read_bounds(self, mdl):
            self.mins = mdl.read_byte(4)[:3]    #discard normal index
            self.maxs = mdl.read_byte(4)[:3]    #discard normal index

        def read_verts(self, mdl, num):
            self.verts = []
            for i in range(num):
                self.verts.append(MDL.Vert().read(mdl))

    class Vert:
        def __init__(self):
            pass
        def read(self, mdl):
            self.r = mdl.read_byte(3)
            self.ni = mdl.read_byte()
            return self

    def read_byte(self, count=1):
        size = 1 * count
        data = self.file.read(size)
        data = unpack("<%dB" % count, data)
        if count == 1:
            return data[0]
        return data

    def read_int(self, count=1):
        size = 4 * count
        data = self.file.read(size)
        data = unpack("<%di" % count, data)
        if count == 1:
            return data[0]
        return data

    def read_float(self, count=1):
        size = 4 * count
        data = self.file.read(size)
        data = unpack("<%df" % count, data)
        if count == 1:
            return data[0]
        return data

    def read_bytes(self, size):
        return self.file.read(size)

    def read_string(self, size):
        data = self.file.read(size)
        s = ""
        for c in data:
            s = s + chr(c)
        return s

    def __init__(self):
        pass
    def read(self, filepath):
        self.file = open(filepath, "rb")
        self.name = filepath.split('/')[-1]
        self.name = self.name.split('.')[0]
        self.ident = self.read_string(4)
        self.version = self.read_int()
        if self.ident not in ["IDPO", "MD16"] or self.version not in [3, 6]:
            return None
        self.scale = Vector(self.read_float(3))
        self.scale_origin = Vector(self.read_float(3))
        self.boundingradius = self.read_float()
        self.eyeposition = Vector(self.read_float(3))
        numskins = self.read_int()
        self.skinwidth, self.skinheight = self.read_int(2)
        numverts, numtris, numframes = self.read_int(3)
        self.synctype = self.read_int()
        if self.version == 6:
            self.flags = self.read_int()
            self.size = self.read_float()
        # read in the skin data
        self.skins = []
        for i in range(numskins):
            self.skins.append(MDL.Skin().read(self))
        #read in the st verts (uv map)
        self.stverts = []
        for i in range(numverts):
            self.stverts.append (MDL.STVert().read(self))
        #read in the tris
        self.tris = []
        for i in range(numtris):
            self.tris.append(MDL.Tri().read(self))
        #read in the frames
        self.frames = []
        for i in range(numframes):
            self.frames.append(MDL.Frame().read(self, numverts))
        return self

def make_verts(mdl, framenum, subframenum=0):
    frame = mdl.frames[framenum]
    if frame.type:
        frame = frame.frames[subframenum]
    verts = []
    s = mdl.scale
    o = mdl.scale_origin
    m = Matrix(((s.x,  0,  0,  0),
                (  0,s.y,  0,  0),
                (  0,  0,s.z,  0),
                (o.x,o.y,o.z,  1)))
    for v in frame.verts:
        try:    #FIXME
            verts.append(Vector(v.r) * m)
        except ValueError:
            verts.append(m * Vector(v.r))
    return verts

def make_faces(mdl):
    faces = []
    uvs = []
    for tri in mdl.tris:
        tv = tri.verts
        tv = tv[2], tv[1], tv[0]    # flip the normal by reversing the winding
        faces.append (tv)
        sts = []
        for v in tri.verts:
            stv = mdl.stverts[v]
            s = stv.s
            t = stv.t
            if stv.onseam and not tri.facesfront:
                s += mdl.skinwidth / 2
            # quake textures are top to bottom, but blender images
            # are bottom to top
            sts.append((s * 1.0 / mdl.skinwidth, 1 - t * 1.0 / mdl.skinheight))
        sts = sts[2], sts[1], sts[0]    # to match face vert reversal
        uvs.append(sts)
    return faces, uvs

def load_skins(mdl):
    def load_skin(skin, name):
        img = bpy.data.images.new(name, mdl.skinwidth, mdl.skinheight)
        mdl.images.append(img)
        p = [0.0] * mdl.skinwidth * mdl.skinheight * 4
        d = skin.pixels
        for j in range(mdl.skinheight):
            for k in range(mdl.skinwidth):
                c = quakepal.palette[d[j * mdl.skinwidth + k]]
                # quake textures are top to bottom, but blender images
                # are bottom to top
                l = ((mdl.skinheight - 1 - j) * mdl.skinwidth + k) * 4
                p[l + 0] = c[0] / 255.0
                p[l + 1] = c[1] / 255.0
                p[l + 2] = c[2] / 255.0
                p[l + 3] = 1.0
        img.pixels[:] = p[:]
        if hasattr(img, "pack"):
            img.pack(True)

    mdl.images=[]
    for i, skin in enumerate(mdl.skins):
        if skin.type:
            for j, subskin in enumerate(skin.skins):
                load_skin (subskin, "%s_%d_%d" % (mdl.name, i, j))
        else:
            load_skin (skin, "%s_%d" % (mdl.name, i))

def setup_skins (mdl, uvs):
    load_skins (mdl)
    img = mdl.images[0]   # use the first skin for now
    uvlay = mdl.mesh.uv_textures.new(mdl.name)
    for i, f in enumerate(uvlay.data):
        mdl_uv = uvs[i]
        for j, uv in enumerate(f.uv):
            uv[0], uv[1] = mdl_uv[j]
        f.image = img
        f.use_image = True
    mat = bpy.data.materials.new(mdl.name)
    mat.diffuse_color = (1,1,1)
    mat.use_raytrace = False
    tex = bpy.data.textures.new(mdl.name, 'IMAGE')
    tex.extension = 'CLIP'
    tex.use_preview_alpha = True
    tex.image = img
    mat.texture_slots.add()
    ts = mat.texture_slots[0]
    ts.texture = tex
    ts.use_map_alpha = True
    ts.texture_coords = 'UV'
    mdl.mesh.materials.append(mat)

def make_shape_key(mdl, framenum, subframenum=0):
    frame = mdl.frames[framenum]
    name = "%s_%d" % (mdl.name, framenum)
    if frame.type:
        frame = frame.frames[subframenum]
        name = "%s_%d_%d" % (mdl.name, framenum, subframenum)
    if frame.name:
        name = frame.name
    else:
        frame.name = name
    frame.key = mdl.obj.shape_key_add(name)
    frame.key.value = 0.0
    mdl.keys.append (frame.key)
    s = mdl.scale
    o = mdl.scale_origin
    m = Matrix(((s.x,  0,  0,  0),
                (  0,s.y,  0,  0),
                (  0,  0,s.z,  0),
                (o.x,o.y,o.z,  1)))
    for i, v in enumerate(frame.verts):
        try:    #FIXME
            frame.key.data[i].co = Vector(v.r) * m
        except ValueError:
            frame.key.data[i].co = m * Vector(v.r)

def build_shape_keys(mdl):
    mdl.keys = []
    mdl.obj.shape_key_add("Basis")
    for i, frame in enumerate(mdl.frames):
        frame = mdl.frames[i]
        if frame.type:
            for j in range(len(frame.frames)):
                make_shape_key(mdl, i, j)
        else:
            make_shape_key(mdl, i)

def set_keys(act, data):
    for d in data:
        key, co = d
        dp = """key_blocks["%s"].value""" % key.name
        fc = act.fcurves.new(data_path = dp)
        fc.keyframe_points.add(len(co))
        for i in range(len(co)):
            fc.keyframe_points[i].co = co[i]

def build_actions(mdl):
    sk = mdl.mesh.shape_keys
    for frame in mdl.frames:
        sk.animation_data_create()
        sk.animation_data.action = bpy.data.actions.new(frame.name)
        act=sk.animation_data.action
        data = []
        other_keys = mdl.keys[:]
        if frame.type:
            for j, subframe in enumerate(frame.frames):
                co = []
                if j > 1:
                    co.append ((1.0, 0.0))
                if j > 0:
                    co.append ((j * 1.0, 0.0))
                co.append (((j + 1) * 1.0, 1.0))
                if j < len(frame.frames) - 2:
                    co.append (((j + 2) * 1.0, 0.0))
                if j < len(frame.frames) - 1:
                    co.append ((len(frame.frames) * 1.0, 0.0))
                data.append((subframe.key, co))
                if subframe.key in other_keys:
                    del(other_keys[other_keys.index(subframe.key)])
            co = [(1.0, 0.0), (len(frame.frames) * 1.0, 0.0)]
            for k in other_keys:
                data.append((k, co))
        else:
            data.append((frame.key, [(1.0, 1.0)]))
            if frame.key in other_keys:
                del(other_keys[other_keys.index(frame.key)])
            co = [(1.0, 0.0)]
            for k in other_keys:
                data.append((k, co))
        set_keys (act, data)

def merge_frames(mdl):
    def get_base(name):
        i = 0
        while i < len(name) and name[i] not in "0123456789":
            i += 1
        return name[:i]

    i = 0
    while i < len(mdl.frames):
        if mdl.frames[i].type:
            i += 1
            continue
        base = get_base(mdl.frames[i].name)
        j = i + 1
        while j < len(mdl.frames):
            if mdl.frames[j].type:
                break
            if get_base(mdl.frames[j].name) != base:
                break
            j += 1
        f = MDL.Frame()
        f.name = base
        f.type = 1
        f.frames = mdl.frames[i:j]
        mdl.frames[i:j] = [f]
        i += 1

def import_mdl(operator, context, filepath):
    bpy.context.user_preferences.edit.use_global_undo = False

    for obj in bpy.context.scene.objects:
        obj.select = False

    mdl = MDL()
    if not mdl.read(filepath):
        operator.report({'ERROR'},
            "Unrecognized format: %s %d" % (mdl.ident, mdl.version))
        return {'CANCELLED'}
    faces, uvs = make_faces (mdl)
    verts = make_verts (mdl, 0)
    mdl.mesh = bpy.data.meshes.new(mdl.name)
    mdl.mesh.from_pydata(verts, [], faces)
    mdl.obj = bpy.data.objects.new(mdl.name, mdl.mesh)
    bpy.context.scene.objects.link(mdl.obj)
    bpy.context.scene.objects.active = mdl.obj
    mdl.obj.select = True
    setup_skins (mdl, uvs)
    if mdl.images and not hasattr(mdl.images[0], "pack"):
        operator.report({'WARNING'},
            "Unable to pack skins. They must be packed by hand."
            +" Some may have been lost")
    if len(mdl.frames) > 1 or mdl.frames[0].type:
        build_shape_keys(mdl)
        merge_frames(mdl)
        build_actions(mdl)

    mdl.mesh.update()

    bpy.context.user_preferences.edit.use_global_undo = True
    return {'FINISHED'}
