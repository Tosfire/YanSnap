# YanSnap 发布指南

本文面向项目维护者。普通用户只需从 [GitHub Releases](https://github.com/Li2190158085/YanSnap/releases/latest) 下载便携包。

## 版本准备

发布前确认版本号在以下位置保持一致：

- `CMakeLists.txt` 中的项目版本
- 程序“关于”信息
- `CHANGELOG.md`
- 打包命令的 `-Version` 参数

版本号遵循 Semantic Versioning：

- 修复：`1.0.1`
- 向后兼容的新功能：`1.1.0`
- 不兼容变化：`2.0.0`

## 构建与自动测试

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

然后完成[测试指南](testing.md)中的发布前人工检查，至少覆盖 Windows 10 或 11 x64、截图、保存、贴图、设置和退出。

## 生成便携包

在项目根目录运行：

```powershell
.\package.ps1 -Version 1.0.0 -Configuration Release
```

脚本会：

1. 使用 MSVC Release `/MT` 构建
2. 创建临时便携目录
3. 放入 EXE、`portable.flag`、用户说明、许可证和 EXE 校验值
4. 生成 ZIP 和 ZIP 的 `.sha256`
5. 删除临时打包目录

输出：

```text
release/
├─ YanSnap-1.0.0-win-x64-portable.zip
└─ YanSnap-1.0.0-win-x64-portable.sha256
```

## 发布包检查

- [ ] 在没有 Visual Studio 或 CMake 的电脑或虚拟机中测试
- [ ] 完整解压后无需安装和管理员权限
- [ ] 启动后只驻留托盘，不立即截图
- [ ] 托盘名称、通知和文件属性均显示 `YanSnap`
- [ ] `F1` 截图、`F3` 贴图、复制、保存和设置正常
- [ ] ZIP 内没有 `config.ini`、日志、截图或开发机路径
- [ ] ZIP SHA-256 与 `.sha256` 文件一致
- [ ] 使用可信杀毒工具扫描最终产物

## 创建 GitHub Release

1. 将变更合并到 `main`。
2. 确认工作区干净，`main` 与远端同步。
3. 创建带注释或普通版本标签，例如 `v1.0.0`。
4. 创建非草稿、非预发布的 GitHub Release。
5. 上传 ZIP 和 `.sha256`。
6. 发布说明包含主要变化、系统要求、已知限制和升级说明。
7. 下载 GitHub 上的附件，再次核对 SHA-256。

使用 GitHub CLI 的示例：

```powershell
gh release create v1.0.0 `
  release\YanSnap-1.0.0-win-x64-portable.zip `
  release\YanSnap-1.0.0-win-x64-portable.sha256 `
  --title "YanSnap v1.0.0" `
  --notes-file release-notes.md
```

`release-notes.md` 是临时发布说明，不应在没有维护价值时提交到仓库。

## 代码签名

当前公开版本没有商业代码签名证书，因此 Windows SmartScreen 可能显示“未知发布者”。

如果后续获得受 Windows 信任的证书，应在打包前对最终 `YanSnap.exe` 签名，并在签名后重新生成所有哈希和 ZIP。不得签名后继续修改 EXE。

## 发布后

- [ ] Releases 页面可公开访问
- [ ] “Latest” 指向新版本
- [ ] 两个附件均可下载
- [ ] README 下载链接指向最新 Release
- [ ] `CHANGELOG.md` 包含新版本
- [ ] 仓库中没有构建目录或便携配置
- [ ] 新版本问题使用对应 GitHub milestone 或标签跟踪
