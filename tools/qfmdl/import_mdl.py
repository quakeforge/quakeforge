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
    pass
class skin:
    pass
class stvert:
    pass
class tri:
    pass
class frame:
    pass
class vert:
    pass

def load_mdl(filepath):
    data = open(filepath, "rb").read()
    m = unpack("<4s i 3f 3f f 3f i i i i i i i i f", data[:84])
    data = data[84:]
    mdl = MDL()
    mdl.name = filepath.split('/')[-1]
    mdl.name = mdl.name.split('.')[0]
    mdl.ident = m[0]
    mdl.version = m[1]
    mdl.scale = Vector(m[2:5])
    mdl.scale_origin = Vector(m[5:8])
    mdl.boundingradius = m[8]
    mdl.eyeposition = Vector(m[9:12])
    mdl.numskins = m[12]
    mdl.skinwidth = m[13]
    mdl.skinheight = m[14]
    mdl.numverts = m[15]
    mdl.numtris = m[16]
    mdl.numframes = m[17]
    mdl.synctype = m[18]
    mdl.flags = m[19]
    mdl.size = m[20]
    # read in the skin data
    size = mdl.skinwidth * mdl.skinheight
    mdl.skins = []
    for i in range(mdl.numskins):
        s = skin()
        mdl.skins.append(s)
        s.type = unpack ("<i", data[:4])[0]
        data = data[4:]
        if s.type == 0:
            # single skin
            s.pixels = data[:size]
            data = data[size:]
        else:
            # skin group
            s.numskins = unpack ("<i", data[:4])[0]
            data = data[4:]
            s.times = unpack("<" + repr(n) + "f", data[:n * 4])
            data = data[n * 4:]
            s.skins = []
            for j in range(n):
                ss = skin()
                ss.type = 0
                ss.pixels = data[:size]
                data = data[size:]
                s.skins.append(ss)
    #read in the st verts (uv map)
    mdl.stverts = []
    for i in range(mdl.numverts):
        st = stvert ()
        st.onseam, st.s, st.t = unpack ("<i i i", data[:12])
        data = data[12:]
        mdl.stverts.append(st)
    #read in the tris
    mdl.tris = []
    for i in range(mdl.numtris):
        t = unpack("<i 3i", data[:16])
        data = data[16:]
        mdl.tris.append(tri())
        mdl.tris[-1].facesfront = t[0]
        mdl.tris[-1].verts = t[1:]
    #read in the frames
    mdl.frames = []
    for i in range(mdl.numframes):
        f = frame()
        f.type = unpack("<i", data[:4])[0]
        data = data[4:]
        if f.type == 0:
            x = unpack("<3B B 3B B 16s", data[:24])
            data = data[24:]
            f.mins = x[0:3]
            f.maxs = x[4:7]
            name = x[8]
            if b"\0" in name:
                name = name[:name.index(b"\0")]
            f.name = name
            f.verts = []
            for j in range(mdl.numverts):
                x = unpack("<3B B", data[:4])
                data = data[4:]
                v = vert()
                v.r = x[:3]
                v.ni = x[3]
                f.verts.append(v)
        else:
            g = f
            x = unpack("<i 3B B 3B B", data[:12])
            data = data[12:]
            g.numframes = n = x[0]
            g.mins = x[1:4]
            g.maxs = x[5:8]
            g.times = unpack("<" + repr(n) + "f", data[:n * 4])
            data = data[n * 4:]
            g.frames = []
            for k in range(g.numframes):
                f = frame()
                x = unpack("<3B B 3B B 16s", data[:24])
                data = data[24:]
                f.mins = x[0:3]
                f.maxs = x[4:7]
                f.name = x[8]
                f.verts = []
                for j in range(mdl.numverts):
                    x = unpack("<3B B", data[:4])
                    data = data[4:]
                    v = vert()
                    v.r = x[:3]
                    v.ni = x[3]
                    f.verts.append(v)
                g.frames.append(f)
            f = g
        mdl.frames.append(f)
    return mdl

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
        verts.append(Vector(v.r) * m)
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

    mdl.images=[]
    for i in range(mdl.numskins):
        if mdl.skins[i].type:
            for j in range(mdl.skins[i].numskins):
                load_skin (mdl.skins[i].skins[j],
                           "%s_%d_%d" % (mdl.name, i, j))
        else:
            load_skin (mdl.skins[i], "%s_%d" % (mdl.name, i))

def import_mdl(operator, context, filepath):
    mdl = load_mdl(filepath)

    bpy.context.user_preferences.edit.use_global_undo = False

    faces, uvs = make_faces (mdl)
    verts = make_verts (mdl, 0)
    load_skins (mdl)
    mesh = bpy.data.meshes.new(mdl.name)
    mesh.from_pydata(verts, [], faces)
    uvlay = mesh.uv_textures.new(mdl.name)
    for i, f in enumerate(uvlay.data):
        mdl_uv = uvs[i]
        for j, uv in enumerate(f.uv):
            uv[0], uv[1] = mdl_uv[j]

    mat = bpy.data.materials.new(mdl.name)
    mat.diffuse_color = (1,1,1)
    mat.use_raytrace = False
    tex = bpy.data.textures.new(mdl.name, 'IMAGE')
    tex.extension = 'CLIP'
    tex.use_preview_alpha = True
    tex.image = mdl.images[0]   # use the first skin for now
    mat.texture_slots.add()
    ts = mat.texture_slots[0]
    ts.texture = tex
    ts.use_map_alpha = True
    ts.texture_coords = 'UV'
    mesh.update()
    object_data_add(context, mesh, operator=None)
    act = bpy.context.active_object
    if not act.material_slots:
        bpy.ops.object.material_slot_add()
    act.material_slots[0].material = mat

    bpy.context.user_preferences.edit.use_global_undo = True
    return 'FINISHED'

