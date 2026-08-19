<p align="center">
  <img src="assets/icon/YanSnap-icon-256.png" width="128" height="128" alt="YanSnap 图标">
</p>

<h1 align="center">YanSnap</h1>

<p align="center">
  轻量、便携、纯本地的 Windows 截图与贴图工具。
</p>

<p align="center">
  <a href="https://github.com/Li2190158085/YanSnap/releases/latest"><img src="https://img.shields.io/github/v/release/Li2190158085/YanSnap?label=release" alt="GitHub Release"></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D4" alt="Windows 10 / 11">
  <a href="LICENSE"><img src="https://img.shields.io/github/license/Li2190158085/YanSnap" alt="MIT License"></a>
</p>

<p align="center">
  <a href="README.en.md">English</a> · 简体中文
</p>

YanSnap 面向希望“解压即用”的 Windows 用户：无需安装、无需账号，也不会上传截图。程序启动后驻留系统托盘，按 `F1` 即可截图，按 `F3` 可将剪贴板图片贴到屏幕。

## 下载

前往 [GitHub Releases](https://github.com/Li2190158085/YanSnap/releases/latest) 下载：

```text
YanSnap-*-win-x64-portable.zip
```

完整解压 ZIP，然后双击 `YanSnap.exe`。不要直接在压缩包预览窗口中运行。

YanSnap 目前没有商业代码签名证书。Windows SmartScreen 可能在首次运行时显示“未知发布者”，这是未签名程序的系统提示。请只从本仓库的 Releases 页面下载，并可使用随版本提供的 `.sha256` 文件核对完整性。

## 主要功能

- 默认全局快捷键 `F1`，支持在设置中录入单键或组合键
- 自由区域截图、窗口自动识别和跨显示器选区
- 选区移动、八点缩放、尺寸提示和像素放大镜
- 矩形、箭头、画笔、中文文字和真实像素马赛克
- 截图内直接识别中文、英文、数字和 URL，结果可编辑复制
- 撤销、重做、删除、复制和 PNG 保存
- 将当前选区直接贴到屏幕
- 按 `F3` 将剪贴板图片贴到屏幕
- 贴图拖动、缩放、透明度调节、隐藏和置顶
- 可选复制后自动保存、包含鼠标指针和成功通知
- 可选随 Windows 登录自动启动
- 便携模式设置跟随程序目录迁移
- 单实例运行，无账号、遥测、上传或自动更新

## 快速开始

1. 双击 `YanSnap.exe`。程序会驻留托盘，不会立即开始截图。
2. 按 `F1`，拖动选择区域，或单击高亮的窗口。
3. 直接按 `Enter` 复制，或使用工具栏添加标注、识别选区文字。
4. 选择“贴到屏幕”可保留置顶贴图；右键贴图可查看更多操作。
5. 右键系统托盘中的 YanSnap 图标，可打开截图、贴图、设置和退出菜单。

常用快捷键：

| 快捷键 | 操作 |
| --- | --- |
| `F1` | 开始截图，可在设置中修改 |
| `F3` | 将剪贴板图片贴到屏幕 |
| `Enter` / `Ctrl+C` | 复制当前选区并完成 |
| `Ctrl+S` | 保存为 PNG |
| `Ctrl+T` | 将当前选区贴到屏幕 |
| `Ctrl+R` | 在当前截图内识别选区文字 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Delete` | 删除当前标注 |
| `Esc` | 取消当前操作或退出截图 |
| 方向键 | 移动选区 1 个物理像素 |
| `Shift+方向键` | 移动选区 10 个物理像素 |

完整操作、设置、卸载和故障排查请参阅[用户指南](docs/user-guide.md)。

## 隐私

YanSnap 的截图、标注、文字识别和编码均在本机完成。OCR 使用 Windows 本地语言组件，不上传图片或识别结果。程序不包含账号、网络上传、遥测、广告、截图历史数据库或自动更新功能。

只有在用户勾选“开机自动启动”并保存后，YanSnap 才会在当前用户的 Windows 登录启动项中写入程序路径；取消勾选会移除该启动项。

更多信息见[安全说明](SECURITY.md)和[技术架构](docs/architecture.md)。

## 从源码构建

要求：

- Windows 10 或 Windows 11 x64
- Visual Studio 2022 Build Tools，包含 MSVC 和 Windows SDK
- CMake 3.24 或更高版本

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

MSVC Release 使用静态运行库 `/MT`。主程序输出到 `build\Release\YanSnap.exe`。

也可以使用仓库提供的脚本：

```powershell
.\build.ps1 -Configuration Release -Clean
```

架构、测试和发布流程分别见：

- [技术架构](docs/architecture.md)
- [测试指南](docs/testing.md)
- [发布指南](docs/releasing.md)

## 参与贡献

欢迎提交缺陷报告、兼容性反馈和范围明确的改进。开始编码前请阅读[贡献指南](CONTRIBUTING.md)，安全问题请按[安全策略](SECURITY.md)私下报告。

项目坚持轻量、原生和本地优先。录屏、云上传、账号系统和大型 GUI 框架不属于当前核心范围。

## 已知限制

- UAC 安全桌面无法由普通权限程序捕获
- DRM 或系统保护内容可能显示为黑色
- 部分独占全屏游戏可能无法被 GDI 正确捕获
- HDR 桌面可能出现亮度或色彩差异
- OCR 可用语言取决于 Windows 已安装的光学字符识别语言功能
- 当前仅提供 Windows x64 版本，界面语言为简体中文

## 致谢

YanSnap 的交互体验受到 Snipaste、ShareX、Flameshot 和 Greenshot 等优秀工具启发。YanSnap 为独立实现，不包含这些项目的源代码或素材。

## 许可证

YanSnap 使用 [MIT License](LICENSE)。
