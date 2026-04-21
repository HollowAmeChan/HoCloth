# HoCloth Distribution

Related docs:

- [发布流程与构建说明](./发布流程与构建说明.md)
- [目录功能说明](./目录功能说明.md)
- [project_layout.md](./project_layout.md)

## Local install target

The repository root itself is the Blender addon directory.

That means this folder can be installed directly in Blender during local
development, even though it also contains the `_native` C++ project.

## Compiled output plan

When the native module is introduced, put the compiled runtime payload in a
separate runtime folder at the plugin root so the zip remains self-contained
without bundling C++ source.

Recommended layout:

- `_native/` or `cpp/`: C++ source project
- `_bin/`: compiled extension modules and runtime DLLs used by Blender
- `_dist/`: packaged release zips

That means:

- the project folder stays installable as a Blender addon
- compiled runtime files live in `_bin/`
- release packaging excludes `_native` and other dev-only folders

## Packaging flow

Local packaging:

```powershell
powershell -ExecutionPolicy Bypass -File .\package_addon.ps1 -Version dev
```

The resulting zip is written to:

`_dist/HoCloth-dev.zip`

## Verified local release workflow

Use this flow when you want a release zip from the current working tree.

If the PhysX libraries are missing or need a refresh, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_physx.ps1
```

Then run the verified packaging command:

```powershell
powershell -ExecutionPolicy Bypass -File .\package_addon.ps1 -Version manual-check -IncludeNativeBuild -CreateZip
```

This command:

- configures CMake with preset `vs2022-release-physx`
- builds the native module into `_bin/`
- stages a clean addon folder in `_build/release-<Version>/HoCloth`
- writes the release zip to `_dist/HoCloth-<Version>.zip`

Verified outputs from the current repository layout:

- `_bin/hocloth_native.cp311-win_amd64.pyd`
- `_bin/PhysX_64.dll`
- `_bin/PhysXCommon_64.dll`
- `_bin/PhysXFoundation_64.dll`
- `_bin/PhysXCooking_64.dll`
- `_dist/HoCloth-manual-check.zip`

The release package excludes development-only content such as:

- `_native/`
- `_third_party/`
- `_docs/`
- `_build/`
- `_dist/`
- `build_physx.ps1`
- `package_addon.ps1`

## Release flow

GitHub Actions packages the repository root as the addon, while excluding
development-only folders such as `_native`, `_third_party`, and `_docs`.
