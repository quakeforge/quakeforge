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

class MDL:
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
        skintype = unpack ("<i", data[:4])[0]
        data = data[4:]
        if skintype == 0:
            # single skin
            mdl.skins.append((0, data[:size]))
            data = data[size:]
        else:
            # skin group
            n = unpack ("<i", data[:4])[0]
            data = data[4:]
            k = (n, unpack("<" + repr(n) + "f", data[:n * 4]), [])
            data = data[n * 4:]
            for j in range(n):
                k[2].append(data[:size])
                data = data[size:]
            mdl.skins.append(k)
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

def make_mesh(mdl, framenum, subframenum=0):
    frame = mdl.frames[framenum]
    if frame.type:
        frame = frame.frames[subframenum]
    verts = []
    faces = []
    s = mdl.scale
    o = mdl.scale_origin
    m = Matrix(((s.x,  0,  0,  0),
                (  0,s.y,  0,  0),
                (  0,  0,s.z,  0),
                (o.x,o.y,o.z,  1)))
    for v in frame.verts:
        verts.append(Vector(v.r) * m)
    for t in mdl.tris:
        faces.append (t.verts)
    return verts, faces

def import_mdl(operator, context, filepath):
    mdl = load_mdl(filepath)
    verts, faces = make_mesh (mdl, 0)
    name = filepath.split('/')[-1]
    name = name.split('.')[0]
    bpy.context.user_preferences.edit.use_global_undo = False
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    object_data_add(context, mesh, operator=None)
    bpy.context.user_preferences.edit.use_global_undo = True
    return 'FINISHED'

