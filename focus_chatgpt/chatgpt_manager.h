#pragma once

#ifndef CHATGPT_MANAGER_H
#define CHATGPT_MANAGER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

namespace ChatGPTManager {

enum class FocusStatus {
    AlreadyFocused,             // Window was already the foreground window
    FocusedExisting,            // Found existing running window and focused it
    LaunchedAndFocused,         // ChatGPT was not running / hidden; launched/activated and focused
    LaunchFailed,               // Failed to launch ChatGPT
    WindowWaitTimeout,          // Launched, but window did not appear within timeout
    FocusFailed                 // Window found/launched, but failed to focus
};

struct FocusResult {
    FocusStatus status;
    HWND hWnd;
    DWORD pid;
    std::wstring message;
};

struct ChatGPTInfo {
    bool isRunning = false;
    std::vector<DWORD> processIds;
    DWORD mainPid = 0;
    std::wstring exePath;
    std::wstring aumid;
    HWND mainWindow = NULL;
    std::wstring windowTitle;
    std::wstring windowClass;
    bool isVisible = false;
    bool isIconic = false;
    bool isCloaked = false;
    bool isForeground = false;
    RECT windowRect = { 0, 0, 0, 0 };
};

// Query detailed status of ChatGPT processes and windows.
ChatGPTInfo QueryChatGPTInfo();

// Locate the primary ChatGPT top-level application window.
// Checks current desktop as well as the Default interactive desktop.
HWND FindChatGPTWindow();

// Check if any ChatGPT process is currently running.
bool IsChatGPTRunning();

// Retrieve the Application User Model ID (AUMID) for Windows Store ChatGPT.
std::wstring GetChatGPTAumid();

// Launch or activate ChatGPT via COM, Shell AppsFolder, Execution Alias, or URI protocol.
bool LaunchChatGPT(std::wstring* pOutAumid = nullptr, DWORD* pOutPid = nullptr);

// Bring the specified window to the foreground and set focus, handling minimized
// or cloaked windows and bypassing Windows focus-stealing prevention.
bool FocusWindow(HWND hWnd);

// Primary action: Focus existing ChatGPT window, or start ChatGPT and focus it.
FocusResult FocusOrLaunchChatGPT(bool launchIfNeeded = true, DWORD timeoutMs = 8000);

} // namespace ChatGPTManager

#endif // CHATGPT_MANAGER_H
