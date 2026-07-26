#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include <Windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include <shellapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <hidsdi.h>
#include <setupapi.h>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

#include "PolicyConfig.h"

#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")

#define ID_TRAY_EXIT            1001
#define ID_TRAY_ICON            100
#define WM_TRAYICON             (WM_USER + 1)
#define WM_AUDIO_DEVICE_CHANGED (WM_USER + 2)
#define WM_CYCLE_AUDIO          (WM_USER + 3)

// Global handles for inter-thread messaging
HWND g_hWnd = NULL;
IMMDeviceEnumerator* g_pEnumerator = NULL;
class CVolumeCallback;
CVolumeCallback* g_pVolumeCallback = NULL;

struct AudioDeviceInfo {
    std::wstring endpointId;   // Unique endpoint ID string
    std::wstring friendlyName; // e.g., "Speakers (USB Audio Device)"
};

void RebindAudioDevice(const std::wstring& deviceId = L"");


void CycleAudioDevice() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;

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

    case WM_AUDIO_DEVICE_CHANGED: {
            wchar_t* pDeviceId = reinterpret_cast<wchar_t*>(lParam);
            std::wstring deviceIdStr = pDeviceId ? pDeviceId : L"";
            if (pDeviceId) {
                free(pDeviceId); // Free allocated memory from _wcsdup
            }

            std::thread([deviceIdStr]() {
                CoInitializeEx(NULL, COINIT_MULTITHREADED);
                RebindAudioDevice(deviceIdStr);
                CoUninitialize();
                }).detach();
            break;
        }

    case WM_CYCLE_AUDIO:
        std::thread([]() {
            CoInitializeEx(NULL, COINIT_MULTITHREADED);
            CycleAudioDevice();
            CoUninitialize();
        }).detach();
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


AudioDeviceInfo GetAudioDeviceInfo(IMMDevice* pDevice) {
    AudioDeviceInfo info;
    if (!pDevice) return info;

    // 1. Get Unique Endpoint ID
    LPWSTR pwszID = NULL;
    if (SUCCEEDED(pDevice->GetId(&pwszID)) && pwszID) {
        info.endpointId = pwszID;
        CoTaskMemFree(pwszID);
    }

    // 2. Query Property Store for Friendly Name
    IPropertyStore* pProps = NULL;
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT var;
        PropVariantInit(&var);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &var)) && var.pwszVal) {
            info.friendlyName = var.pwszVal;
        }
        PropVariantClear(&var);
        pProps->Release();
    }

    return info;
}

// =========================================================
// HID HELPER FUNCTIONS
// =========================================================

// Replace these with your target device's VID and PID
constexpr WORD TARGET_VID = 0x9999;
constexpr WORD TARGET_PID = 0x0283;

// Helper to locate and open a handle to the HID device
HANDLE OpenHidDevice(WORD vid, WORD pid, DWORD desiredAccess = GENERIC_READ | GENERIC_WRITE) {
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
                desiredAccess, // Pass custom access rights here
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
private:
    LONG _cRef;
    HANDLE _hHidReadDevice = INVALID_HANDLE_VALUE;
    HANDLE _hHidWriteDevice = INVALID_HANDLE_VALUE;
    WORD _vid;
    WORD _pid;
    IAudioEndpointVolume* _pVolume;
    IMMDevice* _pDevice = NULL;
    AudioDeviceInfo _currentAudioDevice;
    BYTE _lastReport[8] = { 0 };

    // Async Volume State Queueing
    std::mutex _stateMutex;
    std::condition_variable _cvWriter;
    float _pendingVolume = 0.0f;
    BOOL _pendingMute = FALSE;
    bool _hasPendingUpdate = false;

    std::mutex _hidMutex;             // Protects _hHidDevice during concurrent callbacks
    std::atomic<bool> _running{ true }; // Thread state flag

    std::thread _monitorThread;       // Background thread for USB auto-discovery
    std::thread _readThread;
    std::thread _writerThread;
    std::function<void()> _onCustomButtonPress;

    void DisconnectHid() {
        if (_hHidReadDevice != INVALID_HANDLE_VALUE) {
            CancelIoEx(_hHidReadDevice, NULL);
            CloseHandle(_hHidReadDevice);
            _hHidReadDevice = INVALID_HANDLE_VALUE;
        }
        if (_hHidWriteDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(_hHidWriteDevice);
            _hHidWriteDevice = INVALID_HANDLE_VALUE;
        }
        memset(_lastReport, 0, sizeof(_lastReport));
    }

    bool EnsureConnected() {
        if (_hHidReadDevice != INVALID_HANDLE_VALUE && _hHidWriteDevice != INVALID_HANDLE_VALUE) {
            return true;
        }

        DisconnectHid(); // Clear stale state

        _hHidReadDevice = OpenHidDevice(_vid, _pid, GENERIC_READ);
        _hHidWriteDevice = OpenHidDevice(_vid, _pid, GENERIC_WRITE);

        if (_hHidReadDevice != INVALID_HANDLE_VALUE && _hHidWriteDevice != INVALID_HANDLE_VALUE) {
            memset(_lastReport, 0, sizeof(_lastReport));
            if (_pVolume) {
                float vol = 0.0f;
                BOOL mute = FALSE;
                if (SUCCEEDED(_pVolume->GetMasterVolumeLevelScalar(&vol)) &&
                    SUCCEEDED(_pVolume->GetMute(&mute))) {
                    QueueVolumeUpdate(vol, mute);
                }
            }
            return true;
        }

        DisconnectHid();
        return false;
    }

    uint8_t GetActiveDeviceIndicator() {
        // Example: Match device names or endpoint IDs to assign a device index/color code
        if (_currentAudioDevice.friendlyName.find(L"AMD High Definition Audio Device") != std::wstring::npos) {
            return 0b100;   // red
        }
        else if (_currentAudioDevice.friendlyName.find(L"TOPPING USB DAC") != std::wstring::npos) {
            return 0b010;   // green
        }
        else if (_currentAudioDevice.friendlyName.find(L"HL7BT") != std::wstring::npos) {
            return 0b001;   // blue
        }

        return 0b110;       // Default / Unknown Device, yellow
    }

    // Dedicated USB Write Loop: Ensures intermediate volume events are dropped when slider is moved fast
    void WriterLoop() {
        while (_running) {
            float vol = 0.0f;
            BOOL mute = FALSE;

            {
                std::unique_lock<std::mutex> lock(_stateMutex);
                _cvWriter.wait(lock, [this]() {
                    return !_running || _hasPendingUpdate;
                    });

                if (!_running) break;

                vol = _pendingVolume;
                mute = _pendingMute;
                _hasPendingUpdate = false;
            }

            // Perform USB write on dedicated thread
            SendHidReport(vol, mute);
        }
    }

    void SendHidReport(float fMasterVolume, BOOL bMuted) {
        std::lock_guard<std::mutex> lock(_hidMutex);

        if (_hHidWriteDevice == INVALID_HANDLE_VALUE) {
            if (!EnsureConnected()) return;
        }

        float scaled_volume = fMasterVolume * 5;
        uint8_t colour = bMuted ? 0b01 : 0b10;

        BYTE report[8] = { 0 };
        report[0] = 0x02;                  // Report ID
        report[1] = scaled_volume > 0 ? colour : 0;
        report[2] = scaled_volume >= 1 ? colour : 0;
        report[3] = scaled_volume >= 2 ? colour : 0;
        report[4] = scaled_volume >= 3 ? colour : 0;
        report[5] = scaled_volume >= 4 ? colour : 0;
        report[6] = GetActiveDeviceIndicator();
        report[7] = (bMuted && (scaled_volume != 0)) ? 1 : 0;

        // Skip sending duplicate report
        if (memcmp(report, _lastReport, sizeof(report)) == 0) {
            return;
        }

        BOOL success = HidD_SetOutputReport(_hHidWriteDevice, report, sizeof(report));
        if (success) {
            memcpy(_lastReport, report, sizeof(report));
        }
        else {
            printf("  -> [HID] Send failed (Error: %lu). Resetting connection...\n", GetLastError());
            DisconnectHid();
        }
    }

    void ReadLoop() {
        BYTE inputBuffer[3] = { 0 };
        bool lastButtonState = false;

        while (_running) {
            HANDLE hRead = INVALID_HANDLE_VALUE;
            {
                std::lock_guard<std::mutex> lock(_hidMutex);
                hRead = _hHidReadDevice;
            }

            if (hRead != INVALID_HANDLE_VALUE) {
                DWORD bytesRead = 0;
                // ReadFile runs on read handle without blocking the write handle!
                BOOL result = ReadFile(hRead, inputBuffer, sizeof(inputBuffer), &bytesRead, NULL);

                if (result && bytesRead > 0) {
                    if (inputBuffer[0] == 0x01) {
                        bool currentButtonState = (inputBuffer[2] & 0x02) != 0;
                        if (currentButtonState && !lastButtonState) {
                            if (_onCustomButtonPress) _onCustomButtonPress();
                        }
                        lastButtonState = currentButtonState;
                    }
                }
                else {
                    if (!_running) break;
                    std::lock_guard<std::mutex> lock(_hidMutex);
                    DisconnectHid();
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }

    void MonitorLoop() {
        while (_running) {
            {
                std::lock_guard<std::mutex> lock(_hidMutex);
                if (_hHidReadDevice == INVALID_HANDLE_VALUE) {
                    EnsureConnected();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    void QueueVolumeUpdate(float vol, BOOL mute) {
        {
            std::lock_guard<std::mutex> lock(_stateMutex);
            _pendingVolume = vol;
            _pendingMute = mute;
            _hasPendingUpdate = true;
        }
        _cvWriter.notify_one();
    }

public:
    CVolumeCallback(WORD vid, WORD pid, IAudioEndpointVolume* pVolume, std::function<void()> onCustomBtn)
        : _cRef(1), _hHidReadDevice(INVALID_HANDLE_VALUE), _hHidWriteDevice(INVALID_HANDLE_VALUE), _vid(vid), _pid(pid),
        _pVolume(pVolume), _onCustomButtonPress(onCustomBtn)
    {
        if (_pVolume) {
            _pVolume->AddRef();
        }

        {
            std::lock_guard<std::mutex> lock(_hidMutex);
            EnsureConnected();
        }

        // Start worker threads
        _writerThread = std::thread(&CVolumeCallback::WriterLoop, this);
        _monitorThread = std::thread(&CVolumeCallback::MonitorLoop, this);
        _readThread = std::thread(&CVolumeCallback::ReadLoop, this);
    }

    ~CVolumeCallback() {
        _running = false;

        // 1. Wake up the writer thread if waiting on CV
        _cvWriter.notify_all();

        // 2. Unblock ReadFile stuck waiting on the USB handle
        {
            std::lock_guard<std::mutex> lock(_hidMutex);
            DisconnectHid(); // calls CancelIoEx & CloseHandle
        }

        // 3. Join threads (will now return instantly)
        if (_writerThread.joinable()) _writerThread.join();
        if (_readThread.joinable()) _readThread.join();
        if (_monitorThread.joinable()) _monitorThread.join();

        // 4. Release COM resources
        if (_pVolume) {
            _pVolume->Release();
            _pVolume = NULL;
        }
        if (_pDevice) {
            _pDevice->Release();
            _pDevice = NULL;
        }
    }

    void SetAudioEndpointVolume(IAudioEndpointVolume* pVolume, IMMDevice* pDevice) {
        std::lock_guard<std::mutex> lock(_hidMutex);

        if (_pVolume) {
            _pVolume->UnregisterControlChangeNotify(this);
            _pVolume->Release();
            _pVolume = NULL;
        }
        if (_pDevice) {
            _pDevice->Release();
            _pDevice = NULL;
        }

        _pVolume = pVolume;
        _pDevice = pDevice;

        if (_pVolume) {
            _pVolume->AddRef();
            _pVolume->RegisterControlChangeNotify(this);
        }

        if (_pDevice) {
            _pDevice->AddRef();
            _currentAudioDevice = GetAudioDeviceInfo(_pDevice);

            printf("\n[AUDIO] Default Device Changed to: %ls\n", _currentAudioDevice.friendlyName.c_str());
        }

        // Force _lastReport cache reset so the new indicator byte transmits immediately
        memset(_lastReport, 0, sizeof(_lastReport));

        if (_pVolume) {
            float vol = 0.0f;
            BOOL mute = FALSE;
            if (SUCCEEDED(_pVolume->GetMasterVolumeLevelScalar(&vol)) &&
                SUCCEEDED(_pVolume->GetMute(&mute))) {
                QueueVolumeUpdate(vol, mute);
            }
        }
    }

    // COM Callbacks
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

    // Non-blocking WASAPI Volume Callback
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
        if (pNotify == NULL) return E_INVALIDARG;

        // Simply queue updates instantly without blocking WASAPI thread
        QueueVolumeUpdate(pNotify->fMasterVolume, pNotify->bMuted);

        return S_OK;
    }
};

// Rebinds the volume listener when Windows switches default audio output
void RebindAudioDevice(const std::wstring& deviceId) {
    if (!g_pVolumeCallback) return;

    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr) && pEnumerator) {
        IMMDevice* pDevice = NULL;
        IAudioEndpointVolume* pVolume = NULL;

        // Fetch the specific new device by ID; fall back to default if ID is empty
        if (!deviceId.empty()) {
            hr = pEnumerator->GetDevice(deviceId.c_str(), &pDevice);
        }
        else {
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        }

        if (SUCCEEDED(hr) && pDevice) {
            hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pVolume);
            if (SUCCEEDED(hr) && pVolume) {
                // Rebind listener and immediately sync LED report to new device
                g_pVolumeCallback->SetAudioEndpointVolume(pVolume, pDevice);
                pVolume->Release();
            }
            pDevice->Release();
        }
        pEnumerator->Release();
    }
}

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

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
    {
        if (flow == eRender && (role == eConsole || role == eMultimedia)) {
            printf("\n[CALLBACK] Default Audio Output Device Changed!\n");
            if (g_hWnd && pwstrDefaultDeviceId) {
                // Allocate string copy to pass cleanly across Win32 thread boundaries
                wchar_t* pDeviceIdCopy = _wcsdup(pwstrDefaultDeviceId);
                PostMessage(g_hWnd, WM_AUDIO_DEVICE_CHANGED, 0, (LPARAM)pDeviceIdCopy);
            }
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
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // 1. Register hidden window & Tray Icon
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
    g_hWnd = hWnd;

    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"Volume Control HID Sync");
    Shell_NotifyIcon(NIM_ADD, &nid);

    // 2. Initialize IMMDeviceEnumerator
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioEndpointVolume* pVolume = NULL;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr)) {
        g_pEnumerator = pEnumerator;
        pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    }

    if (pDevice) {
        pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pVolume);
    }

    // 3. Instantiate CVolumeCallback with lambda that posts WM_CYCLE_AUDIO to g_hWnd
    g_pVolumeCallback = new CVolumeCallback(TARGET_VID, TARGET_PID, pVolume, []() {
        if (g_hWnd) {
            PostMessage(g_hWnd, WM_CYCLE_AUDIO, 0, 0);
        }
        });

    // 4. Bind initial audio endpoint
    if (pDevice && pVolume) {
        g_pVolumeCallback->SetAudioEndpointVolume(pVolume, pDevice);
    }

    // 5. Register WASAPI endpoint change notifications
    CMMNotificationClient* pClient = new CMMNotificationClient();
    if (SUCCEEDED(hr) && pEnumerator) {
        pEnumerator->RegisterEndpointNotificationCallback(pClient);
    }

    // 6. Win32 Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 7. Cleanup
    Shell_NotifyIcon(NIM_DELETE, &nid);

    if (pEnumerator && pClient) {
        pEnumerator->UnregisterEndpointNotificationCallback(pClient);
    }

    if (pVolume && g_pVolumeCallback) {
        pVolume->UnregisterControlChangeNotify(g_pVolumeCallback);
    }

    if (pDevice) pDevice->Release();
    if (pVolume) pVolume->Release();
    if (pClient) pClient->Release();
    if (pEnumerator) pEnumerator->Release();
    if (g_pVolumeCallback) g_pVolumeCallback->Release();

    CoUninitialize();
    return 0;
}
