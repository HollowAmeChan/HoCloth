param(
    [string]$Platform = "vc17win64",
    [string]$Configuration = "release",
    [string]$CMakeExecutable = "",
    [string]$PackmanRoot = ""
)

$ErrorActionPreference = "Stop"

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

function Resolve-MSBuildExecutable {
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($null -ne $command -and $command.Source) {
        return $command.Source
    }

    $vsWhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($vsWhere in $vsWhereCandidates) {
        $installPath = & $vsWhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2>$null | Select-Object -First 1
        if ($installPath) {
            $resolvedPath = $installPath.Trim()
            if (Test-Path -LiteralPath $resolvedPath) {
                return $resolvedPath
            }
        }
    }

    $fallbackPatterns = @(
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe")
    )

    foreach ($pattern in $fallbackPatterns) {
        $candidate = Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $candidate) {
            return $candidate.FullName
        }
    }

    return $null
}

$pluginRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourcePhysxRoot = Join-Path $pluginRoot "_third_party\PhysX\physx"

if (-not (Test-Path -LiteralPath $sourcePhysxRoot)) {
    throw "PhysX source root not found: $sourcePhysxRoot"
}

$resolvedCMake = Resolve-CMakeExecutable -RequestedPath $CMakeExecutable
if (-not $resolvedCMake) {
    throw "Unable to locate cmake.exe automatically. Pass -CMakeExecutable <full-path-to-cmake.exe>."
}

$cmakeDir = Split-Path -Parent $resolvedCMake

$physxRoot = $sourcePhysxRoot
$generateScript = Join-Path $physxRoot "generate_projects.bat"
$compilerRoot = Join-Path $physxRoot "compiler"
$platformRoot = Join-Path $compilerRoot $Platform
$physxRepoRoot = Join-Path $pluginRoot "_third_party\\PhysX"
if (-not $PackmanRoot) {
    $PackmanRoot = Join-Path $physxRepoRoot "packman-cache"
}
$packmanRoot = $PackmanRoot
$packmanPython = Join-Path $packmanRoot "python"
$packmanCommon = Join-Path $packmanRoot "packman-common"
$binaryRoots = @(
    (Join-Path $physxRoot "bin\win.x86_64.vc143.mt\$Configuration"),
    (Join-Path $physxRoot "bin\win.x86_64.vc143.md\$Configuration"),
    (Join-Path $physxRoot "bin\win.x86_64.vc142.mt\$Configuration"),
    (Join-Path $physxRoot "bin\win.x86_64.vc142.md\$Configuration")
)

New-Item -ItemType Directory -Force -Path $packmanRoot | Out-Null
New-Item -ItemType Directory -Force -Path $packmanPython | Out-Null
New-Item -ItemType Directory -Force -Path $packmanCommon | Out-Null

Write-Host "Physical plugin root: $pluginRoot"
Write-Host "PhysX source root: $sourcePhysxRoot"
Write-Host "PhysX work root: $physxRoot"
Write-Host "Requested platform: $Platform"
Write-Host "Requested configuration: $Configuration"
Write-Host "Packman cache: $packmanRoot"
Write-Host "CMake executable: $resolvedCMake"

if (-not (Test-Path -LiteralPath $generateScript)) {
    throw "PhysX generate script not found: $generateScript"
}

$env:PM_PACKAGES_ROOT = $packmanRoot
$env:PM_DISABLE_VS_WARNING = "1"
$env:PATH = "$cmakeDir;$env:PATH"

Push-Location $physxRoot
try {
    & $generateScript $Platform
    if ($LASTEXITCODE -ne 0) {
        throw "PhysX project generation failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $platformRoot)) {
    Write-Warning "PhysX compiler platform directory was not created: $platformRoot"
    Write-Host "Available compiler directories:"
    if (Test-Path -LiteralPath $compilerRoot) {
        Get-ChildItem -LiteralPath $compilerRoot -Directory | Select-Object FullName
    }
    exit 1
}

$solution = Get-ChildItem -LiteralPath $platformRoot -Filter *.sln -File | Select-Object -First 1
if ($null -eq $solution) {
    Write-Warning "No solution file was found under: $platformRoot"
    Write-Host "Available files:"
    Get-ChildItem -LiteralPath $platformRoot -Force | Select-Object Name, FullName
    exit 1
}

Write-Host "PhysX solution: $($solution.FullName)"
Write-Host "Note: compiler\$Platform is only the generated project directory."
Write-Host "The actual PhysX .lib/.dll outputs will appear under physx\\bin\\win.x86_64.*\\$Configuration."

$resolvedMSBuild = Resolve-MSBuildExecutable
if (-not $resolvedMSBuild) {
    Write-Warning "msbuild.exe was not found on PATH."
    Write-Host "The PhysX solution was generated successfully. Open and build it manually:"
    Write-Host "  $($solution.FullName)"
    Write-Host "After the build, check one of these output directories:"
    foreach ($binaryRoot in $binaryRoots) {
        Write-Host "  $binaryRoot"
    }
    exit 0
}

Write-Host "MSBuild executable: $resolvedMSBuild"
& $resolvedMSBuild $solution.FullName /m /p:Configuration=$Configuration /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "PhysX solution build failed with exit code $LASTEXITCODE"
}

Write-Host "PhysX build completed."
foreach ($binaryRoot in $binaryRoots) {
    if (Test-Path -LiteralPath $binaryRoot) {
        Write-Host "PhysX binary output: $binaryRoot"
    }
}
