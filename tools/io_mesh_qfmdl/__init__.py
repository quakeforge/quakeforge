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

# copied from io_scene_obj

# <pep8 compliant>

bl_info = {
    "name": "Quake MDL format",
    "author": "Bill Currie",
    "blender": (2, 6, 3),
    "api": 35622,
    "location": "File > Import-Export",
    "description": "Import-Export Quake MDL (version 6) files. (.mdl)",
    "warning": "not even alpha",
    "wiki_url": "",
    "tracker_url": "",
#    "support": 'OFFICIAL',
    "category": "Import-Export"}

# To support reload properly, try to access a package var, if it's there,
# reload everything
if "bpy" in locals():
    import imp
    if "import_mdl" in locals():
        imp.reload(import_mdl)
    #if "export_mdl" in locals():
    #    imp.reload(export_mdl)


import bpy
from bpy.props import BoolProperty, FloatProperty, StringProperty, EnumProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper, path_reference_mode, axis_conversion


class ImportMDL6(bpy.types.Operator, ImportHelper):
    '''Load a Quake MDL (v6) File'''
    bl_idname = "import_mesh.quake_mdl_v6"
    bl_label = "Import MDL"

    filename_ext = ".mdl"
    filter_glob = StringProperty(default="*.mdl", options={'HIDDEN'})

    def execute(self, context):
        from . import import_mdl
        keywords = self.as_keywords (ignore=("filter_glob",))
        return import_mdl.import_mdl(self, context, **keywords)

class ExportMDL6(bpy.types.Operator, ExportHelper):
    '''Save a Quake MDL (v6) File'''

    bl_idname = "export_mesh.quake_mdl_v6"
    bl_label = "Export MDL"

    filename_ext = ".mdl"
    filter_glob = StringProperty(default="*.mdl", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        return (context.active_object != None
                and type(context.active_object.data) == bpy.types.Mesh)

    def execute(self, context):
        from . import export_mdl
        keywords = self.as_keywords (ignore=("check_existing", "filter_glob"))
        return export_mdl.export_mdl(self, context, **keywords)


def menu_func_import(self, context):
    self.layout.operator(ImportMDL6.bl_idname, text="Quake MDL (.mdl)")


def menu_func_export(self, context):
    self.layout.operator(ExportMDL6.bl_idname, text="Quake MDL (.mdl)")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()
