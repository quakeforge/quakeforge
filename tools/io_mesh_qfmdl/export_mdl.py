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

def check_faces(mesh):
    #Check that all faces are tris because mdl does not support anything else.
    #Because the diagonal on which a quad is split can make a big difference,
    #quad to tri conversion will not be done automatically.
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
        return False
    #reset selection to what it was before the check.
    for f, s in map(lambda x, y: (x, y), mesh.faces, save_select):
        f.select = s
    mesh.update()
    return True

def make_skin(mdl, mesh):
    if (not mesh.uv_textures or not mesh.uv_textures[0].data
        or not mesh.uv_textures[0].data[0].image):
        mdl.skinwidth = mdl.skinheight = 4
        skin = MDL.Skin()
        skin.type = 0
        skin.pixels = bytes(mdl.skinwidth * mdl.skinheight) # black skin
    else:
        image = mesh.uv_textures[0].data[0].image
        mdl.skinwidth, mdl.skinheight = image.size
        skin = MDL.Skin()
        skin.type = 0
        skin.pixels = bytearray(mdl.skinwidth * mdl.skinheight) # preallocate
        for y in range(mdl.skinheight):
            for x in range(mdl.skinwidth):
                outind = y * mdl.skinwidth + x
                # quake textures are top to bottom, but blender images
                # are bottom to top
                inind = ((mdl.skinheight - 1 - y) * mdl.skinwidth + x) * 4
                rgb = image.pixels[inind : inind + 3] # ignore alpha
                rgb = tuple(map(lambda x: int(x * 255 + 0.5), rgb))
                best = (3*256*256, -1)
                for i, p in enumerate(palette):
                    if i > 255:     # should never happen
                        break
                    r = 0
                    for x in map (lambda a, b: (a - b) ** 2, rgb, p):
                        r += x
                    if r < best[0]:
                        best = (r, i)
                skin.pixels[outind] = best[1]
    mdl.skins.append(skin)

def export_mdl(operator, context, filepath):
    obj = context.active_object
    mesh = obj.data
    if not check_faces (mesh):
        operator.report({'ERROR'},
                        "Mesh has faces with more than 3 vertices.")
        return {'CANCELLED'}
    mdl = MDL(obj.name)
    make_skin(mdl, mesh)
    mdl.write(filepath)
    return {'FINISHED'}
