# HoCloth Initial Layout

Related docs:

- [发布流程与构建说明](./发布流程与构建说明.md)
- [目录功能说明](./目录功能说明.md)
- [distribution.md](./distribution.md)

This repository starts from a Blender addon entrypoint at the repository root
and grows into a mixed Python + C++ project.

## Directory plan

repository root
: Blender-installable addon folder used directly by Blender during local
development.

plugin root python modules
: Future Blender-side Python modules can live directly under the plugin root to
keep the addon layout simple and obvious.

`native/`
: C++ source, headers, and CMake files for the PhysX-backed runtime. This is
kept in the plugin repository but excluded from release zips.

`native/src/`
: Builder/runtime/exporter implementation files.

`native/include/`
: Public internal headers shared by the native module.

`native/cmake/`
: Helper CMake modules for PhysX, nanobind, and platform-specific setup.

`third_party/`
: Git-cloned external dependencies vendored into the repo for stable local
bootstrapping.

`_bin/`
: Compiled runtime payload copied next to the addon so Blender can load it
without needing the C++ source tree at runtime.

`docs/`
: Engineering notes, architecture docs, and milestone plans.

`package_addon.ps1`
: Local packaging script for producing a clean Blender addon zip from the
repository root.

## Recommended next files

- `pyproject.toml`: Python build metadata with `scikit-build-core`
- `CMakeLists.txt`: top-level native build entry
- `native/CMakeLists.txt`: native module target definition
- `_bin/`: runtime binary output directory
- `.github/workflows/release-addon.yml`: addon zip packaging and release
