import bpy

from .inputs import build_runtime_inputs, reset_runtime_input_tracking
from .pose_apply import (
    apply_runtime_transforms_to_scene,
    capture_pose_baseline,
    capture_pose_state,
    restore_pose_state,
)
from .session import (
    get_compiled_scene,
    get_pose_baseline,
    set_pose_baseline,
    step_runtime,
)


_LIVE_STATE = {
    "active_scene_name": "",
    "last_processed_identity": "",
    "last_frame": None,
    "last_source_pose": {},
}


def _reset_runtime_tracking():
    _LIVE_STATE["last_processed_identity"] = ""
    _LIVE_STATE["last_frame"] = None
    _LIVE_STATE["last_source_pose"] = {}
    reset_runtime_input_tracking()


def _clear_active_scene():
    _LIVE_STATE["active_scene_name"] = ""
    _reset_runtime_tracking()


def _mark_stopped(scene: bpy.types.Scene | None, status: str = ""):
    if scene is not None:
        scene.hocloth_runtime_live_running = False
        if status:
            scene.hocloth_runtime_status = status
    _clear_active_scene()


def _is_scene_live_enabled(scene: bpy.types.Scene | None) -> bool:
    return bool(scene is not None and scene.hocloth_runtime_live_running)


def _scene_from_name():
    scene_name = _LIVE_STATE["active_scene_name"]
    if not scene_name:
        return None
    return bpy.data.scenes.get(scene_name)


def _check_frame_continuity(scene: bpy.types.Scene) -> bool:
    current_frame = int(scene.frame_current)
    last_frame = _LIVE_STATE["last_frame"]
    if last_frame is None:
        _LIVE_STATE["last_frame"] = current_frame
        return True

    if current_frame == last_frame:
        return True

    if abs(current_frame - last_frame) != 1:
        _mark_stopped(
            scene,
            f"Live runtime stopped: discontinuous frame jump {last_frame} -> {current_frame}",
        )
        if bpy.context.screen is not None and bpy.context.screen.is_animation_playing:
            bpy.ops.screen.animation_cancel(restore_frame=False)
        return False

    _LIVE_STATE["last_frame"] = current_frame
    return True


@bpy.app.handlers.persistent
def on_animation_playback_pre(scene, depsgraph):
    _ = depsgraph
    if not _is_scene_live_enabled(scene):
        return

    if scene.hocloth_runtime_handle == 0 or get_compiled_scene() is None:
        _mark_stopped(scene, "Live runtime stopped: build the runtime first")
        return

    _LIVE_STATE["active_scene_name"] = scene.name
    _reset_runtime_tracking()
    set_pose_baseline(capture_pose_baseline(scene, get_compiled_scene()))
    scene.hocloth_runtime_status = "Live runtime playback started"


@bpy.app.handlers.persistent
def on_frame_change_pre(scene, depsgraph):
    _ = depsgraph
    if not _is_scene_live_enabled(scene):
        return

    if _LIVE_STATE["active_scene_name"] != scene.name:
        return

    compiled_scene = get_compiled_scene()
    if compiled_scene is None:
        return

    restore_pose_state(scene, compiled_scene, _LIVE_STATE["last_source_pose"])


@bpy.app.handlers.persistent
def on_animation_playback_post(scene, depsgraph):
    _ = depsgraph
    active_scene = _scene_from_name() or scene
    if not _is_scene_live_enabled(active_scene):
        _clear_active_scene()
        return

    _mark_stopped(active_scene, "Live runtime stopped")


@bpy.app.handlers.persistent
def on_frame_change_post(scene, depsgraph):
    _ = depsgraph
    if not _is_scene_live_enabled(scene):
        return

    if _LIVE_STATE["active_scene_name"] != scene.name:
        return

    runtime_handle = int(scene.hocloth_runtime_handle)
    processed_identity = f"{runtime_handle}:{scene.frame_current}"
    if processed_identity == _LIVE_STATE["last_processed_identity"]:
        return
    _LIVE_STATE["last_processed_identity"] = processed_identity

    if runtime_handle == 0 or get_compiled_scene() is None:
        _mark_stopped(scene, "Live runtime stopped: runtime is unavailable")
        return

    if not _check_frame_continuity(scene):
        return

    compiled_scene = get_compiled_scene()
    source_pose = capture_pose_state(scene, compiled_scene)
    runtime_inputs = build_runtime_inputs(scene, compiled_scene)

    try:
        result = step_runtime(
            scene.hocloth_runtime_dt,
            scene.hocloth_runtime_substeps,
            runtime_inputs,
        )
    except RuntimeError as exc:
        _mark_stopped(scene, f"Live runtime failed: {exc}")
        if bpy.context.screen is not None and bpy.context.screen.is_animation_playing:
            bpy.ops.screen.animation_cancel(restore_frame=False)
        return

    runtime_state = result["runtime_state"]
    scene.hocloth_runtime_step_count = runtime_state["step_count"]
    scene.hocloth_runtime_transform_count = runtime_state["bone_transform_count"]

    status_suffix = ""
    if scene.hocloth_apply_pose_on_step:
        apply_result = apply_runtime_transforms_to_scene(
            scene,
            compiled_scene,
            result["transforms"],
            get_pose_baseline(),
        )
        status_suffix = f", applied={apply_result['applied_count']}"
    _LIVE_STATE["last_source_pose"] = source_pose

    scene.hocloth_runtime_status = (
        f"Live stepping on frame {scene.frame_current}, "
        f"steps={runtime_state['step_count']}, "
        f"transforms={runtime_state['bone_transform_count']}"
        f"{status_suffix}"
    )


def start_live_runtime(scene: bpy.types.Scene):
    scene.hocloth_runtime_live_running = True
    scene.hocloth_runtime_status = "Live runtime armed"
    _clear_active_scene()


def stop_live_runtime(scene: bpy.types.Scene | None = None, status: str = ""):
    _mark_stopped(scene, status)


def register():
    if on_animation_playback_pre not in bpy.app.handlers.animation_playback_pre:
        bpy.app.handlers.animation_playback_pre.append(on_animation_playback_pre)
    if on_animation_playback_post not in bpy.app.handlers.animation_playback_post:
        bpy.app.handlers.animation_playback_post.append(on_animation_playback_post)
    if on_frame_change_pre not in bpy.app.handlers.frame_change_pre:
        bpy.app.handlers.frame_change_pre.append(on_frame_change_pre)
    if on_frame_change_post not in bpy.app.handlers.frame_change_post:
        bpy.app.handlers.frame_change_post.append(on_frame_change_post)


def unregister():
    if on_animation_playback_pre in bpy.app.handlers.animation_playback_pre:
        bpy.app.handlers.animation_playback_pre.remove(on_animation_playback_pre)
    if on_animation_playback_post in bpy.app.handlers.animation_playback_post:
        bpy.app.handlers.animation_playback_post.remove(on_animation_playback_post)
    if on_frame_change_pre in bpy.app.handlers.frame_change_pre:
        bpy.app.handlers.frame_change_pre.remove(on_frame_change_pre)
    if on_frame_change_post in bpy.app.handlers.frame_change_post:
        bpy.app.handlers.frame_change_post.remove(on_frame_change_post)
    _clear_active_scene()
