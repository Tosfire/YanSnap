param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot "build"

if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
    $resolvedRoot = [System.IO.Path]::GetFullPath($projectRoot)
    $resolvedBuild = [System.IO.Path]::GetFullPath($buildDir)
    if (-not $resolvedBuild.StartsWith($resolvedRoot + [System.IO.Path]::DirectorySeparatorChar)) {
        throw "Refusing to remove build directory outside the project."
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    $cmakeExecutable = "C:\Program Files\CMake\bin\cmake.exe"
    if (-not (Test-Path -LiteralPath $cmakeExecutable)) {
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vsWhere) {
            $visualStudioRoot = & $vsWhere -latest -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath
            if ($visualStudioRoot) {
                $cmakeExecutable = Join-Path $visualStudioRoot `
                    "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            }
        }
    }
    if (-not (Test-Path -LiteralPath $cmakeExecutable)) {
        throw "CMake was not found. Install Visual Studio 2022 Build Tools with MSVC, the Windows SDK, and CMake."
    }
} else {
    $cmakeExecutable = $cmakeCommand.Source
}

& $cmakeExecutable -S $projectRoot -B $buildDir -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmakeExecutable --build $buildDir --config $Configuration --parallel
exit $LASTEXITCODE
