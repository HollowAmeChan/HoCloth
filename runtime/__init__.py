from . import bridge, inspector, inspector_bridge, live, session, settings


def register():
    settings.register()
    inspector_bridge.register()
    inspector.register()
    live.register()
    return None


def unregister():
    live.unregister()
    inspector.unregister()
    inspector_bridge.unregister()
    session.reset_runtime_state()
    settings.unregister()
