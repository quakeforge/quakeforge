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
import sys

import bpy
import bmesh
from mathutils import Vector,Matrix
from math import pi

def active_uv(mesh):
    for uvt in mesh.uv_textures:
        if uvt.active:
            return uvt
    return None

def make_face(bmface, mesh):
    uvtex = active_uv(mesh)
    uvfaces = mesh.uv_layers[uvtex.name].data
    face = mesh.polygons[bmface.index]
    mat = mesh.materials[face.material_index]
    uvs = uvfaces[face.loop_start:face.loop_start + face.loop_total]
    uvs = list(map(lambda a: a.uv, uvs))
    v = list(face.vertices[-3:])
    v.reverse()
    return (mesh.vertices[v[0]].co, mesh.vertices[v[1]].co,
            mesh.vertices[v[2]].co, mat.name, 0, 0, 0, 1, 1)

def make_brushes(obj):
    act = bpy.context.scene.objects.active
    bpy.context.scene.objects.active = obj
    bpy.ops.object.editmode_toggle()
    brushmesh = bmesh.from_edit_mesh(obj.data).copy()
    bpy.ops.object.editmode_toggle()
    bpy.context.scene.objects.active = act
    brushes = []
    face_set = set(brushmesh.faces)
    while face_set:
        face_queue = [face_set.pop()]
        brush = []
        while face_queue:
            face = face_queue.pop()
            brush.append(face)
            for edge in face.edges:
                for link_face in edge.link_faces:
                    if link_face in face_set:
                        face_set.remove(link_face)
                        face_queue.append(link_face)
        brushes.append(brush)
    mesh = obj.to_mesh(bpy.context.scene, True, 'PREVIEW')
    mesh.transform(obj.matrix_world)
    for b in brushes:
        for i in range(len(b)):
            b[i] = make_face(b[i], mesh)
    return brushes

def export_mesh(obj):
    qfe = obj.qfentity
    edict = entity_dict(qfe)
    if qfe.classname:
        edict["classname"] = qfe.classname
    ec = bpy.context.scene.qfmap.entity_classes.get(edict["classname"])
    if not ec or ec.size:
        edict["origin"] = "{l.x:.0f} {l.y:.0f} {l.z:.0f}".format(l=obj.location)
        return edict, []
    return edict, make_brushes(obj)

def export_lamp(obj):
    qfe = obj.qfentity
    edict = entity_dict(qfe)
    if qfe.classname:
        edict["classname"] = qfe.classname
    if not edict.get("classname"):
        edict["classname"] = "light"
    edict["origin"] = "{l.x:.0f} {l.y:.0f} {l.z:.0f}".format(l=obj.location)
    light = obj.data
    edict["light"] = light.distance
    return edict

def vector_str(vect):
    return "{v.x:.0f} {v.y:.0f} {v.z:.0f}".format(v=vect)

def export_empty(obj):
    qfe = obj.qfentity
    edict = entity_dict(qfe)
    if qfe.classname:
        edict["classname"] = qfe.classname
    edict["origin"] = vector_str(obj.location)
    return edict

def write_brush(outfile, brush):
    outfile.write("{\n")
    for f in brush:
        outfile.write("( {} ) ( {} ) ( {} ) {} {} {} {} {} {}\n".format(
            vector_str(f[0]), vector_str(f[1]), vector_str(f[2]),
            f[3], f[4], f[5], f[6], f[7], f[8]))
    outfile.write("}\n")

def write_entity(outfile, edict, brushes):
    outfile.write("{\n")
    for item in edict.items():
        outfile.write("\"{item[0]}\" \"{item[1]}\"\n".format(item=item))
    for brush in brushes:
        write_brush(outfile, brush)
    outfile.write("}\n")

def entity_dict(ent):
    edict = {}
    for field in ent.fields:
        edict[field.name] = field.value
    return edict

def export_map(operator, context, filepath):
    bpy.context.user_preferences.edit.use_global_undo = False

    world_objects = []
    entities = []
    for obj in bpy.context.scene.objects:
        qfe = obj.qfentity
        edict = entity_dict(qfe)
        if (obj.type == 'MESH'
            and (qfe.classname == 'worldspawn'
                 or (not qfe.classname and "classname" not in edict))):
            world_objects.append(obj)
        elif (obj.type in {'MESH', 'LAMP', 'EMPTY'}
              and (qfe.classname or ("classname" in edict))):
            entities.append(obj)
    world_brushes = []
    world_dict = {"classname":"worldspawn"}
    for obj in world_objects:
        world_dict.update(entity_dict(obj.qfentity))
        world_brushes.extend(make_brushes(obj))
    outfile = open(filepath, "wt")
    write_entity(outfile, world_dict, world_brushes)
    for obj in entities:
        if obj.type == 'MESH':
            edict, brushes = export_mesh(obj)
        elif obj.type == 'LAMP':
            brushes = []
            edict = export_lamp(obj)
        elif obj.type == 'EMPTY':
            brushes = []
            edict = export_empty(obj)
        write_entity(outfile, edict, brushes)
    outfile.close()
    bpy.context.user_preferences.edit.use_global_undo = True
    return {'FINISHED'}
