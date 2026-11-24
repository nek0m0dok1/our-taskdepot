#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$ProjectRoot,
    [hashtable]$ToolPaths = @{},
    [hashtable]$VisualStudioToolchain,
    [string]$LogPath,
    [string]$Configuration = 'Debug',
    [switch]$SkipTidy
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'visual-studio-2026.ps1')
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
    $path = $null
    if ($ToolPaths.ContainsKey($Name)) { $path = [string]$ToolPaths[$Name] }
    if (-not $path) {
        $fileProperties = @{ python=@('python','python'); git=@('git','git'); 'clang-format'=@('clangFormat','clang-format'); 'clang-tidy'=@('clangTidy','clang-tidy') }
        if (-not $fileProperties.ContainsKey($Name)) { throw "Unknown project tool '$Name'." }
        $property = $fileProperties[$Name][0]
        $entry = $manifest.tools.$property
        if (-not $entry) { throw "No manifest entry for project tool '$Name'." }
        $relativePath = [string]$entry.executables.($fileProperties[$Name][1])
        if (-not $relativePath) { throw "No executable is defined for project tool '$Name'." }
        $path = Join-Path (Join-Path $ProjectRoot ('.tools/' + $entry.installPath)) $relativePath
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Verified tool executable is missing: $path" }
    return [IO.Path]::GetFullPath($path)
}

try {
    if (-not $ProjectRoot) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }
    $ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
    Push-Location $ProjectRoot
    try {
        $manifestPath = Join-Path $ProjectRoot 'tools/versions.json'
        $presetPath = Join-Path $ProjectRoot 'CMakePresets.json'
        foreach ($required in @($manifestPath,$presetPath)) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required project file is missing: $required" } }
        $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if (-not $VisualStudioToolchain) { $VisualStudioToolchain = Get-VisualStudio2026Toolchain }
        foreach ($property in @('Instance','Cl','CMake','CTest')) {
            if (-not $VisualStudioToolchain[$property] -or -not [IO.Path]::IsPathRooted([string]$VisualStudioToolchain[$property])) { throw "Selected VS2026 toolchain is missing absolute path '$property'." }
            $pathType = if ($property -eq 'Instance') { 'Container' } else { 'Leaf' }
            if (-not (Test-Path -LiteralPath $VisualStudioToolchain[$property] -PathType $pathType)) { throw "Selected VS2026 $property path is not an existing ${pathType}: $($VisualStudioToolchain[$property])" }
        }
        Write-CheckLog "VS2026 instance: $($VisualStudioToolchain.Instance)"
        Write-CheckLog "MSVC compiler: $($VisualStudioToolchain.Cl)"
        Write-CheckLog "CMake: $($VisualStudioToolchain.CMake)"
        Write-CheckLog "CTest: $($VisualStudioToolchain.CTest)"
        $presets = Get-Content -LiteralPath $presetPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $selected = @($presets.configurePresets | Where-Object { $_.name -eq 'default' })
        if ($selected.Count -ne 1) { throw "Configure preset 'default' is missing or ambiguous." }
        foreach ($name in @('python','clang-format','clang-tidy','git')) { [void](Get-ToolPath $name) }
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

        $cmake = [string]$VisualStudioToolchain.CMake
        $ctest = [string]$VisualStudioToolchain.CTest
        $cachePath = Join-Path $buildDir 'CMakeCache.txt'
        if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
            $cache = Get-Content -LiteralPath $cachePath
            $expectedInstance = ([string]$VisualStudioToolchain.Instance).Replace('/','\').TrimEnd('\')
            foreach ($expectation in @(
                @{ pattern='^CMAKE_GENERATOR:INTERNAL=(.*)$'; expected='Visual Studio 18 2026'; label='generator' },
                @{ pattern='^CMAKE_GENERATOR_INSTANCE:[^=]+=(.*)$'; expected=$expectedInstance; label='Visual Studio instance' },
                @{ pattern='^CMAKE_GENERATOR_PLATFORM:INTERNAL=(.*)$'; expected='x64'; label='platform' },
                @{ pattern='^CMAKE_GENERATOR_TOOLSET:INTERNAL=(.*)$'; expected='v145'; label='toolset' }
            )) {
                $line = $cache | Where-Object { $_ -match $expectation.pattern } | Select-Object -First 1
                $actual = if ($line -match $expectation.pattern) { $Matches[1].Replace('/','\').TrimEnd('\') } else { '' }
                if (-not [string]::Equals($actual,$expectation.expected,[StringComparison]::OrdinalIgnoreCase)) {
                    throw "Existing build cache has a mismatched $($expectation.label) ('$actual'). It was not changed. Review '$buildDir', then move that directory aside yourself or run .\tools\clean-build.ps1 to preserve it as build-old-<timestamp> before retrying setup."
                }
            }
        }
        Invoke-Checked -File $cmake -Arguments @('--preset','default',"-DCMAKE_GENERATOR_INSTANCE=$($VisualStudioToolchain.Instance)")
        # The Visual Studio generator records the compiler in CMakeFiles, outside CMakeCache.txt.
        $compilerInfoPath = Join-Path $buildDir "CMakeFiles/$($VisualStudioToolchain.CMakeVersion)/CMakeCCompiler.cmake"
        if (-not (Test-Path -LiteralPath $compilerInfoPath -PathType Leaf)) {
            throw "CMake did not write compiler information to '$compilerInfoPath'."
        }
        $compilerInfo = Select-String -LiteralPath $compilerInfoPath -Pattern '^set\(CMAKE_C_COMPILER "([^"]+)"\)$' | Select-Object -First 1
        $configuredCompiler = if ($compilerInfo) { [IO.Path]::GetFullPath($compilerInfo.Matches[0].Groups[1].Value) } else { '' }
        if (-not [string]::Equals($configuredCompiler,[IO.Path]::GetFullPath([string]$VisualStudioToolchain.Cl),[StringComparison]::OrdinalIgnoreCase)) {
            throw "Configured C compiler does not match selected VS2026 compiler '$($VisualStudioToolchain.Cl)'. Cache was left unchanged; review '$buildDir' and move it aside before retrying."
        }
        Invoke-Checked -File $cmake -Arguments @('--build',$buildDir,'--config',$Configuration,'--parallel')
        Invoke-Checked -File $ctest -Arguments @('--test-dir',$buildDir,'-C',$Configuration,'--output-on-failure','--no-tests=error')

        if (-not $SkipTidy -and $sourceFiles.Count -gt 0) {
            $tidy = Get-ToolPath 'clang-tidy'
            foreach ($source in @($sourceFiles | Where-Object { $_.Extension -eq '.c' })) {
                # CMake と同じ場所からプロジェクトの公開ヘッダーを探す。
                # 警告は参考情報とし、解析自体が失敗したらチェックを止める。
                $tidyArgs = @($source.FullName,'--','-x','c','-std=c17','-I',(Join-Path $ProjectRoot 'include'))
                foreach ($includePath in ([string]$env:INCLUDE -split ';' | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Container) })) { $tidyArgs += @('-I',$includePath) }
                $tidyArgs += @('-I',(Join-Path $buildDir '_deps/cjson-src'),'-I',(Join-Path $buildDir '_deps/unity-src/src'))
                Invoke-Checked -File $tidy -Arguments $tidyArgs
            }
            Write-CheckLog 'clang-tidy completed for current C translation units using C17 and explicit include paths; target compile definitions/options are not imported from a compile database. Diagnostics are advisory.' 'OK'
        }
        Write-CheckLog 'Configure, build, CTest, formatting, JSON, and enabled analysis completed.' 'OK'
    } finally { Pop-Location }
} catch {
    Write-CheckLog $_.Exception.ToString() 'ERROR'
    throw
}
