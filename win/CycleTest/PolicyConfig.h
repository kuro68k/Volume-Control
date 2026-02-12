#include <windows.h>
#include <mmdeviceapi.h>

// Linker instructions
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "MmDevAPI.lib")

interface __declspec(uuid("F8679F50-850A-41CF-9C72-430F290290C8")) IPolicyConfig : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, struct DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, struct DeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR wszDeviceId, ERole eRole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

// The Class ID for the Policy Config Client
class __declspec(uuid("870AF99C-171D-4F9E-AF0D-E63DF40C2BC9")) PolicyConfigClient;

/*
// This defines the vtable layout exactly as Windows expects it
MIDL_INTERFACE("870AF99C-171D-4F15-A20D-37C28FF65345")
IPolicyConfig : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetGroupId(PCWSTR, LPGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDefaultEndpoint(PCWSTR, ERole, IMMDevice**) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR wszDeviceId, ERole eRole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

// GUID for the internal class that implements the interface
class __declspec(uuid("294935CE-F637-4E7C-A41B-ABEDFE54E3E5")) PolicyConfigClient;
//struct __declspec(uuid("62841838-5C97-4D4F-9310-092E47EB7733")) PolicyConfigClient;
*/
