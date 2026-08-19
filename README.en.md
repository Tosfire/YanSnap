<p align="center">
  <img src="assets/icon/YanSnap-icon-256.png" width="128" height="128" alt="YanSnap icon">
</p>

<h1 align="center">YanSnap</h1>

<p align="center">
  A lightweight, portable screenshot and image-pinning tool for Windows.
</p>

<p align="center">
  <a href="https://github.com/Li2190158085/YanSnap/releases/latest"><img src="https://img.shields.io/github/v/release/Li2190158085/YanSnap?label=release" alt="GitHub Release"></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D4" alt="Windows 10 / 11">
  <a href="LICENSE"><img src="https://img.shields.io/github/license/Li2190158085/YanSnap" alt="MIT License"></a>
</p>

<p align="center">
  English · <a href="README.md">简体中文</a>
</p>

YanSnap is designed for a simple workflow: download, unzip, and run. It requires no installation or account, processes screenshots locally, and stays in the system tray. Press `F1` to capture a screenshot or `F3` to pin an image from the clipboard.

> The application UI is currently available in Simplified Chinese.

## Download

Download `YanSnap-*-win-x64-portable.zip` from the [latest GitHub release](https://github.com/Li2190158085/YanSnap/releases/latest), extract the archive completely, and run `YanSnap.exe`.

YanSnap is not currently signed with a commercial code-signing certificate. Windows SmartScreen may show an “Unknown publisher” warning on first launch. Download only from this repository and verify the archive with the accompanying `.sha256` file when needed.

## Highlights

- Configurable global capture hotkey, `F1` by default
- Free-region, window, multi-monitor, and negative-coordinate capture
- Selection movement, eight resize handles, dimensions, and pixel magnifier
- Rectangle, arrow, pen, Chinese text, and destructive pixel mosaic tools
- In-capture OCR for Chinese, English, numbers, and URLs, with editable results
- Undo, redo, delete, clipboard copy, and PNG export
- Pin the current selection directly on screen
- Pin clipboard images with `F3`
- Move, scale, fade, hide, and manage always-on-top pinned images
- Optional automatic saving, cursor capture, notifications, and launch at sign-in
- Portable INI settings stored beside the executable
- Single-instance operation with no account, telemetry, upload, or auto-update

## Quick start

1. Run `YanSnap.exe`. It stays in the system tray and does not capture immediately.
2. Press `F1`, then drag a region or click a highlighted window.
3. Press `Enter` to copy, or use the floating toolbar to annotate or recognize text.
4. Use “Pin to screen” to keep the selection visible above other windows.
5. Right-click the YanSnap tray icon for capture, pin, settings, and exit actions.

| Shortcut | Action |
| --- | --- |
| `F1` | Start capture; configurable |
| `F3` | Pin an image from the clipboard |
| `Enter` / `Ctrl+C` | Copy and finish |
| `Ctrl+S` | Save as PNG |
| `Ctrl+T` | Pin the current selection |
| `Ctrl+R` | Recognize text inside the current selection |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Delete` | Delete the selected annotation |
| `Esc` | Cancel the current action or capture |
| Arrow keys | Move the selection by 1 physical pixel |
| `Shift` + Arrow keys | Move the selection by 10 physical pixels |

The detailed user guide is currently maintained in Chinese: [User Guide](docs/user-guide.md).

## Privacy

Capture, annotation, OCR, clipboard conversion, and PNG encoding happen locally. OCR uses Windows language components and does not upload the image or recognized text. YanSnap contains no account system, telemetry, advertising, screenshot-history database, cloud upload, or automatic updater.

YanSnap writes a current-user Windows sign-in startup entry only after the user explicitly enables “Start with Windows.” Disabling the option removes that entry.

## Build from source

Requirements:

- Windows 10 or Windows 11 x64
- Visual Studio 2022 Build Tools with MSVC and the Windows SDK
- CMake 3.24 or newer

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The MSVC Release build uses the static `/MT` runtime. The executable is written to `build\Release\YanSnap.exe`.

## Contributing

Bug reports, compatibility feedback, and focused improvements are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code. Report security issues according to [SECURITY.md](SECURITY.md).

YanSnap intentionally remains a small, native, local-first tool. Screen recording, cloud uploads, accounts, and large GUI frameworks are outside the current core scope.

## Known limitations

- Standard processes cannot capture the UAC secure desktop
- DRM-protected content may appear black
- Some exclusive full-screen games are not captured correctly by GDI
- HDR desktops may show brightness or color differences
- OCR languages depend on the optical-character-recognition language features installed in Windows
- Windows x64 only; the current UI language is Simplified Chinese

## Acknowledgements

YanSnap's interaction design is inspired by tools such as Snipaste, ShareX, Flameshot, and Greenshot. YanSnap is an independent implementation and does not include their source code or assets.

## License

YanSnap is released under the [MIT License](LICENSE).
