# 参与 YanSnap

感谢你愿意改进 YanSnap。项目欢迎缺陷修复、Windows 兼容性反馈、文档改进和范围明确的新功能。

## 提交问题

提交 Issue 前请先搜索是否已有相同问题。缺陷报告尽量包含：

- Windows 版本、系统架构和显示缩放比例
- 单屏或多屏布局，以及是否存在负坐标显示器
- YanSnap 版本和下载来源
- 可重复的操作步骤
- 预期结果与实际结果
- 不包含隐私内容的截图或错误信息

请勿在公开 Issue 中粘贴密码、令牌、私人截图、完整用户名路径或其他敏感数据。安全漏洞请按照 [SECURITY.md](SECURITY.md) 报告。

## 开发环境

推荐环境：

- Windows 10 / 11 x64
- Visual Studio 2022 Build Tools
- MSVC、Windows SDK
- CMake 3.24 或更高版本

构建与测试：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

需要运行交互测试时：

```powershell
cmake -S . -B build -A x64 -DYANSNAP_BUILD_INTEGRATION_TESTS=ON
cmake --build build --config Release
```

详细测试范围见[测试指南](docs/testing.md)。

## 设计原则

提交代码时请保持以下边界：

- 使用 Windows 原生 API，避免仅为少量功能增加大型依赖
- 保持便携运行，不要求安装服务、驱动或第三方运行时
- 截图数据默认只在本地处理
- 不添加账号、遥测、广告或隐式网络请求
- 正确处理 Unicode、多显示器、负坐标和 Per-Monitor DPI
- Win32、GDI、COM 等资源应具有明确的生命周期
- 错误场景不能静默丢失用户正在编辑的截图

架构说明见 [docs/architecture.md](docs/architecture.md)。

## Pull Request

建议每个 PR 只解决一个清晰问题，并包含：

- 变更目的和用户影响
- 关键实现说明
- 已执行的自动测试
- 涉及界面时的前后对比截图
- 涉及多显示器或 DPI 时的测试环境
- 新增第三方依赖时的用途、体积和许可证说明

提交前请确认：

- Release x64 可以编译
- 自动测试全部通过
- 没有提交 `build`、`release`、`config.ini` 等本地产物
- README 或相关文档已随用户可见行为同步更新
- 新增文本使用 UTF-8，界面字符串可在中文 Windows 中正确显示

## 代码风格

- 使用 C++20
- 遵循现有目录和命名方式
- 优先小而清晰的类与函数
- 不为尚未确认的功能提前引入复杂抽象
- 注释解释原因和约束，避免重复代码本身

提交即表示你同意按项目的 [MIT License](LICENSE) 授权所提交的内容。
