# Packages the repository root as a Blender addon zip while excluding
# development-only folders such as native source, third-party dependencies,
# docs, and local build artifacts.
param(
    [string]$Version = "dev",
    [switch]$IncludeNativeBuild
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$RepoRoot = $PSScriptRoot
$DistDir = Join-Path $RepoRoot "dist"
$SessionId = Get-Date -Format "yyyyMMdd-HHmmss"
$StageRoot = Join-Path $RepoRoot ("build\\package-{0}" -f $SessionId)
$StageAddon = Join-Path $StageRoot "HoCloth"
$ZipPath = Join-Path $DistDir ("HoCloth-{0}.zip" -f $Version)
$ExcludeNames = @(
    ".git",
    ".github",
    ".gitignore",
    "build",
    "CMakeLists.txt",
    "dist",
    "docs",
    "native",
    "package_addon.ps1",
    "pyproject.toml",
    "scripts",
    "tests",
    "third_party",
    "cpp",
    "src",
    "__pycache__"
)

if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "__init__.py"))) {
    throw "Plugin root must contain __init__.py: $RepoRoot"
}

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
New-Item -ItemType Directory -Force -Path $StageAddon | Out-Null

Get-ChildItem -LiteralPath $RepoRoot -Force | Where-Object {
    ($ExcludeNames -notcontains $_.Name) -and
    (-not ($_.Extension -eq ".md"))
} | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $StageAddon -Recurse -Force
}

if ($IncludeNativeBuild) {
    foreach ($runtimeDir in @(
        (Join-Path $RepoRoot "_bin"),
        (Join-Path $RepoRoot "cpp\\install\\HoCloth"),
        (Join-Path $RepoRoot "native\\install\\HoCloth")
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
