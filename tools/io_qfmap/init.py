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
from bpy.props import BoolProperty, FloatProperty, StringProperty, EnumProperty
from bpy.props import FloatVectorProperty, PointerProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper, path_reference_mode, axis_conversion
from bpy.app.handlers import persistent

from .entityclass import EntityClassDict, EntityClassError
from . import entity
from . import import_map
from . import export_map

def ecm_draw(self, context):
    layout = self.layout
    for item in self.menu_items:
        if type(item[1]) is str:
            ec = context.scene.qfmap.entity_classes[item[1]]
            if ec.size:
                icon = 'OBJECT_DATA'
            else:
                icon = 'MESH_DATA'
            op = layout.operator("object.add_entity", text=item[0], icon=icon)
            op.entclass=item[1]
            if ec.comment:
                pass
        else:
            layout.menu(item[1].bl_idname)

class EntityClassMenu:
    @classmethod
    def clear(cls):
        while cls.menu_items:
            if type(cls.menu_item[0][1]) is not str:
                bpy.utils.unregister_class(cls.menu_items[0][1])
                cls.menu_items[0][1].clear()
            del cls.menu_items[0]
    @classmethod
    def build(cls, menudict, name="INFO_MT_entity_add", label="entity"):
        items = list(menudict.items())
        items.sort()
        menu_items = []
        for i in items:
            i = list(i)
            if type(i[1]) is dict:
                if i[0]:
                    nm = "_".join((name, i[0]))
                else:
                    nm = name
                i[1] = cls.build(i[1], nm, i[0])
            menu_items.append(i)
        attrs = {}
        attrs["menu_items"] = menu_items
        attrs["draw"] = ecm_draw
        attrs["bl_idname"] = name
        attrs["bl_label"] = label
        attrs["clear"] = cls.clear
        menu = type(name, (bpy.types.Menu,), attrs)
        bpy.utils.register_class(menu)
        return menu

@persistent
def scene_load_handler(dummy):
    for scene in bpy.data.scenes:
        if hasattr(scene, "qfmap"):
            scene.qfmap.script_update(bpy.context)

class MapeditMessage(bpy.types.Operator):
    bl_idname = "qfmapedit.message"
    bl_label = "Message"
    type: StringProperty()
    message: StringProperty()

    def execute(self, context):
        self.report({'INFO'}, message)
        return {'FINISHED'}
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_popup(self, width=400, height=200)
    def draw(self, context):
        self.layout.label(self.type, icon='ERROR')
        self.layout.label(self.message)

def scan_entity_classes(context):
    qfmap = context.scene.qfmap
    if not qfmap.dirpath:
        return
    qfmap.entity_classes.from_source_tree(qfmap.dirpath)
    name = context.scene.name + '-EntityClasses'
    if name in bpy.data.texts:
        txt = bpy.data.texts[name]
    else:
        txt = bpy.data.texts.new(name)
    txt.from_string(qfmap.entity_classes.to_plist())
    qfmap.script = name

def parse_entity_classes(context):
    context.scene.qfmap.script_update(context)

def ec_dir_update(self, context):
    try:
        scan_entity_classes(context)
    except EntityClassError as err:
        self.dirpath = ""
        bpy.ops.qfmapedit.message('INVOKE_DEFAULT', type="Error",
                                  message="Entity Class Error: %s" % err)

def ec_script_update(self, context):
    self.script_update(context)

class AddEntity(bpy.types.Operator):
    '''Add an entity'''
    bl_idname = "object.add_entity"
    bl_label = "Entity"
    entclass: StringProperty(name = "entclass")

    def execute(self, context):
        keywords = self.as_keywords()
        return entity.add_entity(self, context, **keywords)

class QFEntityClassScan(bpy.types.Operator):
    '''Rescan the specified QuakeC source tree'''
    bl_idname = "scene.scan_entity_classes"
    bl_label = "RELOAD"

    def execute(self, context):
        scan_entity_classes(context)
        return {'FINISHED'}

class QFEntityClassParse(bpy.types.Operator):
    '''Reparse the specified entity class script'''
    bl_idname = "scene.parse_entity_classes"
    bl_label = "RELOAD"

    def execute(self, context):
        parse_entity_classes(context)
        return {'FINISHED'}

class QFEntityClasses(bpy.types.PropertyGroup):
    wadpath: StringProperty(
        name="wadpath",
        description="Path to search for wad files",
        subtype='DIR_PATH')
    dirpath: StringProperty(
        name="dirpath",
        description="Path to qc source tree",
        subtype='DIR_PATH',
        update=ec_dir_update)
    script: StringProperty(
        name="script",
        description="entity class storage",
        update=ec_script_update)
    entity_classes = EntityClassDict()
    ecm = EntityClassMenu.build({})
    entity_targets = {}
    target_entities = []

    def script_update(self, context):
        if self.script in bpy.data.texts:
            script = bpy.data.texts[self.script].as_string()
            self.entity_classes.from_plist(script)
            menudict = {}
            entclasses = self.entity_classes.keys()
            for ec in entclasses:
                ecsub = ec.split("_")
                d = menudict
                for sub in ecsub[:-1]:
                    if sub not in d:
                        d[sub] = {}
                    elif type(d[sub]) is str:
                        d[sub] = {"":d[sub]}
                    d = d[sub]
                sub = ecsub[-1]
                if sub in d:
                    d[sub][""] = ec
                else:
                    d[sub] = ec
            self.__class__.ecm = EntityClassMenu.build(menudict)

class OBJECT_PT_QFECPanel(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    #bl_context = 'scene'
    bl_category = "View"
    bl_label = 'QF Entity Classes'

    @classmethod
    def poll(cls, context):
        return True

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        row = layout.row()
        layout.prop(scene.qfmap, "wadpath")
        row = layout.row()
        row.prop(scene.qfmap, "dirpath")
        row.operator("scene.scan_entity_classes", text="", icon="FILE_REFRESH")
        row = layout.row()
        row.prop(scene.qfmap, "script")
        row.operator("scene.parse_entity_classes", text="", icon="FILE_REFRESH")

class ImportPoints(bpy.types.Operator, ImportHelper):
    '''Load a Quake points File'''
    bl_idname = "import_mesh.quake_points"
    bl_label = "Import points"

    filename_ext = ".pts"
    filter_glob: StringProperty(default="*.pts", options={'HIDDEN'})

    def execute(self, context):
        keywords = self.as_keywords (ignore=("filter_glob",))
        return import_map.import_pts(self, context, **keywords)

class ImportMap(bpy.types.Operator, ImportHelper):
    '''Load a Quake map File'''
    bl_idname = "import_mesh.quake_map"
    bl_label = "Import map"

    filename_ext = ".map"
    filter_glob: StringProperty(default="*.map", options={'HIDDEN'})

    def execute(self, context):
        keywords = self.as_keywords (ignore=("filter_glob",))
        return import_map.import_map(self, context, **keywords)

class ExportMap(bpy.types.Operator, ExportHelper):
    '''Save a Quake map File'''

    bl_idname = "export_mesh.quake_map"
    bl_label = "Export map"

    filename_ext = ".map"
    filter_glob: StringProperty(default="*.map", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        keywords = self.as_keywords (ignore=("check_existing", "filter_glob"))
        return export_map.export_map(self, context, **keywords)

def menu_func_import(self, context):
    self.layout.operator(ImportMap.bl_idname, text="Quake map (.map)")
    self.layout.operator(ImportPoints.bl_idname, text="Quake points (.pts)")

def menu_func_export(self, context):
    self.layout.operator(ExportMap.bl_idname, text="Quake map (.map)")

def menu_func_add(self, context):
    self.layout.menu(context.scene.qfmap.ecm.bl_idname, icon='PLUGIN')

classes_to_register = (
    MapeditMessage,
    AddEntity,
    QFEntityClassScan,
    QFEntityClassParse,
    QFEntityClasses,
    OBJECT_PT_QFECPanel,
    ImportPoints,
    ImportMap,
    ExportMap,
)
menus_to_register = (
    (bpy.types.TOPBAR_MT_file_import, menu_func_import),
    (bpy.types.TOPBAR_MT_file_export, menu_func_export),
    (bpy.types.VIEW3D_MT_add, menu_func_add),
)
custom_properties_to_register = (
    (bpy.types.Scene, "qfmap", QFEntityClasses),
)
handlers_to_register = (
    ("load_post", scene_load_handler),
)
