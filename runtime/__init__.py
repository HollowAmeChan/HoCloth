from . import bridge, session


def register():
    return None


def unregister():
    session.reset_runtime_state()
