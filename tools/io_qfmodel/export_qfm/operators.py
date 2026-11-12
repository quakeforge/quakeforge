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
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, EnumProperty
from mathutils import Vector

from . import export_qfm
#from . import volume

def strip_nnn(name):
    # ∧ separates the export name from the modeling name
    ind = name.rfind("∧")
    if ind >= 0:
        return name[:ind]
    # legacy check for blender's duplicate name separator
    # causes problems for naming schemes that includ the part size
    # but forcing people to add ∧ after all their objects isn't a
    # great idea, so it's useful anyway.
    ind = name.rfind(".")
    if ind < 0 or len(name) - ind != 4:
        return name
    if not name[ind+1:].isdigit():
        return name
    return name[:ind]

def do_export_qfm(operator, context, filepath):
    #collections = export_qfm.enable_collections()
    try:
        qfm = export_qfm.export_qfm (context, filepath)
    finally:
        #export_qfm.restore_collections(collections)
        pass
    #for m in qfm.messages:
    #    operator.report(m[0], m[1])
    return {'FINISHED'}

exportable_objects = {
    type(None),
    bpy.types.Mesh,
    bpy.types.Armature,
}

class QFM_OT_ExportQfm(bpy.types.Operator, ExportHelper):
    '''Save a Quakeforge QFM (.qfm) File'''
    bl_idname = "export_object.qf_qfm"
    bl_label = "Export QFM"

    filename_ext = ".qfm"
    filter_glob: StringProperty(default="*.qfm", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj != None and type(obj.data) in exportable_objects

    def execute(self, context):
        keywords = self.as_keywords (ignore=("check_existing", "filter_glob",
                                             "axis_forward", "axis_up"))
        return do_export_qfm(self, context, **keywords)

class QFM_OT_ExportQfm_quick(bpy.types.Operator, ExportHelper):
    '''Save a Quakeforge QFM (.qfm) File, defaulting name to selected object'''
    bl_idname = "export_object.qf_qfm_quick"
    bl_label = "Export QFM (quick)"

    filename_ext = ".qfm"
    filter_glob: StringProperty(default="*.qfm", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj != None and type(obj.data) in exportable_objects

    def execute(self, context):
        keywords = self.as_keywords (ignore=("check_existing", "filter_glob",
                                             "axis_forward", "axis_up"))
        return do_export_qfm(self, context, **keywords)

    def invoke(self, context, event):
        obj = context.active_object
        if obj != None:
            self.filepath = strip_nnn(obj.name) + self.filename_ext
        return ExportHelper.invoke(self, context, event)
