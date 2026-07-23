#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include <Windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include <shellapi.h>

#include <hidsdi.h>
#include <setupapi.h>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

#include "PolicyConfig.h"

#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_ICON 100


// Window procedure to handle context menu events from the system tray icon
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        // Right-click on tray icon
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

            // Required so clicking away from the menu closes it
            SetForegroundWindow(hwnd);

            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        // Clicked "Exit" in the context menu
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


void CycleAudioDevice() {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    IMMDeviceEnumerator* pEnumerator = NULL;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* pCollection = NULL;
        hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);

        IMMDevice* pDefaultDevice = NULL;
        LPWSTR pDefaultID = NULL;
        pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDefaultDevice);
        pDefaultDevice->GetId(&pDefaultID);

        UINT count;
        pCollection->GetCount(&count);

        for (UINT i = 0; i < count; i++) {
            IMMDevice* pDevice = NULL;
            LPWSTR pID = NULL;
            pCollection->Item(i, &pDevice);
            pDevice->GetId(&pID);

            if (wcscmp(pID, pDefaultID) == 0) {
                // Select the next device
                IMMDevice* pNextDevice = NULL;
                pCollection->Item((i + 1) % count, &pNextDevice);
                LPWSTR pNextID = NULL;
                pNextDevice->GetId(&pNextID);

                // Apply the change
                IPolicyConfig* pConfig = NULL;
                IUnknown* pUnk = NULL;

                // 1. Create the object as a generic Unknown
                //HRESULT hr = CoCreateInstance(__uuidof(PolicyConfigClient),
                //    NULL, CLSCTX_ALL,
                //    IID_IUnknown, (void**)&pUnk);

                //if (SUCCEEDED(hr)) {
                //    // 2. Query for the specific interface
                //    hr = pUnk->QueryInterface(__uuidof(IPolicyConfig), (void**)&pConfig);
                //    pUnk->Release();
                //}
                //else {
                //    // If THIS still returns 0x80040154, the CLSID itself is wrong for your OS build
                //    printf("Failed to create Client: 0x%08X\n", hr);
                //}

                HRESULT hr_config = CoCreateInstance(__uuidof(PolicyConfigClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfig), (void**)&pConfig);
                if (FAILED(hr_config)) {
                    // Print this hex code. 
                    // 0x80040154 = Class not registered
                    // 0x80004002 = No such interface supported
                    printf("CoCreateInstance failed with HR: 0x%08X\n", hr_config);
                }
                if (pConfig) {
                    pConfig->SetDefaultEndpoint(pNextID, eMultimedia);
                    pConfig->Release();
                }

                CoTaskMemFree(pNextID);
                pNextDevice->Release();
                CoTaskMemFree(pID);
                pDevice->Release();
                break;
            }
            CoTaskMemFree(pID);
            pDevice->Release();
        }

        // Clean up
        CoTaskMemFree(pDefaultID);
        pDefaultDevice->Release();
        pCollection->Release();
        pEnumerator->Release();
    }
    CoUninitialize();
}

// =========================================================
// HID HELPER FUNCTIONS
// =========================================================

// Replace these with your target device's VID and PID
constexpr WORD TARGET_VID = 0x9999;
constexpr WORD TARGET_PID = 0x0283;

// Helper to locate and open a handle to the HID device
HANDLE OpenHidDevice(WORD vid, WORD pid) {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    SP_DEVICE_INTERFACE_DATA devInterfaceData = { 0 };
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    HANDLE hDevice = INVALID_HANDLE_VALUE;

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &hidGuid, i, &devInterfaceData); ++i) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, NULL, 0, &requiredSize, NULL);

        std::vector<BYTE> detailDataBuffer(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA devDetailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detailDataBuffer.data());
        devDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, devDetailData, requiredSize, NULL, NULL)) {
            // Open handle with write access
            HANDLE hCandidate = CreateFile(
                devDetailData->DevicePath,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );

            if (hCandidate != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attrib = { 0 };
                attrib.Size = sizeof(HIDD_ATTRIBUTES);

                if (HidD_GetAttributes(hCandidate, &attrib)) {
                    if (attrib.VendorID == vid && attrib.ProductID == pid) {
                        hDevice = hCandidate;
                        break; // Matching device found!
                    }
                }
                CloseHandle(hCandidate);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return hDevice;
}

// ---------------------------------------------------------
// THE VOLUME LISTENER CLASS
// ---------------------------------------------------------
// ---------------------------------------------------------
// THE VOLUME LISTENER CLASS (WITH THREAD-SAFE AUTO-RECONNECT)
// ---------------------------------------------------------
class CVolumeCallback : public IAudioEndpointVolumeCallback
{
    LONG _cRef;
    HANDLE _hHidDevice;
    WORD _vid;
    WORD _pid;
    IAudioEndpointVolume* _pVolume;

    std::mutex _hidMutex;             // Protects _hHidDevice during concurrent callbacks
    std::thread _monitorThread;       // Background thread for USB auto-discovery
    std::atomic<bool> _running{ true }; // Thread state flag

    void DisconnectHid() {
        // Call within lock or ensure mutex is held
        if (_hHidDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(_hHidDevice);
            _hHidDevice = INVALID_HANDLE_VALUE;
            printf("[HID] Connection closed / device disconnected.\n");
        }
    }

    bool EnsureConnected() {
        if (_hHidDevice != INVALID_HANDLE_VALUE) {
            return true;
        }

        _hHidDevice = OpenHidDevice(_vid, _pid);
        if (_hHidDevice != INVALID_HANDLE_VALUE) {
            printf("[HID] Connected to target device successfully.\n");
            SendCurrentVolumeReportInternal();
            return true;
        }

        return false;
    }

    // Unlocked internal helper used when mutex is already held
    void SendCurrentVolumeReportInternal() {
        if (!_pVolume) return;

        float fMasterVolume = 0.0f;
        BOOL bMuted = FALSE;

        _pVolume->GetMasterVolumeLevelScalar(&fMasterVolume);
        _pVolume->GetMute(&bMuted);

        float scaled_volume = fMasterVolume * 5;
        uint8_t colour = bMuted ? 0b01 : 0b10;
        BYTE report[8] = { 0 };
        report[0] = 0x02;                  // Report ID
        report[1] = scaled_volume > 0 ? colour : 0;
        report[2] = scaled_volume >= 1 ? colour : 0;
        report[3] = scaled_volume >= 2 ? colour : 0;
        report[4] = scaled_volume >= 3 ? colour : 0;
        report[5] = scaled_volume >= 4 ? colour : 0;
        report[6] = 0;
        report[7] = (bMuted && (scaled_volume != 0)) ? 1 : 0;

        BOOL success = HidD_SetOutputReport(_hHidDevice, report, sizeof(report));
        if (success) {
            printf("  -> [HID] Sent volume report (%zu bytes)\n", sizeof(report));
        }
        else {
            printf("  -> [HID] Send failed (Error: %lu). Resetting connection...\n", GetLastError());
            DisconnectHid();
        }
    }

    // Background loop to detect physical USB insertion
    void MonitorLoop() {
        while (_running) {
            {
                std::lock_guard<std::mutex> lock(_hidMutex);
                if (_hHidDevice == INVALID_HANDLE_VALUE) {
                    EnsureConnected(); // Attempts reconnect & syncs volume if plugged in
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

public:
    CVolumeCallback(WORD vid, WORD pid, IAudioEndpointVolume* pVolume)
        : _cRef(1), _hHidDevice(INVALID_HANDLE_VALUE), _vid(vid), _pid(pid), _pVolume(pVolume)
    {
        if (_pVolume) {
            _pVolume->AddRef();
        }

        // Initial connect attempt
        {
            std::lock_guard<std::mutex> lock(_hidMutex);
            EnsureConnected();
        }

        // Start background auto-discovery thread
        _monitorThread = std::thread(&CVolumeCallback::MonitorLoop, this);
    }

    ~CVolumeCallback() {
        // Stop background thread cleanly before teardown
        _running = false;
        if (_monitorThread.joinable()) {
            _monitorThread.join();
        }

        std::lock_guard<std::mutex> lock(_hidMutex);
        DisconnectHid();

        if (_pVolume) {
            _pVolume->Release();
            _pVolume = NULL;
        }
    }

    // Public method for sending explicit volume data (e.g. from OnNotify)
    void SendVolumeReport(float fMasterVolume, BOOL bMuted) {
        std::lock_guard<std::mutex> lock(_hidMutex);

        if (!EnsureConnected()) return;

        float scaled_volume = fMasterVolume * 5;
        uint8_t colour = bMuted ? 0b01 : 0b10;
        BYTE report[8] = { 0 };
        report[0] = 0x02;                  // Report ID
        report[1] = scaled_volume > 0 ? colour : 0;
        report[2] = scaled_volume >= 1 ? colour : 0;
        report[3] = scaled_volume >= 2 ? colour : 0;
        report[4] = scaled_volume >= 3 ? colour : 0;
        report[5] = scaled_volume >= 4 ? colour : 0;
        report[6] = 0;
        report[7] = (bMuted && (scaled_volume != 0)) ? 1 : 0;

        BOOL success = HidD_SetOutputReport(_hHidDevice, report, sizeof(report));
        if (success) {
            printf("  -> [HID] Sent volume report (%zu bytes)\n", sizeof(report));
        }
        else {
            printf("  -> [HID] Send failed (Error: %lu). Retrying connection...\n", GetLastError());
            DisconnectHid();
            // Retry once immediately with a fresh handle if reconnected
            if (EnsureConnected()) {
                HidD_SetOutputReport(_hHidDevice, report, sizeof(report));
            }
        }
    }

    // IUnknown Implementation
    ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&_cRef); }
    ULONG STDMETHODCALLTYPE Release() {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef) delete this;
        return ulRef;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) {
        if (IID_IUnknown == riid) {
            AddRef();
            *ppvInterface = (IUnknown*)this;
        }
        else if (__uuidof(IAudioEndpointVolumeCallback) == riid) {
            AddRef();
            *ppvInterface = (IAudioEndpointVolumeCallback*)this;
        }
        else {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    // ---------------------------------------------------------
    // The Volume Notification Method
    // ---------------------------------------------------------
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
        if (pNotify == NULL) return E_INVALIDARG;

        printf("\n[VOLUME CALLBACK]\n");
        printf("  -> Muted:    %s\n", pNotify->bMuted ? "YES" : "NO");
        printf("  -> Volume:   %.0f%%\n", pNotify->fMasterVolume * 100.0f);

        SendVolumeReport(pNotify->fMasterVolume, pNotify->bMuted);

        return S_OK;
    }
};

// ---------------------------------------------------------
// THE LISTENER CLASS
// ---------------------------------------------------------
class CMMNotificationClient : public IMMNotificationClient
{
    LONG _cRef;

public:
    CMMNotificationClient() : _cRef(1) {}
    ~CMMNotificationClient() {}

    // IUnknown Implementation (Required for COM)
    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface)
    {
        if (IID_IUnknown == riid)
        {
            AddRef();
            *ppvInterface = (IUnknown*)this;
        }
        else if (__uuidof(IMMNotificationClient) == riid)
        {
            AddRef();
            *ppvInterface = (IMMNotificationClient*)this;
        }
        else
        {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    // ---------------------------------------------------------
    // IMMNotificationClient Implementation
    // ---------------------------------------------------------

    // This is the one you care about!
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
    {
        if (flow == eRender && role == eMultimedia) {
            printf("\n[CALLBACK] Default Device Changed!\n");
            printf("  -> New Device ID: %ls\n", pwstrDefaultDeviceId ? pwstrDefaultDeviceId : L"(null)");
        }
        return S_OK;
    }

    // Other required methods (stubs)
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) { return S_OK; }
};

int main()
{
    CoInitialize(NULL);

    // 1. Register a hidden window class to receive tray messages
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VolumeControlTrayClass";
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(
        0, wc.lpszClassName, L"Volume Control HID Sync",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL
    );

    // 2. Add System Tray Icon
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Uses standard Windows app icon
    wcscpy_s(nid.szTip, L"Volume Control HID Sync");
    Shell_NotifyIcon(NIM_ADD, &nid);

    // 3. Initialize Audio & HID Stack
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioEndpointVolume* pVolume = NULL;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr)) {
        pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    }

    if (pDevice) {
        pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pVolume);
    }

    CVolumeCallback* pVolumeCallback = new CVolumeCallback(TARGET_VID, TARGET_PID, pVolume);

    if (pVolume) {
        pVolume->RegisterControlChangeNotify(pVolumeCallback);
    }

    CMMNotificationClient* pClient = new CMMNotificationClient();
    if (SUCCEEDED(hr)) {
        pEnumerator->RegisterEndpointNotificationCallback(pClient);
    }

    // 4. Win32 Message Loop (Replaces getchar())
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 5. Cleanup Tray Icon & COM Interfaces
    Shell_NotifyIcon(NIM_DELETE, &nid);

    if (pEnumerator && pClient) {
        pEnumerator->UnregisterEndpointNotificationCallback(pClient);
    }

    if (pVolume) {
        pVolume->UnregisterControlChangeNotify(pVolumeCallback);
        pVolume->Release();
    }
    if (pDevice) pDevice->Release();
    if (pClient) pClient->Release();
    if (pEnumerator) pEnumerator->Release();
    if (pVolumeCallback) pVolumeCallback->Release();

    CoUninitialize();
    return 0;
}
