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

import bpy
from bpy_extras.object_utils import object_data_add
from mathutils import Vector,Matrix

from .map import parse_map, MapError

def process_entity(ent):
    if "classname" in ent.d and ent.d["classname"][:5] == "light":
        pass
    elif ent.b:
        name = "brush"
        verts = []
        faces = []
        for bverts, bfaces in ent.b:
            base = len(verts)
            verts.extend(bverts)
            for f in bfaces:
                for i in range(len(f)):
                    f[i] += base
                f.reverse()
                if not f[-1]:
                    t = f[0]
                    del f[0]
                    f.append(t)
            faces.extend(bfaces)
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(verts, [], faces)
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.objects.link(obj)
        bpy.context.scene.objects.active=obj
        obj.select = True
    else:
        pass

def import_map(operator, context, filepath):
    bpy.context.user_preferences.edit.use_global_undo = False

    for obj in bpy.context.scene.objects:
        obj.select = False

    try:
        entities = parse_map (filepath)
    except MapError as err:
        raise
        operator.report({'ERROR'}, repr(err))
        return {'CANCELLED'}
    for ent in entities:
        process_entity(ent)
    bpy.context.user_preferences.edit.use_global_undo = True
    return {'FINISHED'}
