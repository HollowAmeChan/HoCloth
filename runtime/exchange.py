from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
from typing import Any, Literal, TypedDict


SCHEMA_NAME = "hocloth.exchange"
SCHEMA_VERSION = 1
COORDINATE_SPACE = "blender_world"
LENGTH_UNIT = "meter"
QUATERNION_ORDER = "wxyz"

PayloadType = Literal[
    "authoring_snapshot",
    "compiled_scene",
    "build_output",
    "frame_inputs",
    "step_output",
    "runtime_debug",
]


class ExchangeEnvelope(TypedDict):
    schema: str
    schema_version: int
    payload_type: str
    coordinate_space: str
    length_unit: str
    quaternion_order: str
    payload: dict[str, Any]


@dataclass(frozen=True)
class ExchangeInfo:
    schema: str = SCHEMA_NAME
    schema_version: int = SCHEMA_VERSION
    coordinate_space: str = COORDINATE_SPACE
    length_unit: str = LENGTH_UNIT
    quaternion_order: str = QUATERNION_ORDER

    def label(self) -> str:
        return f"{self.schema} v{self.schema_version}"

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema": self.schema,
            "schema_version": self.schema_version,
            "coordinate_space": self.coordinate_space,
            "length_unit": self.length_unit,
            "quaternion_order": self.quaternion_order,
        }


INFO = ExchangeInfo()


def schema_label() -> str:
    return INFO.label()


def schema_metadata() -> dict[str, Any]:
    return INFO.to_dict()


def is_exchange_envelope(value: Any) -> bool:
    return (
        isinstance(value, dict)
        and value.get("schema") == SCHEMA_NAME
        and isinstance(value.get("payload"), dict)
        and isinstance(value.get("payload_type"), str)
    )


def make_envelope(
    payload_type: PayloadType,
    payload: dict[str, Any] | None = None,
) -> ExchangeEnvelope:
    return {
        "schema": SCHEMA_NAME,
        "schema_version": SCHEMA_VERSION,
        "payload_type": payload_type,
        "coordinate_space": COORDINATE_SPACE,
        "length_unit": LENGTH_UNIT,
        "quaternion_order": QUATERNION_ORDER,
        "payload": payload or {},
    }


def unwrap_payload(value: dict[str, Any] | None, expected_type: PayloadType | None = None) -> dict[str, Any]:
    if value is None:
        return {}
    if not is_exchange_envelope(value):
        return value
    if expected_type is not None and value.get("payload_type") != expected_type:
        raise ValueError(
            f"Expected exchange payload '{expected_type}', got '{value.get('payload_type')}'."
        )
    return value.get("payload") or {}


def wrap_compiled_scene(compiled_scene_or_dict: Any) -> ExchangeEnvelope:
    if hasattr(compiled_scene_or_dict, "to_dict"):
        scene_data = compiled_scene_or_dict.to_dict()
    else:
        scene_data = dict(compiled_scene_or_dict or {})
    return make_envelope(
        "compiled_scene",
        {"scene": deepcopy(scene_data)},
    )


def wrap_authoring_snapshot(snapshot_or_dict: Any) -> ExchangeEnvelope:
    if is_exchange_envelope(snapshot_or_dict):
        if snapshot_or_dict.get("payload_type") != "authoring_snapshot":
            raise ValueError(
                f"Expected exchange payload 'authoring_snapshot', got '{snapshot_or_dict.get('payload_type')}'."
            )
        return deepcopy(snapshot_or_dict)
    return make_envelope("authoring_snapshot", deepcopy(dict(snapshot_or_dict or {})))


def empty_build_output() -> dict[str, Any]:
    return {"particles": [], "lines": [], "baselines": [], "colliders": []}


def wrap_build_output(build_output: dict[str, Any] | None) -> ExchangeEnvelope:
    if is_exchange_envelope(build_output):
        if build_output.get("payload_type") != "build_output":
            raise ValueError(
                f"Expected exchange payload 'build_output', got '{build_output.get('payload_type')}'."
            )
        return deepcopy(build_output)

    payload = deepcopy(build_output or empty_build_output())
    payload.setdefault("particles", [])
    payload.setdefault("lines", [])
    payload.setdefault("baselines", [])
    payload.setdefault("colliders", [])
    return make_envelope("build_output", payload)


def empty_frame_inputs() -> dict[str, Any]:
    return {"bone_chains": [], "collision_objects": []}


def wrap_frame_inputs(frame_inputs: dict[str, Any] | None) -> ExchangeEnvelope:
    if is_exchange_envelope(frame_inputs):
        if frame_inputs.get("payload_type") != "frame_inputs":
            raise ValueError(
                f"Expected exchange payload 'frame_inputs', got '{frame_inputs.get('payload_type')}'."
            )
        return deepcopy(frame_inputs)

    payload = deepcopy(frame_inputs or empty_frame_inputs())
    payload.setdefault("bone_chains", [])
    payload.setdefault("collision_objects", [])
    return make_envelope("frame_inputs", payload)


def frame_inputs_payload(frame_inputs: dict[str, Any] | None) -> dict[str, Any]:
    payload = unwrap_payload(frame_inputs, "frame_inputs") if frame_inputs else empty_frame_inputs()
    payload.setdefault("bone_chains", [])
    payload.setdefault("collision_objects", [])
    return payload


def wrap_step_output(
    runtime_state: dict[str, Any],
    transforms: list[dict[str, Any]],
    mesh_outputs: list[dict[str, Any]] | None = None,
) -> ExchangeEnvelope:
    return make_envelope(
        "step_output",
        {
            "runtime_state": dict(runtime_state),
            "transforms": list(transforms),
            "mesh_outputs": list(mesh_outputs or []),
        },
    )


def wrap_runtime_debug(
    runtime_state: dict[str, Any],
    compiled_scene: Any,
    runtime_inputs: dict[str, Any] | None,
    transforms: list[dict[str, Any]],
    mesh_outputs: list[dict[str, Any]] | None = None,
    build_output: dict[str, Any] | None = None,
    authoring_snapshot: dict[str, Any] | None = None,
) -> ExchangeEnvelope:
    compiled_envelope = wrap_compiled_scene(compiled_scene) if compiled_scene is not None else None
    return make_envelope(
        "runtime_debug",
        {
            "runtime_state": dict(runtime_state),
            "authoring_snapshot": wrap_authoring_snapshot(authoring_snapshot) if authoring_snapshot is not None else None,
            "compiled_scene": compiled_envelope,
            "build_output": wrap_build_output(build_output),
            "runtime_inputs": wrap_frame_inputs(runtime_inputs),
            "step_output": wrap_step_output(runtime_state, transforms, mesh_outputs),
        },
    )
