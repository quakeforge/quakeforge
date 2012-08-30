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

from . import import_map
#from . import export_map

SYNCTYPE=(
    ('ST_SYNC', "Syncronized", "Automatic animations are all together"),
    ('ST_RAND', "Random", "Automatic animations have random offsets"),
)

EFFECTS=(
    ('EF_NONE', "None", "No effects"),
    ('EF_ROCKET', "Rocket", "Leave a rocket trail"),
    ('EF_GRENADE', "Grenade", "Leave a grenade trail"),
    ('EF_GIB', "Gib", "Leave a trail of blood"),
    ('EF_TRACER', "Tracer", "Green split trail"),
    ('EF_ZOMGIB', "Zombie Gib", "Leave a smaller blood trail"),
    ('EF_TRACER2', "Tracer 2", "Orange split trail + rotate"),
    ('EF_TRACER3', "Tracer 3", "Purple split trail"),
)

class QFMDLSettings(bpy.types.PropertyGroup):
    eyeposition = FloatVectorProperty(
        name="Eye Position",
        description="View possion relative to object origin")
    synctype = EnumProperty(
        items=SYNCTYPE,
        name="Sync Type",
        description="Add random time offset for automatic animations")
    rotate = BoolProperty(
        name="Rotate",
        description="Rotate automatically (for pickup items)")
    effects = EnumProperty(
        items=EFFECTS,
        name="Effects",
        description="Particle trail effects")
    #doesn't work :(
    #script = PointerProperty(
    #    type=bpy.types.Object,
    #    name="Script",
    #    description="Script for animating frames and skins")
    script = StringProperty(
        name="Script",
        description="Script for animating frames and skins")
    xform = BoolProperty(
        name="Auto transform",
        description="Auto-apply location/rotation/scale when exporting",
        default=True)
    md16 = BoolProperty(
        name="16-bit",
        description="16 bit vertex coordinates: QuakeForge only")

class ImportMDL6(bpy.types.Operator, ImportHelper):
    '''Load a Quake map File'''
    bl_idname = "import_mesh.quake_map"
    bl_label = "Import map"

    filename_ext = ".map"
    filter_glob = StringProperty(default="*.map", options={'HIDDEN'})

    def execute(self, context):
        keywords = self.as_keywords (ignore=("filter_glob",))
        return import_map.import_map(self, context, **keywords)

class ExportMDL6(bpy.types.Operator, ExportHelper):
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
    self.layout.operator(ImportMDL6.bl_idname, text="Quake map (.map)")


def menu_func_export(self, context):
    self.layout.operator(ExportMDL6.bl_idname, text="Quake map (.map)")


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
