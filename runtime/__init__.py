from . import bridge, inspector, live, session, settings


def register():
    settings.register()
    inspector.register()
    live.register()
    return None


def unregister():
    live.unregister()
    inspector.unregister()
    session.reset_runtime_state()
    settings.unregister()
