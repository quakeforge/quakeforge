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

from .quakepal import palette
from .mdl import MDL

def export_mdl(operator, context, filepath):
    obj = context.active_object
    mesh = obj.data
    faces_ok = True
    save_select = []
    for f in mesh.faces:
        save_select.append(f.select)
        f.select = False
        if len(f.vertices) > 3:
            f.select = True
            faces_ok = False
    if not faces_ok:
        mesh.update()
        operator.report({'ERROR'},
                        "Mesh has faces with more than 3 vertices.")
        return {'CANCELLED'}
    #reset selection to what it was before the check.
    for f, s in map(lambda x,y: (x, y), mesh.faces, save_select):
        f.select = s
    return {'FINISHED'}
