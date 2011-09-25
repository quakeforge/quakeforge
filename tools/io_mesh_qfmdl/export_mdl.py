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
        cache = {}
        pixels = image.pixels[:]
        for y in range(mdl.skinheight):
            for x in range(mdl.skinwidth):
                outind = y * mdl.skinwidth + x
                # quake textures are top to bottom, but blender images
                # are bottom to top
                inind = ((mdl.skinheight - 1 - y) * mdl.skinwidth + x) * 4
                rgb = pixels[inind : inind + 3] # ignore alpha
                rgb = tuple(map(lambda x: int(x * 255 + 0.5), rgb))
                if rgb not in cache:
                    best = (3*256*256, -1)
                    for i, p in enumerate(palette):
                        if i > 255:     # should never happen
                            break
                        r = 0
                        for x in map (lambda a, b: (a - b) ** 2, rgb, p):
                            r += x
                        if r < best[0]:
                            best = (r, i)
                    cache[rgb] = best[1]
                skin.pixels[outind] = cache[rgb]
    mdl.skins.append(skin)

def build_tris(mesh):
    # mdl files have a 1:1 relationship between stverts and 3d verts.
    # a bit sucky, but it does allow faces to take less memory
    #
    # modelgen's algorithm for generating UVs is very efficient in that no
    # vertices are duplicated (thanks to the onseam flag), but it can result
    # in fairly nasty UV layouts, and worse: the artist has no control over
    # the layout. However, there seems to be nothing in the mdl format
    # preventing the use of duplicate 3d vertices to allow complete freedom
    # of the UV layout.
    uvfaces = mesh.uv_textures[0].data
    stverts = []
    tris = []
    vertmap = []    # map mdl vert num to blender vert num (for 3d verts)
    uvdict = {}
    for face, uvface in map(lambda a,b: (a,b), mesh.faces, uvfaces):
        fv = list(face.vertices)
        uv = list(uvface.uv)
        # blender's and quake's vertex order seem to be opposed
        fv.reverse()
        uv.reverse()
        tv = []
        for v, u in map(lambda a,b: (a,b), fv, uv):
            k = tuple(u)
            if k not in uvdict:
                uvdict[k] = len(stverts)
                vertmap.append(v)
                stverts.append(u)
            tv.append(uvdict[k])
        tris.append(MDL.Tri(tv))
    return tris, stverts, vertmap

def convert_stverts(mdl, stverts):
    for i, st in enumerate (stverts):
        s, t = st
        # quake textures are top to bottom, but blender images
        # are bottom to top
        s = int (s * mdl.skinwidth + 0.5)
        t = int ((1 - t) * mdl.skinheight + 0.5)
        # ensure st is within the skin
        s = ((s % mdl.skinwidth) + mdl.skinwidth) % mdl.skinwidth
        t = ((t % mdl.skinheight) + mdl.skinheight) % mdl.skinheight
        stverts[i] = MDL.STVert ((s, t))

def make_frame(mesh, vertmap):
    frame = MDL.Frame()
    for v in vertmap:
        vert = MDL.Vert(tuple(mesh.vertices[v].co))
        frame.add_vert(vert)
    return frame

def scale_verts(mdl):
    tf = MDL.Frame()
    for f in mdl.frames:
        tf.add_frame(f, 0.0)    # let the frame class do the dirty work for us
    size = Vector(tf.maxs) - Vector(tf.mins)
    rsqr = tuple(map(lambda a, b: max(abs(a), abs(b)) ** 2, tf.mins, tf.maxs))
    mdl.boundingradius = (rsqr[0] + rsqr[1] + rsqr[2]) ** 0.5
    mdl.scale_origin = tf.mins
    mdl.scale = tuple(map(lambda x: x / 255.0, size))
    for f in mdl.frames:
        f.scale(mdl)

def calc_average_area(mdl):
    frame = mdl.frames[0]
    if frame.type:
        frame = frame.frames[0]
    totalarea = 0.0
    for tri in mdl.tris:
        verts = tuple(map(lambda i: frame.verts[i], tri.verts))
        a = Vector(verts[0].r) - Vector(verts[1].r)
        b = Vector(verts[2].r) - Vector(verts[1].r)
        c = a.cross(b)
        totalarea += (c * c) ** 0.5 / 2.0
    return totalarea / len(mdl.tris)

def export_mdl(operator, context, filepath):
    obj = context.active_object
    mesh = obj.to_mesh (context.scene, True, 'PREVIEW') #wysiwyg?
    if not check_faces (mesh):
        operator.report({'ERROR'},
                        "Mesh has faces with more than 3 vertices.")
        return {'CANCELLED'}
    mdl = MDL(obj.name)
    make_skin(mdl, mesh)
    mdl.tris, mdl.stverts, vertmap = build_tris(mesh)
    convert_stverts (mdl, mdl.stverts)
    mdl.frames.append(make_frame(mesh, vertmap))
    mdl.size = calc_average_area(mdl)
    scale_verts(mdl)
    mdl.write(filepath)
    return {'FINISHED'}
