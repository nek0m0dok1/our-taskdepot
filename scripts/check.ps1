#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [Parameter(Mandatory)][hashtable]$ToolPaths,
    [string]$LogPath,
    [string]$Configuration = 'Debug',
    [switch]$SkipTidy
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Write-CheckLog([string]$Message,[string]$Level='INFO') {
    $line = '[{0}] [{1}] {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'),$Level,$Message
    Write-Host $line
    if ($LogPath) { Add-Content -LiteralPath $LogPath -Value $line -Encoding UTF8 }
}
function Invoke-Checked {
    param([string]$File,[string[]]$Arguments)
    Write-CheckLog ('Running: {0} {1}' -f $File,($Arguments -join ' '))
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = & $File @Arguments 2>&1; $code = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previousErrorAction }
    foreach ($line in $output) { Write-CheckLog ([string]$line) }
    if ($code -ne 0) { throw "Command failed (exit code $code): $File $($Arguments -join ' ')" }
}
function Get-ToolPath([string]$Name) {
    if (-not $ToolPaths.ContainsKey($Name)) { throw "Missing verified tool path '$Name'." }
    $path = [string]$ToolPaths[$Name]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Verified tool executable is missing: $path" }
    return $path
}

try {
    $ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
    Push-Location $ProjectRoot
    try {
        $manifestPath = Join-Path $ProjectRoot 'tools/versions.json'
        $presetPath = Join-Path $ProjectRoot 'CMakePresets.json'
        foreach ($required in @($manifestPath,$presetPath)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required project file is missing: $required" } }
        $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $presets = Get-Content -LiteralPath $presetPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $selected = @($presets.configurePresets | Where-Object { $_.name -eq 'default' })
        if ($selected.Count -ne 1) { throw "Configure preset 'default' is missing or ambiguous." }
        foreach ($name in @('python','cmake','ctest','ninja','clang-format','clang-tidy','git')) { [void](Get-ToolPath $name) }
        $buildDir = [string]$selected[0].binaryDir
        $buildDir = $buildDir.Replace('${sourceDir}',$ProjectRoot)
        if (-not [IO.Path]::IsPathRooted($buildDir)) { $buildDir = Join-Path $ProjectRoot $buildDir }

        # Validate the complete pinned manifest as JSON before configure/build.
        $json = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($json.schemaVersion -ne 1) { throw 'Unsupported tools/versions.json schemaVersion.' }
        Write-CheckLog 'Manifest JSON validation passed.' 'OK'

        $sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'src'),(Join-Path $ProjectRoot 'tests') -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in @('.c','.h','.cc','.cpp','.hpp') -and $_.FullName -notmatch '[\\/]_deps[\\/]' })
        $formatter = Get-ToolPath 'clang-format'
        if ($sourceFiles.Count -gt 0) {
            $formatArgs = @('--dry-run','--Werror') + @($sourceFiles | ForEach-Object { $_.FullName })
            Invoke-Checked -File $formatter -Arguments $formatArgs
        }
        Write-CheckLog "clang-format validation passed for $($sourceFiles.Count) C/C++ files." 'OK'

        $cmake = Get-ToolPath 'cmake'
        Invoke-Checked -File $cmake -Arguments @('--preset','default')
        Invoke-Checked -File $cmake -Arguments @('--build',$buildDir,'--config',$Configuration,'--parallel')
        $ctest = Get-ToolPath 'ctest'
        Invoke-Checked -File $ctest -Arguments @('--test-dir',$buildDir,'-C',$Configuration,'--output-on-failure','--no-tests=error')

        if (-not $SkipTidy -and $sourceFiles.Count -gt 0) {
            $tidy = Get-ToolPath 'clang-tidy'
            foreach ($source in @($sourceFiles | Where-Object { $_.Extension -in @('.c','.cc','.cpp') })) {
                # clang-tidy findings are advisory; a process/internal failure remains fatal.
                Invoke-Checked -File $tidy -Arguments @('-p',$buildDir,$source.FullName,'-checks=-*,clang-analyzer-*,bugprone-*')
            }
            Write-CheckLog 'clang-tidy completed; diagnostics are advisory.' 'OK'
        }
        Write-CheckLog 'Configure, build, CTest, formatting, JSON, and enabled analysis completed.' 'OK'
    } finally { Pop-Location }
} catch {
    Write-CheckLog $_.Exception.ToString() 'ERROR'
    throw
}
