# Desktop Tools

A collection of tiny, high-performance native Windows desktop utilities designed for hotkey and macro binding (e.g. for the **SXS-K811 Model II** mini programmable keyboard).

## Features

- **Tiny & Lightweight:** Each executable is ~41 KB, statically linked with zero external runtime DLL dependencies.
- **Silent & Zero-Flicker:** Built with the Windows GUI subsystem (`WIN32`), so no black console/cmd prompt window ever flashes or steals focus when hotkeys are pressed.
- **Automatic Desktop Creation:** If Windows Desktop 2 does not exist yet when switching to it, the utility automatically creates it and switches to it seamlessly.
- **Multi-OS Support:** Compatible with Windows 10 (1809 through 22H2) and Windows 11 (21H2, 22H2, 23H2, 24H2).
- **Clean Taskbar Focus:** Activates the shell input cleanly to avoid taskbar icon flashing or focus glitches.
- **Terminal Friendly:** If executed from PowerShell or Command Prompt with `--info`, `--verbose`, or `--help`, it attaches to the existing console and prints diagnostics without needing a separate console build.

---

## Subprojects & Executables

The project is structured as a single CMake solution with separate subprojects:

| Subproject | Executable | Description |
|---|---|---|
| `switch_desktop_1` | `switch_desktop_1.exe` | Switches immediately to Windows Desktop 1. |
| `switch_desktop_2` | `switch_desktop_2.exe` | Switches immediately to Windows Desktop 2 (creates it if needed). |
| `focus_chatgpt` | `focus_chatgpt.exe` | Focuses the ChatGPT window (restoring from minimized/background) or launches ChatGPT from Windows Store if not started. |
| `common` | `desktop_tools_common.lib` | Shared core library managing internal COM interfaces and desktop switching. |

---

## Directory Structure

```text
z:\desktop-tools\
├── CMakeLists.txt              # Root CMake configuration
├── AGENTS.md                   # Build and environment rules
├── README.md                   # Documentation and usage guide
├── build.bat                   # One-click build script using Visual Studio 14.0
├── common\                     # Core desktop switching library
│   ├── CMakeLists.txt
│   ├── virtual_desktop.h
│   └── virtual_desktop.cpp
├── switch_desktop_1\           # Subproject: Switch to Desktop 1
│   ├── CMakeLists.txt
│   ├── main.cpp
│   └── version.rc
├── switch_desktop_2\           # Subproject: Switch to Desktop 2
│   ├── CMakeLists.txt
│   ├── main.cpp
│   └── version.rc
└── focus_chatgpt\              # Subproject: Focus or launch ChatGPT
    ├── CMakeLists.txt
    ├── chatgpt_manager.h
    ├── chatgpt_manager.cpp
    ├── main.cpp
    └── version.rc
```

---

## Building the Project

As specified in `AGENTS.md`, the project uses **CMake** and **Visual Studio 14.0** (`C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin`).

### Option 1: One-Click Build Script

Simply run:
```cmd
z:\desktop-tools\build.bat
```

### Option 2: Command Line (CMake + MSVC 14.0)

Open a Command Prompt and run:
```cmd
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64 8.1
cd /d z:\desktop-tools
mkdir build
cd build
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_SYSTEM_VERSION=8.1 ..
cmake --build . --config Release
```

The compiled binaries will be placed in:
- `z:\desktop-tools\build\bin\switch_desktop_1.exe`
- `z:\desktop-tools\build\bin\switch_desktop_2.exe`
- `z:\desktop-tools\build\bin\focus_chatgpt.exe`

---

## SXS-K811 Model II Keyboard Binding

To map the utilities to keys on your **SXS-K811 Model II** mini keyboard:

1. Open the companion macro / key-mapping software for your SXS-K811 keyboard.
2. Select the desired key on the keypad interface (for example, Key 1).
3. Set the action / function to **"Launch Program"** or **"Run Application"**.
4. Browse and select `z:\desktop-tools\build\bin\switch_desktop_1.exe`.
5. Select another key (for example, Key 2), set action to **"Launch Program"**, and select `z:\desktop-tools\build\bin\switch_desktop_2.exe`.
6. Select another key (for example, Key 3), set action to **"Launch Program"**, and select `z:\desktop-tools\build\bin\focus_chatgpt.exe`.
7. Save and apply the configuration to the keypad.

Now, tapping the mapped keys on your mini keyboard will switch instantly between Desktops or bring ChatGPT to the foreground without any console popups or interruptions.

---

## Command-Line Options (Optional)

All utilities can also be invoked from a terminal for testing or diagnostics:

```powershell
# Display help and usage
switch_desktop_1.exe --help
focus_chatgpt.exe --help

# Query current virtual desktop and total count
switch_desktop_1.exe --info

# Inspect ChatGPT process and window status
focus_chatgpt.exe --info

# Verbose execution with diagnostics
focus_chatgpt.exe --verbose
```
