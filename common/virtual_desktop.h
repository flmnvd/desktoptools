#pragma once

#ifndef VIRTUAL_DESKTOP_H
#define VIRTUAL_DESKTOP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace DesktopTools {

enum class SwitchStatus {
    Success,                    // Switched to desktop successfully
    AlreadyOnDesktop,           // Desktop was already the active desktop
    DesktopCreatedAndSwitched,  // Target desktop did not exist, so it was created and switched to
    ComError,                   // COM initialization or query error
    NotFound,                   // Target desktop could not be found or reached
    InvalidArgument             // Invalid desktop index (< 1)
};

struct SwitchResult {
    SwitchStatus status;
    int targetDesktop;          // 1-based index
    int totalDesktops;          // Total desktops available
    std::wstring message;       // Descriptive message or error detail
};

struct DesktopInfo {
    int currentDesktop;         // 1-based index (or -1 if unable to determine)
    int totalDesktops;          // Total count of virtual desktops
};

// Switch to a virtual desktop by 1-based number (e.g. 1 for Desktop 1, 2 for Desktop 2).
// Automatically creates the desktop if it doesn't exist yet.
SwitchResult SwitchToDesktop(int desktopNumber);

// Query the current virtual desktop and total count.
DesktopInfo QueryDesktopInfo();

// Print a message to stdout / parent console if available (interactive console or redirected pipe).
// If launched by hotkey or GUI without console, this is a silent no-op.
void PrintConsole(const std::wstring& message);

// Print an error message to stderr / parent console if available.
void PrintConsoleError(const std::wstring& message);

} // namespace DesktopTools

#endif // VIRTUAL_DESKTOP_H
