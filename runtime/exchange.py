from __future__ import annotations

from copy import deepcopy
from typing import Any, Literal, TypedDict


SCHEMA_NAME = "hocloth.exchange"
SCHEMA_VERSION = 1
COORDINATE_SPACE = "blender_world"
LENGTH_UNIT = "meter"
QUATERNION_ORDER = "wxyz"

PayloadType = Literal[
    "authoring_snapshot",
    "build_output",
    "frame_inputs",
    "step_output",
]


class ExchangeEnvelope(TypedDict):
    schema: str
    schema_version: int
    payload_type: str
    coordinate_space: str
    length_unit: str
    quaternion_order: str
    payload: dict[str, Any]


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
