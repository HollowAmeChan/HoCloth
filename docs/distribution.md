# HoCloth Distribution

Related docs:

- [发布流程与构建说明](./发布流程与构建说明.md)
- [目录功能说明](./目录功能说明.md)
- [project_layout.md](./project_layout.md)

## Local install target

The repository root itself is the Blender addon directory.

That means this folder can be installed directly in Blender during local
development, even though it also contains the native C++ project.

## Compiled output plan

When the native module is introduced, put the compiled runtime payload in a
separate runtime folder at the plugin root so the zip remains self-contained
without bundling C++ source.

Recommended layout:

- `native/` or `cpp/`: C++ source project
- `_bin/`: compiled extension modules and runtime DLLs used by Blender
- `dist/`: packaged release zips

That means:

- the project folder stays installable as a Blender addon
- compiled runtime files live in `_bin/`
- release packaging excludes C++ source directories and other dev-only folders

## Packaging flow

Local packaging:

```powershell
powershell -ExecutionPolicy Bypass -File .\package_addon.ps1 -Version dev
```

The resulting zip is written to:

`dist/HoCloth-dev.zip`

## Release flow

GitHub Actions packages the repository root as the addon, while excluding
development-only folders such as native source and tests.
