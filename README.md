# Window Shield for OBS（窗口保护）

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)

Window Shield for OBS（窗口保护）是一款仅支持 Windows 的 [OBS Studio](https://obsproject.com/) 插件，可以让你在屏幕捕获、屏幕共享和录屏软件中隐藏任意窗口。

## 工作原理

插件对选中的窗口调用 Win32 API [`SetWindowDisplayAffinity`](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity)，并传入 `WDA_EXCLUDEFROMCAPTURE`。被标记的窗口在大多数捕获软件（包括 OBS 本身）中会显示为空白/黑色（或直接被排除）。

由于 `SetWindowDisplayAffinity` 必须在拥有该窗口的进程内部调用，插件通过跨进程代码注入来实现：

1. 枚举系统中所有可见、有标题且未被 cloaked 的顶层窗口。
2. 通过解析 PE 导出表，在目标进程的 `user32.dll` 中定位 `SetWindowDisplayAffinity` 函数地址。
3. 将一小段 shellcode（x86 或 x64）写入目标进程，并通过 `CreateRemoteThread` 执行。
4. 由 shellcode 应用或移除窗口保护。

## 功能特性

- 以可勾选列表的形式列出所有当前可见的窗口。
- 勾选窗口即保护，取消勾选即解除保护。
- 保护状态即时生效，并对仍然存活的窗口自动恢复。
- 同时支持 32 位和 64 位目标进程。
- 窗口列表自动刷新，新打开或已关闭的窗口会实时反映。

## 环境要求

> 由于插件源码依赖 Win32 API，仅支持 Windows。

| 组件 | 版本 |
|------|------|
| OBS Studio | 31.1+ |
| Visual Studio | 17 2022 |
| CMake | 3.28 - 3.30 |

## 构建

本项目基于 [OBS 插件模板](https://github.com/obsproject/obs-plugintemplate)，使用相同的 CMake 构建系统。

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

构建成功后，将生成的 `window-shield-for-obs` 模块复制到 OBS 插件目录（例如 `C:\Program Files\obs-studio\obs-plugins\64bit\`），然后重启 OBS。

> **注意：** 插件必须使用与你已安装 OBS 版本相匹配的 `libobs` 编译，才能正常加载。

## 使用方法

1. 启动 OBS Studio。
2. 打开 **工具 → 窗口保护设置**。
3. 勾选想要隐藏的窗口，取消勾选即可恢复显示。

## 免责声明

插件使用远程线程注入对其它进程施加保护，部分杀毒软件或反作弊软件可能会将其标记为风险行为。请仅对你自己拥有或有权管理的窗口使用，并在保护第三方应用前充分了解相关风险。

## 许可证

本项目采用 [GNU General Public License v2](LICENSE) 许可协议。

© 2026 [Cyan](https://github.com/jiejiebiezheyang) &lt;ltpcloud@qq.com&gt;
