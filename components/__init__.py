from . import properties, registry


def register():
    registry.register()
    properties.register()


def unregister():
    properties.unregister()
    registry.unregister()
