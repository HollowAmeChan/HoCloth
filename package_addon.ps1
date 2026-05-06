# Packages the repository root as a Blender addon zip while excluding
# development-only folders such as _native source, _third_party dependencies,
# _docs, and local build artifacts.
param(
    [string]$Version = "dev",
    [switch]$IncludeNativeBuild,
    [switch]$RunNativeSmoke,
    [switch]$CreateZip,
    [switch]$FreshConfigure,
    [string]$CMakeExecutable = "",
    [string]$PythonExecutable = "",
    [string]$ConfigurePreset = "vs2022-release-native",
    [string]$BuildPreset = "build-vs2022-release-native",
    [string]$BuildConfiguration = "Release"
)

$ErrorActionPreference = "Stop"
if ($CreateZip) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
}

function Resolve-CMakeExecutable {
    param(
        [string]$RequestedPath
    )

    if ($RequestedPath -and (Test-Path -LiteralPath $RequestedPath)) {
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command -and $command.Source) {
        return $command.Source
    }

    $wellKnownCandidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "D:\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "D:\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "D:\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "E:\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "E:\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "E:\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($candidatePath in $wellKnownCandidates) {
        if (Test-Path -LiteralPath $candidatePath) {
            return $candidatePath
        }
    }

    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $vsInstallations = & $vswhere -products * -requires Microsoft.Component.MSBuild -property installationPath
        foreach ($vsInstall in $vsInstallations) {
            $candidatePath = Join-Path $vsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path -LiteralPath $candidatePath) {
                return $candidatePath
            }
        }
    }

    $searchRoots = @(
        "D:\_SOFTWARE",
        "$env:ProgramFiles\JetBrains",
        "$env:LOCALAPPDATA\Programs",
        "$env:LOCALAPPDATA\JetBrains\Toolbox\apps"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($root in $searchRoots) {
        $candidate = Get-ChildItem -Path $root -Recurse -Filter cmake.exe -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like '*\cmake\win\x64\bin\cmake.exe' } |
            Select-Object -First 1
        if ($null -ne $candidate) {
            return $candidate.FullName
        }
    }

    return $null
}

function Test-PythonExecutable {
    param(
        [string]$CandidatePath
    )

    if (-not $CandidatePath -or -not (Test-Path -LiteralPath $CandidatePath)) {
        return $false
    }

    try {
        $version = & $CandidatePath -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"
        return $LASTEXITCODE -eq 0 -and $version -eq "3.11"
    } catch {
        return $false
    }
}

function Resolve-PythonExecutable {
    param(
        [string]$RequestedPath
    )

    if ($RequestedPath) {
        if (Test-PythonExecutable -CandidatePath $RequestedPath) {
            return (Resolve-Path -LiteralPath $RequestedPath).Path
        }
        throw "Requested Python executable is missing or is not Python 3.11: $RequestedPath"
    }

    if ($env:HOCLOTH_PYTHON_EXECUTABLE -and (Test-PythonExecutable -CandidatePath $env:HOCLOTH_PYTHON_EXECUTABLE)) {
        return (Resolve-Path -LiteralPath $env:HOCLOTH_PYTHON_EXECUTABLE).Path
    }

    foreach ($commandName in @("python.exe", "python3.exe")) {
        $command = Get-Command $commandName -ErrorAction SilentlyContinue
        if ($null -ne $command -and $command.Source -and (Test-PythonExecutable -CandidatePath $command.Source)) {
            return $command.Source
        }
    }

    $candidatePaths = @(
        "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe",
        "$env:ProgramFiles\Python311\python.exe",
        "${env:ProgramFiles(x86)}\Python311\python.exe",
        "$env:ProgramFiles\Blender Foundation\Blender 4.5\4.5\python\bin\python.exe",
        "$env:ProgramFiles\Blender Foundation\Blender 4.4\4.4\python\bin\python.exe",
        "$env:ProgramFiles\Blender Foundation\Blender 4.3\4.3\python\bin\python.exe",
        "D:\Blender\Blender 4.5\4.5\python\bin\python.exe",
        "D:\Blender\Blender 4.4\4.4\python\bin\python.exe",
        "D:\Blender\Blender 4.3\4.3\python\bin\python.exe",
        "D:\Blender Foundation\Blender 4.5\4.5\python\bin\python.exe",
        "E:\Blender Foundation\Blender 4.5\4.5\python\bin\python.exe",
        "E:\Blender\Blender 4.5\4.5\python\bin\python.exe"
    )
    foreach ($candidatePath in $candidatePaths) {
        if (Test-PythonExecutable -CandidatePath $candidatePath) {
            return (Resolve-Path -LiteralPath $candidatePath).Path
        }
    }

    return $null
}

$RepoRoot = $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "_build"
$DistDir = Join-Path $RepoRoot "_dist"
$SessionId = Get-Date -Format "yyyyMMdd-HHmmss"
$SafeVersion = ($Version -replace '[^A-Za-z0-9._-]', '_')
$StageRoot = Join-Path $BuildDir ("release-{0}" -f $SafeVersion)
$StageAddon = Join-Path $StageRoot "HoCloth"
$ZipPath = Join-Path $DistDir ("HoCloth-{0}.zip" -f $Version)
$ExcludeNames = @(
    ".git",
    ".github",
    ".gitignore",
    ".idea",
    "_build",
    "_bin",
    "build",
    "CMakeLists.txt",
    "CMakePresets.json",
    "_dist",
    "_docs",
    "_native",
    "_ReferenceProject",
    "package_addon.ps1",
    "pyproject.toml",
    "scripts",
    "tests",
    "_third_party",
    "cpp",
    "src",
    "__pycache__"
)
$ExcludePrefixes = @(
    "cmake-build-"
)

if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "__init__.py"))) {
    throw "Plugin root must contain __init__.py: $RepoRoot"
}

if ($IncludeNativeBuild) {
    $resolvedCMake = Resolve-CMakeExecutable -RequestedPath $CMakeExecutable
    if (-not $resolvedCMake) {
        throw "Unable to locate cmake.exe automatically. Pass -CMakeExecutable <full-path-to-cmake.exe>."
    }

    $presetFile = Join-Path $RepoRoot "CMakePresets.json"
    if (-not (Test-Path -LiteralPath $presetFile)) {
        throw "CMakePresets.json not found: $presetFile"
    }

    $resolvedPython = Resolve-PythonExecutable -RequestedPath $PythonExecutable
    $configureArgs = @("-S", $RepoRoot, "--preset", $ConfigurePreset)
    if ($FreshConfigure) {
        $configureArgs = @("--fresh") + $configureArgs
    }
    if ($resolvedPython) {
        $pythonRoot = Split-Path -Parent $resolvedPython
        Write-Host "Using Python 3.11 executable: $resolvedPython"
        $configureArgs += @(
            "-DHOCLOTH_PYTHON_EXECUTABLE=$resolvedPython",
            "-DPython_EXECUTABLE=$resolvedPython",
            "-DPython_ROOT_DIR=$pythonRoot"
        )
    } else {
        Write-Warning "Python 3.11 executable was not found by the package script; CMake will try its own Python discovery."
    }

    Write-Host "Using CMake executable: $resolvedCMake"
    Write-Host "Configuring native build with preset: $ConfigurePreset"
    & $resolvedCMake @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Native configure failed with exit code $LASTEXITCODE"
    }

    Write-Host "Building native module with preset: $BuildPreset"
    & $resolvedCMake --build --preset $BuildPreset --config $BuildConfiguration
    if ($LASTEXITCODE -ne 0) {
        throw "Native build failed with exit code $LASTEXITCODE"
    }

    if ($RunNativeSmoke) {
        $smokeExe = Join-Path $RepoRoot "_bin\hocloth_mc2_core_smoke.exe"
        if (-not (Test-Path -LiteralPath $smokeExe)) {
            throw "Native smoke executable was not found after build: $smokeExe"
        }
        Write-Host "Running native smoke test: $smokeExe"
        & $smokeExe
        if ($LASTEXITCODE -ne 0) {
            throw "Native smoke test failed with exit code $LASTEXITCODE"
        }
    }
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
if ($CreateZip) {
    New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
}

if (Test-Path -LiteralPath $StageRoot) {
    Remove-Item -LiteralPath $StageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $StageAddon | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StageAddon "_bin") | Out-Null

Get-ChildItem -LiteralPath $RepoRoot -Force | Where-Object {
    $item = $_
    ($ExcludeNames -notcontains $_.Name) -and
    (-not ($ExcludePrefixes | Where-Object { $item.Name.StartsWith($_) })) -and
    (-not ($_.Extension -eq ".md"))
} | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $StageAddon -Recurse -Force
}

Get-ChildItem -LiteralPath $StageAddon -Recurse -Directory -Force | Where-Object {
    $_.Name -eq "__pycache__"
} | ForEach-Object {
    Remove-Item -LiteralPath $_.FullName -Recurse -Force
}

if ($IncludeNativeBuild) {
    $runtimeExcludeNames = @(
        "Debug",
        "Release",
        "RelWithDebInfo",
        "MinSizeRel",
        "__pycache__"
    )
    foreach ($runtimeDir in @(
        (Join-Path $RepoRoot "_bin"),
        (Join-Path $RepoRoot "cpp\\install\\HoCloth"),
        (Join-Path $RepoRoot "_native\\install\\HoCloth")
    )) {
        if (Test-Path -LiteralPath $runtimeDir) {
            Get-ChildItem -LiteralPath $runtimeDir -Force | Where-Object {
                $runtimeExcludeNames -notcontains $_.Name
            } | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $StageAddon "_bin") -Recurse -Force
            }
        }
    }
} elseif (Test-Path -LiteralPath (Join-Path $RepoRoot "_bin")) {
    Get-ChildItem -LiteralPath (Join-Path $RepoRoot "_bin") -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $StageAddon "_bin") -Recurse -Force
    }
}

Write-Host "Packaged addon root: $RepoRoot"
Write-Host "Prepared release folder: $StageRoot"
Write-Host "Install in Blender from folder: $StageAddon"

if ($CreateZip) {
    if (Test-Path -LiteralPath $ZipPath) {
        $ZipPath = Join-Path $DistDir ("HoCloth-{0}-{1}.zip" -f $Version, $SessionId)
    }

    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }

    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $StageRoot,
        $ZipPath,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $false
    )

    Write-Host "Created release zip: $ZipPath"
}
