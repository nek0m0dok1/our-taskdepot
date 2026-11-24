#Requires -Version 5.1
Set-StrictMode -Version Latest

function Get-VisualStudio2026Toolchain {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'Visual Studio Installer vswhere.exe was not found. Install Visual Studio 2026 with its C++ x64/x86 and CMake tools; setup does not install or modify Visual Studio.'
    }

    $matches = @(& $vswhere -products '*' -version '[18.0,19.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.CMake.Project -latest -property installationPath 2>&1 | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    $vsCode = $LASTEXITCODE
    if ($vsCode -ne 0 -or $matches.Count -ne 1) {
        throw 'No unique Visual Studio 2026 18.x installation with C++ x64/x86 and CMake tools was found. Visual Studio 2022 and other MSVC versions are unsupported. Install the required components through Visual Studio Installer; setup does not install or alter Visual Studio.'
    }

    $instance = [IO.Path]::GetFullPath([string]$matches[0]).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $devCmd = Join-Path $instance 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $devCmd -PathType Leaf)) {
        throw "VsDevCmd.bat is missing in '$instance'. Repair that Visual Studio installation using Visual Studio Installer; setup will not modify it."
    }

    $cmdLine = 'call "' + $devCmd + '" -arch=x64 -host_arch=x64 >nul && set'
    $environment = @(& $env:ComSpec /d /s /c $cmdLine 2>&1)
    $environmentCode = $LASTEXITCODE
    if ($environmentCode -ne 0) {
        throw "VsDevCmd.bat failed with exit code $environmentCode for '$instance'. Repair the existing Visual Studio installation; setup did not modify it."
    }
    foreach ($line in $environment) {
        $text = [string]$line
        $separator = $text.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable($text.Substring(0,$separator),$text.Substring($separator+1),'Process')
        }
    }

    $clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $clCommand) { throw "VsDevCmd.bat did not expose cl.exe for '$instance'." }
    $cl = [IO.Path]::GetFullPath($clCommand.Source)
    $instancePrefix = $instance + [IO.Path]::DirectorySeparatorChar
    if (-not $cl.StartsWith($instancePrefix,[StringComparison]::OrdinalIgnoreCase)) {
        throw "The selected cl.exe '$cl' is outside the chosen Visual Studio instance '$instance'."
    }

    $compilerVersionText = [Diagnostics.FileVersionInfo]::GetVersionInfo($cl).ProductVersion
    $compilerVersionMatch = [regex]::Match([string]$compilerVersionText,'\d+\.\d+\.\d+(?:\.\d+)?')
    $compilerVersion = $null
    if ($compilerVersionMatch.Success) {
        $fileVersion = [version]$compilerVersionMatch.Value
        if ($fileVersion.Major -eq 14) { $compilerVersion = [version]::new(19,$fileVersion.Minor,$fileVersion.Build,$fileVersion.Revision) }
        else { $compilerVersion = $fileVersion }
    }
    if ($null -eq $compilerVersion -or $compilerVersion -lt [version]'19.50' -or $compilerVersion -ge [version]'20.0') {
        throw "Unsupported MSVC compiler version '$compilerVersionText' at '$cl'. Visual Studio 2026 MSVC 19.50 or later in the 19.x series is required."
    }

    $sdkValue = ([string]$env:WindowsSDKVersion).TrimEnd('\')
    $sdkMatch = [regex]::Match($sdkValue,'\d+\.\d+\.\d+\.\d+')
    if (-not $sdkMatch.Success -or [version]$sdkMatch.Value -lt [version]'10.0.22000.0') {
        throw "Windows 11 SDK 10.0.22000.0 or later is required; VsDevCmd selected '$sdkValue' for '$instance'."
    }

    $cmake = Join-Path $instance 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
    foreach ($path in @($cmake,$ctest)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Visual Studio 2026 bundled tool is missing: '$path'. Add the C++ CMake tools component in Visual Studio Installer."
        }
    }

    $cmakeVersionOutput = @(& $cmake --version 2>&1)
    $cmakeExitCode = $LASTEXITCODE
    $cmakeVersionText = [string]($cmakeVersionOutput | Select-Object -First 1)
    $cmakeVersionMatch = [regex]::Match([string]$cmakeVersionText,'\d+\.\d+\.\d+')
    if ($cmakeExitCode -ne 0 -or -not $cmakeVersionMatch.Success -or [version]$cmakeVersionMatch.Value -lt [version]'4.2.0') {
        throw "Unsupported Visual Studio bundled CMake '$cmakeVersionText' (exit $cmakeExitCode) at '$cmake'; CMake 4.2.0 or later is required."
    }

    $ctestVersionOutput = @(& $ctest --version 2>&1)
    $ctestExitCode = $LASTEXITCODE
    $ctestVersionText = [string]($ctestVersionOutput | Select-Object -First 1)
    $expectedCtestVersionPattern = 'ctest version\s+' + [regex]::Escape($cmakeVersionMatch.Value)
    if ($ctestExitCode -ne 0 -or $ctestVersionText -notmatch $expectedCtestVersionPattern) {
        throw "Visual Studio bundled CTest '$ctestVersionText' (exit $ctestExitCode) does not match CMake $($cmakeVersionMatch.Value) at '$ctest'."
    }

    $env:PATH = (Split-Path -Parent $cmake) + ';' + $env:PATH
    Write-Host "Selected VS2026 instance: $instance"
    Write-Host "Verified MSVC ${compilerVersion}: $cl"
    Write-Host "Verified Windows SDK $($sdkMatch.Value)"
    Write-Host "Verified CMake $($cmakeVersionMatch.Value): $cmake"
    Write-Host "Verified CTest: $ctest"

    return @{
        Instance = $instance
        Cl = $cl
        MsvcVersion = $compilerVersion
        SdkVersion = $sdkMatch.Value
        CMake = [IO.Path]::GetFullPath($cmake)
        CMakeVersion = ($cmakeVersionText -replace '^cmake version\s+', '').Trim()
        CTest = [IO.Path]::GetFullPath($ctest)
        Toolset = 'v145'
    }
}
