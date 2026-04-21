import os
import sys

import bpy
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


PLUGIN_DIR = os.path.dirname(__file__)
LIB_DIR = os.path.join(PLUGIN_DIR, "_Lib")
BIN_DIR = os.path.join(PLUGIN_DIR, "_bin")

for path in (PLUGIN_DIR, LIB_DIR, BIN_DIR):
    if os.path.isdir(path) and path not in sys.path:
        sys.path.append(path)


class HoClothAddonPreferences(bpy.types.AddonPreferences):
    bl_idname = __package__ or __name__

    def draw(self, context):
        _ = context
        self.layout.label(text="HoCloth plugin root is this repository folder.")


CLASSES = (
    HoClothAddonPreferences,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
