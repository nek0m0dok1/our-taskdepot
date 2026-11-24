#Requires -Version 5.1
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$scriptUnderTest = Join-Path $PSScriptRoot 'cleanup-legacy-build.ps1'
$scratch = [IO.Path]::GetFullPath((Join-Path $env:TEMP ('our-taskdepot-cleanup-fixture-' + [guid]::NewGuid().ToString('N'))))
$temp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not ($scratch + [IO.Path]::DirectorySeparatorChar).StartsWith($temp, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped the temporary directory.' }
$null = New-Item -ItemType Directory -Path $scratch
function New-Fixture([string]$Name, [bool]$IncludeLegacyTools = $true) {
    $root = Join-Path $scratch $Name
    $null = New-Item -ItemType Directory -Path $root
    $null = New-Item -ItemType Directory -Path (Join-Path $root 'tools')
    $null = New-Item -ItemType Directory -Path (Join-Path $root '.setup/downloads') -Force
    $null = New-Item -ItemType Directory -Path (Join-Path $root '.setup/staging') -Force
    if ($IncludeLegacyTools) { $manifest = '{"schemaVersion":1,"tools":{"cmake":{"version":"3.31.8","installPath":"cmake"},"ninja":{"version":"1.13.1","installPath":"ninja"}}}' }
    else { $manifest = '{"schemaVersion":1,"tools":{"git":{"version":"2.50.1","installPath":"git"}}}' }
    Set-Content -LiteralPath (Join-Path $root 'tools/versions.json') -Value $manifest -Encoding UTF8
    & git -C $root init --quiet
    & git -C $root config user.email fixture@example.invalid
    & git -C $root config user.name fixture
    & git -C $root add tools/versions.json
    & git -C $root commit --quiet -m fixture
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize fixture Git repository.' }
    return $root
}
function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
try {
    # Dry run, WhatIf, deletion opt-in, unknown content, and idempotence.
    $root = New-Fixture 'normal'
    $archive = Join-Path $root '.setup/downloads/cmake-3.30.0-cmake-3.30.0-windows-x86_64.zip'
    $partial = Join-Path $root '.setup/downloads/ninja-1.12.0-ninja-win.zip.partial'
    $unknown = Join-Path $root '.setup/downloads/my-cmake-backup.zip'
    $oldStage = Join-Path $root '.setup/staging/ninja-1.12.0-0123456789abcdef0123456789abcdef'
    Set-Content -LiteralPath $archive -Value old
    Set-Content -LiteralPath $partial -Value partial
    Set-Content -LiteralPath $unknown -Value keep
    $null = New-Item -ItemType Directory -Path $oldStage
    Set-Content -LiteralPath (Join-Path $oldStage 'ninja.exe') -Value fixture
    $report = & $scriptUnderTest -ProjectRoot $root 2>&1 | Out-String
    Assert ($report.Contains('CANDIDATE:') -and $report.Contains('Dry run only')) 'Default run did not report candidates as a dry run.'
    Assert (Test-Path -LiteralPath $archive) 'Dry run changed candidate files.'
    $null = & $scriptUnderTest -ProjectRoot $root -Apply -WhatIf 2>&1
    Assert (Test-Path -LiteralPath $archive) '-WhatIf removed a candidate.'
    $null = & $scriptUnderTest -ProjectRoot $root -Apply
    Assert (-not (Test-Path -LiteralPath $archive) -and -not (Test-Path -LiteralPath $partial) -and -not (Test-Path -LiteralPath $oldStage)) '-Apply did not remove recognized stale artifacts.'
    Assert (Test-Path -LiteralPath $unknown) 'Unknown cache content was removed.'
    $repeatReport = & $scriptUnderTest -ProjectRoot $root -Apply | Out-String
    Assert (-not $repeatReport.Contains('CANDIDATE:') -and (Test-Path -LiteralPath $unknown)) 'Repeated run was not idempotent or changed unknown content.'

    # Current tools, tracked content, and CMake user presets remain untouched.
    $root2 = New-Fixture 'preserve'
    $null = New-Item -ItemType Directory -Path (Join-Path $root2 '.tools/cmake') -Force
    $null = New-Item -ItemType Directory -Path (Join-Path $root2 '.tools/ninja') -Force
    Set-Content -LiteralPath (Join-Path $root2 '.tools/cmake/user.txt') -Value keep
    Set-Content -LiteralPath (Join-Path $root2 'CMakeUserPresets.json') -Value '{}'
    $tracked = Join-Path $root2 '.setup/downloads/cmake-3.30.0-cmake-3.30.0-windows-x86_64.zip'
    Set-Content -LiteralPath $tracked -Value keep
    & git -C $root2 add .setup/downloads/cmake-3.30.0-cmake-3.30.0-windows-x86_64.zip CMakeUserPresets.json
    & git -C $root2 commit --quiet -m tracked
    $null = & $scriptUnderTest -ProjectRoot $root2 -Apply
    Assert (Test-Path -LiteralPath (Join-Path $root2 '.tools/cmake/user.txt')) 'Project tool content was removed.'
    Assert (Test-Path -LiteralPath (Join-Path $root2 'CMakeUserPresets.json')) 'CMakeUserPresets.json was removed.'
    Assert (Test-Path -LiteralPath $tracked) 'Tracked content was removed.'

    # Removed manifest entries allow only an explicit, reversible quarantine of legacy installs.
    $root4 = New-Fixture 'removed-manifest' $false
    $legacyCmake = Join-Path $root4 '.tools/cmake'
    $legacyNinja = Join-Path $root4 '.tools/ninja'
    $null = New-Item -ItemType Directory -Path $legacyCmake,$legacyNinja -Force
    Set-Content -LiteralPath (Join-Path $legacyCmake 'user-added.txt') -Value 'keep this'
    Set-Content -LiteralPath (Join-Path $legacyNinja 'ninja.exe') -Value 'fixture bytes'
    $withoutOption = & $scriptUnderTest -ProjectRoot $root4 | Out-String
    Assert ($withoutOption.Contains('-QuarantineLegacyTools')) 'Removed manifest keys were not handled safely.'
    Assert (Test-Path -LiteralPath (Join-Path $legacyCmake 'user-added.txt')) 'Legacy tool content changed without opt-in.'
    $quarantinePreview = & $scriptUnderTest -ProjectRoot $root4 -QuarantineLegacyTools | Out-String
    Assert ($quarantinePreview.Contains('.tools\cmake') -and $quarantinePreview.Contains('legacy-quarantine')) 'Quarantine preview did not show absolute source and destination.'
    $null = & $scriptUnderTest -ProjectRoot $root4 -Apply -QuarantineLegacyTools -WhatIf 2>&1
    Assert (Test-Path -LiteralPath (Join-Path $legacyCmake 'user-added.txt')) 'Quarantine -WhatIf moved user content.'
    $quarantineResult = & $scriptUnderTest -ProjectRoot $root4 -Apply -QuarantineLegacyTools | Out-String
    Assert (-not (Test-Path -LiteralPath $legacyCmake) -and -not (Test-Path -LiteralPath $legacyNinja)) 'Quarantine did not move legacy tool directories.'
    $quarantineRootPath = Join-Path $root4 '.setup/legacy-quarantine'
    $savedCmake = Get-ChildItem -LiteralPath $quarantineRootPath -Directory -Recurse | Where-Object { $_.Name -eq 'cmake' } | Select-Object -First 1
    $savedNinja = Get-ChildItem -LiteralPath $quarantineRootPath -Directory -Recurse | Where-Object { $_.Name -eq 'ninja' } | Select-Object -First 1
    Assert ($null -ne $savedCmake -and (Get-Content -LiteralPath (Join-Path $savedCmake.FullName 'user-added.txt') -Raw).Trim() -eq 'keep this') 'Quarantine did not retain CMake user content.'
    Assert ($null -ne $savedNinja -and (Get-Content -LiteralPath (Join-Path $savedNinja.FullName 'ninja.exe') -Raw).Trim() -eq 'fixture bytes') 'Quarantine did not retain Ninja content.'
    $again = & $scriptUnderTest -ProjectRoot $root4 -Apply -QuarantineLegacyTools | Out-String
    Assert (-not $again.Contains('QUARANTINED:')) 'Repeated quarantine moved already-quarantined content again.'

    # A junction in a candidate ancestor blocks deletion and leaves its target intact.
    $root3 = New-Fixture 'junction'
    $outside = Join-Path $scratch 'outside'
    $null = New-Item -ItemType Directory -Path $outside
    $externalFile = Join-Path $outside 'cmake-3.30.0-cmake-3.30.0-windows-x86_64.zip'
    Set-Content -LiteralPath $externalFile -Value keep
    Remove-Item -LiteralPath (Join-Path $root3 '.setup/downloads') -Force
    New-Item -ItemType Junction -Path (Join-Path $root3 '.setup/downloads') -Target $outside | Out-Null
    $junctionReport = & $scriptUnderTest -ProjectRoot $root3 -Apply 2>&1 | Out-String
    Assert (Test-Path -LiteralPath $externalFile) 'Junction target was deleted.'
    Assert ($junctionReport.Contains('Reparse point')) 'Junction ancestor was not reported and skipped.'

    # Git tracking-query failures are fatal during the full preflight, before any candidate is removed.
    $root5 = New-Fixture 'git-query-failure'
    $failureCmake = Join-Path $root5 '.setup/downloads/cmake-3.30.0-cmake-3.30.0-windows-x86_64.zip'
    $failureNinja = Join-Path $root5 '.setup/downloads/ninja-1.12.0-ninja-win.zip'
    Set-Content -LiteralPath $failureCmake -Value cmake
    Set-Content -LiteralPath $failureNinja -Value ninja
    $realGitExe = (Get-Command git -CommandType Application).Source
    $gitWrapperDir = Join-Path $scratch 'git-wrapper'
    $null = New-Item -ItemType Directory -Path $gitWrapperDir
    $gitWrapper = Join-Path $gitWrapperDir 'git.cmd'
    @('@echo off','setlocal','for %%A in (%*) do if /I "%%~A"=="ls-files" exit /b 128',('"{0}" %*' -f $realGitExe),'exit /b %ERRORLEVEL%') | Set-Content -LiteralPath $gitWrapper -Encoding Ascii
    $originalPath = $env:PATH
    $env:PATH = $gitWrapperDir + ';' + $originalPath
    $gitFailureCaught = $false
    try { $null = & $scriptUnderTest -ProjectRoot $root5 -Apply }
    catch { $gitFailureCaught = $_.Exception.Message -match 'Unable to check Git tracking status' }
    finally { $env:PATH = $originalPath }
    Assert $gitFailureCaught 'Git ls-files failure was not propagated as a fatal preflight error.'
    Assert ((Test-Path -LiteralPath $failureCmake) -and (Test-Path -LiteralPath $failureNinja)) 'A candidate was deleted before Git preflight completed.'

    # A blocked quarantine parent is rejected during global preflight before stale archives are removed.
    $root6 = New-Fixture 'quarantine-parent-file' $false
    $blockedArchive = Join-Path $root6 '.setup/downloads/cmake-3.30.0-cmake-3.30.0-windows-x86_64.zip'
    $blockedTool = Join-Path $root6 '.tools/cmake'
    $blockedContent = Join-Path $blockedTool 'user-added.txt'
    $quarantineBlocker = Join-Path $root6 '.setup/legacy-quarantine'
    Set-Content -LiteralPath $blockedArchive -Value archive
    $null = New-Item -ItemType Directory -Path $blockedTool -Force
    Set-Content -LiteralPath $blockedContent -Value preserve
    Set-Content -LiteralPath $quarantineBlocker -Value 'blocking file'
    $quarantineFailureCaught = $false
    try { $null = & $scriptUnderTest -ProjectRoot $root6 -Apply -QuarantineLegacyTools }
    catch { $quarantineFailureCaught = $_.Exception.Message -match 'not a directory' }
    Assert $quarantineFailureCaught 'A file blocking the quarantine parent was not rejected during preflight.'
    Assert ((Test-Path -LiteralPath $blockedArchive) -and (Test-Path -LiteralPath $blockedContent) -and (Test-Path -LiteralPath $quarantineBlocker -PathType Leaf)) 'A preflight failure mutated an archive, legacy tool, or blocker.'

    # Project root boundary rejects a non-root path.
    $caught = $false
    try { $null = & $scriptUnderTest -ProjectRoot $outside -Apply 2>&1 } catch { $caught = $true }
    Assert $caught 'A non-project directory was accepted as ProjectRoot.'
    Write-Output 'PASS: dry-run, -WhatIf, -Apply, boundary, junction, tracked/user preservation, removed manifest keys, quarantine, Git and destination preflight failure atomicity, and idempotence.'
}
finally {
    Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
}
