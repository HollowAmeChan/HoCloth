import bpy
from bpy.types import Operator,Panel

import os  # NOQA: E402
import sys  # NOQA: E402
"""
bl安装插件时无法识别到内部写为模块的文件夹(仅安装阶段，安装完毕后使用正常),
需要单独添加模块的路径才能找到
"""
plugin_dir = os.path.dirname(__file__)
sys.path.append(plugin_dir)
lib_dir = os.path.join(plugin_dir, "_Lib")
sys.path.append(lib_dir)

from bpy.props import BoolProperty, FloatProperty

# 内置的绘制快捷键ui的接口
import rna_keymap_ui


bl_info = {
    "name": "HoCloth",
    "author": "Hollow_ame",
    "version": (0, 0, 1),
    "blender": (4, 5, 0),
    "location": "Hollow",
    "description": "https://space.bilibili.com/60340452",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh",
}

class AddonPreference(bpy.types.AddonPreferences):
    """插件的参数，不随着文件改变而改变"""
    bl_idname = __name__

    def draw(self, context):
        pass


cls = [AddonPreference,]


def register():
    for i in cls:
        bpy.utils.register_class(i)
    prefs = bpy.context.preferences.addons[__name__].preferences


def unregister():
    for i in cls:
        bpy.utils.unregister_class(i)

    
