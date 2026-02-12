#include <iostream>
#include <vector>
#include <string>
#include <Windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>

#include "PolicyConfig.h"

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

// ---------------------------------------------------------
// THE VOLUME LISTENER CLASS
// ---------------------------------------------------------
class CVolumeCallback : public IAudioEndpointVolumeCallback
{
    LONG _cRef;

public:
    CVolumeCallback() : _cRef(1) {}
    ~CVolumeCallback() {}

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

        // Channels (Left/Right)
        for (UINT i = 0; i < pNotify->nChannels; i++) {
            printf("  -> Channel %u: %.2f\n", i, pNotify->afChannelVolumes[i]);
        }
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

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioEndpointVolume* pVolume = NULL;
    CVolumeCallback* pVolumeCallback = new CVolumeCallback();

    // 1. Get Enumerator
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    // 2. Get Default Device (Speakers)
    pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);

    // 3. Activate the Volume Interface
    //    Note: IID_IAudioEndpointVolume is defined in endpointvolume.h
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pVolume);

    // 4. Register the callback
    pVolume->RegisterControlChangeNotify(pVolumeCallback);

    if (SUCCEEDED(hr))
    {
        // 1. Create our listener
        CMMNotificationClient* pClient = new CMMNotificationClient();

        // 2. Register it
        hr = pEnumerator->RegisterEndpointNotificationCallback(pClient);

        if (SUCCEEDED(hr))
        {
            printf("Listening for audio device changes... (Press Enter to quit)\n");
            getchar(); // Block here so the app stays alive to receive callbacks

            // 3. Unregister before quitting
            pEnumerator->UnregisterEndpointNotificationCallback(pClient);
        }

        // 5. Cleanup
        if (pVolume) {
            pVolume->UnregisterControlChangeNotify(pVolumeCallback);
            pVolume->Release();
        }
        if (pDevice) pDevice->Release();
        if (pClient) pClient->Release();
        if (pEnumerator) pEnumerator->Release();
        pVolumeCallback->Release(); // Release our local ref
    }

    CoUninitialize();

    //CycleAudioDevice();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
