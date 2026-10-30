#Requires -Version 5.1
[CmdletBinding()]
param([switch]$SkipHostOSCheck)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
$setupDir = Join-Path $root '.setup'
$logDir = Join-Path $setupDir 'logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$logPath = Join-Path $logDir ('setup-{0}-{1}.log' -f (Get-Date -Format 'yyyyMMdd-HHmmss'),$PID)
$script:logFile = $logPath

function Write-Log {
    param([string]$Message,[ValidateSet('INFO','OK','WARN','ERROR')][string]$Level='INFO')
    $line = '[{0}] [{1}] {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'),$Level,$Message
    Write-Host $line
    Add-Content -LiteralPath $script:logFile -Value $line -Encoding UTF8
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
function Invoke-Logged {
    param([Parameter(Mandatory)][string]$File,[string[]]$Arguments=@())
    Write-Log ('Running: {0} {1}' -f $File,($Arguments -join ' '))
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = & $File @Arguments 2>&1; $code = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previousErrorAction }
    foreach ($line in $output) { Write-Log ([string]$line) }
    if ($code -ne 0) { throw "Command failed with exit code ${code}: $File $($Arguments -join ' ')" }
}
function Test-ProjectVenv([string]$PythonPath) {
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = & $PythonPath -m pip --version 2>&1; $code = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previousErrorAction }
    Write-Log "Python venv probe: $($output -join ' ') (exit $code)"
    return ($code -eq 0 -and ([string]($output -join ' ')) -match 'pip\s+[0-9]+')
}
function Assert-WindowsX64 {
    if (-not [Environment]::Is64BitOperatingSystem) { throw '64-bit Windows is required.' }
    $processor = Get-CimInstance Win32_Processor | Select-Object -First 1
    if ([int]$processor.Architecture -ne 9) { throw "AMD64 is required; detected processor architecture $($processor.Architecture)." }
    if (-not $SkipHostOSCheck) {
        $os = Get-CimInstance Win32_OperatingSystem
        if ([int]$os.BuildNumber -lt 22000 -or $os.Caption -notmatch 'Windows 11') { throw 'Windows 11 x64 is required. Use a Windows 11 x64 machine to run setup.cmd.' }
    }
    Write-Log 'Windows x64 environment verified.' 'OK'
}
function Enter-MsvcEnvironment {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'Visual Studio Installer vswhere.exe was not found. Install Visual Studio 2022/2026 Build Tools with the C++ x64/x86 tools and Windows 10/11 SDK, then rerun setup.cmd. The setup does not modify Visual Studio.' }
    $matches = @(& $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath 2>&1 | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    $vsCode = $LASTEXITCODE
    if ($vsCode -ne 0 -or $matches.Count -eq 0) { throw 'No existing Visual Studio installation with Microsoft.VisualStudio.Component.VC.Tools.x86.x64 was found. Install Visual Studio Build Tools and the Windows SDK through Visual Studio Installer, then rerun setup.cmd. The setup will not install or alter Visual Studio.' }
    $selected = [string]$matches[0]
    $devCmd = Join-Path $selected 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $devCmd -PathType Leaf)) { throw "VsDevCmd.bat is missing in '$selected'. Repair that Visual Studio installation using Visual Studio Installer; setup will not modify it." }
    Write-Log "Loading process-local MSVC environment from $devCmd"
    $cmdLine = 'call "' + $devCmd + '" -arch=x64 -host_arch=x64 >nul && set'
    $environment = @(& $env:ComSpec /d /s /c $cmdLine 2>&1)
    $code = $LASTEXITCODE
    if ($code -ne 0) { throw "VsDevCmd.bat failed with exit code $code. Repair the existing Visual Studio installation; setup did not modify it." }
    foreach ($line in $environment) {
        $text = [string]$line
        $separator = $text.IndexOf('=')
        if ($separator -gt 0) { [Environment]::SetEnvironmentVariable($text.Substring(0,$separator),$text.Substring($separator+1),'Process') }
    }
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $cl) { throw 'VsDevCmd.bat completed but cl.exe is unavailable. Repair the existing MSVC C++ x64 tools using Visual Studio Installer.' }
    $compilerVersionText = [Diagnostics.FileVersionInfo]::GetVersionInfo($cl.Source).ProductVersion
    $compilerVersionMatch = [regex]::Match([string]$compilerVersionText,'\d+\.\d+\.\d+(?:\.\d+)?')
    $compilerVersion = $null
    if ($compilerVersionMatch.Success) {
        $fileVersion = [version]$compilerVersionMatch.Value
        if ($fileVersion.Major -eq 14) { $compilerVersion = [version]::new(19,$fileVersion.Minor,$fileVersion.Build,$fileVersion.Revision) }
        else { $compilerVersion = $fileVersion }
    }
    if ($null -eq $compilerVersion -or $compilerVersion -lt [version]'19.38') {
        throw "Unsupported MSVC compiler version '$compilerVersionText'. Use Visual Studio 2022 17.8 or later with MSVC 19.38+, or Visual Studio 2026."
    }
    $sdkValue = ([string]$env:WindowsSDKVersion).TrimEnd('\')
    $sdkMatch = [regex]::Match($sdkValue,'\d+\.\d+\.\d+\.\d+')
    if (-not $sdkMatch.Success -or [version]$sdkMatch.Value -lt [version]'10.0.22000.0') {
        throw "Windows 11 SDK 10.0.22000 or later is required; VsDevCmd selected '$sdkValue'. Install a supported SDK in the existing Visual Studio Installer."
    }
    Write-Log "MSVC compiler $compilerVersion (file version $compilerVersionText): $($cl.Source); Windows SDK $sdkValue" 'OK'
}

try {
    Write-Log '=== Project-local development setup ==='
    Assert-WindowsX64
    Enter-MsvcEnvironment
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $manifest = Join-Path $root 'tools/versions.json'
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { throw 'tools/versions.json is missing.' }
    $versions = Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($versions.schemaVersion -ne 1 -or [string]::IsNullOrWhiteSpace([string]$versions.preCommit.version)) { throw 'Invalid tools/versions.json or missing preCommit.version.' }

    . (Join-Path $PSScriptRoot 'install-tools.ps1')
    $toolPaths = Invoke-ProjectToolSetup -ManifestPath $manifest -ProjectRoot $root -LogPath $logPath
    $processToolDirs = @((Join-Path $root '.tools/git/cmd'),(Join-Path $setupDir 'venv/Scripts'))
    $env:PATH = (($processToolDirs -join ';') + ';' + $env:PATH)

    # The Python distribution and pre-commit environment remain inside this checkout.
    $venvDir = Join-Path $setupDir 'venv'
    $venvPython = Join-Path $venvDir 'Scripts/python.exe'
    $venvReady = (Test-Path -LiteralPath $venvPython -PathType Leaf) -and (Test-ProjectVenv -PythonPath $venvPython)
    if (-not $venvReady) {
        if (Test-Path -LiteralPath $venvDir) {
            Assert-OwnedDirectory -Path $venvDir -Boundary $setupDir -Description 'the project virtual environment'
            Remove-Item -LiteralPath $venvDir -Recurse -Force
        }
        Write-Log 'Creating the project-local Python virtual environment.'
        Invoke-Logged -File $toolPaths['python'] -Arguments @('-m','venv',$venvDir)
    }
    if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) { throw 'venv creation did not produce .setup/venv/Scripts/python.exe.' }
    if (-not (Test-ProjectVenv -PythonPath $venvPython)) { throw 'The project-local Python virtual environment cannot run pip after recovery.' }
    $preCommitVersion = [string]$versions.preCommit.version
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $installed = & $venvPython -m pre_commit --version 2>&1; $preCommitExitCode = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previousErrorAction }
    $installedText = [string]($installed -join ' ').Trim()
    Write-Log "pre-commit version probe: $installedText (exit $preCommitExitCode)"
    $installedMatch = [regex]::Match($installedText,'^pre-commit\s+(\S+)\s*$')
    if ($preCommitExitCode -ne 0 -or -not $installedMatch.Success -or $installedMatch.Groups[1].Value -cne $preCommitVersion) {
        Write-Log "Installing pinned pre-commit $preCommitVersion into .setup/venv."
        Invoke-Logged -File $venvPython -Arguments @('-m','pip','install','--disable-pip-version-check',"pre-commit==$preCommitVersion")
    }
    Invoke-Logged -File $venvPython -Arguments @('-m','pre_commit','install')

    $checker = Join-Path $PSScriptRoot 'check.ps1'
    if (-not (Test-Path -LiteralPath $checker -PathType Leaf)) { throw 'scripts/check.ps1 is missing.' }
    & $checker -ProjectRoot $root -ToolPaths $toolPaths -LogPath $logPath
    Write-Log '=== Setup completed successfully ===' 'OK'
    Write-Log 'Start work with: code . (after setup completes), then create a feature branch.'
    Write-Log "Full log: $logPath"
    exit 0
}
catch {
    Write-Log $_.Exception.ToString() 'ERROR'
    Write-Log "Setup failed. Review the full log: $logPath" 'ERROR'
    exit 1
}
