#Requires -Version 5.1
[CmdletBinding()]
param([string]$ScintillaArchivePath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$installer = Join-Path $PSScriptRoot 'install-tools.ps1'
. $installer
$tempRoot = [IO.Path]::GetFullPath((Join-Path $env:TEMP ('our-taskdepot-tool-fixture-' + [guid]::NewGuid().ToString('N'))))
$tempBoundary = [IO.Path]::GetFullPath($env:TEMP).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not $tempRoot.StartsWith($tempBoundary,[StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture path escaped the temporary directory.' }
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
try {
    $source = Join-Path $tempRoot 'source'
    $null = New-Item -ItemType Directory -Path (Join-Path $source 'bin') -Force
    $fakeExe = Join-Path $source 'bin/fake.cmd'
    @('@echo off','echo fixture tool 1.2.3','exit /b 0') | Set-Content -LiteralPath $fakeExe -Encoding Ascii
    $archive = Join-Path $tempRoot 'fixture.zip'
    Compress-Archive -Path (Join-Path $source '*') -DestinationPath $archive -CompressionLevel Optimal
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifestPath = Join-Path $tempRoot 'versions.json'
    $manifest = [ordered]@{
        schemaVersion = 1
        tools = [ordered]@{
            fixture = [ordered]@{
                version = '1.2.3'
                url = 'https://127.0.0.1:1/fixture.zip'
                sha256 = $hash
                provenance = 'local test fixture'
                archiveType = 'zip'
                installPath = 'fixture'
                stripTopLevel = $false
                executables = [ordered]@{ fixture = 'bin/fake.cmd' }
                versionArguments = [ordered]@{ fixture = @('--version') }
            }
        }
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    $projectRoot = Join-Path $tempRoot 'project'
    $cacheDir = Join-Path $projectRoot '.setup/downloads'
    New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null
    $cache = Join-Path $cacheDir 'fixture-1.2.3-fixture.zip'
    Copy-Item -LiteralPath $archive -Destination $cache
    $logPath = Join-Path $tempRoot 'fixture.log'

    # First run extracts the real ZIP fixture, verifies its command version, and installs it.
    $paths = Invoke-ProjectToolSetup -ManifestPath $manifestPath -ProjectRoot $projectRoot -LogPath $logPath
    if (-not (Test-Path -LiteralPath $paths.fixture -PathType Leaf)) { throw 'Fresh fixture install did not produce its executable.' }

    # Second run reuses the installed tree and cache, rechecking both hash and reported version.
    $null = Invoke-ProjectToolSetup -ManifestPath $manifestPath -ProjectRoot $projectRoot -LogPath $logPath

    # A partial installed tree is discarded only after the verified archive stages successfully.
    Set-Content -LiteralPath $paths.fixture -Value '@echo off' -Encoding Ascii
    $null = Invoke-ProjectToolSetup -ManifestPath $manifestPath -ProjectRoot $projectRoot -LogPath $logPath
    $recoveredOutput = & $paths.fixture --version 2>&1
    $recoveredCode = $LASTEXITCODE
    if ($recoveredCode -ne 0 -or ([string]($recoveredOutput -join ' ')) -notmatch 'fixture tool 1\.2\.3') { throw 'Partial installed tree was not recovered.' }

    # A damaged cache must fail closed and must not reuse an otherwise valid installed executable.
    Set-Content -LiteralPath $cache -Value 'corrupt cache bytes' -Encoding Ascii
    $hashFailure = $false
    try { $null = Invoke-ProjectToolSetup -ManifestPath $manifestPath -ProjectRoot $projectRoot -LogPath $logPath }
    catch { $hashFailure = $true }
    if (-not $hashFailure) { throw 'Corrupt cache unexpectedly succeeded.' }
    if ((Get-Content -LiteralPath $logPath -Raw) -notmatch 'Discarding corrupted cache') { throw 'Corrupt-cache failure was not logged.' }

    # A stale .partial file cannot be mistaken for a complete archive.
    Remove-Item -LiteralPath $cache -Force -ErrorAction SilentlyContinue
    $partial = "$cache.partial"
    Set-Content -LiteralPath $partial -Value 'incomplete' -Encoding Ascii
    $downloadFailure = $false
    try { $null = Invoke-ProjectToolSetup -ManifestPath $manifestPath -ProjectRoot $projectRoot -LogPath $logPath }
    catch { $downloadFailure = $true }
    if (-not $downloadFailure -or (Test-Path -LiteralPath $cache -PathType Leaf)) { throw 'Partial cache was incorrectly reused as a complete artifact after a failed download.' }

    # Deterministically reject resume, then supply the real ZIP on a full retry.
    $resumeProject = Join-Path $tempRoot 'resume-project'
    $resumeCacheDir = Join-Path $resumeProject '.setup/downloads'
    New-Item -ItemType Directory -Path $resumeCacheDir -Force | Out-Null
    $resumeCache = Join-Path $resumeCacheDir 'fixture-1.2.3-fixture.zip'
    $resumePartial = "$resumeCache.partial"
    Set-Content -LiteralPath $resumePartial -Value 'incomplete' -Encoding Ascii
    $resumeLog = Join-Path $tempRoot 'resume.log'
    $curlCalls = [System.Collections.Generic.List[string]]::new()
    $curlStub = Join-Path $tempRoot 'curl-stub.ps1'
    @'
$curlCalls.Add(($args -join ' '))
if ($curlCalls.Count -eq 1) {
    if ($args -notcontains '--continue-at' -or -not (Test-Path -LiteralPath $resumePartial)) { throw 'Expected an existing partial resume attempt.' }
    'fixture resume rejected'
    $global:LASTEXITCODE = 33
} elseif ($curlCalls.Count -eq 2) {
    if ($args -contains '--continue-at' -or (Test-Path -LiteralPath $resumePartial)) { throw 'Full retry must discard the partial and omit resume.' }
    $outputIndex = [Array]::IndexOf($args, '--output')
    if ($outputIndex -lt 0 -or $args[$outputIndex + 1] -ne $resumePartial) { throw 'Unexpected full retry destination.' }
    Copy-Item -LiteralPath $archive -Destination $resumePartial
    'HTTP 200 bytes=fixture'
    'fixture full download complete'
    $global:LASTEXITCODE = 0
} else { throw 'Unexpected extra curl attempt.' }
'@ | Set-Content -LiteralPath $curlStub -Encoding UTF8
    function Get-Command {
        param([string]$Name)
        if ($Name -ne 'curl.exe') { throw "Unexpected command lookup: $Name" }
        [pscustomobject]@{ Source = $curlStub }
    }
    try {
        $resumePaths = Invoke-ProjectToolSetup -ManifestPath $manifestPath -ProjectRoot $resumeProject -LogPath $resumeLog
    } finally { Remove-Item Function:\Get-Command }
    if ($curlCalls.Count -ne 2) { throw 'Expected exactly one resume and one full download.' }
    $messages = @(Get-Content -LiteralPath $resumeLog | ForEach-Object { $_ -replace '^\[[^\]]+\] \[[^\]]+\] ', '' })
    $expectedMessages = @(
        'fixture resume rejected',
        'Resuming the partial fixture download failed (exit 33); retrying once from the beginning.',
        'HTTP 200 bytes=fixture',
        'fixture full download complete',
        'fixture artifact SHA-256 verified (provenance: local test fixture).'
    )
    if (($messages[1..5] -join "`n") -cne ($expectedMessages -join "`n")) { throw 'Resume/full-retry output or verification log differed.' }
    if ((Get-FileHash -LiteralPath $resumeCache -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hash -or (Test-Path -LiteralPath $resumePartial)) { throw 'Full retry did not promote the verified archive to cache.' }
    $resumeOutput = & $resumePaths.fixture --version 2>&1
    if ($LASTEXITCODE -ne 0 -or ([string]($resumeOutput -join ' ')) -cne 'fixture tool 1.2.3') { throw 'Full retry did not extract and install the exact fixture executable.' }
    Write-Host '[PASS] Resume exit 33, full retry exit 0, exact output, SHA-256, cache promotion, and extraction.'

    if ($ScintillaArchivePath) {
        $manifestRoot = Split-Path -Parent $PSScriptRoot
        $fullManifest = Get-Content -LiteralPath (Join-Path $manifestRoot 'tools/versions.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        $scintilla = $fullManifest.tools.scintilla
        $actualHash = (Get-FileHash -LiteralPath $ScintillaArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -cne $scintilla.sha256) { throw "Scintilla fixture hash mismatch: $actualHash" }
        $scintillaManifestPath = Join-Path $tempRoot 'scintilla-versions.json'
        [ordered]@{ schemaVersion = 1; tools = [ordered]@{ scintilla = $scintilla } } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $scintillaManifestPath -Encoding UTF8
        $scintillaProject = Join-Path $tempRoot 'scintilla-project'
        $scintillaCache = Join-Path $scintillaProject '.setup/downloads'
        New-Item -ItemType Directory -Path $scintillaCache -Force | Out-Null
        Copy-Item -LiteralPath $ScintillaArchivePath -Destination (Join-Path $scintillaCache 'scintilla-5.6.7-wscite567.zip')
        $scintillaPaths = Invoke-ProjectToolSetup -ManifestPath $scintillaManifestPath -ProjectRoot $scintillaProject -LogPath $logPath
        if (-not (Test-Path -LiteralPath $scintillaPaths.scintilla -PathType Leaf)) { throw 'Verified Scintilla archive did not install its DLL.' }
        Write-Host '[PASS] Actual Scintilla archive SHA, extraction, PE64 header, and embedded version.'
    }

    Write-Host '[PASS] Fixture install, cache reuse, partial-tree recovery, hash mismatch, and failed download.'
} finally {
    if ($tempRoot.StartsWith($tempBoundary,[StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $tempRoot)) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
