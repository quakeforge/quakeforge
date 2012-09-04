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
    "name": "Quake map format",
    "author": "Bill Currie",
    "blender": (2, 6, 3),
    "api": 35622,
    "location": "File > Import-Export",
    "description": "Import-Export Quake maps",
    "warning": "not even alpha",
    "wiki_url": "",
    "tracker_url": "",
#    "support": 'OFFICIAL',
    "category": "Import-Export"}

# To support reload properly, try to access a package var, if it's there,
# reload everything
if "bpy" in locals():
    import imp
    if "import_map" in locals():
        imp.reload(import_map)
    if "export_map" in locals():
        imp.reload(export_map)


import bpy
from bpy.props import BoolProperty, FloatProperty, StringProperty, EnumProperty
from bpy.props import FloatVectorProperty, PointerProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper, path_reference_mode, axis_conversion
from bpy.app.handlers import persistent

from .entityclass import EntityClassDict
from . import import_map
#from . import export_map

@persistent
def scene_load_handler(dummy):
    for scene in bpy.data.scenes:
        if hasattr(scene, "qfmap"):
            qfmap = scene.qfmap
            if qfmap.script in bpy.data.texts:
                script = bpy.data.texts[qfmap.script].as_string()
                qfmap.entity_classes.from_plist(script)

def ec_dir_update(self, context):
    print("ec_dir_update")
    self.entity_classes.from_source_tree(self.dirpath)
    name = context.scene.name + '-EntityClasses'
    if name in bpy.data.texts:
        txt = bpy.data.texts[name]
    else:
        txt = bpy.data.texts.new(name)
    txt.from_string(self.entity_classes.to_plist())
    self.script = name

def ec_script_update(self, context):
    print("ec_script_update")
    if self.script in bpy.data.texts:
        self.entity_classes.from_plist(bpy.data.texts[self.script].as_string())

class QFEntityClasses(bpy.types.PropertyGroup):
    dirpath = StringProperty(
        name="dirpath",
        description="Path to qc source tree",
        subtype='DIR_PATH',
        update=ec_dir_update)
    script = StringProperty(
        name="script",
        description="entity class storage",
        update=ec_script_update)
    entity_classes = EntityClassDict()

class QFECPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'scene'
    bl_label = 'QF Entity Classes'

    @classmethod
    def poll(cls, context):
        return True

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        layout.prop(scene.qfmap, "dirpath")
        layout.prop(scene.qfmap, "script")

class ImportMap(bpy.types.Operator, ImportHelper):
    '''Load a Quake map File'''
    bl_idname = "import_mesh.quake_map"
    bl_label = "Import map"

    filename_ext = ".map"
    filter_glob = StringProperty(default="*.map", options={'HIDDEN'})

    def execute(self, context):
        keywords = self.as_keywords (ignore=("filter_glob",))
        return import_map.import_map(self, context, **keywords)

class ExportMap(bpy.types.Operator, ExportHelper):
    '''Save a Quake map File'''

    bl_idname = "export_mesh.quake_map"
    bl_label = "Export map"

    filename_ext = ".map"
    filter_glob = StringProperty(default="*.map", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        return (context.active_object != None
                and type(context.active_object.data) == bpy.types.Mesh)

    def execute(self, context):
        keywords = self.as_keywords (ignore=("check_existing", "filter_glob"))
        return export_map.export_map(self, context, **keywords)

def menu_func_import(self, context):
    self.layout.operator(ImportMap.bl_idname, text="Quake map (.map)")


def menu_func_export(self, context):
    self.layout.operator(ExportMap.bl_idname, text="Quake map (.map)")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.qfmap = PointerProperty(type=QFEntityClasses)

    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)

    bpy.app.handlers.load_post.append(scene_load_handler)


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()
