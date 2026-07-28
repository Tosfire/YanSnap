# YanSnap 便携版发布指南

## 生成发布包

在项目目录运行：

```powershell
.\package.ps1 -Version 1.0.0 -Configuration Release
```

脚本会使用 MSVC Release `/MT` 构建，并在 `release` 目录生成：

- `YanSnap-1.0.0-win-x64-portable.zip`
- `.sha256` 校验文件
- `YanSnap-icon-256.png` 和 `YanSnap.ico` 发布图标素材

脚本会在压缩完成后删除临时打包目录。ZIP 内只有运行所需的 `YanSnap.exe`、便携模式标记、使用说明、
许可证和 EXE 校验值。用户完整解压后直接双击即可，不需要安装。

## 发布前检查

1. 在一台没有开发环境的 Windows 10/11 x64 电脑或虚拟机上解压测试。
2. 确认启动后只驻留托盘，不自动截屏。
3. 确认托盘悬停名称、通知来源和文件属性均显示 `YanSnap`。
4. 测试默认快捷键、保存、剪贴板、贴图和退出。
5. 将 ZIP 的 SHA-256 与 `.sha256` 文件核对一致。
6. 用杀毒软件扫描最终 ZIP。

## 对外发布

可以把 ZIP 和 `.sha256` 上传到 GitHub Releases、Gitee Releases、
个人网站或网盘。发布页面至少应包含版本号、支持的 Windows 版本、
主要功能、校验值、许可证和更新说明。

当前 EXE 没有商业代码签名证书。公开发布时建议购买受 Windows 信任的
代码签名证书，对 `YanSnap.exe` 签名后再运行 `package.ps1`，否则部分电脑
首次运行可能出现 SmartScreen“未知发布者”提示。
