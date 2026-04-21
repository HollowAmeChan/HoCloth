from . import bridge, live, session


def register():
    live.register()
    return None


def unregister():
    live.unregister()
    session.reset_runtime_state()
