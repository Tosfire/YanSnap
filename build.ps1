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

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    cmake -S $projectRoot -B $buildDir -A x64
    cmake --build $buildDir --config $Configuration
    exit $LASTEXITCODE
}

$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) {
    throw "No supported compiler found. Install Visual Studio 2022 Build Tools with CMake, or place g++ on PATH."
}

$sources = @(
    "src/main.cpp",
    "src/app/App.cpp",
    "src/app/SingleInstance.cpp",
    "src/app/HotkeyManager.cpp",
    "src/app/StartupManager.cpp",
    "src/app/TrayIcon.cpp",
    "src/common/AppIcon.cpp",
    "src/capture/DesktopCapture.cpp",
    "src/capture/WindowDetector.cpp",
    "src/annotation/Annotations.cpp",
    "src/annotation/UndoStack.cpp",
    "src/overlay/OverlayWindow.cpp",
    "src/overlay/Toolbar.cpp",
    "src/pin/PinWindow.cpp",
    "src/export/ClipboardExporter.cpp",
    "src/export/ImageComposer.cpp",
    "src/export/PngEncoder.cpp",
    "src/settings/Settings.cpp",
    "src/settings/SettingsWindow.cpp"
)

$resourceObject = Join-Path $buildDir "YanSnap.res.o"
$windres = Get-Command windres -ErrorAction Stop
& $windres.Source "-I$projectRoot" "-i" (Join-Path $projectRoot "resources/YanSnap.rc") "-o" $resourceObject
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$flags = @(
    "-std=c++20", "-municode", "-mwindows",
    "-DUNICODE", "-D_UNICODE", "-DWIN32_LEAN_AND_MEAN", "-DNOMINMAX",
    "-DWINVER=0x0A00", "-D_WIN32_WINNT=0x0A00",
    "-Isrc", "-I.", "-Wall", "-Wextra", "-Wpedantic",
    "-static", "-static-libgcc", "-static-libstdc++"
)
if ($Configuration -eq "Release") {
    $flags += @("-O2", "-DNDEBUG", "-s")
} else {
    $flags += @("-O0", "-g")
}

$output = Join-Path $buildDir "YanSnap.exe"
Push-Location $projectRoot
try {
    & $compiler.Source @flags @sources $resourceObject "-o" $output `
        "-luser32" "-lgdi32" "-lshell32" "-lole32" "-lcomctl32" `
        "-lcomdlg32" "-lwindowscodecs" "-ldwmapi" "-ladvapi32" "-luuid"
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
