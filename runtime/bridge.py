class NativeBridgeStub:
    """Temporary stand-in until the native extension is available."""

    def build_scene(self, compiled_scene):
        return {
            "handle": 1,
            "summary": compiled_scene.summary(),
            "backend": "stub",
        }

    def destroy_scene(self, handle):
        return handle


def load_bridge():
    # The real bridge can later attempt to import a module from _bin.
    return NativeBridgeStub()
