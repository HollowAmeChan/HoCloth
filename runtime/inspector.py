from __future__ import annotations

import os
import subprocess


_INSPECTOR_PROCESS: subprocess.Popen | None = None


def _inspector_executable_path() -> str:
    return os.path.join(os.path.dirname(os.path.dirname(__file__)), "_bin", "hocloth_inspector_app.exe")


def is_inspector_running() -> bool:
    global _INSPECTOR_PROCESS
    if _INSPECTOR_PROCESS is None:
        return False
    if _INSPECTOR_PROCESS.poll() is not None:
        _INSPECTOR_PROCESS = None
        return False
    return True


def get_inspector_status() -> str:
    if is_inspector_running():
        return f"running (pid={_INSPECTOR_PROCESS.pid})"
    executable_path = _inspector_executable_path()
    if os.path.isfile(executable_path):
        return "stopped"
    return "missing exe"


def launch_inspector() -> tuple[bool, str]:
    global _INSPECTOR_PROCESS
    if is_inspector_running():
        return True, f"HoCloth inspector already running (pid={_INSPECTOR_PROCESS.pid})."

    executable_path = _inspector_executable_path()
    if not os.path.isfile(executable_path):
        return False, f"Inspector executable not found: {executable_path}"

    try:
        _INSPECTOR_PROCESS = subprocess.Popen(
            [executable_path],
            cwd=os.path.dirname(executable_path),
        )
    except Exception as exc:
        _INSPECTOR_PROCESS = None
        return False, f"Failed to launch inspector: {exc}"

    return True, f"HoCloth inspector started (pid={_INSPECTOR_PROCESS.pid})."


def stop_inspector() -> tuple[bool, str]:
    global _INSPECTOR_PROCESS
    if not is_inspector_running():
        _INSPECTOR_PROCESS = None
        return True, "HoCloth inspector is not running."

    process = _INSPECTOR_PROCESS
    try:
        process.terminate()
        process.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=2.0)
    except Exception as exc:
        return False, f"Failed to stop inspector: {exc}"

    _INSPECTOR_PROCESS = None
    return True, "HoCloth inspector stopped."


def toggle_inspector() -> tuple[bool, str]:
    if is_inspector_running():
        return stop_inspector()
    return launch_inspector()


def register():
    return None


def unregister():
    stop_inspector()
