#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter()][string]$ProjectRoot,
    [switch]$Apply,
    [switch]$QuarantineLegacyTools
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

function Get-FullPath([string]$Path) {
    return [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
}
function Test-Within([string]$Path, [string]$Parent) {
    $p = (Get-FullPath $Path) + [IO.Path]::DirectorySeparatorChar
    $b = (Get-FullPath $Parent) + [IO.Path]::DirectorySeparatorChar
    return $p.StartsWith($b, [StringComparison]::OrdinalIgnoreCase)
}
function Assert-NoReparseInPath([string]$Path, [string]$Root) {
    $full = Get-FullPath $Path
    $rootFull = Get-FullPath $Root
    if ($full -ne $rootFull -and -not (Test-Within $full $rootFull)) { throw "Candidate escapes project root: $full" }
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse point in candidate path: $cursor" }
        }
        $parent = Split-Path -Parent $cursor
        if (-not $parent -or $parent -eq $cursor) { break }
        $cursor = $parent
    }
}
function Assert-ExistingAncestorsAreDirectories([string]$Path, [string]$Root) {
    $full = Get-FullPath $Path
    $rootFull = Get-FullPath $Root
    if ($full -ne $rootFull -and -not (Test-Within $full $rootFull)) { throw "Path escapes project root: $full" }
    $cursor = $full
    while ($true) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (-not $item.PSIsContainer) { throw "Existing path component is not a directory: $cursor" }
        }
        if ($cursor -eq $rootFull) { break }
        $cursor = Split-Path -Parent $cursor
        if (-not $cursor) { throw "Could not resolve parent path for: $full" }
    }
}
function Assert-NoReparseDescendants([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $item = Get-Item -LiteralPath $Path -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Candidate is a reparse point: $Path" }
    if ($item.PSIsContainer) {
        foreach ($child in Get-ChildItem -LiteralPath $Path -Force -Recurse) {
            if (($child.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse point below candidate: $($child.FullName)" }
        }
    }
}
function Test-Tracked([string]$Path, [string]$Root) {
    $relative = $Path.Substring($Root.TrimEnd([IO.Path]::DirectorySeparatorChar).Length).TrimStart([IO.Path]::DirectorySeparatorChar).Replace('\','/')
    $tracked = @(& git -C $Root ls-files -- "$relative" 2>$null)
    if ($LASTEXITCODE -ne 0) { throw 'Unable to check Git tracking status; refusing cleanup.' }
    return $tracked.Count -gt 0
}
function Get-ToolEntry([string]$Name) {
    if ($null -eq $manifest.tools) { return $null }
    if ($manifest.tools.PSObject.Properties.Name -notcontains $Name) { return $null }
    return $manifest.tools.$Name
}
function Invoke-RemoveCandidate {
    [CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
    param([string]$Path, [string]$Reason)
    if ($PSCmdlet.ShouldProcess($Path, "Remove $Reason")) {
        Remove-Item -LiteralPath $Path -Recurse -Force
        Write-Output "REMOVED: $Path"
    }
}
function Invoke-QuarantineCandidate {
    [CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
    param([string]$Source, [string]$Destination, [string]$Reason)
    if ($PSCmdlet.ShouldProcess($Source, "Move $Reason to $Destination")) {
        $destinationParent = Split-Path -Parent $Destination
        if (Test-Path -LiteralPath $Destination) { throw "Quarantine destination already exists: $Destination" }
        Assert-NoReparseInPath $destinationParent $root
        $null = New-Item -ItemType Directory -Path $destinationParent -Force
        Assert-NoReparseInPath $destinationParent $root
        if (Test-Path -LiteralPath $Destination) { throw "Quarantine destination appeared during operation: $Destination" }
        Move-Item -LiteralPath $Source -Destination $Destination
        Write-Output "QUARANTINED: $Source -> $Destination"
    }
}

$root = Get-FullPath $ProjectRoot
if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "Project root does not exist: $root" }
$gitRoot = (& git -C $root rev-parse --show-toplevel 2>$null | Select-Object -First 1)
if (-not [String]::Equals((Get-FullPath $gitRoot), $root, [StringComparison]::OrdinalIgnoreCase)) { throw 'ProjectRoot must be the Git checkout root.' }
$manifestPath = Join-Path $root 'tools/versions.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'tools/versions.json is missing; refusing to infer tool ownership.' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1) { throw 'Unsupported tools/versions.json; refusing cleanup.' }

$candidates = New-Object System.Collections.Generic.List[object]
$skipped = New-Object System.Collections.Generic.List[string]
$downloadRoot = Join-Path $root '.setup/downloads'
$stageRoot = Join-Path $root '.setup/staging'

# Installer cache filenames are <tool>-<version>-<official archive basename>[.partial].
# Only stale CMake/Ninja archives with recognized upstream basenames are eligible.
if (Test-Path -LiteralPath $downloadRoot -PathType Container) {
    foreach ($file in Get-ChildItem -LiteralPath $downloadRoot -Force -File) {
        $match = [regex]::Match($file.Name, '^(cmake|ninja)-([0-9]+\.[0-9]+\.[0-9]+)-(.+?)(\.partial)?$')
        if (-not $match.Success) { $skipped.Add("$($file.FullName) — unrecognized cache name; provenance unclear."); continue }
        $tool = $match.Groups[1].Value
        $version = $match.Groups[2].Value
        $archive = $match.Groups[3].Value
        $allowed = ($tool -eq 'cmake' -and $archive -eq "cmake-$version-windows-x86_64.zip") -or ($tool -eq 'ninja' -and $archive -eq 'ninja-win.zip')
        if (-not $allowed) { $skipped.Add("$($file.FullName) — archive name does not match the pinned upstream pattern."); continue }
        $toolEntry = Get-ToolEntry $tool
        $current = if ($null -eq $toolEntry) { '' } else { [string]$toolEntry.version }
        if ($version -eq $current) { $skipped.Add("$($file.FullName) — current manifest version $current is preserved."); continue }
        $candidates.Add([pscustomobject]@{ Path = $file.FullName; Operation = 'Remove'; Reason = "installer cache for old $tool $version" })
    }
}

# Staging directory names are generated by install-tools.ps1 as tool-version-GUID.
# Current-version stages are kept; unknown names and nested reparse points are skipped.
if (Test-Path -LiteralPath $stageRoot -PathType Container) {
    foreach ($dir in Get-ChildItem -LiteralPath $stageRoot -Force -Directory) {
        $match = [regex]::Match($dir.Name, '^(cmake|ninja)-([0-9]+\.[0-9]+\.[0-9]+)-([0-9a-f]{32})$')
        if (-not $match.Success) { $skipped.Add("$($dir.FullName) — staging provenance unclear."); continue }
        $tool = $match.Groups[1].Value
        $version = $match.Groups[2].Value
        $toolEntry = Get-ToolEntry $tool
        if ($null -ne $toolEntry -and $version -eq [string]$toolEntry.version) { $skipped.Add("$($dir.FullName) — current manifest version is preserved."); continue }
        try { Assert-NoReparseDescendants $dir.FullName }
        catch { $skipped.Add("$($dir.FullName) — $($_.Exception.Message)"); continue }
        $candidates.Add([pscustomobject]@{ Path = $dir.FullName; Operation = 'Remove'; Reason = "installer staging directory for old $tool $version" })
    }
}

# Without a full inventory, legacy installations are never recursively deleted. If their
# manifest entries are absent, an explicit quarantine option can move them intact in-checkout.
$quarantineRoot = Join-Path $root ('.setup/legacy-quarantine/cleanup-' + [guid]::NewGuid().ToString('N'))
foreach ($tool in @('cmake','ninja')) {
    $toolEntry = Get-ToolEntry $tool
    $installPath = if ($null -ne $toolEntry -and -not [string]::IsNullOrWhiteSpace([string]$toolEntry.installPath)) { [string]$toolEntry.installPath } else { $tool }
    $installed = Join-Path $root ('.tools/' + $installPath)
    if (-not (Test-Path -LiteralPath $installed)) { continue }
    if ($null -ne $toolEntry) { $skipped.Add("$installed — preserved: '$tool' remains in the active manifest."); continue }
    if (-not $QuarantineLegacyTools) { $skipped.Add("$installed — legacy tool contents preserved; rerun with -QuarantineLegacyTools to preview an intact in-project move."); continue }
    $destination = Join-Path $quarantineRoot $tool
    $candidates.Add([pscustomobject]@{ Path = $installed; Destination = $destination; Operation = 'Quarantine'; Reason = "legacy .tools/$tool directory (contents retained intact)" })
}

$safeCandidates = New-Object System.Collections.Generic.List[object]
foreach ($entry in $candidates) {
    $candidate = Get-FullPath $entry.Path
    try {
        if (-not (Test-Within $candidate $root)) { throw "Candidate escaped project root: $candidate" }
        Assert-NoReparseInPath $candidate $root
        Assert-NoReparseDescendants $candidate
    } catch {
        $skipped.Add("$candidate — $($_.Exception.Message)")
        continue
    }
    if ($entry.Operation -eq 'Quarantine') {
        $destination = Get-FullPath $entry.Destination
        if (-not (Test-Within $destination $root)) { throw "Quarantine destination escaped project root: $destination" }
        $destinationParent = Split-Path -Parent $destination
        Assert-NoReparseInPath $destinationParent $root
        Assert-ExistingAncestorsAreDirectories $destinationParent $root
        if (Test-Path -LiteralPath $destination) { throw "Quarantine destination collision: $destination" }
    }
    # Git/tool validation errors are fatal, never translated into an intentional safety skip.
    if (Test-Tracked $candidate $root) { $skipped.Add("$candidate — tracked by Git."); continue }
    $safeCandidates.Add($entry)
}
$candidates = $safeCandidates

Write-Output "Project root: $root"
foreach ($entry in $candidates) {
    if ($entry.Operation -eq 'Quarantine') { Write-Output ("CANDIDATE: {0} -> {1} — {2}" -f (Get-FullPath $entry.Path), (Get-FullPath $entry.Destination), $entry.Reason) }
    else { Write-Output ("CANDIDATE: {0} — {1}" -f (Get-FullPath $entry.Path), $entry.Reason) }
}
foreach ($entry in $skipped) { Write-Output "SKIPPED: $entry" }
if (-not $Apply) { Write-Output 'Dry run only. No files were changed. Pass -Apply to opt in to deletion.'; return }

foreach ($entry in $candidates) {
    $candidate = Get-FullPath $entry.Path
    if (-not (Test-Within $candidate $root)) { throw "Candidate escaped project root: $candidate" }
    Assert-NoReparseInPath $candidate $root
    Assert-NoReparseDescendants $candidate
    if ($entry.Operation -eq 'Quarantine') {
        $destination = Get-FullPath $entry.Destination
        if (-not (Test-Within $destination $root)) { throw "Quarantine destination escaped project root: $destination" }
        Assert-NoReparseInPath $candidate $root
        Assert-NoReparseDescendants $candidate
        Assert-NoReparseInPath (Split-Path -Parent $destination) $root
        Invoke-QuarantineCandidate -Source $candidate -Destination $destination -Reason $entry.Reason
    } else {
        Invoke-RemoveCandidate -Path $candidate -Reason $entry.Reason
    }
}
