#include "virtual_desktop.h"

#include <servprov.h>
#include <ole2.h>
#include <shobjidl.h>
#include <objectarray.h>
#include <sstream>

#undef CreateDesktop

namespace DesktopTools {

namespace {

// Internal CLSIDs
static const CLSID CLSID_ImmersiveShell_ = 
    { 0xC2F03A33, 0x21F5, 0x47FA, { 0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39 } };

static const CLSID CLSID_VirtualDesktopManagerInternal_ = 
    { 0xC5E0CDCA, 0x7B6E, 0x41B2, { 0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B } };

// Windows 10 IIDs
static const IID IID_IVirtualDesktop_Win10_ = 
    { 0xFF72FFDD, 0xBE7E, 0x43FC, { 0x9C, 0x03, 0xAD, 0x81, 0x68, 0x1E, 0x88, 0xE4 } };

static const IID IID_IVirtualDesktopManagerInternal_Win10_ = 
    { 0xF31574D6, 0xB682, 0x4CDC, { 0xBD, 0x56, 0x18, 0x27, 0x86, 0x0A, 0xBE, 0xC6 } };

// Windows 11 IIDs
static const IID IID_IVirtualDesktop_Win11_ = 
    { 0x3F07F4BE, 0xB107, 0x441A, { 0xAF, 0x0F, 0x39, 0xD8, 0x25, 0x29, 0x07, 0x2C } };

static const IID IID_IVirtualDesktopManagerInternal_Win11_22H2_ = 
    { 0x53F5CA0B, 0x158F, 0x4124, { 0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27 } };

static const IID IID_IVirtualDesktopManagerInternal_Win11_21H2_ = 
    { 0xB2F925B9, 0x5A0F, 0x4D2E, { 0x9F, 0x4D, 0x2B, 0x15, 0x07, 0x59, 0x3C, 0x10 } };

MIDL_INTERFACE("FF72FFDD-BE7E-43FC-9C03-AD81681E88E4")
IVirtualDesktopCore : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(void* pView, int* pfVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetID(GUID* pGuid) = 0;
};

MIDL_INTERFACE("F31574D6-B682-4CDC-BD56-1827860ABEC6")
IVirtualDesktopManagerInternalCore : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT* pCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(void* pView, IUnknown* pDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(void* pView, int* pfCanMove) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(IUnknown** ppDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(IObjectArray** ppArray) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(IUnknown* pDesktopReference, int direction, IUnknown** ppAdjacentDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(IUnknown* pDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateDesktop(IUnknown** ppDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(IUnknown* pDesktop, IUnknown* pFallbackDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindDesktop(GUID* pDesktopId, IUnknown** ppDesktop) = 0;
};

// RAII helper to clean up COM interfaces safely
template <typename T>
class ComReleaseGuard {
public:
    ComReleaseGuard(T* ptr = nullptr) : ptr_(ptr) {}
    ~ComReleaseGuard() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }
    T* get() const { return ptr_; }
    T** operator&() { return &ptr_; }
    T* operator->() const { return ptr_; }
    operator bool() const { return ptr_ != nullptr; }
    void reset(T* ptr = nullptr) {
        if (ptr_) ptr_->Release();
        ptr_ = ptr;
    }
    T* detach() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
private:
    T* ptr_;
    ComReleaseGuard(const ComReleaseGuard&) = delete;
    ComReleaseGuard& operator=(const ComReleaseGuard&) = delete;
};

// RAII helper to initialize and uninitialize COM safely
class CoInitGuard {
public:
    CoInitGuard() : hr_(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)) {}
    ~CoInitGuard() {
        if (SUCCEEDED(hr_)) {
            CoUninitialize();
        }
    }
    bool succeeded() const { return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE; }
    HRESULT hr() const { return hr_; }
private:
    HRESULT hr_;
    CoInitGuard(const CoInitGuard&) = delete;
    CoInitGuard& operator=(const CoInitGuard&) = delete;
};

struct ManagerContext {
    ComReleaseGuard<IServiceProvider> serviceProvider;
    ComReleaseGuard<IVirtualDesktopManagerInternalCore> manager;
    IID activeDesktopIID = IID_IVirtualDesktop_Win10_;
    bool isWindows11 = false;
};

bool InitializeManager(ManagerContext& ctx, std::wstring& errorMsg) {
    IServiceProvider* pSp = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ImmersiveShell_, NULL, CLSCTX_LOCAL_SERVER, 
                                  IID_IServiceProvider, (void**)&pSp);
    if (FAILED(hr) || !pSp) {
        std::wostringstream ss;
        ss << L"Failed to create ImmersiveShell instance (hr = 0x" << std::hex << hr << L")";
        errorMsg = ss.str();
        return false;
    }
    ctx.serviceProvider.reset(pSp);

    IVirtualDesktopManagerInternalCore* pMgr = nullptr;

    // 1. Try Windows 10
    hr = ctx.serviceProvider->QueryService(CLSID_VirtualDesktopManagerInternal_,
                                           IID_IVirtualDesktopManagerInternal_Win10_,
                                           (void**)&pMgr);
    if (SUCCEEDED(hr) && pMgr) {
        ctx.manager.reset(pMgr);
        ctx.activeDesktopIID = IID_IVirtualDesktop_Win10_;
        ctx.isWindows11 = false;
        return true;
    }

    // 2. Try Windows 11 (22H2 / 23H2 / 24H2)
    hr = ctx.serviceProvider->QueryService(CLSID_VirtualDesktopManagerInternal_,
                                           IID_IVirtualDesktopManagerInternal_Win11_22H2_,
                                           (void**)&pMgr);
    if (SUCCEEDED(hr) && pMgr) {
        ctx.manager.reset(pMgr);
        ctx.activeDesktopIID = IID_IVirtualDesktop_Win11_;
        ctx.isWindows11 = true;
        return true;
    }

    // 3. Try Windows 11 (21H2)
    hr = ctx.serviceProvider->QueryService(CLSID_VirtualDesktopManagerInternal_,
                                           IID_IVirtualDesktopManagerInternal_Win11_21H2_,
                                           (void**)&pMgr);
    if (SUCCEEDED(hr) && pMgr) {
        ctx.manager.reset(pMgr);
        ctx.activeDesktopIID = IID_IVirtualDesktop_Win11_;
        ctx.isWindows11 = true;
        return true;
    }

    std::wostringstream ss;
    ss << L"Failed to query IVirtualDesktopManagerInternal interface (hr = 0x" << std::hex << hr << L")";
    errorMsg = ss.str();
    return false;
}

// Seamless switch activation: prevents taskbar flashing & ensures clean focus transfer
void PrepareDesktopSwitchFocus() {
    HWND hWndProgman = FindWindowW(L"Progman", L"Program Manager");
    DWORD desktopThreadId = hWndProgman ? GetWindowThreadProcessId(hWndProgman, NULL) : 0;
    DWORD foregroundThreadId = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    DWORD currentThreadId = GetCurrentThreadId();

    if (desktopThreadId != 0 && foregroundThreadId != 0 && foregroundThreadId != currentThreadId) {
        AttachThreadInput(desktopThreadId, currentThreadId, TRUE);
        AttachThreadInput(foregroundThreadId, currentThreadId, TRUE);
        SetForegroundWindow(hWndProgman);
        AttachThreadInput(foregroundThreadId, currentThreadId, FALSE);
        AttachThreadInput(desktopThreadId, currentThreadId, FALSE);
    }
}

void RestoreDesktopSwitchFocus() {
    HWND hWndProgman = FindWindowW(L"Progman", L"Program Manager");
    if (hWndProgman) {
        ShowWindow(hWndProgman, SW_MINIMIZE);
    }
}

void WriteHandle(DWORD nStdHandle, const std::wstring& text) {
    if (text.empty()) return;

    HANDLE hStd = GetStdHandle(nStdHandle);
    DWORD fileType = (hStd && hStd != INVALID_HANDLE_VALUE) ? GetFileType(hStd) : FILE_TYPE_UNKNOWN;

    // If redirected to pipe, file, or character device
    if (fileType == FILE_TYPE_PIPE || fileType == FILE_TYPE_DISK || fileType == FILE_TYPE_CHAR) {
        DWORD written = 0;
        int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), NULL, 0, NULL, NULL);
        if (len > 0) {
            std::string utf8(len, 0);
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), &utf8[0], len, NULL, NULL);
            WriteFile(hStd, utf8.c_str(), static_cast<DWORD>(utf8.length()), &written, NULL);
        }
        return;
    }

    // Otherwise, attach to parent console if available
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        HANDLE hCon = CreateFileW(nStdHandle == STD_ERROR_HANDLE ? L"CONERR$" : L"CONOUT$",
                                  GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hCon != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteConsoleW(hCon, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
            CloseHandle(hCon);
        }
    }
}

} // namespace

void PrintConsole(const std::wstring& message) {
    WriteHandle(STD_OUTPUT_HANDLE, message);
}

void PrintConsoleError(const std::wstring& message) {
    WriteHandle(STD_ERROR_HANDLE, message);
}

DesktopInfo QueryDesktopInfo() {
    DesktopInfo info = { -1, 0 };
    CoInitGuard coInit;
    if (!coInit.succeeded()) {
        return info;
    }

    std::wstring err;
    ManagerContext ctx;
    if (!InitializeManager(ctx, err)) {
        return info;
    }

    // Determine current desktop GUID
    GUID currentGuid = { 0 };
    ComReleaseGuard<IUnknown> currentDesktopObj;
    HRESULT hr = ctx.manager->GetCurrentDesktop(&currentDesktopObj);
    if (SUCCEEDED(hr) && currentDesktopObj) {
        ComReleaseGuard<IVirtualDesktopCore> curVD;
        if (SUCCEEDED(currentDesktopObj->QueryInterface(ctx.activeDesktopIID, (void**)&curVD))) {
            curVD->GetID(&currentGuid);
        }
    }

    // Retrieve desktop collection
    ComReleaseGuard<IObjectArray> desktops;
    hr = ctx.manager->GetDesktops(&desktops);
    if (SUCCEEDED(hr) && desktops) {
        UINT count = 0;
        desktops->GetCount(&count);
        info.totalDesktops = static_cast<int>(count);

        for (UINT i = 0; i < count; ++i) {
            ComReleaseGuard<IVirtualDesktopCore> vd;
            if (SUCCEEDED(desktops->GetAt(i, ctx.activeDesktopIID, (void**)&vd))) {
                GUID g = { 0 };
                vd->GetID(&g);
                if (IsEqualGUID(g, currentGuid)) {
                    info.currentDesktop = static_cast<int>(i + 1);
                }
            }
        }
    }

    return info;
}

SwitchResult SwitchToDesktop(int desktopNumber) {
    SwitchResult result;
    result.status = SwitchStatus::ComError;
    result.targetDesktop = desktopNumber;
    result.totalDesktops = 0;

    if (desktopNumber < 1) {
        result.status = SwitchStatus::InvalidArgument;
        result.message = L"Desktop number must be greater than or equal to 1.";
        return result;
    }

    CoInitGuard coInit;
    if (!coInit.succeeded()) {
        std::wostringstream ss;
        ss << L"CoInitializeEx failed (hr = 0x" << std::hex << coInit.hr() << L")";
        result.message = ss.str();
        return result;
    }

    UINT targetIndex = static_cast<UINT>(desktopNumber - 1);
    ManagerContext ctx;
    if (!InitializeManager(ctx, result.message)) {
        return result;
    }

    // Get current desktops
    ComReleaseGuard<IObjectArray> desktops;
    HRESULT hr = ctx.manager->GetDesktops(&desktops);
    if (FAILED(hr) || !desktops) {
        std::wostringstream ss;
        ss << L"GetDesktops failed (hr = 0x" << std::hex << hr << L")";
        result.message = ss.str();
        return result;
    }

    UINT count = 0;
    desktops->GetCount(&count);
    result.totalDesktops = static_cast<int>(count);

    bool created = false;
    // If target desktop doesn't exist yet, automatically create desktops up to target
    if (count <= targetIndex) {
        while (count <= targetIndex) {
            ComReleaseGuard<IUnknown> newDesktop;
            hr = ctx.manager->CreateDesktop(&newDesktop);
            if (FAILED(hr)) {
                std::wostringstream ss;
                ss << L"CreateDesktop failed when attempting to create desktop " << (count + 1)
                   << L" (hr = 0x" << std::hex << hr << L")";
                result.message = ss.str();
                return result;
            }
            count++;
            created = true;
        }

        // Re-query desktops list after creation
        desktops.reset();
        hr = ctx.manager->GetDesktops(&desktops);
        if (FAILED(hr) || !desktops) {
            result.message = L"Failed to reload desktops array after creation.";
            return result;
        }
        desktops->GetCount(&count);
        result.totalDesktops = static_cast<int>(count);
    }

    // Retrieve target desktop interface
    ComReleaseGuard<IVirtualDesktopCore> targetDesktop;
    hr = desktops->GetAt(targetIndex, ctx.activeDesktopIID, (void**)&targetDesktop);
    if (FAILED(hr) || !targetDesktop) {
        result.status = SwitchStatus::NotFound;
        std::wostringstream ss;
        ss << L"Failed to obtain desktop interface at index " << targetIndex 
           << L" (hr = 0x" << std::hex << hr << L")";
        result.message = ss.str();
        return result;
    }

    // Check if already on this desktop
    ComReleaseGuard<IUnknown> curUnknown;
    if (SUCCEEDED(ctx.manager->GetCurrentDesktop(&curUnknown)) && curUnknown) {
        ComReleaseGuard<IVirtualDesktopCore> curVD;
        if (SUCCEEDED(curUnknown->QueryInterface(ctx.activeDesktopIID, (void**)&curVD))) {
            GUID curGuid = { 0 };
            GUID targetGuid = { 0 };
            curVD->GetID(&curGuid);
            targetDesktop->GetID(&targetGuid);
            if (IsEqualGUID(curGuid, targetGuid)) {
                result.status = SwitchStatus::AlreadyOnDesktop;
                result.message = L"Already active on requested desktop.";
                return result;
            }
        }
    }

    // Prepare focus to prevent taskbar icon flashing
    PrepareDesktopSwitchFocus();

    // Perform desktop switch
    hr = ctx.manager->SwitchDesktop(targetDesktop.get());
    RestoreDesktopSwitchFocus();

    if (FAILED(hr)) {
        std::wostringstream ss;
        ss << L"SwitchDesktop failed (hr = 0x" << std::hex << hr << L")";
        result.message = ss.str();
        return result;
    }

    result.status = created ? SwitchStatus::DesktopCreatedAndSwitched : SwitchStatus::Success;
    std::wostringstream ss;
    ss << L"Successfully switched to Windows Desktop " << desktopNumber;
    result.message = ss.str();

    return result;
}

} // namespace DesktopTools
