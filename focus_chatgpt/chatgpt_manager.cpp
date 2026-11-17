#include "chatgpt_manager.h"

#include <shobjidl.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <dwmapi.h>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

namespace ChatGPTManager {

namespace {

static const CLSID CLSID_ApplicationActivationManager_ = 
    { 0x45BA127D, 0x10A5, 0x46EA, { 0x8B, 0xB7, 0x56, 0x51, 0xAB, 0xF6, 0x02, 0x31 } };

static const IID IID_IApplicationActivationManager_ = 
    { 0x2e941141, 0x7f97, 0x4756, { 0xba, 0x1d, 0x9d, 0xec, 0xde, 0x89, 0x4a, 0x3d } };

static HDESK g_hInteractiveDesktop = NULL;

// Ensures calling thread is attached to the interactive "Default" desktop
void EnsureInteractiveDesktop() {
    if (g_hInteractiveDesktop) {
        return;
    }

    HDESK hCurDesk = GetThreadDesktop(GetCurrentThreadId());
    if (hCurDesk) {
        wchar_t name[256] = { 0 };
        DWORD needed = 0;
        if (GetUserObjectInformationW(hCurDesk, UOI_NAME, name, sizeof(name), &needed)) {
            if (_wcsicmp(name, L"Default") == 0) {
                return; // Already running on the Default interactive desktop
            }
        }
    }

    g_hInteractiveDesktop = OpenDesktopW(L"Default", 0, FALSE, GENERIC_ALL);
    if (g_hInteractiveDesktop) {
        SetThreadDesktop(g_hInteractiveDesktop);
    }
}

// Helper to determine if a string contains another (case-insensitive)
bool ContainsCaseInsensitive(const std::wstring& str, const std::wstring& substr) {
    if (substr.empty()) return true;
    if (str.length() < substr.length()) return false;
    auto it = std::search(
        str.begin(), str.end(),
        substr.begin(), substr.end(),
        [](wchar_t ch1, wchar_t ch2) {
            return towlower(ch1) == towlower(ch2);
        }
    );
    return it != str.end();
}

bool CheckProcessIsChatGPT(DWORD pid, std::wstring* pOutPath = nullptr) {
    if (pid == 0) return false;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return false;

    wchar_t path[MAX_PATH * 2] = { 0 };
    DWORD size = sizeof(path) / sizeof(path[0]);
    bool match = false;
    if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
        if (pOutPath) *pOutPath = path;
        std::wstring pathStr(path);
        if (ContainsCaseInsensitive(pathStr, L"ChatGPT.exe") ||
            ContainsCaseInsensitive(pathStr, L"OpenAI.Codex")) {
            match = true;
        }
    }
    CloseHandle(hProc);
    return match;
}

struct WindowScoreCandidate {
    HWND hWnd = NULL;
    DWORD pid = 0;
    std::wstring title;
    std::wstring className;
    std::wstring exePath;
    bool isVisible = false;
    bool isIconic = false;
    bool isCloaked = false;
    RECT rect = { 0, 0, 0, 0 };
    int score = 0;
};

BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam) {
    auto* pCandidate = reinterpret_cast<WindowScoreCandidate*>(lParam);
    if (!IsWindow(hWnd)) return TRUE;

    // Must be top-level root window
    if (GetAncestor(hWnd, GA_ROOT) != hWnd) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid == 0) return TRUE;

    wchar_t className[256] = { 0 };
    GetClassNameW(hWnd, className, 256);

    wchar_t title[512] = { 0 };
    GetWindowTextW(hWnd, title, 512);

    std::wstring exePath;
    bool isChatGPTProc = CheckProcessIsChatGPT(pid, &exePath);

    int score = 0;

    // Process matching
    if (isChatGPTProc) {
        score += 100;
    }

    // Class matching
    if (wcscmp(className, L"Chrome_WidgetWin_1") == 0) {
        score += 50;
    } else if (wcscmp(className, L"ApplicationFrameWindow") == 0) {
        score += 20;
    } else if (isChatGPTProc && wcslen(className) > 0) {
        // Filter out helper windows
        if (wcscmp(className, L"Chrome_WidgetWin_0") == 0 ||
            wcscmp(className, L"OleDdeWndClass") == 0 ||
            wcscmp(className, L"MSCTFIME UI") == 0 ||
            wcscmp(className, L"IME") == 0 ||
            ContainsCaseInsensitive(className, L"crashpad") ||
            ContainsCaseInsensitive(className, L"NotifyIcon")) {
            return TRUE;
        }
    }

    // Title matching
    std::wstring titleStr(title);
    if (ContainsCaseInsensitive(titleStr, L"ChatGPT")) {
        score += 60;
    }

    // Window hierarchy
    HWND hOwner = GetWindow(hWnd, GW_OWNER);
    if (hOwner == NULL) {
        score += 30;
    }

    // Styles
    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        score -= 60; // Tool windows are helper/tray popups
    } else {
        score += 20;
    }

    if (exStyle & WS_EX_APPWINDOW) {
        score += 30;
    }

    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    bool vis = (style & WS_VISIBLE) != 0;
    if (vis) {
        score += 40;
    }

    bool iconic = (IsIconic(hWnd) != FALSE);
    if (iconic) {
        score += 35; // Minimized main window
    }

    // Cloaked check (virtual desktops)
    int cloakedVal = 0;
    bool cloaked = false;
    if (SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloakedVal, sizeof(cloakedVal))) && cloakedVal != 0) {
        cloaked = true;
        score += 35; // Window is on another virtual desktop or cloaked by shell
    }

    RECT rc = { 0, 0, 0, 0 };
    GetWindowRect(hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width > 150 && height > 150) {
        score += 25;
    }

    // Update best candidate
    if (score > pCandidate->score) {
        pCandidate->score = score;
        pCandidate->hWnd = hWnd;
        pCandidate->pid = pid;
        pCandidate->title = title;
        pCandidate->className = className;
        pCandidate->exePath = exePath;
        pCandidate->isVisible = vis;
        pCandidate->isIconic = iconic;
        pCandidate->isCloaked = cloaked;
        pCandidate->rect = rc;
    }

    return TRUE;
}

WindowScoreCandidate FindBestChatGPTCandidate() {
    EnsureInteractiveDesktop();

    WindowScoreCandidate candidate;

    // 1. Enumerate on current desktop
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&candidate));

    if (candidate.score >= 120 && candidate.hWnd != NULL) {
        return candidate;
    }

    // 2. Fall back to enumerating the "Default" interactive desktop
    HDESK hDesk = OpenDesktopW(L"Default", 0, FALSE, GENERIC_ALL);
    if (hDesk) {
        SetThreadDesktop(hDesk);
        EnumDesktopWindows(hDesk, EnumWindowsCallback, reinterpret_cast<LPARAM>(&candidate));
        // Keep g_hInteractiveDesktop if not already set
        if (!g_hInteractiveDesktop) {
            g_hInteractiveDesktop = hDesk;
        } else {
            CloseDesktop(hDesk);
        }
    }

    return candidate;
}

} // namespace

std::wstring GetChatGPTAumid() {
    HKEY hKey = NULL;
    LPCWSTR regPath = L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t subKeyName[256];
        DWORD index = 0;
        DWORD nameLen = 256;
        while (RegEnumKeyExW(hKey, index++, subKeyName, &nameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            if (ContainsCaseInsensitive(subKeyName, L"OpenAI.Codex")) {
                wchar_t* pDunder = wcsstr(subKeyName, L"__");
                if (pDunder && wcslen(pDunder) > 2) {
                    std::wstring familyName = L"OpenAI.Codex_" + std::wstring(pDunder + 2);
                    RegCloseKey(hKey);
                    return familyName + L"!App";
                }
            }
            nameLen = 256;
        }
        RegCloseKey(hKey);
    }
    // Default known AUMID for Windows Store ChatGPT
    return L"OpenAI.Codex_2p2nqsd0c76g0!App";
}

bool IsChatGPTRunning() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    bool running = false;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"ChatGPT.exe") == 0) {
                running = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return running;
}

HWND FindChatGPTWindow() {
    auto candidate = FindBestChatGPTCandidate();
    if (candidate.score >= 120 && candidate.hWnd != NULL) {
        return candidate.hWnd;
    }
    return NULL;
}

bool LaunchChatGPT(std::wstring* pOutAumid, DWORD* pOutPid) {
    EnsureInteractiveDesktop();

    std::wstring aumid = GetChatGPTAumid();
    if (pOutAumid) *pOutAumid = aumid;

    // Method 1: IApplicationActivationManager (cleanest official Win32 COM activation)
    IApplicationActivationManager* pAAM = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationActivationManager_, NULL,
                                  CLSCTX_LOCAL_SERVER,
                                  IID_IApplicationActivationManager_,
                                  reinterpret_cast<void**>(&pAAM));
    if (SUCCEEDED(hr) && pAAM) {
        DWORD pid = 0;
        hr = pAAM->ActivateApplication(aumid.c_str(), NULL, AO_NONE, &pid);
        pAAM->Release();
        if (SUCCEEDED(hr)) {
            if (pOutPid) *pOutPid = pid;
            return true;
        }
    }

    // Method 2: ShellExecute shell:AppsFolder\<AUMID>
    std::wstring shellPath = L"shell:AppsFolder\\" + aumid;
    HINSTANCE hInst = ShellExecuteW(NULL, L"open", shellPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hInst) > 32) {
        return true;
    }

    // Method 3: Execution Alias (chatgpt-desktop.exe in %LOCALAPPDATA%\Microsoft\WindowsApps)
    wchar_t localAppData[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        std::wstring alias = std::wstring(localAppData) + L"\\Microsoft\\WindowsApps\\chatgpt-desktop.exe";
        if (GetFileAttributesW(alias.c_str()) != INVALID_FILE_ATTRIBUTES) {
            hInst = ShellExecuteW(NULL, L"open", alias.c_str(), NULL, NULL, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(hInst) > 32) {
                return true;
            }
        }
    }

    // Method 4: Protocol handler (codex://)
    hInst = ShellExecuteW(NULL, L"open", L"codex://", NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hInst) > 32) {
        return true;
    }

    return false;
}

bool FocusWindow(HWND hWnd) {
    EnsureInteractiveDesktop();

    if (!IsWindow(hWnd)) return false;

    // Restore if minimized; show if hidden
    if (IsIconic(hWnd)) {
        ShowWindow(hWnd, SW_RESTORE);
    } else if (!IsWindowVisible(hWnd)) {
        ShowWindow(hWnd, SW_SHOW);
    }

    SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    HWND hForeground = GetForegroundWindow();
    DWORD foreThread = hForeground ? GetWindowThreadProcessId(hForeground, NULL) : 0;
    DWORD curThread = GetCurrentThreadId();
    DWORD targetThread = GetWindowThreadProcessId(hWnd, NULL);

    if (foreThread && foreThread != curThread) {
        AttachThreadInput(curThread, foreThread, TRUE);
    }
    if (targetThread && targetThread != curThread) {
        AttachThreadInput(curThread, targetThread, TRUE);
    }

    DWORD targetPid = 0;
    GetWindowThreadProcessId(hWnd, &targetPid);
    AllowSetForegroundWindow(targetPid != 0 ? targetPid : ASFW_ANY);

    // Synthesize an Alt key tap to unlock foreground rights in User32
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MENU;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_MENU;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));

    BringWindowToTop(hWnd);
    BOOL setForeOk = SetForegroundWindow(hWnd);
    SetFocus(hWnd);

    // SwitchToThisWindow switches across virtual desktops if needed
    typedef void (WINAPI* PFN_SwitchToThisWindow)(HWND, BOOL);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        PFN_SwitchToThisWindow pfn = reinterpret_cast<PFN_SwitchToThisWindow>(
            GetProcAddress(hUser32, "SwitchToThisWindow"));
        if (pfn) {
            pfn(hWnd, TRUE);
        }
    }

    if (targetThread && targetThread != curThread) {
        AttachThreadInput(curThread, targetThread, FALSE);
    }
    if (foreThread && foreThread != curThread) {
        AttachThreadInput(curThread, foreThread, FALSE);
    }

    return (setForeOk != FALSE) || (GetForegroundWindow() == hWnd) || IsWindow(hWnd);
}

ChatGPTInfo QueryChatGPTInfo() {
    EnsureInteractiveDesktop();

    ChatGPTInfo info;
    info.aumid = GetChatGPTAumid();

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"ChatGPT.exe") == 0) {
                    info.isRunning = true;
                    info.processIds.push_back(pe.th32ProcessID);
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    auto candidate = FindBestChatGPTCandidate();
    if (candidate.score >= 120 && candidate.hWnd != NULL) {
        info.mainWindow = candidate.hWnd;
        info.mainPid = candidate.pid;
        info.windowTitle = candidate.title;
        info.windowClass = candidate.className;
        info.exePath = candidate.exePath;
        info.isVisible = candidate.isVisible;
        info.isIconic = candidate.isIconic;
        info.isCloaked = candidate.isCloaked;
        info.windowRect = candidate.rect;
        info.isForeground = (GetForegroundWindow() == candidate.hWnd);
    } else if (!info.processIds.empty()) {
        info.mainPid = info.processIds[0];
        CheckProcessIsChatGPT(info.mainPid, &info.exePath);
    }

    return info;
}

FocusResult FocusOrLaunchChatGPT(bool launchIfNeeded, DWORD timeoutMs) {
    EnsureInteractiveDesktop();

    FocusResult res;
    res.status = FocusStatus::FocusFailed;
    res.hWnd = NULL;
    res.pid = 0;

    // 1. Check for existing window
    auto candidate = FindBestChatGPTCandidate();
    if (candidate.score >= 120 && candidate.hWnd != NULL) {
        HWND hWnd = candidate.hWnd;
        res.pid = candidate.pid;
        res.hWnd = hWnd;

        HWND hFore = GetForegroundWindow();
        if (hFore == hWnd && !IsIconic(hWnd)) {
            res.status = FocusStatus::AlreadyFocused;
            res.message = L"ChatGPT window is already in the foreground and focused.";
            return res;
        }

        if (FocusWindow(hWnd)) {
            res.status = FocusStatus::FocusedExisting;
            res.message = L"Successfully brought existing ChatGPT window to the foreground.";
            return res;
        } else {
            res.status = FocusStatus::FocusFailed;
            res.message = L"Found ChatGPT window, but SetForegroundWindow failed.";
            return res;
        }
    }

    // 2. If no window found, verify if launch is requested
    if (!launchIfNeeded) {
        res.status = FocusStatus::FocusFailed;
        res.message = L"ChatGPT window not found, and --no-launch was specified.";
        return res;
    }

    bool wasRunning = IsChatGPTRunning();

    std::wstring aumid;
    DWORD launchPid = 0;
    if (!LaunchChatGPT(&aumid, &launchPid)) {
        res.status = FocusStatus::LaunchFailed;
        res.message = L"Failed to launch or activate ChatGPT via Windows Store.";
        return res;
    }

    // 3. Poll for the window to appear
    DWORD startTick = GetTickCount();
    while (GetTickCount() - startTick < timeoutMs) {
        Sleep(100);
        auto newCandidate = FindBestChatGPTCandidate();
        if (newCandidate.score >= 120 && newCandidate.hWnd != NULL) {
            HWND hWnd = newCandidate.hWnd;
            res.pid = newCandidate.pid;
            res.hWnd = hWnd;

            // Wait briefly for the window to finish rendering
            Sleep(150);
            FocusWindow(hWnd);

            res.status = FocusStatus::LaunchedAndFocused;
            if (wasRunning) {
                res.message = L"Activated ChatGPT from background and focused its window.";
            } else {
                res.message = L"Started ChatGPT instance and focused its window.";
            }
            return res;
        }
    }

    res.status = FocusStatus::WindowWaitTimeout;
    res.message = L"ChatGPT instance launched, but window did not appear within timeout.";
    return res;
}

} // namespace ChatGPTManager
