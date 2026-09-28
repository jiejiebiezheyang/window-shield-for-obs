# Window Shield for OBS

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg?style=flat-square)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%20only-0078D6.svg?style=flat-square&logo=windows&logoColor=white)](#requirements)
[![OBS Studio: 31.1+](https://img.shields.io/badge/OBS%20Studio-31.1%2B-302E31.svg?style=flat-square&logo=obsstudio&logoColor=white)](https://obsproject.com/)
[![Version: 1.0.0](https://img.shields.io/badge/version-1.0.0-green.svg?style=flat-square)](buildspec.json)
[![GitHub stars](https://img.shields.io/github/stars/jiejiebiezheyang/window-shield-for-obs?style=flat-square)](https://github.com/jiejiebiezheyang/window-shield-for-obs/stargazers)

**English** | [简体中文](README.zh-CN.md)

Window Shield for OBS is a Windows-only plugin for [OBS Studio](https://obsproject.com/) that lets you hide any window from screen capture, screen sharing, and screen recording software.

## How It Works

The plugin calls the Win32 API [`SetWindowDisplayAffinity`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity) with `WDA_EXCLUDEFROMCAPTURE` on the selected window. A protected window appears blank/black (or is excluded entirely) in most capture software, including OBS itself.

Because `SetWindowDisplayAffinity` must be invoked inside the process that owns the window, the plugin relies on cross-process code injection:

1. Enumerate all top-level windows that are visible, have a title, and are not cloaked.
2. Resolve the address of `SetWindowDisplayAffinity` in the target process's `user32.dll` by parsing the PE export table.
3. Write a small shellcode stub (x86 or x64) into the target process and execute it via `CreateRemoteThread`.
4. The shellcode applies or removes the window protection.

## Features

- Lists all currently visible windows in a checkable list.
- Check a window to protect it; uncheck it to remove the protection.
- Protection takes effect immediately and is automatically restored for windows that are still alive.
- Supports both 32-bit and 64-bit target processes.
- The window list refreshes automatically, so newly opened or closed windows are reflected in real time.

## Requirements

> The plugin source depends on the Win32 API, so it is Windows-only.

| Component     | Version     |
| ------------- | ----------- |
| OBS Studio    | 31.1+       |
| Visual Studio | 17 2022     |
| CMake         | 3.28 - 3.30 |

## Building

This project is based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate) and uses the same CMake build system.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

After a successful build, copy the produced `window-shield-for-obs` module into the OBS plugin directory (for example `C:\Program Files\obs-studio\obs-plugins\64bit\`), then restart OBS.

> **Note:** The plugin must be compiled against a `libobs` version matching your installed OBS build, otherwise it will fail to load.

## Usage

1. Launch OBS Studio.
2. Open **Tools → Window Shield Settings**.
3. Check the windows you want to hide; uncheck them to restore visibility.

## Disclaimer

The plugin uses remote thread injection to apply protection to other processes, which may be flagged as risky behavior by some antivirus or anti-cheat software. Only use it on windows you own or are authorized to manage, and make sure you understand the risks before protecting third-party applications.

## License

This project is licensed under the [GNU General Public License v2](LICENSE).

© 2026 [jiejiebiezheyang](https://github.com/jiejiebiezheyang) &lt;1964234252@qq.com&gt;
