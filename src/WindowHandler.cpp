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
 * @file WindowHandler.cpp
 * @brief 窗口处理器实现文件
 *
 * 实现了窗口枚举和跨进程 Display Affinity 控制功能。
 * 核心技术：通过解析 PE 导出表定位函数地址，生成 shellcode，使用 CreateRemoteThread 注入执行。
 */

#include "WindowHandler.h"
#include <dwmapi.h>
#include <psapi.h>
#include <vector>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "psapi.lib")

/**
  * @brief EnumWindows 回调函数的用户数据结构
  */
struct EnumWindowsData {
	std::vector<WindowInfo> *windows; ///< 存储窗口信息的向量指针
};

/**
 * @brief EnumWindows 回调函数
 * @param hWnd 当前枚举到的窗口句柄
 * @param lParam 用户自定义数据 (EnumWindowsData*)
 * @return TRUE 继续枚举，FALSE 停止枚举
 *
 * 过滤逻辑：
 * 1. 跳过不可见窗口
 * 2. 跳过 cloaked 窗口（如最小化的 UWP 应用）
 * 3. 跳过无标题窗口
 */
static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	EnumWindowsData *data = reinterpret_cast<EnumWindowsData *>(lParam);

	// 过滤不可见窗口
	if (!IsWindowVisible(hWnd)) {
		return TRUE;
	}

	// 过滤 cloaked 窗口（DWMWA_CLOAKED 用于检测隐藏的 UWP 窗口等）
	int pvAttribute = 0;
	DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &pvAttribute, sizeof(pvAttribute));
	if (pvAttribute > 0) {
		return TRUE;
	}

	// 过滤无标题窗口
	int length = GetWindowTextLengthW(hWnd);
	if (length == 0) {
		return TRUE;
	}

	// 获取窗口标题
	std::wstring title(length + 1, L'\0');
	GetWindowTextW(hWnd, &title[0], static_cast<int>(title.size()));
	title.resize(wcslen(title.c_str()));

	// 将 "Program Manager" 重命名为更友好的名称
	if (title == L"Program Manager") {
		title = L"Desktop and Icons";
	}

	// 构建窗口信息并添加到列表
	WindowInfo info;
	info.handle = hWnd;
	info.title = title;
	info.stillExists = true;
	data->windows->push_back(info);

	return TRUE;
}

/**
 * @brief 获取所有可见窗口列表
 * @return 包含所有可见窗口信息的向量
 */
std::vector<WindowInfo> WindowHandler::GetVisibleWindows()
{
	std::vector<WindowInfo> windows;
	EnumWindowsData data;
	data.windows = &windows;
	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
	return windows;
}

/**
 * @brief 获取窗口的 Display Affinity 状态
 * @param hWnd 目标窗口句柄
 * @return Display Affinity 值
 *
 * 返回值含义：
 * - 0: 窗口无保护，可以被捕获
 * - >0: 窗口受保护，不会被屏幕共享/录屏捕获
 */
int WindowHandler::GetWindowDisplayAffinity(HWND hWnd)
{
	DWORD dwAffinity = 0;
	::GetWindowDisplayAffinity(hWnd, &dwAffinity);
	return static_cast<int>(dwAffinity);
}

/**
 * @brief 从目标进程读取 32 位整数
 * @param procHandle 进程句柄
 * @param addr 内存地址
 * @param is32Bit 目标进程是否为 32 位（未使用，保留用于扩展）
 * @return 读取的 32 位整数值
 */
int WindowHandler::ReadInt32(HANDLE procHandle, unsigned long long addr, bool is32Bit)
{
	(void)is32Bit; // 未使用参数
	BYTE buffer[8] = {0};
	SIZE_T bytesRead = 0;
	ReadProcessMemory(procHandle, reinterpret_cast<LPCVOID>(addr), buffer, 8, &bytesRead);
	return *reinterpret_cast<int *>(buffer);
}

/**
 * @brief 在目标进程中查找 SetWindowDisplayAffinity 函数地址
 * @param procHandle 进程句柄
 * @param is32Bit 目标进程是否为 32 位
 * @return 函数地址，失败返回 0
 *
 * 实现原理：
 * 1. 枚举目标进程加载的所有模块
 * 2. 找到 user32.dll 模块
 * 3. 解析 PE 文件格式的导出表
 * 4. 查找 "SetWindowDisplayAffinity" 导出函数
 *
 * PE 结构解析：
 * - DOS Header (e_lfanew 指向 NT Headers)
 * - NT Headers -> Optional Header -> Data Directories
 * - Export Directory 包含函数名、序号和地址表
 */
unsigned long long WindowHandler::FindSetWindowDisplayAffinityAddress(HANDLE procHandle, bool is32Bit)
{
	HMODULE hMods[1024];
	DWORD cbNeeded = 0;

	// 枚举目标进程的所有模块
	if (EnumProcessModulesEx(procHandle, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL) == 0) {
		return 0;
	}

	int moduleCount = cbNeeded / sizeof(HMODULE);

	// 遍历所有模块
	for (int i = 0; i < moduleCount; i++) {
		WCHAR szModName[MAX_PATH];
		if (GetModuleFileNameExW(procHandle, hMods[i], szModName, MAX_PATH) == 0) {
			continue;
		}

		// 检查是否为 user32.dll
		std::wstring modName = szModName;
		std::transform(modName.begin(), modName.end(), modName.begin(), ::towlower);

		if (modName.find(L"user32.dll") == std::wstring::npos) {
			continue;
		}

		// 获取模块信息
		MODULEINFO modInfo;
		if (GetModuleInformation(procHandle, hMods[i], &modInfo, sizeof(modInfo)) == 0) {
			continue;
		}

		unsigned long long baseAddr = reinterpret_cast<unsigned long long>(modInfo.lpBaseOfDll);

		// === 解析 PE 导出表 ===

		// 读取 DOS Header 中的 e_lfanew（NT Headers 偏移）
		int e_lfanew = ReadInt32(procHandle, baseAddr + 0x3C, is32Bit);
		unsigned long long ntHeaders = baseAddr + e_lfanew;

		// 定位 Optional Header
		unsigned long long optionalHeader = ntHeaders + 0x18;

		// Data Directory 偏移：x86=0x60, x64=0x70
		unsigned long long dataDirectory = optionalHeader + (is32Bit ? 0x60 : 0x70);

		// 读取导出表 RVA 并转换为 VA
		unsigned long long exportDirectory = baseAddr + ReadInt32(procHandle, dataDirectory, is32Bit);

		// 读取导出表的各个子表地址
		unsigned long long names =
			baseAddr + ReadInt32(procHandle, exportDirectory + 0x20, is32Bit); // 函数名表
		unsigned long long ordinals =
			baseAddr + ReadInt32(procHandle, exportDirectory + 0x24, is32Bit); // 序号表
		unsigned long long functions =
			baseAddr + ReadInt32(procHandle, exportDirectory + 0x1C, is32Bit); // 函数地址表
		int numFuncs = ReadInt32(procHandle, exportDirectory + 0x18, is32Bit);     // 导出函数数量

		// 遍历导出函数
		for (int j = 0; j < numFuncs; j++) {
			// 读取函数名
			unsigned long long offset = ReadInt32(procHandle, names + j * 4, is32Bit);
			char nameBuffer[32] = {0};
			SIZE_T bytesRead = 0;
			ReadProcessMemory(procHandle, reinterpret_cast<LPCVOID>(baseAddr + offset), nameBuffer, 32,
					  &bytesRead);

			// 查找 SetWindowDisplayAffinity 函数
			if (strncmp(nameBuffer, "SetWindowDisplayAffinity", 24) == 0) {
				// 通过序号表获取函数地址
				int ordinal = ReadInt32(procHandle, ordinals + j * 2, is32Bit) & 0xFFFF;
				unsigned long long funcAddr =
					baseAddr + ReadInt32(procHandle, functions + ordinal * 4, is32Bit);
				return funcAddr;
			}
		}
	}

	return 0;
}

/**
 * @brief 设置窗口的 Display Affinity（核心函数）
 * @param hWnd 目标窗口句柄
 * @param dwAffinity 亲和性值
 *
 * 实现原理：
 * 1. 获取目标窗口所属进程 ID
 * 2. 打开目标进程
 * 3. 检测目标进程架构（x86/x64）
 * 4. 在目标进程中查找 SetWindowDisplayAffinity 函数地址
 * 5. 生成对应的 shellcode
 * 6. 在目标进程中分配内存并写入 shellcode
 * 7. 创建远程线程执行 shellcode
 * 8. 等待执行完成并清理资源
 *
 * Shellcode 说明：
 * - x86: 使用 push 参数 + mov eax,addr + call eax
 * - x64: 使用 sub rsp + mov rcx/rdx/rax + call rax + add rsp
 */
void WindowHandler::SetWindowDisplayAffinity(HWND hWnd, int dwAffinity)
{
	// 获取窗口所属进程 ID
	DWORD procId = 0;
	GetWindowThreadProcessId(hWnd, &procId);

	// 打开目标进程，请求必要的权限
	HANDLE procHandle = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD |
						PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
					TRUE, procId);
	if (procHandle == nullptr) {
		return;
	}

	// 检测目标进程是否为 32 位（在 64 位系统上运行）
	BOOL is32Bit = FALSE;
	IsWow64Process(procHandle, &is32Bit);

	// 查找 SetWindowDisplayAffinity 函数地址
	unsigned long long funcAddr = FindSetWindowDisplayAffinityAddress(procHandle, is32Bit);
	if (funcAddr == 0) {
		CloseHandle(procHandle);
		return;
	}

	// === 生成 Shellcode ===
	std::vector<BYTE> asmCode;

	if (is32Bit) {
		// === x86 Shellcode ===
		// 调用约定：stdcall，参数从右到左入栈
		// SetWindowDisplayAffinity(hWnd, dwAffinity)

		// push dwAffinity (第二个参数)
		asmCode.push_back(0x68);
		size_t pos = asmCode.size();
		asmCode.resize(pos + 4);
		*reinterpret_cast<int *>(&asmCode[pos]) = dwAffinity;

		// push hWnd (第一个参数)
		asmCode.push_back(0x68);
		pos = asmCode.size();
		asmCode.resize(pos + 4);
		*reinterpret_cast<unsigned int *>(&asmCode[pos]) = reinterpret_cast<unsigned long long>(hWnd) &
								   0xFFFFFFFF;

		// mov eax, SetWindowDisplayAffinityAddr
		asmCode.push_back(0xB8);
		pos = asmCode.size();
		asmCode.resize(pos + 4);
		*reinterpret_cast<unsigned int *>(&asmCode[pos]) = static_cast<unsigned int>(funcAddr);

		// call eax
		asmCode.push_back(0xFF);
		asmCode.push_back(0xD0);
	} else {
		// === x64 Shellcode ===
		// 调用约定：Microsoft x64，前四个参数通过 rcx, rdx, r8, r9 传递
		// SetWindowDisplayAffinity(rcx=hWnd, rdx=dwAffinity)

		// sub rsp, 0x30 (分配栈空间，32 字节影子空间 + 对齐)
		asmCode.insert(asmCode.end(), {0x48, 0x83, 0xEC, 0x30});

		// mov rcx, hWnd (第一个参数)
		asmCode.push_back(0x48);
		asmCode.push_back(0xB9);
		size_t pos = asmCode.size();
		asmCode.resize(pos + 8);
		*reinterpret_cast<unsigned long long *>(&asmCode[pos]) = reinterpret_cast<unsigned long long>(hWnd);

		// mov rdx, dwAffinity (第二个参数)
		asmCode.push_back(0x48);
		asmCode.push_back(0xBA);
		pos = asmCode.size();
		asmCode.resize(pos + 8);
		*reinterpret_cast<unsigned long long *>(&asmCode[pos]) = static_cast<unsigned long long>(dwAffinity);

		// mov rax, SetWindowDisplayAffinityAddr
		asmCode.push_back(0x48);
		asmCode.push_back(0xB8);
		pos = asmCode.size();
		asmCode.resize(pos + 8);
		*reinterpret_cast<unsigned long long *>(&asmCode[pos]) = funcAddr;

		// call rax
		asmCode.push_back(0xFF);
		asmCode.push_back(0xD0);

		// add rsp, 0x30 (恢复栈)
		asmCode.insert(asmCode.end(), {0x48, 0x83, 0xC4, 0x30});
	}

	// ret (返回)
	asmCode.push_back(0xC3);

	// === 在目标进程中分配内存 ===
	LPVOID codePtr =
		VirtualAllocEx(procHandle, nullptr, asmCode.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (codePtr == nullptr) {
		CloseHandle(procHandle);
		return;
	}

	// 写入 shellcode
	SIZE_T bytesWritten = 0;
	WriteProcessMemory(procHandle, codePtr, asmCode.data(), asmCode.size(), &bytesWritten);

	// 创建远程线程执行 shellcode
	HANDLE thread = CreateRemoteThread(procHandle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(codePtr),
					   nullptr, 0, nullptr);
	if (thread != nullptr) {
		// 等待执行完成（最多 10 秒）
		WaitForSingleObject(thread, 10000);
		CloseHandle(thread);
	}

	// 清理：释放分配的内存
	VirtualFreeEx(procHandle, codePtr, 0, MEM_RELEASE);
	CloseHandle(procHandle);
}
