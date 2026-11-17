#include "virtual_desktop.h"
#include <shellapi.h>
#include <sstream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    int targetDesktop = 2;
    bool verbose = false;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help" || arg == L"-h" || arg == L"/?" || arg == L"/help") {
                DesktopTools::PrintConsole(
                    L"switch_desktop_2 - Switch directly to Windows Desktop 2\n"
                    L"Designed for SXS-K811 Model II mini keyboard hotkey binding.\n\n"
                    L"Usage: switch_desktop_2.exe [options] [desktop_number]\n\n"
                    L"Options:\n"
                    L"  -v, --verbose   Print verbose status message to console\n"
                    L"  -i, --info      Print current desktop information\n"
                    L"  -h, --help      Show this help message\n"
                );
                LocalFree(argv);
                return 0;
            } else if (arg == L"--verbose" || arg == L"-v" || arg == L"/v") {
                verbose = true;
            } else if (arg == L"--info" || arg == L"-i" || arg == L"/i") {
                auto info = DesktopTools::QueryDesktopInfo();
                std::wostringstream ss;
                ss << L"Current Desktop: " << info.currentDesktop
                   << L", Total Desktops: " << info.totalDesktops << L"\n";
                DesktopTools::PrintConsole(ss.str());
                LocalFree(argv);
                return 0;
            } else if (!arg.empty() && iswdigit(arg[0])) {
                targetDesktop = _wtoi(arg.c_str());
            }
        }
        LocalFree(argv);
    }

    auto result = DesktopTools::SwitchToDesktop(targetDesktop);

    if (verbose) {
        std::wostringstream ss;
        ss << result.message << L" (Total Desktops: " << result.totalDesktops << L")\n";
        DesktopTools::PrintConsole(ss.str());
    }

    bool ok = (result.status == DesktopTools::SwitchStatus::Success ||
               result.status == DesktopTools::SwitchStatus::AlreadyOnDesktop ||
               result.status == DesktopTools::SwitchStatus::DesktopCreatedAndSwitched);

    if (!ok && verbose) {
        std::wostringstream ss;
        ss << L"Error: " << result.message << L"\n";
        DesktopTools::PrintConsoleError(ss.str());
    }

    return ok ? 0 : 1;
}
