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

import os

import bpy
from bpy_extras.object_utils import object_data_add
from mathutils import Vector,Matrix
from math import pi

from .map import parse_map, MapError
from .quakepal import palette
from .wad import WadFile
from .entity import entity_box, set_entity_props

def parse_vector(vstr):
    v = vstr.split()
    for i in range(len(v)):
        v[i] = float(v[i])
    return Vector(v)

def load_image(tx):
    if tx.name in bpy.data.images:
        return bpy.data.images[tx.name]
    img = bpy.data.images.new(tx.name, tx.miptex.width, tx.miptex.height)
    p = [0.0] * tx.miptex.width * tx.miptex.height * 4
    d = tx.miptex.texels[0]
    for j in range(tx.miptex.height):
        for k in range(tx.miptex.width):
            c = palette[d[j * tx.miptex.width + k]]
            # quake textures are top to bottom, but blender images
            # are bottom to top
            l = ((tx.miptex.height - 1 - j) * tx.miptex.width + k) * 4
            p[l + 0] = c[0] / 255.0
            p[l + 1] = c[1] / 255.0
            p[l + 2] = c[2] / 255.0
            p[l + 3] = 1.0
    img.pixels[:] = p[:]
    img.pack()
    return img

def load_material(tx):
    if tx.name in bpy.data.materials:
        return bpy.data.materials[tx.name]
    mat = bpy.data.materials.new(tx.name)
    mat.blend_method = 'OPAQUE'
    mat.diffuse_color = (1, 1, 1, 1)
    mat.metallic = 1
    mat.roughness = 1
    mat.specular_intensity = 0
    mat.use_nodes = True
    if tx.image:
        emissionNode = mat.node_tree.nodes.new("ShaderNodeEmission")
        shaderOut = mat.node_tree.nodes["Material Output"]
        mat.node_tree.nodes.remove(mat.node_tree.nodes["Principled BSDF"])

        tex_node = mat.node_tree.nodes.new("ShaderNodeTexImage")
        tex_node.image = tx.image
        tex_node.interpolation = "Closest"

        emissionNode.location = (0, 0)
        shaderOut.location = (200, 0)
        tex_node.location = (-300, 0)

        mat.node_tree.links.new(tex_node.outputs[0], emissionNode.inputs[0])
        mat.node_tree.links.new(emissionNode.outputs[0], shaderOut.inputs[0])

        #FIXME uvs etc
        #tex = bpy.data.textures.new(tx.name, 'IMAGE')
        #tex.extension = 'REPEAT'
        #tex.use_preview_alpha = True
        #tex.image = tx.image
        #ts = mat.texture_paint_slots.new()
        #ts = mat.texture_paint_slots[0]
        #ts.texture = tex
        #ts.use_map_alpha = True
        #ts.texture_coords = 'UV'
    return mat

def load_textures(texdefs, wads):
    class MT:
        def __init__(self, x, y):
            self.width = x
            self.height = y
    for tx in texdefs:
        if hasattr(tx, "miptex"):
            continue
        if not wads or not wads[0]:
            tx.miptex = MT(64,64)
            tx.image = None
        else:
            try:
                tx.miptex = wads[0].getData(tx.name)
                tx.image = load_image(tx)
            except KeyError:
                tx.miptex = MT(64,64)
                tx.image = None
        tx.material = load_material(tx)

def build_uvs(verts, faces, texdefs):
    uvs = [None] * len(faces)
    for i, f in enumerate(faces):
        tx = texdefs[i]
        fuv = []
        for vi in f:
            v = Vector(verts[vi])
            s = (v.dot(tx.vecs[0][0]) + tx.vecs[0][1]) / tx.miptex.width
            t = (v.dot(tx.vecs[1][0]) + tx.vecs[1][1]) / tx.miptex.height
            fuv.append((s, 1 - t))
        uvs[i] = fuv
    return uvs

def process_entity(ent, wads):
    qfmap = bpy.context.scene.qfmap
    classname = ent.d["classname"]
    name = classname
    if "classname" in ent.d and ent.d["classname"][:5] == "light":
        light = bpy.data.lights.new("light", 'POINT')
        light.use_custom_distance = True
        if "light" in ent.d:
            light.cutoff_distance = float(ent.d["light"])
        elif "_light" in ent.d:
            light.cutoff_distance = float(ent.d["_light"])
        else:
            light.cutoff_distance = 300.0
        #light.falloff_type = 'CUSTOM_CURVE'
        obj = bpy.data.objects.new(name, light)
    elif ent.b:
        verts = []
        faces = []
        texdefs = []
        for bverts, bfaces, btexdefs in ent.b:
            base = len(verts)
            verts.extend(bverts)
            texdefs.extend(btexdefs)
            for f in bfaces:
                for i in range(len(f)):
                    f[i] += base
                f.reverse()
                if not f[-1]:
                    t = f[0]
                    del f[0]
                    f.append(t)
            faces.extend(bfaces)
        load_textures(texdefs, wads)
        uvs = build_uvs(verts, faces, texdefs)
        mesh = bpy.data.meshes.new(name)
        for tx in texdefs:
            if hasattr(tx, "matindex"):
                continue
            for i, mat in enumerate(mesh.materials):
                if mat.name == tx.material.name:
                    tx.matindex = i
            if hasattr(tx, "matindex"):
                continue
            tx.matindex = len(mesh.materials)
            mesh.materials.append(tx.material)
        mesh.from_pydata(verts, [], faces)
        """uvlay = mesh.uv_textures.new(name)
        uvloop = mesh.uv_layers[0]
        for i, texpoly in enumerate(uvlay.data):
            poly = mesh.polygons[i]
            uv = uvs[i]
            tx = texdefs[i]
            texpoly.image = tx.image
            poly.material_index = tx.matindex
            for j, k in enumerate(poly.loop_indices):
                uvloop.data[k].uv = uv[j]"""#FIXME
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
    else:
        try:
            entityclass = qfmap.entity_classes[classname]
        except KeyError:
            entityclass = None
        if entityclass and entityclass.size:
            mesh = entity_box(entityclass)
            obj = bpy.data.objects.new(name, mesh)
        else:
            obj = bpy.data.objects.new(name, None)
            obj.empty_display_type = 'CUBE'
            obj.empty_display_size = 8
        obj.show_name = True
    if "origin" in ent.d:
        obj.location = parse_vector (ent.d["origin"])
    angles = Vector()
    if not ent.b:
        #brush entities doen't normally support rotation
        if "angle" in ent.d:
            angles = Vector((0, 0, float(ent.d["angle"])))
            del ent.d["angle"]
        elif "angles" in ent.d:
            angles = parse_vector(ent.d["angles"])
            angles = angles.xzy
            del ent.d["angles"]
    obj.rotation_mode = 'XZY'
    obj.rotation_euler = angles * pi / 180
    bpy.context.layer_collection.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    set_entity_props(obj, ent)

def import_map(operator, context, filepath):
    undo = bpy.context.preferences.edit.use_global_undo
    bpy.context.preferences.edit.use_global_undo = False

    try:
        for obj in bpy.context.scene.objects:
            obj.select_set(False)
        entities = parse_map (filepath)
    except MapError as err:
        operator.report({'ERROR'}, repr(err))
        return {'CANCELLED'}
    else:
        wads=[]
        if entities:
            if "_wad" in entities[0].d:
                wads = entities[0].d["_wad"].split(";")
            elif "wad" in entities[0].d:
                wads = entities[0].d["wad"].split(";")
            wadpath = bpy.context.scene.qfmap.wadpath
            for i in range(len(wads)):
                try:
                    wads[i] = WadFile.load(os.path.join(wadpath, wads[i]))
                except IOError:
                    try:
                        wads[i] = WadFile.load(os.path.join(wadpath,
                                               os.path.basename(wads[i])))
                    except IOError:
                        #give up
                        operator.report({'INFO'}, "Cant't find %s" % wads[i])
                        wads[i] = None
        for ent in entities:
            process_entity(ent, wads)
    finally:
        bpy.context.preferences.edit.use_global_undo = undo
    return {'FINISHED'}

def import_pts(operator, context, filepath):
    bpy.context.user_preferences.edit.use_global_undo = False

    for obj in bpy.context.scene.objects:
        obj.select_set(False)

    lines = open(filepath, "rt").readlines()
    verts = [None] * len(lines)
    edges = [None] * (len(lines) - 1)
    for i, line in enumerate(lines):
        if i:
            edges[i - 1] = (i - 1, i)
        v = line.split(" ")
        verts[i] = tuple(map(lambda x: float(x), v))
    mesh = bpy.data.meshes.new("leak points")
    mesh.from_pydata(verts, edges, [])
    mesh.update()
    obj = bpy.data.objects.new("leak points", mesh)
    bpy.context.scene.objects.link(obj)
    bpy.context.scene.objects.active=obj
    obj.select_set(True)
    bpy.context.user_preferences.edit.use_global_undo = True
    return {'FINISHED'}
