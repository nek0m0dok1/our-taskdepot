#Requires -Version 5.1
Set-StrictMode -Version Latest

function Invoke-ProjectToolSetup {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ManifestPath,
        [Parameter(Mandatory)][string]$ProjectRoot,
        [string]$LogPath
    )

    $ProgressPreference = 'SilentlyContinue'
    $ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
    $ManifestPath = (Resolve-Path -LiteralPath $ManifestPath).Path
    $manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 1) { throw 'Unsupported tools/versions.json schemaVersion.' }
    $toolRoot = Join-Path $ProjectRoot '.tools'
    $cacheRoot = Join-Path $ProjectRoot '.setup\downloads'
    $stageRoot = Join-Path $ProjectRoot '.setup\staging'
    New-Item -ItemType Directory -Path $toolRoot,$cacheRoot,$stageRoot -Force | Out-Null
    foreach ($directory in @($toolRoot,$cacheRoot,$stageRoot)) {
        $directoryInfo = Get-Item -LiteralPath $directory -Force
        if (($directoryInfo.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to use linked provisioning directory '$directory'." }
    }

    function Write-ToolLog([string]$Message,[string]$Level='INFO') {
        $line = '[{0}] [{1}] {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'),$Level,$Message
        Write-Host $line
        if ($LogPath) { Add-Content -LiteralPath $LogPath -Value $line -Encoding UTF8 }
    }
    function Assert-OwnedDirectory([string]$Path,[string]$Boundary,[string]$Description) {
        $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar)
        $fullBoundary = [IO.Path]::GetFullPath($Boundary).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        $boundaryRoot = $fullBoundary.TrimEnd([IO.Path]::DirectorySeparatorChar)
        if (-not $fullPath.StartsWith($fullBoundary,[StringComparison]::OrdinalIgnoreCase)) { throw "Refusing to remove $Description outside '$Boundary': '$Path'." }
        $current = $fullPath
        while ($current -eq $boundaryRoot -or $current.StartsWith($fullBoundary,[StringComparison]::OrdinalIgnoreCase)) {
            if (Test-Path -LiteralPath $current) {
                $item = Get-Item -LiteralPath $current -Force
                if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to remove $Description through linked path '$current'." }
            }
            if ($current -eq $boundaryRoot) { break }
            $current = Split-Path -Parent $current
        }
    }
    function Assert-Entry([string]$Name,$Entry) {
        foreach ($field in @('version','url','sha256','provenance','archiveType','installPath','executables')) {
            if ($null -eq $Entry.$field -or [string]::IsNullOrWhiteSpace([string]$Entry.$field)) { throw "Tool '$Name' has no '$field' in tools/versions.json." }
        }
        if ([string]$Entry.url -notmatch '^https://') { throw "Tool '$Name' must use an HTTPS URL." }
        if ([string]$Entry.sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw "Invalid SHA-256 for '$Name'." }
        if ([string]$Entry.version -notmatch '^[0-9A-Za-z][0-9A-Za-z._+-]*$') { throw "Invalid version for '$Name'." }
        if ([string]$Entry.archiveType -notin @('zip','wheel','tar.gz','tar.xz','exe')) { throw "Unsupported archiveType '$($Entry.archiveType)' for '$Name'." }
        if ([string]$Entry.installPath -notmatch '^[A-Za-z0-9][A-Za-z0-9._\\/-]*$' -or [string]$Entry.installPath -match '(^|[\\/])\.\.([\\/]|$)') { throw "Unsafe installPath for '$Name'." }
        if ($Entry.executables -isnot [System.Management.Automation.PSCustomObject] -and $Entry.executables -isnot [System.Collections.IDictionary]) { throw "Tool '$Name' must declare executable paths." }
        if (@($Entry.executables.PSObject.Properties).Count -eq 0) { throw "Tool '$Name' has no executable paths." }
    }
    function Get-VerifiedArtifact([string]$Name,$Entry) {
        $uri = [uri][string]$Entry.url
        $fileName = [IO.Path]::GetFileName($uri.AbsolutePath)
        if (-not $fileName) { throw "Tool '$Name' URL has no artifact filename." }
        $cacheBoundary = [IO.Path]::GetFullPath($cacheRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        $path = [IO.Path]::GetFullPath((Join-Path $cacheRoot ("{0}-{1}-{2}" -f $Name,$Entry.version,$fileName)))
        if (-not $path.StartsWith($cacheBoundary,[StringComparison]::OrdinalIgnoreCase)) { throw "Cache path for '$Name' escapes .setup/downloads." }
        $partial = "$path.partial"
        if (-not ([IO.Path]::GetFullPath($partial)).StartsWith($cacheBoundary,[StringComparison]::OrdinalIgnoreCase)) { throw "Partial cache path for '$Name' escapes .setup/downloads." }
        $expected = ([string]$Entry.sha256).ToUpperInvariant()
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $cacheItem = Get-Item -LiteralPath $path -Force
            if (($cacheItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to hash linked cache file '$path'." }
            # Recheck the cache on every invocation; never trust a previous run's verification.
            $cached = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            if ($cached -ne $expected) { Write-ToolLog "Discarding corrupted cache for $Name." 'WARN'; Remove-Item -LiteralPath $path -Force }
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            Write-ToolLog "Downloading $Name $($Entry.version) from $($Entry.url)."
            $hadPartial = Test-Path -LiteralPath $partial
            if ($hadPartial) {
                $partialItem = Get-Item -LiteralPath $partial -Force
                if (($partialItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to replace linked partial cache file '$partial'." }
            }
            $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
            if ($curl) {
                $curlArgs = @('--disable','--fail','--location','--proto','=https','--proto-redir','=https','--silent','--show-error','--retry','5','--retry-all-errors','--retry-delay','2','--connect-timeout','30')
                if ($hadPartial) { $curlArgs += @('--continue-at','-') }
                $curlArgs += @('--output',$partial,'--write-out',"HTTP %{http_code} bytes=%{size_download}",$uri.AbsoluteUri)
                $previousErrorAction = $ErrorActionPreference
                $ErrorActionPreference = 'Continue'
                try { $downloadOutput = & $curl.Source @curlArgs 2>&1; $downloadCode = $LASTEXITCODE }
                finally { $ErrorActionPreference = $previousErrorAction }
                foreach ($line in $downloadOutput) { Write-ToolLog ([string]$line) }
                if ($downloadCode -ne 0 -and $hadPartial) {
                    Write-ToolLog "Resuming the partial $Name download failed (exit $downloadCode); retrying once from the beginning." 'WARN'
                    Remove-Item -LiteralPath $partial -Force -ErrorAction SilentlyContinue
                    $curlArgs = @('--disable','--fail','--location','--proto','=https','--proto-redir','=https','--silent','--show-error','--retry','5','--retry-all-errors','--retry-delay','2','--connect-timeout','30','--output',$partial,'--write-out',"HTTP %{http_code} bytes=%{size_download}",$uri.AbsoluteUri)
                    $previousErrorAction = $ErrorActionPreference
                    $ErrorActionPreference = 'Continue'
                    try { $downloadOutput = & $curl.Source @curlArgs 2>&1; $downloadCode = $LASTEXITCODE }
                    finally { $ErrorActionPreference = $previousErrorAction }
                    foreach ($line in $downloadOutput) { Write-ToolLog ([string]$line) }
                }
                if ($downloadCode -ne 0) { throw "Download for '$Name' failed with curl exit code $downloadCode." }
            } else {
                Write-ToolLog 'curl.exe is unavailable; using Windows PowerShell Invoke-WebRequest.' 'WARN'
                Invoke-WebRequest -Uri $uri.AbsoluteUri -OutFile $partial -UseBasicParsing
            }
            $actual = (Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash
            if ($actual -ne $expected) {
                Remove-Item -LiteralPath $partial -Force -ErrorAction SilentlyContinue
                throw "SHA-256 mismatch for '$Name': expected $expected, received $actual."
            }
            Move-Item -LiteralPath $partial -Destination $path -Force
        }
        # A second check protects against replacement between download and use.
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($actual -ne $expected) { throw "SHA-256 mismatch for '$Name': expected $expected, received $actual." }
        Write-ToolLog "$Name artifact SHA-256 verified (provenance: $($Entry.provenance))." 'OK'
        return $path
    }
    function Test-InstalledTool([string]$Name,$Entry,[string]$Directory) {
        foreach ($property in $Entry.executables.PSObject.Properties) {
            if ([string]$property.Value -notmatch '^[A-Za-z0-9][A-Za-z0-9._\\/-]*$' -or [string]$property.Value -match '(^|[\\/])\.\.([\\/]|$)') { throw "Unsafe executable path for '$Name'." }
            $candidate = Join-Path $Directory ([string]$property.Value)
            if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $false }
            $currentPath = [IO.Path]::GetFullPath($candidate)
            $directoryPath = [IO.Path]::GetFullPath($Directory)
            while ($currentPath.StartsWith($directoryPath,[StringComparison]::OrdinalIgnoreCase)) {
                $pathItem = Get-Item -LiteralPath $currentPath -Force
                if (($pathItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to execute linked installed file '$currentPath'." }
                if ($currentPath -eq $directoryPath) { break }
                $currentPath = Split-Path -Parent $currentPath
            }
            $validation = ''
            if ($Entry.PSObject.Properties.Name -contains 'fileValidation') { $validation = [string]$Entry.fileValidation.$($property.Name) }
            if ($validation -eq 'pe64-version') {
                $stream = [IO.File]::OpenRead($candidate)
                try {
                    $reader = [IO.BinaryReader]::new($stream)
                    if ($reader.ReadUInt16() -ne 0x5a4d) { return $false }
                    $stream.Position = 0x3c
                    $peOffset = $reader.ReadInt32()
                    if ($peOffset -lt 64 -or $peOffset -gt ($stream.Length - 6)) { return $false }
                    $stream.Position = $peOffset
                    if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) { return $false }
                } finally { $stream.Dispose() }
                $fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($candidate).FileVersion
                $expectedFileVersion = '^(?:' + [regex]::Escape([string]$Entry.version) + ')(?:\.0)?$'
                if ([string]::IsNullOrWhiteSpace($fileVersion) -or $fileVersion -notmatch $expectedFileVersion) {
                    Write-ToolLog "$Name file version '$fileVersion' does not match $($Entry.version)." 'WARN'
                    return $false
                }
                continue
            }
            if ($validation) { throw "Unsupported file validation '$validation' for '$Name'." }
            $versionArgs = @('--version')
            if ($Entry.PSObject.Properties.Name -contains 'versionArguments' -and $Entry.versionArguments.PSObject.Properties[$property.Name]) { $versionArgs = @($Entry.versionArguments.$($property.Name)) }
            $previousErrorAction = $ErrorActionPreference
            $ErrorActionPreference = 'Continue'
            try { $output = & $candidate @versionArgs 2>&1; $code = $LASTEXITCODE }
            finally { $ErrorActionPreference = $previousErrorAction }
            $joined = [string]($output -join ' ')
            $version = [string]$Entry.version
            $versionPattern = '(?<![A-Za-z0-9.+-])' + [regex]::Escape($version) + '(?![A-Za-z0-9.+-])'
            if ($Entry.PSObject.Properties.Name -contains 'versionPattern' -and $Entry.versionPattern.PSObject.Properties[$property.Name]) { $versionPattern = [string]$Entry.versionPattern.$($property.Name) }
            if ($code -ne 0 -or $joined -notmatch $versionPattern) {
                Write-ToolLog "$Name validation failed for '$($property.Name)': exit=$code output=$joined" 'WARN'
                return $false
            }
            Write-ToolLog "$Name $($property.Name) reported: $joined" 'OK'
        }
        return $true
    }
    function Expand-Artifact([string]$Name,$Entry,[string]$Artifact,[string]$Destination) {
        if ($Entry.archiveType -eq 'exe') {
            $target = Join-Path $ProjectRoot $Destination
            New-Item -ItemType Directory -Path $target -Force | Out-Null
            # The Python.org installer supports a private TargetDir and does not touch PATH or a
            # machine-wide installation. Per-user registration for this project copy is avoided
            # by using the installer's embeddable format instead; installer manifests must opt in
            # only when they explicitly declare a safe localInstallArguments array.
            if ($Entry.PSObject.Properties.Name -notcontains 'localInstallArguments' -or -not $Entry.localInstallArguments) { throw "Executable installer '$Name' requires explicit localInstallArguments; refusing to run it." }
            $args = @($Entry.localInstallArguments | ForEach-Object { ([string]$_).Replace('{target}',$target) })
            Write-ToolLog "Running verified local installer for ${Name}: $Artifact $($args -join ' ')"
            $proc = Start-Process -FilePath $Artifact -ArgumentList $args -Wait -PassThru -NoNewWindow
            if ($proc.ExitCode -ne 0) { throw "Installer for '$Name' failed with exit code $($proc.ExitCode)." }
            return
        }
        if ($Entry.archiveType -in @('zip','wheel')) {
            Add-Type -AssemblyName System.IO.Compression
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $root = [IO.Path]::GetFullPath($Destination).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
            $archive = [IO.Compression.ZipFile]::OpenRead($Artifact)
            try {
                foreach ($zipEntry in $archive.Entries) {
                    if ($zipEntry.FullName -match '(^[/\\]|(^|[/\\])\.\.([/\\]|$))') { throw "Unsafe archive entry '$($zipEntry.FullName)' in '$Name'." }
                    $target = [IO.Path]::GetFullPath((Join-Path $Destination $zipEntry.FullName))
                    if (-not $target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw "Archive entry escapes extraction directory: '$($zipEntry.FullName)'." }
                    if ($zipEntry.FullName.EndsWith('/')) { New-Item -ItemType Directory -Path $target -Force | Out-Null; continue }
                    $parent = Split-Path -Parent $target
                    New-Item -ItemType Directory -Path $parent -Force | Out-Null
                    $inputStream = $zipEntry.Open()
                    try {
                        $outputStream = [IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
                        try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose() }
                    } finally { $inputStream.Dispose() }
                }
            } finally { $archive.Dispose() }
            return
        }
        $tarArgs = if ($Entry.archiveType -eq 'tar.gz') { @('-xzf',$Artifact,'-C',$Destination) } else { @('-xJf',$Artifact,'-C',$Destination) }
        $previousErrorAction = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try { $output = & tar.exe @tarArgs 2>&1; $code = $LASTEXITCODE }
        finally { $ErrorActionPreference = $previousErrorAction }
        foreach ($line in $output) { Write-ToolLog ([string]$line) }
        if ($code -ne 0) { throw "Extraction for '$Name' failed with exit code $code." }
    }

    $paths = @{}
    foreach ($name in $manifest.tools.PSObject.Properties.Name) {
        $entry = $manifest.tools.$name
        Assert-Entry -Name $name -Entry $entry
        if ($name -notmatch '^[A-Za-z0-9_-]+$') { throw "Unsafe tool name '$name'." }
        $installDir = [IO.Path]::GetFullPath((Join-Path $toolRoot ([string]$entry.installPath)))
        $toolBoundary = [IO.Path]::GetFullPath($toolRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        if (-not $installDir.StartsWith($toolBoundary,[StringComparison]::OrdinalIgnoreCase)) { throw "Install path for '$name' escapes .tools." }
        $stageBoundary = [IO.Path]::GetFullPath($stageRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        $artifact = Get-VerifiedArtifact -Name $name -Entry $entry
        if (Test-Path -LiteralPath $installDir) {
            $installedDirectory = Get-Item -LiteralPath $installDir -Force
            if (($installedDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to inspect linked install path '$installDir'." }
        }
        if (Test-InstalledTool -Name $name -Entry $entry -Directory $installDir) {
            Write-ToolLog "$name $($entry.version) already installed and verified." 'OK'
        } else {
            # Validate archive integrity even when an installed tree exists. Existing trees are
            # reusable only when both their executable versions and the pinned cached artifact pass.
            $stage = Join-Path $stageRoot ("{0}-{1}-{2}" -f $name,$entry.version,[guid]::NewGuid().ToString('N'))
            if (-not ([IO.Path]::GetFullPath($stage)).StartsWith($stageBoundary,[StringComparison]::OrdinalIgnoreCase)) { throw "Staging path for '$name' escapes .setup/staging." }
            New-Item -ItemType Directory -Path $stage -Force | Out-Null
            try {
                Expand-Artifact -Name $name -Entry $entry -Artifact $artifact -Destination $stage
                $source = $stage
                if ($entry.stripTopLevel) {
                    $subdirs = @(Get-ChildItem -LiteralPath $stage -Directory -Force)
                    if ($subdirs.Count -ne 1) { throw "Expected one top-level folder for '$name', found $($subdirs.Count)." }
                    $source = $subdirs[0].FullName
                }
                $validationRoot = $source
                if ($source -ne $stage) {
                    # Keep requested canonical subdirectories (e.g. wscite/SciLexer.dll)
                    # when the archive contains a single wrapper directory.
                    $validationRoot = $source
                }
                if (-not (Test-InstalledTool -Name $name -Entry $entry -Directory $validationRoot)) { throw "Extracted '$name' failed executable/version validation; expected $($entry.version)." }
                if (Test-Path -LiteralPath $installDir) {
                    $existing = Get-Item -LiteralPath $installDir -Force
                    if (($existing.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing to replace linked install path '$installDir'." }
                    Assert-OwnedDirectory -Path $installDir -Boundary $toolRoot -Description "installed tool '$name'"
                    Remove-Item -LiteralPath $installDir -Recurse -Force
                }
                $parent = Split-Path -Parent $installDir
                New-Item -ItemType Directory -Path $parent -Force | Out-Null
                if ($source -eq $stage) { Move-Item -LiteralPath $source -Destination $installDir }
                else {
                    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
                    Get-ChildItem -LiteralPath $source -Force | Move-Item -Destination $installDir -Force
                }
                if (-not (Test-InstalledTool -Name $name -Entry $entry -Directory $installDir)) { throw "Installed '$name' failed final version validation." }
            } finally {
                if (Test-Path -LiteralPath $stage) {
                    Assert-OwnedDirectory -Path $stage -Boundary $stageRoot -Description "staging data for '$name'"
                    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
                }
            }
        }
        foreach ($property in $entry.executables.PSObject.Properties) {
            $paths[$property.Name] = Join-Path $installDir ([string]$property.Value)
        }
    }
    return $paths
}
