# vim:ts=4:et
#
#  Copyright (C) 2012 Bill Currie <bill@taniwha.org>
#  Date: 2012/2/20
#
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

bl_info = {
    "name": "Strut Generator",
    "author": "Bill Currie",
    "blender": (2, 6, 2),
    "api": 35622,
    "location": "View3D > Add > Mesh > Struts",
    "description": "Add struts meshes based on selected truss meshes",
    "warning": "can get very high-poly",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Add Mesh"}

import bpy
from bpy.props import FloatProperty,IntProperty,BoolProperty
from mathutils import Vector,Matrix
from math import pi, cos, sin

cossin = []

def build_cossin(n):
    global cossin
    cossin = []
    for i in range(n):
        a = 2 * pi * i / n
        cossin.append((cos(a), sin(a)))


def make_strut(v1, v2, id, od, n, solid, loops):
    v1 = Vector(v1)
    v2 = Vector(v2)
    axis = v2 - v1
    pos = [(0, od / 2)]
    if loops:
        pos += [((od - id) / 2, od / 2),
                (axis.length - (od - id) / 2, od / 2)]
    pos += [(axis.length, od / 2)]
    if solid:
        pos += [(axis.length, id / 2)]
        if loops:
            pos += [(axis.length - (od - id) / 2, id / 2),
                    ((od - id) / 2, id / 2)]
        pos += [(0, id / 2)]
    vps = len(pos)
    fps = vps
    if not solid:
        fps -= 1
    fw = axis.copy()
    fw.normalize()
    if (abs(axis[0] / axis.length) < 1e-5
        and abs(axis[1] / axis.length) < 1e-5):
        up = Vector((-1, 0, 0))
    else:
        up = Vector((0, 0, 1))
    lf = up.cross(fw)
    lf.normalize()
    up = fw.cross(lf)
    mat = Matrix((fw, lf, up))
    mat.transpose()
    verts = [None] * n * vps
    faces = [None] * n * fps
    for i in range(n):
        base = (i - 1) * vps
        x = cossin[i][0]
        y = cossin[i][1]
        for j in range(vps):
            p = Vector((pos[j][0], pos[j][1] * x, pos[j][1] * y))
            p = mat * p
            verts[i * vps + j] = p + v1
        if i:
            for j in range(fps):
                f = (i - 1) * fps + j
                faces[f] = [base + j, base + vps + j,
                            base + vps + (j + 1) % vps, base + (j + 1) % vps]
    base = len(verts) - vps
    i = n
    for j in range(fps):
        f = (i - 1) * fps + j
        faces[f] = [base + j, j, (j + 1) % vps, base + (j + 1) % vps]
    #print(verts,faces)
    return verts, faces

def create_struts(self, context, id, od, segments, solid, loops):
    build_cossin(segments)
    vps = 2
    if solid:
        vps *= 2
    if loops:
        vps *= 2
    fps = vps
    if not solid:
        fps -= 1

    bpy.context.user_preferences.edit.use_global_undo = False
    for truss_obj in bpy.context.scene.objects:
        if not truss_obj.select:
            continue
        truss_obj.select = False
        truss_mesh = truss_obj.to_mesh(context.scene, True, 'PREVIEW')
        if not truss_mesh.edges:
            continue
        mesh = bpy.data.meshes.new("Struts")
    
        verts = [None] * len(truss_mesh.edges) * segments * vps
        faces = [None] * len(truss_mesh.edges) * segments * fps
        vbase = 0
        fbase = 0
        for e in truss_mesh.edges:
            v1 = truss_mesh.vertices[e.vertices[0]]
            v2 = truss_mesh.vertices[e.vertices[1]]
            v, f = make_strut(v1.co, v2.co, id, od, segments, solid, loops)
            for fv in f:
                for i in range(len(fv)):
                    fv[i] += vbase
            for i in range(len(v)):
                verts[vbase + i] = v[i]
            for i in range(len(f)):
                faces[fbase + i] = f[i]
            #if not base % 12800:
            #    print (base * 100 / len(verts))
            vbase += vps * segments
            fbase += fps * segments
        #print(verts,faces)
        mesh.from_pydata(verts, [], faces)
        obj = bpy.data.objects.new("Struts", mesh)
        bpy.context.scene.objects.link(obj)
        obj.select = True
        obj.location = truss_obj.location
        bpy.context.scene.objects.active = obj
        mesh.update()
        bpy.context.user_preferences.edit.use_global_undo = True
    return {'FINISHED'}

class Struts(bpy.types.Operator):
    """Add one or more struts meshes based on selected truss meshes"""
    bl_idname = "mesh.generate_struts"
    bl_label = "Struts"
    bl_description = """Add one or more struts meshes based on selected truss meshes"""
    bl_options = {'REGISTER', 'UNDO'}

    id = FloatProperty(name = "Inside Diameter",
                       description = "diameter of inner surface",
                       min = 0.0,
                       soft_min = 0.0,
                       max = 100,
                       soft_max = 100,
                       default = 0.04)
    od = FloatProperty(name = "Outside Diameter",
                       description = "diameter of outer surface",
                       min = 0.01,
                       soft_min = 0.01,
                       max = 100,
                       soft_max = 100,
                       default = 0.05)
    solid = BoolProperty(name="Solid",
                         description="Create inner surface.",
                         default=True)
    loops = BoolProperty(name="Loops",
                         description="Create sub-surf friendly loops.",
                         default=False)
    segments = IntProperty(name="Segments",
                           description="Number of segments around strut",
                           min=3, soft_min=3,
                           max=64, soft_max=64,
                           default=32)

    def execute(self, context):
        keywords = self.as_keywords ()
        return create_struts(self, context, **keywords)

def menu_func(self, context):
    self.layout.operator(Struts.bl_idname, text = "Struts", icon='PLUGIN')

def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_mesh_add.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
