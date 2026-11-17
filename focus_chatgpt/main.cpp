#include "chatgpt_manager.h"
#include "virtual_desktop.h"
#include <objbase.h>
#include <shellapi.h>
#include <sstream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    bool verbose = false;
    bool launchIfNeeded = true;
    DWORD timeoutMs = 8000;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help" || arg == L"-h" || arg == L"/?" || arg == L"/help") {
                DesktopTools::PrintConsole(
                    L"focus_chatgpt - Focus or launch Windows Store ChatGPT window\n"
                    L"Designed for hotkey, macro, or programmable keyboard binding.\n\n"
                    L"Usage: focus_chatgpt.exe [options]\n\n"
                    L"Options:\n"
                    L"  -v, --verbose       Print verbose status message to console\n"
                    L"  -i, --info          Print detailed ChatGPT process & window info\n"
                    L"  --no-launch         Only focus existing window, do not start if not running\n"
                    L"  -t, --timeout <ms>  Timeout in milliseconds to wait for window (default: 8000)\n"
                    L"  -h, --help          Show this help message\n"
                );
                LocalFree(argv);
                return 0;
            } else if (arg == L"--verbose" || arg == L"-v" || arg == L"/v") {
                verbose = true;
            } else if (arg == L"--no-launch") {
                launchIfNeeded = false;
            } else if (arg == L"--info" || arg == L"-i" || arg == L"/i") {
                CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
                auto info = ChatGPTManager::QueryChatGPTInfo();
                std::wostringstream ss;
                ss << L"=== ChatGPT Status ===\n"
                   << L"Running:               " << (info.isRunning ? L"Yes" : L"No") << L"\n";
                if (info.isRunning) {
                    ss << L"Process Count:         " << info.processIds.size() << L"\n"
                       << L"Main PID:              " << info.mainPid << L"\n"
                       << L"Executable:            " << info.exePath << L"\n";
                }
                ss << L"AUMID:                 " << info.aumid << L"\n"
                   << L"Main Window HWND:      " << (void*)info.mainWindow << L"\n";
                if (info.mainWindow) {
                    ss << L"Window Title:          " << info.windowTitle << L"\n"
                       << L"Window Class:          " << info.windowClass << L"\n"
                       << L"Visible:               " << (info.isVisible ? L"Yes" : L"No") << L"\n"
                       << L"Minimized:             " << (info.isIconic ? L"Yes" : L"No") << L"\n"
                       << L"Cloaked (Virtual Desk):" << (info.isCloaked ? L"Yes" : L"No") << L"\n"
                       << L"Already Focused:       " << (info.isForeground ? L"Yes" : L"No") << L"\n"
                       << L"Dimensions:            " << (info.windowRect.right - info.windowRect.left)
                       << L"x" << (info.windowRect.bottom - info.windowRect.top) << L"\n";
                }
                DesktopTools::PrintConsole(ss.str());
                CoUninitialize();
                LocalFree(argv);
                return 0;
            } else if ((arg == L"--timeout" || arg == L"-t") && i + 1 < argc) {
                timeoutMs = static_cast<DWORD>(_wtoi(argv[++i]));
            }
        }
        LocalFree(argv);
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    auto result = ChatGPTManager::FocusOrLaunchChatGPT(launchIfNeeded, timeoutMs);

    if (verbose) {
        std::wostringstream ss;
        ss << result.message;
        if (result.hWnd) {
            ss << L" (HWND: " << (void*)result.hWnd << L", PID: " << result.pid << L")";
        }
        ss << L"\n";
        DesktopTools::PrintConsole(ss.str());
    }

    bool success = (result.status == ChatGPTManager::FocusStatus::AlreadyFocused ||
                    result.status == ChatGPTManager::FocusStatus::FocusedExisting ||
                    result.status == ChatGPTManager::FocusStatus::LaunchedAndFocused);

    if (!success && verbose) {
        std::wostringstream ss;
        ss << L"Error: " << result.message << L"\n";
        DesktopTools::PrintConsoleError(ss.str());
    }

    CoUninitialize();
    return success ? 0 : 1;
}
