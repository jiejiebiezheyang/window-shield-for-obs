/*
window-shield-for-obs
Copyright (C) <2026> <Cyan> <ltpcloud@qq.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

/**
 * @file WindowHandler.h
 * @brief 窗口处理器头文件 - 提供窗口枚举和 Display Affinity 控制功能
 *
 * 本模块实现了跨进程调用 SetWindowDisplayAffinity API 的功能，
 * 允许隐藏任意窗口，使其不被屏幕共享/录屏软件捕获。
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>

/**
  * @struct WindowInfo
  * @brief 窗口信息结构体
  *
  * 存储窗口的基本信息和状态
  */
struct WindowInfo {
	HWND handle;        ///< 窗口句柄
	std::wstring title; ///< 窗口标题
	bool stillExists;   ///< 窗口是否仍然存在（用于检测已关闭的窗口）
};

/**
 * @class WindowHandler
 * @brief 窗口处理器类
 *
 * 提供静态方法用于：
 * - 枚举系统中所有可见窗口
 * - 查询窗口的 Display Affinity 状态
 * - 跨进程设置窗口的 Display Affinity
 */
class WindowHandler {
public:
	/**
     * @brief 获取所有可见窗口列表
     * @return 包含所有可见窗口信息的向量
     *
     * 过滤条件：
     * - 窗口必须可见 (IsWindowVisible)
     * - 窗口未被 cloaked (DwmGetWindowAttribute)
     * - 窗口有标题
     */
	static std::vector<WindowInfo> GetVisibleWindows();

	/**
     * @brief 获取窗口的 Display Affinity 状态
     * @param hWnd 目标窗口句柄
     * @return Display Affinity 值，>0 表示窗口受保护
     */
	static int GetWindowDisplayAffinity(HWND hWnd);

	/**
     * @brief 设置窗口的 Display Affinity
     * @param hWnd 目标窗口句柄
     * @param dwAffinity 亲和性值 (0x0=无限制, 0x11=排除捕获)
     *
     * 通过跨进程代码注入实现，允许控制其他进程的窗口。
     * 支持 x86 和 x64 目标进程。
     */
	static void SetWindowDisplayAffinity(HWND hWnd, int dwAffinity);

private:
	/**
     * @brief 从目标进程读取 32 位整数
     * @param procHandle 进程句柄
     * @param addr 内存地址
     * @param is32Bit 目标进程是否为 32 位
     * @return 读取的 32 位整数值
     */
	static int ReadInt32(HANDLE procHandle, unsigned long long addr, bool is32Bit);

	/**
     * @brief 在目标进程中查找 SetWindowDisplayAffinity 函数地址
     * @param procHandle 进程句柄
     * @param is32Bit 目标进程是否为 32 位
     * @return 函数地址，失败返回 0
     *
     * 通过解析 user32.dll 的 PE 导出表来定位函数地址。
     */
	static unsigned long long FindSetWindowDisplayAffinityAddress(HANDLE procHandle, bool is32Bit);
};
