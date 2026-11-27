#Requires -Version 5.1
[CmdletBinding()]
param([string]$BuildDir)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $projectRoot 'build' }
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$cmake = (Get-Command cmake.exe -CommandType Application).Source

$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'tools/versions.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$toolchain = $manifest.tools.llvmMingw
$assetName = [IO.Path]::GetFileName(([uri]$toolchain.url).AbsolutePath)
$archive = Join-Path $projectRoot ('.setup/downloads/llvmMingw-{0}-{1}' -f $toolchain.version,$assetName)
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
    throw "Missing LLVM-MinGW archive. Run setup.cmd first: $archive"
}
$outDir = Join-Path $projectRoot 'out'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
if (((Get-Item -LiteralPath $outDir -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
    throw "Refusing to package through a linked output directory: $outDir"
}
$zip = Join-Path $outDir 'our-taskdepot-win-x64.zip'
if (Test-Path -LiteralPath $zip) { throw "Move the existing package before rebuilding: $zip" }

$stage = Join-Path $outDir ('package-' + [guid]::NewGuid().ToString('N'))
$unpack = Join-Path $stage 'unpack'
$payload = Join-Path $stage 'payload'
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    New-Item -ItemType Directory -Path $unpack,$payload | Out-Null
    $stagedArchive = Join-Path $stage 'llvm-mingw.zip'
    Copy-Item -LiteralPath $archive -Destination $stagedArchive
    if ((Get-FileHash -LiteralPath $stagedArchive -Algorithm SHA256).Hash -ne $toolchain.sha256) {
        throw "LLVM-MinGW archive SHA-256 mismatch. Run setup.cmd to restore the archive: $archive"
    }

    & $cmake --build $BuildDir --config Release --target taskdepot_gui
    if ($LASTEXITCODE -ne 0) { throw "Release build failed (exit $LASTEXITCODE)." }
    & $cmake --install $BuildDir --config Release --component Taskdepot --prefix $payload
    if ($LASTEXITCODE -ne 0) { throw "Release install failed (exit $LASTEXITCODE)." }
    $app = Join-Path $payload 'taskdepot_gui.exe'
    if (-not (Test-Path -LiteralPath $app -PathType Leaf)) {
        throw 'The install did not produce taskdepot_gui.exe. Reconfigure the build directory and retry.'
    }
    $reader = [IO.BinaryReader]::new([IO.File]::OpenRead($app))
    try {
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'taskdepot_gui.exe is missing its DOS signature.' }
        $reader.BaseStream.Position = 0x3C
        $reader.BaseStream.Position = $reader.ReadUInt32()
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw 'taskdepot_gui.exe is not a Windows x64 executable.'
        }
    } finally { $reader.Dispose() }

    Push-Location -LiteralPath $unpack
    try {
        & $cmake -E tar xf $stagedArchive
        if ($LASTEXITCODE -ne 0) { throw "LLVM-MinGW extraction failed (exit $LASTEXITCODE)." }
    } finally { Pop-Location }
    $roots = @(Get-ChildItem -LiteralPath $unpack -Force)
    if ($roots.Count -ne 1 -or -not $roots[0].PSIsContainer -or
        -not (Test-Path -LiteralPath (Join-Path $roots[0].FullName $toolchain.executables.clang) -PathType Leaf)) {
        throw 'Unexpected LLVM-MinGW archive layout.'
    }
    Move-Item -LiteralPath $roots[0].FullName -Destination (Join-Path $payload 'compiler')
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/third-party-notices.md') -Destination $payload

    $stagedZip = Join-Path $stage 'package.zip'
    Push-Location -LiteralPath $payload
    try {
        & $cmake -E tar cf $stagedZip --format=zip -- taskdepot_gui.exe compiler third-party-notices.md
        if ($LASTEXITCODE -ne 0) { throw "Package creation failed (exit $LASTEXITCODE)." }
    } finally { Pop-Location }
    [IO.File]::Move($stagedZip, $zip)
    Write-Output $zip
} finally {
    $stageInfo = Get-Item -LiteralPath $stage -Force
    if ($stageInfo.Parent.FullName -eq $outDir -and
        ($stageInfo.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) {
        Remove-Item -LiteralPath $stageInfo.FullName -Recurse -Force
    }
}
