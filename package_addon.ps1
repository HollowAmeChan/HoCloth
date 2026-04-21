# Packages the repository root as a Blender addon zip while excluding
# development-only folders such as _native source, _third_party dependencies,
# _docs, and local build artifacts.
param(
    [string]$Version = "dev",
    [switch]$IncludeNativeBuild
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$RepoRoot = $PSScriptRoot
$DistDir = Join-Path $RepoRoot "_dist"
$SessionId = Get-Date -Format "yyyyMMdd-HHmmss"
$StageRoot = Join-Path $RepoRoot ("_build\\package-{0}" -f $SessionId)
$StageAddon = Join-Path $StageRoot "HoCloth"
$ZipPath = Join-Path $DistDir ("HoCloth-{0}.zip" -f $Version)
$ExcludeNames = @(
    ".git",
    ".github",
    ".gitignore",
    ".idea",
    "_build",
    "build",
    "CMakeLists.txt",
    "CMakePresets.json",
    "_dist",
    "_docs",
    "_native",
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

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
New-Item -ItemType Directory -Force -Path $StageAddon | Out-Null

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
    foreach ($runtimeDir in @(
        (Join-Path $RepoRoot "_bin"),
        (Join-Path $RepoRoot "cpp\\install\\HoCloth"),
        (Join-Path $RepoRoot "_native\\install\\HoCloth")
    )) {
        if (Test-Path -LiteralPath $runtimeDir) {
            Get-ChildItem -LiteralPath $runtimeDir -Force | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $StageAddon -Recurse -Force
            }
        }
    }
}

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

Write-Host "Packaged addon root: $RepoRoot"
Write-Host "Install in Blender from folder: $RepoRoot"
Write-Host "Created release zip: $ZipPath"

try {
    Remove-Item -LiteralPath $StageRoot -Recurse -Force
} catch {
    Write-Warning "Staging cleanup skipped: $StageRoot"
}
