$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$target = Join-Path $root "build"
$backup = Join-Path $root ("build-old-" + (Get-Date -Format "yyyyMMdd-HHmmss"))

if (-not (Test-Path -LiteralPath $target -PathType Container)) {
    Write-Host "Old build directory does not exist."
    exit 0
}

# Check for Git-tracked files.
$tracked = & git -C $root ls-files -- build/

if ($LASTEXITCODE -ne 0) {
    throw "Failed to check Git-tracked files."
}

if ($tracked) {
    throw "Build directory contains Git-tracked files. Aborting."
}

# Keep the Visual Studio build directory untouched.
if (Test-Path -LiteralPath $backup) {
    throw "Backup directory already exists. Aborting."
}

Move-Item -LiteralPath $target -Destination $backup

Write-Host "Old build directory moved successfully."
Write-Host "Backup: $backup"
