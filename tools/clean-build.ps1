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

# This moves the complete build tree, including any active VS2026 build, without deleting files.
# Inspect the destination afterward and only run this when preserving the old tree is intended.
if (Test-Path -LiteralPath $backup) {
    throw "Backup directory already exists. Aborting."
}

Move-Item -LiteralPath $target -Destination $backup

Write-Host "Build directory moved aside; no files were deleted."
Write-Host "Preserved backup: $backup"
