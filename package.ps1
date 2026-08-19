param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = "1.1.0",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot
$buildDirectory = Join-Path $projectRoot "build-msvc"
$releaseRoot = Join-Path $projectRoot "release"
$packageName = "YanSnap-$Version-win-x64-portable"
$stageRoot = Join-Path $releaseRoot ".staging"
$stageDirectory = Join-Path $stageRoot $packageName
$archivePath = Join-Path $releaseRoot "$packageName.zip"
$archiveChecksumPath = Join-Path $releaseRoot "$packageName.sha256"

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    $bundledCMake = "C:\Program Files\CMake\bin\cmake.exe"
    if (-not (Test-Path -LiteralPath $bundledCMake)) {
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vsWhere) {
            $visualStudioRoot = & $vsWhere -latest -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath
            if ($visualStudioRoot) {
                $bundledCMake = Join-Path $visualStudioRoot `
                    "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            }
        }
    }
    if (-not (Test-Path -LiteralPath $bundledCMake)) {
        throw "CMake was not found in PATH, Program Files, or Visual Studio Build Tools."
    }
    $cmakeExecutable = $bundledCMake
} else {
    $cmakeExecutable = $cmakeCommand.Source
}

& $cmakeExecutable -S $projectRoot -B $buildDirectory -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmakeExecutable --build $buildDirectory --config $Configuration --target YanSnap --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
$resolvedReleaseRoot = [System.IO.Path]::GetFullPath($releaseRoot)
$resolvedStageRoot = [System.IO.Path]::GetFullPath($stageRoot)
$resolvedStageDirectory = [System.IO.Path]::GetFullPath($stageDirectory)
$resolvedArchivePath = [System.IO.Path]::GetFullPath($archivePath)
if (-not $resolvedStageRoot.StartsWith(
        $resolvedReleaseRoot + [System.IO.Path]::DirectorySeparatorChar)) {
    throw "Refusing to replace a staging root outside the release directory."
}
if (-not $resolvedStageDirectory.StartsWith(
        $resolvedReleaseRoot + [System.IO.Path]::DirectorySeparatorChar)) {
    throw "Refusing to replace a staging directory outside the release directory."
}
if (-not $resolvedArchivePath.StartsWith(
        $resolvedReleaseRoot + [System.IO.Path]::DirectorySeparatorChar)) {
    throw "Refusing to replace an archive outside the release directory."
}

if (Test-Path -LiteralPath $resolvedStageRoot) {
    Remove-Item -LiteralPath $resolvedStageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $resolvedArchivePath) {
    Remove-Item -LiteralPath $resolvedArchivePath -Force
}
if (Test-Path -LiteralPath $archiveChecksumPath) {
    Remove-Item -LiteralPath $archiveChecksumPath -Force
}

New-Item -ItemType Directory -Path $resolvedStageDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $buildDirectory "$Configuration\YanSnap.exe") `
    -Destination (Join-Path $resolvedStageDirectory "YanSnap.exe")
Copy-Item -LiteralPath (Join-Path $projectRoot "packaging\portable.flag") `
    -Destination (Join-Path $resolvedStageDirectory "portable.flag")
Copy-Item -LiteralPath (Join-Path $projectRoot "packaging\README-PORTABLE.txt") `
    -Destination (Join-Path $resolvedStageDirectory "README.txt")
Copy-Item -LiteralPath (Join-Path $projectRoot "LICENSE") `
    -Destination (Join-Path $resolvedStageDirectory "LICENSE.txt")

$executableHash = Get-FileHash -Algorithm SHA256 `
    -LiteralPath (Join-Path $resolvedStageDirectory "YanSnap.exe")
"$($executableHash.Hash)  YanSnap.exe" |
    Set-Content -LiteralPath (Join-Path $resolvedStageDirectory "SHA256SUMS.txt") `
        -Encoding ASCII

Compress-Archive -LiteralPath $resolvedStageDirectory `
    -DestinationPath $resolvedArchivePath -CompressionLevel Optimal
$archiveHash = Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedArchivePath
"$($archiveHash.Hash)  $packageName.zip" |
    Set-Content -LiteralPath $archiveChecksumPath -Encoding ASCII
Copy-Item -LiteralPath (Join-Path $projectRoot "assets\icon\YanSnap-icon-256.png") `
    -Destination (Join-Path $releaseRoot "YanSnap-icon-256.png") -Force
Copy-Item -LiteralPath (Join-Path $projectRoot "assets\icon\YanSnap.ico") `
    -Destination (Join-Path $releaseRoot "YanSnap.ico") -Force
Remove-Item -LiteralPath $resolvedStageRoot -Recurse -Force

[PSCustomObject]@{
    Archive = $resolvedArchivePath
    ArchiveSize = (Get-Item -LiteralPath $resolvedArchivePath).Length
    SHA256 = $archiveHash.Hash
}
