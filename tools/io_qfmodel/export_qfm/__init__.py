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

#modules for handling obj.data types

#from .export import export_object, strip_nnn
#from .export import enable_collections, restore_collections
#from .operators import QFM_OT_QfmShowTransform
#from .operators import QFM_OT_QfmFindCoM
#from .operators import QFM_OT_QfmVolume
from .operators import QFM_OT_ExportQfm
from .operators import QFM_OT_ExportQfm_quick
#from .panels import WORKSPACE_PT_tools_qfm_export

#from . import export_modules

def export_qfm_menu_func(self, context):
    self.layout.operator(QFM_OT_ExportQfm.bl_idname, text="Quakeforge QFM (.qfm)")

classes_to_register = (
    QFM_OT_ExportQfm,
    QFM_OT_ExportQfm_quick,
    #WORKSPACE_PT_tools_qfm_export,
)

menus_to_register = (
    (bpy.types.TOPBAR_MT_file_export, export_qfm_menu_func),
)
